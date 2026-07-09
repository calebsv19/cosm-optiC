#include "editor/object_editor.h"
#include "editor/object_editor_internal.h"
#include "scene/object_manager.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/object_editor_object_ops.h"
#include "editor/scene_editor_tool_state.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/object_editor_panels.h"
#include "app/animation.h"
#include "app/data_paths.h"
#include "camera/camera.h"
#include "geo/shape_adapter.h"
#include "geo/shape_library.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"
#include "material/material_manager.h"
#include "ui/shared_theme_font_adapter.h"
#include <dirent.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define MAX_POLYGON_POINTS 10  // Limit for custom polygons

int width, height;

int handleRadius = 5;
bool draggingRotationHandle = false;
double lastWorldX = 0.0;
double lastWorldY = 0.0;
bool viewportPanDragging = false;
int viewportPanLastMouseX = 0;
int viewportPanLastMouseY = 0;
bool renderHandles = true;
#define MAX_ASSET_LIST 128
#define ASSET_PANEL_WIDTH 200
#define ASSET_ROW_HEIGHT 22
#define PANEL_HEADER_HEIGHT 26
#define PANEL_PADDING 6
#define PANEL_MAX_HEIGHT 220


ShapeMode shapeMode = SHAPE_SQUARE;  // Default to square mode
bool polygonCreationActive = false;
double polygonPoints[MAX_POLYGON_POINTS][2];
int polygonPointCount = 0;

extern SDL_Rect addButton;  // the existing definition from scene_editor.c
extern SDL_Rect deleteButton;  // the existing definition from scene_editor.c
extern SDL_Rect selectButton;  // the visible shared tool button
SDL_Rect objectHandlesButton = {0};
static SDL_Rect objectListRowRects[MAX_OBJECTS];
static int objectListRowIndices[MAX_OBJECTS];
static int objectListRowCount = 0;
// Nested Buttons for Add Mode (Placed to the left of Add Button)
SDL_Rect circleButton;  
SDL_Rect squareButton;
SDL_Rect polygonButton;
// Buttons for Polygon Creation Mode (Placed to the left of Polygon Button)
SDL_Rect confirmPolygonButton;  
SDL_Rect cancelPolygonButton;

SDL_Rect assetPanelRect;
SDL_Rect assetToggleRect;
SDL_Rect assetCollapseRect;
bool showImports = false;
int selectedAssetIndex = -1;
ShapeAssetLibrary assetLib = {0};
char importNames[MAX_ASSET_LIST][256];
int importCount = 0;
SDL_Rect materialPanelRect;
SDL_Rect materialCollapseRect;
int selectedMaterialIndex = -1;
bool assetsCollapsed = false;
bool materialsCollapsed = false;
int assetScroll = 0;
int materialScroll = 0;
ObjectEditorPanelSliderKind activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;

bool ObjectEditorAddToolActive(void) {
    return SceneEditorToolStateToolIsActive(SCENE_EDITOR_TOOL_ADD);
}

bool ObjectEditorDeleteToolActive(void) {
    return SceneEditorToolStateToolIsActive(SCENE_EDITOR_TOOL_DELETE);
}

bool ObjectEditorPointInRect(int x, int y, const SDL_Rect* rect) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

void ObjectEditorClearObjectListRows(void) {
    memset(objectListRowRects, 0, sizeof(objectListRowRects));
    memset(objectListRowIndices, 0, sizeof(objectListRowIndices));
    objectListRowCount = 0;
}

void ObjectEditorRegisterObjectListRow(int object_index, SDL_Rect rect) {
    if (objectListRowCount >= MAX_OBJECTS) return;
    if (object_index < 0 || object_index >= sceneSettings.objectCount) return;
    objectListRowRects[objectListRowCount] = rect;
    objectListRowIndices[objectListRowCount] = object_index;
    objectListRowCount += 1;
}

int ObjectEditorObjectListIndexAtPoint(int mx, int my) {
    for (int i = 0; i < objectListRowCount; ++i) {
        if (ObjectEditorPointInRect(mx, my, &objectListRowRects[i])) {
            return objectListRowIndices[i];
        }
    }
    return -1;
}

static void ObjectEditorDrawPaneButton(SDL_Renderer* renderer,
                                       SDL_Rect rect,
                                       const char* label,
                                       bool active) {
    RayTracingThemePalette palette = {0};
    SDL_Color fill = {180, 180, 180, 255};
    SDL_Color border = {95, 95, 112, 255};
    SDL_Color text = {0, 0, 0, 255};
    if (!renderer || rect.w <= 0 || rect.h <= 0 || !label) return;
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        fill = active ? ray_tracing_theme_resolve_button_active_fill(palette) : palette.button_fill;
        border = palette.panel_border;
        text = ray_tracing_theme_choose_button_text(fill, palette);
    } else if (active) {
        fill = (SDL_Color){70, 140, 215, 255};
        text = (SDL_Color){245, 247, 250, 255};
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

static SDL_Color ObjectEditorColorFromPackedRGB(int packed, Uint8 alpha) {
    SDL_Color color = {0};
    color.r = (Uint8)((packed >> 16) & 0xFF);
    color.g = (Uint8)((packed >> 8) & 0xFF);
    color.b = (Uint8)(packed & 0xFF);
    color.a = alpha;
    return color;
}

static int ObjectEditorPackedColorWithChannel(int packed_color,
                                              ObjectEditorPanelSliderKind kind,
                                              Uint8 channel_value) {
    Uint8 r = (Uint8)((packed_color >> 16) & 0xFF);
    Uint8 g = (Uint8)((packed_color >> 8) & 0xFF);
    Uint8 b = (Uint8)(packed_color & 0xFF);
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_R) r = channel_value;
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_G) g = channel_value;
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_B) b = channel_value;
    return SceneObjectPackRGBBytes(r, g, b);
}

void ObjectEditorApplySliderValueToSelected(ObjectEditorPanelSliderKind kind,
                                            double slider_value) {
    Uint8 channel_value = 0u;
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount) {
        return;
    }
    if (slider_value < 0.0) slider_value = 0.0;
    if (slider_value > 1.0) slider_value = 1.0;

    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_R ||
        kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_G ||
        kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_B) {
        channel_value = (Uint8)lround(slider_value * 255.0);
        ObjectEditorAssignColorToSelected(
            ObjectEditorPackedColorWithChannel(sceneSettings.sceneObjects[selected_index].color,
                                               kind,
                                               channel_value));
        return;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_A) {
        ObjectEditorAssignAlphaToSelected(slider_value);
        return;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH) {
        ObjectEditorAssignEmissiveStrengthToSelected(slider_value);
    }
}

const char* ShapeAssetDir(void) {
    static char resolved_dir[PATH_MAX];
    const char* dir = getenv("SHAPE_ASSET_DIR");
    return ray_tracing_resolve_shape_asset_dir(dir, resolved_dir, sizeof(resolved_dir));
}

Camera BuildObjectEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

CameraPoint ScreenToWorldObjectEditor(const Camera* camera, int sx, int sy) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, sx, sy);
}

void PanViewportObjectByScreenDelta(int prev_x, int prev_y, int cur_x, int cur_y) {
    Camera previewCam = BuildObjectEditorCamera();
    CameraPoint prev = ScreenToWorldObjectEditor(&previewCam, prev_x, prev_y);
    CameraPoint cur = ScreenToWorldObjectEditor(&previewCam, cur_x, cur_y);
    CameraPan(&sceneSettings.camera, prev.x - cur.x, prev.y - cur.y);
}

static SDL_Point WorldToScreenObjectEditor(const Camera* camera, double wx, double wy) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    CameraPoint screen = SpaceModeAdapter_WorldToScreen(&view_ctx, wx, wy);
    return (SDL_Point){(int)lround(screen.x), (int)lround(screen.y)};
}

static void RenderCameraViewportOverlay(SDL_Renderer* renderer, double margin) {
    SDL_Rect rect = {
        (int)lrint(margin),
        (int)lrint(margin),
        sceneSettings.windowWidth - (int)lrint(margin) * 2,
        sceneSettings.windowHeight - (int)lrint(margin) * 2
    };
    if (rect.w < 0) rect.w = 0;
    if (rect.h < 0) rect.h = 0;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawRect(renderer, &rect);
}

void RefreshAssetLibrary(void) {
    shape_library_free(&assetLib);
    shape_library_load_dir(ShapeAssetDir(), &assetLib);
    if (selectedAssetIndex >= (int)assetLib.count) selectedAssetIndex = -1;
}

static void RefreshImportList(void) {
    char import_root_buf[PATH_MAX];
    const char *import_root = ray_tracing_resolve_import_dir(import_root_buf, sizeof(import_root_buf));
    importCount = 0;
    DIR* dir = opendir(import_root);
    if (!dir) return;
    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL && importCount < MAX_ASSET_LIST) {
        if (ent->d_name[0] == '.') continue;
        const char* dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".json") != 0) continue;
        strncpy(importNames[importCount], ent->d_name, sizeof(importNames[importCount]) - 1);
        importNames[importCount][sizeof(importNames[importCount]) - 1] = '\0';
        importCount++;
    }
    closedir(dir);
}

void InitializeObjectEditor(void) {
    width = sceneSettings.windowWidth;
    height = sceneSettings.windowHeight;
    objectHandlesButton = (SDL_Rect){0, 0, 0, 0};
    circleButton = (SDL_Rect){0, 0, 0, 0};
    squareButton = (SDL_Rect){0, 0, 0, 0};
    polygonButton = (SDL_Rect){0, 0, 0, 0};
    confirmPolygonButton = (SDL_Rect){0, 0, 0, 0};
    cancelPolygonButton = (SDL_Rect){0, 0, 0, 0};

    ObjectEditorPanels_UpdateLayout();
    RefreshAssetLibrary();
    RefreshImportList();
    draggingRotationHandle = false;
    viewportPanDragging = false;
    viewportPanLastMouseX = 0;
    viewportPanLastMouseY = 0;
    SceneEditorToolStateReset();
}

void FinalizePolygonCreation(void) {
    if (polygonPointCount < 3) {  // Need at least 3 points for a valid polygon
        printf("ERROR: Not enough points to create a valid polygon.\n");
        return;
    }

    if (sceneSettings.objectCount >= MAX_OBJECTS) {
        printf("ERROR: Cannot add more objects. Maximum limit reached.\n");
        return;
    }

    int index = sceneSettings.objectCount;
    SceneObject* newObj = &sceneSettings.sceneObjects[index];

    // Compute the center of the polygon as the average of all points
    double centerX = 0, centerY = 0;
    for (int i = 0; i < polygonPointCount; i++) {
        centerX += polygonPoints[i][0];
        centerY += polygonPoints[i][1];
    }
    centerX /= polygonPointCount;
    centerY /= polygonPointCount;

    // Shift points so they are relative to the new center
    double adjustedPoints[MAX_POLYGON_POINTS][2];
    for (int i = 0; i < polygonPointCount; i++) {
        adjustedPoints[i][0] = polygonPoints[i][0] - centerX;
        adjustedPoints[i][1] = polygonPoints[i][1] - centerY;
    }

    // Initialize object as a new polygon
    InitObject(newObj, OBJECT_POLYGON, centerX, centerY, 0, 0, adjustedPoints, polygonPointCount);
    ObjectEditorObjectAssignMaterial(newObj, MaterialManagerDefaultId());

    sceneSettings.objectCount++;
    polygonPointCount = 0;  // Reset for next polygon
    polygonCreationActive = false;  // Exit polygon creation mode

    printf("Finalized Polygon Creation. New Object %d at (%.2f, %.2f)\n", index, centerX, centerY);
}


void AddSceneObject(ObjectType type,
                    double x,
                    double y,
                    double param1,
                    double param2,
                    double points[][2],
                    int numPoints) {
    if (sceneSettings.objectCount >= MAX_OBJECTS) {
        printf("ERROR: Cannot add more objects. Maximum limit reached.\n");
        return;
    }

    int index = sceneSettings.objectCount;
    SceneObject* newObj = &sceneSettings.sceneObjects[index];

    if (type == OBJECT_POLYGON && points == NULL) {
        // Default 50x50 Square
        double squarePoints[4][2] = {
            {-25.0, -25.0}, {25.0, -25.0},
            {25.0, 25.0}, {-25.0, 25.0}
        };

        InitObject(newObj, type, x, y, 50, 50, squarePoints, 4);
    } else {
        InitObject(newObj, type, x, y, param1, param2, points, numPoints);
    }

    ObjectEditorObjectAssignMaterial(newObj, MaterialManagerDefaultId());
    sceneSettings.objectCount++;
    UpdateObject(newObj);
    printf("Added Object %d at (%.2f, %.2f)\n", index, x, y);
}


void RemoveSceneObject(int index) {
    if (index < 0 || index >= sceneSettings.objectCount) {
        printf("ERROR: Invalid object index %d\n", index);
        return;
    }

    printf("Removing Object %d\n", index);

    // Shift objects down to fill the removed object's space
    for (int i = index; i < sceneSettings.objectCount - 1; i++) {
        sceneSettings.sceneObjects[i] = sceneSettings.sceneObjects[i + 1];
    }

    // Reduce object count
    sceneSettings.objectCount--;

    // Ensure last object slot is cleared (optional but good practice)
    memset(&sceneSettings.sceneObjects[sceneSettings.objectCount], 0, sizeof(SceneObject));
}

bool ObjectEditorAddPlacementAt(double world_x, double world_y) {
    if (polygonCreationActive) {
        return false;
    }
    if (selectedAssetIndex >= 0) {
        if (sceneSettings.objectCount >= MAX_OBJECTS) {
            printf("ERROR: Cannot add more objects. Maximum limit reached.\n");
            return false;
        }
        if (selectedAssetIndex < (int)assetLib.count) {
            const ShapeAsset* asset = &assetLib.assets[selectedAssetIndex];
            ShapeAssetBounds b = {0};
            shape_asset_bounds(asset, &b);
            double max_dim = fmax((double)(b.max_x - b.min_x), (double)(b.max_y - b.min_y));
            double desired = 80.0;
            double scale = (max_dim > 1e-6) ? (desired / max_dim) : 1.0;
            ShapeToSceneOptions opts = {.scale = scale, .offset_x = world_x, .offset_y = world_y};
            shape_asset_append_to_scene(asset, &opts);
            SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
            selectedAssetIndex = -1;
            return true;
        }
        return false;
    }

    if (shapeMode == SHAPE_CIRCLE) {
        AddSceneObject(OBJECT_CIRCLE, world_x, world_y, 25, 0, NULL, 0);
        return true;
    }
    if (shapeMode == SHAPE_SQUARE) {
        AddSceneObject(OBJECT_POLYGON, world_x, world_y, 50, 50, NULL, 4);
        return true;
    }
    return false;
}

bool ObjectEditorDeleteObjectIndex(int index) {
    if (index < 0 || index >= sceneSettings.objectCount) {
        return false;
    }
    int selected_before = ObjectEditorGetSelectedObjectIndex();
    RemoveSceneObject(index);
    ObjectEditorSelectionTrackerNotifyDelete(index);
    int selected_after = ObjectEditorGetSelectedObjectIndex();
    if (selected_before == index || selected_after < 0) {
        ObjectEditorSetSelectedMaterialIndex(-1);
    } else if (selected_after < sceneSettings.objectCount) {
        ObjectEditorSetSelectedMaterialIndex(sceneSettings.sceneObjects[selected_after].material_id);
    }
    return true;
}

void RenderHandles(SDL_Renderer* renderer, SceneObject* obj, const Camera* camera){
       double handleDistance = obj->radius * obj->scale;

       double handleX = obj->x + cos(obj->rotation) * handleDistance;
       double handleY = obj->y + sin(obj->rotation) * handleDistance;

       SDL_Point handleScreen = WorldToScreenObjectEditor(camera, handleX, handleY);
       SDL_Point centerScreen = WorldToScreenObjectEditor(camera, obj->x, obj->y);

       SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); // Cyan for rotation handle
       RenderDrawCircle(renderer, handleScreen.x, handleScreen.y, 5);
       SDL_RenderDrawLine(renderer, centerScreen.x, centerScreen.y, handleScreen.x, handleScreen.y);
}


void RenderObjectEditor(SDL_Renderer* renderer) {
    RayTracingThemePalette palette = {0};
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r,
                               palette.background_fill.g,
                               palette.background_fill.b,
                               255);
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    }
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    SDL_RenderFillRect(renderer, &bg);

    ObjectEditorPanels_UpdateLayout();

    Camera preview = BuildObjectEditorCamera();
    Camera original = sceneSettings.camera;
    sceneSettings.camera = preview;

    bool fillObjects = !AnimationUseFluidScene();
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        SDL_Color drawColor = ObjectEditorColorFromPackedRGB(obj->color, 255);
        SDL_SetRenderDrawColor(renderer, drawColor.r, drawColor.g, drawColor.b, drawColor.a);
        RenderSceneObject(renderer, obj, fillObjects);

        if (i == selected_index) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            RenderSceneObject(renderer, obj, false);
            RenderHandles(renderer, obj, &preview);
        } else if (renderHandles) {
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
            RenderHandles(renderer, obj, &preview);
        }
    }

    SDL_Color lightPathColor = {140, 200, 140, 130}; // faded green
    SDL_Color camPathColor = {120, 180, 240, 130};   // faded blue
    RenderBezierPathCameraPassive(renderer,
                                  &sceneSettings.bezierPath,
                                  &preview,
                                  lightPathColor,
                                  4);
    if (sceneSettings.cameraPath.numPoints >= 2) {
        RenderBezierPathCameraPassive(renderer,
                                      &sceneSettings.cameraPath,
                                      &preview,
                                      camPathColor,
                                      4);
    }

    sceneSettings.camera = original;

    RenderCameraViewportOverlay(renderer, GetCurrentMarginPixels());
}


int ObjectEditorRenderPaneControls(SDL_Renderer* renderer, SDL_Rect content_bounds, int top_y, int bottom_y) {
    const int gap = 6;
    const int row_gap = 10;
    const int button_h = 28;
    int cursor_y = top_y;
    int third_w = (content_bounds.w - (row_gap * 2)) / 3;
    char label[128];
    bool runtime_3d_scene =
        animSettings.spaceMode == SPACE_MODE_3D &&
        animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE;

    objectHandlesButton = (SDL_Rect){0, 0, 0, 0};
    circleButton = (SDL_Rect){0, 0, 0, 0};
    squareButton = (SDL_Rect){0, 0, 0, 0};
    polygonButton = (SDL_Rect){0, 0, 0, 0};
    confirmPolygonButton = (SDL_Rect){0, 0, 0, 0};
    cancelPolygonButton = (SDL_Rect){0, 0, 0, 0};

    if (!renderer || content_bounds.w <= 0 || top_y >= bottom_y) return top_y;
    if (cursor_y + button_h > bottom_y) return cursor_y;

    objectHandlesButton = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, button_h};
    snprintf(label, sizeof(label), "Handles: %s", renderHandles ? "On" : "Off");
    ObjectEditorDrawPaneButton(renderer, objectHandlesButton, label, renderHandles);
    cursor_y += button_h + gap;

    if (!runtime_3d_scene && cursor_y + button_h <= bottom_y) {
        int x0 = content_bounds.x;
        int x1 = x0 + third_w + row_gap;
        int x2 = x1 + third_w + row_gap;
        circleButton = (SDL_Rect){x0, cursor_y, third_w, button_h};
        squareButton = (SDL_Rect){x1, cursor_y, third_w, button_h};
        polygonButton = (SDL_Rect){x2, cursor_y, content_bounds.w - (third_w * 2) - (row_gap * 2), button_h};
        ObjectEditorDrawPaneButton(renderer, circleButton, "Circle", shapeMode == SHAPE_CIRCLE && !polygonCreationActive);
        ObjectEditorDrawPaneButton(renderer, squareButton, "Square", shapeMode == SHAPE_SQUARE && !polygonCreationActive);
        ObjectEditorDrawPaneButton(renderer, polygonButton, "Polygon", polygonCreationActive || shapeMode == SHAPE_POLYGON);
        cursor_y += button_h + gap;
    }

    if (polygonCreationActive && cursor_y + button_h <= bottom_y) {
        int half_w = (content_bounds.w - row_gap) / 2;
        confirmPolygonButton = (SDL_Rect){content_bounds.x, cursor_y, half_w, button_h};
        cancelPolygonButton = (SDL_Rect){content_bounds.x + half_w + row_gap,
                                         cursor_y,
                                         content_bounds.w - half_w - row_gap,
                                         button_h};
        ObjectEditorDrawPaneButton(renderer,
                                   confirmPolygonButton,
                                   polygonPointCount >= 3 ? "Confirm Polygon" : "Need 3+ Points",
                                   polygonPointCount >= 3);
        ObjectEditorDrawPaneButton(renderer, cancelPolygonButton, "Cancel", false);
        cursor_y += button_h + gap;
    }

    return cursor_y;
}

int ObjectEditorGetSelectedObjectIndex(void) {
    return ObjectEditorSelectionTrackerCurrent(sceneSettings.objectCount);
}

int ObjectEditorGetLastSelectedObjectIndex(void) {
    return ObjectEditorSelectionTrackerLast(sceneSettings.objectCount);
}

int ObjectEditorGetSelectedMaterialIndex(void) {
    return selectedMaterialIndex;
}

void ObjectEditorSetSelectedMaterialIndex(int material_id) {
    if (material_id < 0 || material_id >= MaterialManagerCount()) {
        selectedMaterialIndex = -1;
        return;
    }
    selectedMaterialIndex = material_id;
}

void ObjectEditorSetSelectedObjectIndex(int index) {
    if (index < 0 || index >= sceneSettings.objectCount) {
        ObjectEditorSetSelectedMaterialIndex(-1);
        ObjectEditorSelectionTrackerSetCurrent(-1, sceneSettings.objectCount);
    } else {
        ObjectEditorSelectionTrackerSetCurrent(index, sceneSettings.objectCount);
        ObjectEditorSetSelectedMaterialIndex(sceneSettings.sceneObjects[index].material_id);
    }
    selectedAssetIndex = -1;
    draggingRotationHandle = false;
    activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
}

void ObjectEditorAssignMaterialToSelected(int material_id) {
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount) {
        return;
    }
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[selected_index], material_id);
    ObjectEditorSetSelectedMaterialIndex(material_id);
}

void ObjectEditorAssignColorToSelected(int packed_color) {
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount) {
        return;
    }
    if (SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[selected_index])) {
        return;
    }
    ObjectEditorObjectAssignColor(&sceneSettings.sceneObjects[selected_index], packed_color);
}

void ObjectEditorAssignAlphaToSelected(double alpha) {
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount) {
        return;
    }
    if (SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[selected_index])) {
        return;
    }
    ObjectEditorObjectAssignAlpha(&sceneSettings.sceneObjects[selected_index],
                                  alpha);
}

void ObjectEditorAssignEmissiveStrengthToSelected(double emissive_strength) {
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount) {
        return;
    }
    ObjectEditorObjectAssignEmissiveStrength(
        &sceneSettings.sceneObjects[selected_index],
        emissive_strength);
}
