#include "editor/object_editor.h"
#include "scene/object_manager.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "editor/editor_mode_router.h"
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

#define MAX_POLYGON_POINTS 10  // Limit for custom polygons

int width, height;

// **Tracks which object is currently selected**
static int selectedObjectIndex = -1;
static int handleRadius = 5;
static bool draggingRotationHandle = false;  // ✅ Tracks whether the handle is being moved
static double lastWorldX = 0.0;
static double lastWorldY = 0.0;
static bool viewportPanDragging = false;
static int viewportPanLastMouseX = 0;
static int viewportPanLastMouseY = 0;
static bool renderHandles = true;
#define MAX_ASSET_LIST 128
#define ASSET_PANEL_WIDTH 200
#define ASSET_ROW_HEIGHT 22
#define PANEL_HEADER_HEIGHT 26
#define PANEL_PADDING 6
#define PANEL_MAX_HEIGHT 220


// Global mode states
static bool addModeActive = false;
// static ObjectType addType = OBJECT_POLYGON; 
static bool deleteModeActive = false;

ShapeMode shapeMode = SHAPE_SQUARE;  // Default to square mode
bool polygonCreationActive = false;
double polygonPoints[MAX_POLYGON_POINTS][2];
int polygonPointCount = 0;

extern SDL_Rect addButton;  // the existing definition from scene_editor.c
extern SDL_Rect deleteButton;  // the existing definition from scene_editor.c
extern SDL_Rect toggleButton;  // the existing definition from scene_editor.c
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
static const int MATERIAL_ROW_HEIGHT = 18;
bool assetsCollapsed = false;
bool materialsCollapsed = false;
int assetScroll = 0;
int materialScroll = 0;

typedef enum ObjectEditorAction {
    OBJECT_EDITOR_ACTION_NONE = 0,
    OBJECT_EDITOR_ACTION_QUIT,
    OBJECT_EDITOR_ACTION_ESCAPE,
    OBJECT_EDITOR_ACTION_MOUSE_DOWN,
    OBJECT_EDITOR_ACTION_MOUSE_UP,
    OBJECT_EDITOR_ACTION_MOUSE_DRAG,
    OBJECT_EDITOR_ACTION_MOUSE_WHEEL,
    OBJECT_EDITOR_ACTION_KEY_DOWN
} ObjectEditorAction;

static ObjectEditorAction ResolveObjectEditorAction(const SDL_Event* event) {
    if (!event) return OBJECT_EDITOR_ACTION_NONE;
    if (event->type == SDL_QUIT) return OBJECT_EDITOR_ACTION_QUIT;
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        return OBJECT_EDITOR_ACTION_ESCAPE;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) return OBJECT_EDITOR_ACTION_MOUSE_DOWN;
    if (event->type == SDL_MOUSEBUTTONUP) return OBJECT_EDITOR_ACTION_MOUSE_UP;
    if (event->type == SDL_MOUSEMOTION) return OBJECT_EDITOR_ACTION_MOUSE_DRAG;
    if (event->type == SDL_MOUSEWHEEL) return OBJECT_EDITOR_ACTION_MOUSE_WHEEL;
    if (event->type == SDL_KEYDOWN) return OBJECT_EDITOR_ACTION_KEY_DOWN;
    return OBJECT_EDITOR_ACTION_NONE;
}

static int ObjectEditorAssetRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, ASSET_ROW_HEIGHT, 18);
}

static int ObjectEditorMaterialRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, MATERIAL_ROW_HEIGHT, 16);
}

static const char* ShapeAssetDir(void) {
    static char resolved_dir[PATH_MAX];
    const char* dir = getenv("SHAPE_ASSET_DIR");
    return ray_tracing_resolve_shape_asset_dir(dir, resolved_dir, sizeof(resolved_dir));
}

static Camera BuildObjectEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

static CameraPoint ScreenToWorldObjectEditor(const Camera* camera, int sx, int sy) {
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(camera,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, sx, sy);
}

static void PanViewportObjectByScreenDelta(int prev_x, int prev_y, int cur_x, int cur_y) {
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

static void RefreshAssetLibrary(void) {
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
    SceneEditorPaneLayout pane_layout = {0};
    int buttonWidth = 50;
    int rowGap = 10;
    int rowY = toggleButton.y + toggleButton.h + 12;
    int baseX = addButton.x;

    width = sceneSettings.windowWidth;
    height = sceneSettings.windowHeight;
    if (SceneEditorGetPaneLayout(&pane_layout)) {
        baseX = pane_layout.left_content_rect.x;
        rowY = toggleButton.y + toggleButton.h + 12;
    }

    circleButton = (SDL_Rect){baseX, rowY, buttonWidth, 35};
    squareButton = (SDL_Rect){baseX + (buttonWidth + rowGap), rowY, buttonWidth, 35};
    polygonButton = (SDL_Rect){baseX + (buttonWidth + rowGap) * 2, rowY, buttonWidth, 35};

    confirmPolygonButton = (SDL_Rect){baseX, rowY + 44, buttonWidth + 10, 30};
    cancelPolygonButton = (SDL_Rect){baseX + buttonWidth + rowGap + 10, rowY + 44, buttonWidth + 10, 30};

    ObjectEditorPanels_UpdateLayout();
    RefreshAssetLibrary();
    RefreshImportList();
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

    sceneSettings.objectCount++;
    polygonPointCount = 0;  // Reset for next polygon
    polygonCreationActive = false;  // Exit polygon creation mode

    printf("Finalized Polygon Creation. New Object %d at (%.2f, %.2f)\n", index, centerX, centerY);
}


void AddSceneObject(ObjectType type, double x, double y, double param1, double param2, double points[][2], int 
numPoints) {
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

    newObj->material_id = MaterialManagerDefaultId();
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

bool IsClickingButton(int mx, int my) {
    if (SceneEditorIsPaneToolButton(mx, my)) {
        return true;  // Click is inside a UI button
    }

    // Check shape selection buttons (Only active when Add Mode is enabled)
    if (addModeActive) {
        if ((mx >= circleButton.x && mx <= circleButton.x + circleButton.w && my >= circleButton.y && my <= 
circleButton.y + circleButton.h) ||
            (mx >= squareButton.x && mx <= squareButton.x + squareButton.w && my >= squareButton.y && my <= 
squareButton.y + squareButton.h) ||
            (mx >= polygonButton.x && mx <= polygonButton.x + polygonButton.w && my >= polygonButton.y && my <= 
polygonButton.y + polygonButton.h)) {
            return true;
        }
    }

    // Check polygon creation confirmation/cancel buttons
    if (polygonCreationActive) {
        if ((mx >= confirmPolygonButton.x && mx <= confirmPolygonButton.x + confirmPolygonButton.w &&
             my >= confirmPolygonButton.y && my <= confirmPolygonButton.y + confirmPolygonButton.h) ||
            (mx >= cancelPolygonButton.x && mx <= cancelPolygonButton.x + cancelPolygonButton.w &&
             my >= cancelPolygonButton.y && my <= cancelPolygonButton.y + cancelPolygonButton.h)) {
            return true;
        }
    }

    return false;  // Click is not inside a UI button
}

ObjectEditorHitRegion ObjectEditorHitRegionAtPoint(int mx, int my) {
    if (IsClickingButtonMain(mx, my) || IsClickingButton(mx, my)) {
        return OBJECT_EDITOR_HIT_CONTROLS;
    }
    if (mx >= assetPanelRect.x && mx <= assetPanelRect.x + assetPanelRect.w &&
        my >= assetPanelRect.y && my <= assetPanelRect.y + assetPanelRect.h) {
        return OBJECT_EDITOR_HIT_ASSET_PANEL;
    }
    if (mx >= materialPanelRect.x && mx <= materialPanelRect.x + materialPanelRect.w &&
        my >= materialPanelRect.y && my <= materialPanelRect.y + materialPanelRect.h) {
        return OBJECT_EDITOR_HIT_MATERIAL_PANEL;
    }
    return OBJECT_EDITOR_HIT_CANVAS;
}


bool CheckObjectClick(double mx, double my) {
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];

        // Compute rotation handle position
        double handleDistance = obj->radius * obj->scale;
        double handleX = obj->x + cos(obj->rotation) * handleDistance;
        double handleY = obj->y + sin(obj->rotation) * handleDistance;

        double dx = mx - handleX;
        double dy = my - handleY;

        // Check if user clicked the rotation handle
        if ((dx * dx + dy * dy) <= (handleRadius * handleRadius)) {
            selectedObjectIndex = i;
            selectedAssetIndex = -1;
            draggingRotationHandle = true;
            printf("Dragging Rotation Handle for Object %d\n", i);
            return true;
        }

        // Check if clicking inside the object
        if (IsInsideObject((int)mx, (int)my, obj)) {
            if (addModeActive) {
                printf("Add Mode Active - Adding New Object\n");
                AddSceneObject(OBJECT_POLYGON, mx, my, 50, 50, NULL, 4);
                return true;
            }
            if (deleteModeActive) {
                printf("Delete Mode Active - Removing Object %d\n", i);
                RemoveSceneObject(i);
                return true;
            }

            selectedObjectIndex = i;
            selectedAssetIndex = -1;
            printf("Selected Object %d\n", i);
            return true;
        }
    }
    return false;
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


void RenderPolygonCreationButtons(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &confirmPolygonButton);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &cancelPolygonButton);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &confirmPolygonButton);
    SDL_RenderDrawRect(renderer, &cancelPolygonButton);
}

void RenderModeButtons(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, addModeActive ? 0 : 255, addModeActive ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &addButton);
    SDL_SetRenderDrawColor(renderer, deleteModeActive ? 255 : 255, deleteModeActive ? 0 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &deleteButton);
    SDL_SetRenderDrawColor(renderer, renderHandles ? 100 : 255, renderHandles ? 130 : 255,
                                renderHandles ? 255 : 0, 255);
    SDL_RenderFillRect(renderer, &toggleButton);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &addButton);
    RenderButtonText(renderer, addButton, "Add");
    SDL_RenderDrawRect(renderer, &deleteButton);
    RenderButtonText(renderer, deleteButton, "Delete");
    SDL_RenderDrawRect(renderer, &toggleButton);  
    RenderButtonText(renderer, toggleButton, "Handles");
}

void RenderAddModeButtons(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, shapeMode == SHAPE_CIRCLE ? 0 : 255, 
				shapeMode == SHAPE_CIRCLE ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &circleButton);

    SDL_SetRenderDrawColor(renderer, shapeMode == SHAPE_SQUARE ? 0 : 255, 
				shapeMode == SHAPE_SQUARE ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &squareButton);

    SDL_SetRenderDrawColor(renderer, shapeMode == SHAPE_POLYGON ? 0 : 255, 
				shapeMode == SHAPE_POLYGON ? 255 : 255, 0, 255);
    SDL_RenderFillRect(renderer, &polygonButton);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &circleButton);
    SDL_RenderDrawRect(renderer, &squareButton);
    SDL_RenderDrawRect(renderer, &polygonButton);
}


void RenderObjectEditor(SDL_Renderer* renderer) {
    RayTracingThemePalette palette = {0};
    SDL_Color objectColor = {255, 255, 255, 255};
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r,
                               palette.background_fill.g,
                               palette.background_fill.b,
                               255);
        objectColor = palette.text_primary;
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
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        SDL_SetRenderDrawColor(renderer, objectColor.r, objectColor.g, objectColor.b, 255);
        RenderSceneObject(renderer, obj, fillObjects);

        if (i == selectedObjectIndex) {
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
    SDL_Color handleColor = (SDL_Color){0, 0, 0, 0};
    SDL_Color selectColor = (SDL_Color){0, 0, 0, 0};
    RenderBezierPathCameraStyled(renderer,
                                 &sceneSettings.bezierPath,
                                 false,
                                 &preview,
                                 lightPathColor,
                                 handleColor,
                                 -1,
                                 selectColor,
                                 4);
    if (sceneSettings.cameraPath.numPoints >= 2) {
        RenderBezierPathCameraStyled(renderer,
                                     &sceneSettings.cameraPath,
                                     false,
                                     &preview,
                                     camPathColor,
                                     handleColor,
                                     -1,
                                     selectColor,
                                     4);
    }

    sceneSettings.camera = original;

    RenderCameraViewportOverlay(renderer, GetCurrentMarginPixels());
    RenderEditorHUD(renderer, "Objects", false);

    RenderModeButtons(renderer);
    if (addModeActive) {
        RenderAddModeButtons(renderer);
    }
    if (polygonCreationActive) {
        RenderPolygonCreationButtons(renderer);
    }

    ObjectEditorPanels_DrawAssetList(renderer);
    ObjectEditorPanels_DrawMaterialList(renderer);
}


void HandleObjectEditorEvents(SDL_Event* event) {
    ObjectEditorAction action = ResolveObjectEditorAction(event);
    switch (action) {
        case OBJECT_EDITOR_ACTION_QUIT:
            SaveAllSettings();
            sceneEditorExitFlag = true;  // Ensure clicking "X" closes the editor properly
            printf("Window closed manually. Exiting Object Editor.\n");
            return;
        case OBJECT_EDITOR_ACTION_ESCAPE:
            SaveAllSettings();
            sceneEditorExitFlag = true;  // Also close with ESC
            return;
        case OBJECT_EDITOR_ACTION_MOUSE_DOWN:
            if (showImports || selectedAssetIndex >= 0) {
                // allow selection without toggling add mode
            }
            HandleObjectEditorMouseClick(event);
            break;
        case OBJECT_EDITOR_ACTION_MOUSE_UP:
            HandleObjectEditorMouseRelease(event);
            break;
        case OBJECT_EDITOR_ACTION_MOUSE_DRAG:
            HandleObjectEditorMouseDrag(event);
            break;
        case OBJECT_EDITOR_ACTION_MOUSE_WHEEL: {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            int scrollDir = event->wheel.y > 0 ? -1 : 1;
            if (mx >= assetPanelRect.x && mx <= assetPanelRect.x + assetPanelRect.w &&
                my >= assetPanelRect.y && my <= assetPanelRect.y + assetPanelRect.h && !assetsCollapsed) {
                int assetRowH = ObjectEditorAssetRowHeight();
                int visible = showImports ? importCount : (int)assetLib.count;
                int listY = assetToggleRect.y + assetToggleRect.h + 4;
                int rowAreaH = assetPanelRect.h - (listY - assetPanelRect.y) - PANEL_PADDING;
                int maxRows;
                int maxScroll;
                if (rowAreaH < assetRowH) rowAreaH = assetRowH;
                maxRows = rowAreaH / assetRowH;
                if (maxRows < 1) maxRows = 1;
                maxScroll = visible - maxRows;
                if (maxScroll < 0) maxScroll = 0;
                assetScroll += scrollDir;
                if (assetScroll < 0) assetScroll = 0;
                if (assetScroll > maxScroll) assetScroll = maxScroll;
            }
            if (mx >= materialPanelRect.x && mx <= materialPanelRect.x + materialPanelRect.w &&
                my >= materialPanelRect.y && my <= materialPanelRect.y + materialPanelRect.h && !materialsCollapsed) {
                int materialRowH = ObjectEditorMaterialRowHeight();
                int count = MaterialManagerCount();
                int rowAreaH = materialPanelRect.h - PANEL_PADDING * 2;
                int maxRows;
                int maxScroll;
                if (rowAreaH < materialRowH) rowAreaH = materialRowH;
                maxRows = rowAreaH / materialRowH;
                if (maxRows < 1) maxRows = 1;
                maxScroll = count - maxRows;
                if (maxScroll < 0) maxScroll = 0;
                materialScroll += scrollDir;
                if (materialScroll < 0) materialScroll = 0;
                if (materialScroll > maxScroll) materialScroll = maxScroll;
            }
            break;
        }
        case OBJECT_EDITOR_ACTION_KEY_DOWN:
            HandleObjectEditorKeyPress(event);
            break;
        case OBJECT_EDITOR_ACTION_NONE:
        default:
            break;
    }
}

void HandleObjectEditorMouseClick(SDL_Event* event) {
    if (event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        viewportPanDragging = false;
        if (mx >= assetPanelRect.x && mx <= assetPanelRect.x + assetPanelRect.w &&
            my >= assetPanelRect.y && my <= assetPanelRect.y + assetPanelRect.h) {
            if (mx >= assetCollapseRect.x && mx <= assetCollapseRect.x + assetCollapseRect.w &&
                my >= assetCollapseRect.y && my <= assetCollapseRect.y + assetCollapseRect.h) {
                assetsCollapsed = !assetsCollapsed;
                return;
            }
            // Toggle button
            if (mx >= assetToggleRect.x && mx <= assetToggleRect.x + assetToggleRect.w &&
                my >= assetToggleRect.y && my <= assetToggleRect.y + assetToggleRect.h) {
                showImports = !showImports;
                selectedAssetIndex = -1;
                return;
            }
            if (!assetsCollapsed) {
                int assetRowH = ObjectEditorAssetRowHeight();
                int listY = assetToggleRect.y + assetToggleRect.h + 6;
                int rowAreaH = assetPanelRect.h - (listY - assetPanelRect.y) - PANEL_PADDING;
                if (rowAreaH < assetRowH) rowAreaH = assetRowH;
                int maxRows = rowAreaH / assetRowH;
                if (maxRows < 1) maxRows = 1;
                int idx = (my - listY) / assetRowH + assetScroll;
                if (idx >= 0) {
                    if (showImports) {
                        if (idx < importCount) {
                                if (event->button.clicks >= 2) {
                                    // Convert import to asset on double-click
                                    char path[PATH_MAX];
                                    char import_root_buf[PATH_MAX];
                                    const char *import_root =
                                        ray_tracing_resolve_import_dir(import_root_buf, sizeof(import_root_buf));
                                    snprintf(path, sizeof(path), "%s/%s", import_root, importNames[idx]);
                                ShapeDocument doc = {0};
                                if (shape_import_load(path, &doc) && doc.shapeCount > 0) {
                                    ShapeAsset asset = {0};
                                    if (shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset)) {
                                        const char* base = importNames[idx];
                                        const char* dot = strrchr(base, '.');
                                        size_t len = dot ? (size_t)(dot - base) : strlen(base);
                                        char outPath[PATH_MAX];
                                        snprintf(outPath, sizeof(outPath), "%s/%.*s.asset.json", ShapeAssetDir(), (int)len, base);
                                        if (shape_asset_save_file(&asset, outPath)) {
                                            printf("Saved asset to %s\n", outPath);
                                            RefreshAssetLibrary();
                                            showImports = false;
                                        }
                                        shape_asset_free(&asset);
                                    }
                                }
                                ShapeDocument_Free(&doc);
                            }
                        }
                    } else {
                        if (idx < (int)assetLib.count) {
                            selectedAssetIndex = idx;
                            selectedObjectIndex = -1;
                        }
                    }
                }
                return;
            }
            // Collapsed: allow click to pass through to scene
        }

        if (mx >= materialPanelRect.x && mx <= materialPanelRect.x + materialPanelRect.w &&
            my >= materialPanelRect.y && my <= materialPanelRect.y + materialPanelRect.h) {
            if (mx >= materialCollapseRect.x && mx <= materialCollapseRect.x + materialCollapseRect.w &&
                my >= materialCollapseRect.y && my <= materialCollapseRect.y + materialCollapseRect.h) {
                materialsCollapsed = !materialsCollapsed;
                return;
            }
            if (!materialsCollapsed) {
                int materialRowH = ObjectEditorMaterialRowHeight();
                int listY = materialPanelRect.y + PANEL_PADDING;
                int rowAreaH = materialPanelRect.h - PANEL_PADDING * 2;
                if (rowAreaH < materialRowH) rowAreaH = materialRowH;
                int maxRows = rowAreaH / materialRowH;
                if (maxRows < 1) maxRows = 1;
                int idx = (my - listY) / materialRowH + materialScroll;
                if (idx >= 0 && idx < MaterialManagerCount()) {
                    selectedMaterialIndex = idx;
                    if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneSettings.objectCount) {
                        sceneSettings.sceneObjects[selectedObjectIndex].material_id = idx;
                    }
                }
                return;
            }
            // Collapsed: allow click to pass through to scene
        }

        Camera previewCam = BuildObjectEditorCamera();
        CameraPoint worldPoint = ScreenToWorldObjectEditor(&previewCam, mx, my);
        double worldX = worldPoint.x;
        double worldY = worldPoint.y;
        lastWorldX = worldX;
        lastWorldY = worldY;

        selectedObjectIndex = -1;
        draggingRotationHandle = false;

        if (addModeActive && selectedAssetIndex >= 0 && !polygonCreationActive) {
            if (sceneSettings.objectCount >= MAX_OBJECTS) {
                printf("ERROR: Cannot add more objects. Maximum limit reached.\n");
                return;
            }
            if (selectedAssetIndex < (int)assetLib.count) {
                const ShapeAsset* asset = &assetLib.assets[selectedAssetIndex];
                ShapeAssetBounds b = {0};
                shape_asset_bounds(asset, &b);
                double max_dim = fmax((double)(b.max_x - b.min_x), (double)(b.max_y - b.min_y));
                double desired = 80.0;
                double scale = (max_dim > 1e-6) ? (desired / max_dim) : 1.0;
                ShapeToSceneOptions opts = {.scale = scale, .offset_x = worldX, .offset_y = worldY};
                shape_asset_append_to_scene(asset, &opts);
            }
            addModeActive = false;
            selectedAssetIndex = -1;
            return;
        }

        // Prevent accidental shape creation when clicking UI buttons
        bool clickedButton = IsClickingButtonMain(mx, my) || IsClickingButton(mx, my);
	

        // Handle UI Buttons Clicks
        if (mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y && 
		my <= addButton.y + addButton.h) {
            addModeActive = !addModeActive;
	    polygonCreationActive = false;
            deleteModeActive = false;
            return;
        }
        if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y && 
		my <= deleteButton.y + deleteButton.h) {
	    polygonCreationActive = false;
            deleteModeActive = !deleteModeActive;
            addModeActive = false;
            return;
        }
        if (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w && my >= toggleButton.y && 
			my <= toggleButton.y + toggleButton.h) {
            renderHandles = !renderHandles;
            return;
        }

        // Handle Shape Selection (Only Active in Add Mode)
        if (addModeActive) {
            if (mx >= circleButton.x && mx <= circleButton.x + circleButton.w && my >= circleButton.y && 
			my <= circleButton.y + circleButton.h) {
		polygonCreationActive = false;
                shapeMode = SHAPE_CIRCLE;
                return;
            } 
            if (mx >= squareButton.x && mx <= squareButton.x + squareButton.w && my >= squareButton.y 
			&& my <= squareButton.y + squareButton.h) {
		polygonCreationActive = false;
                shapeMode = SHAPE_SQUARE;
                return;
            } 
            if (mx >= polygonButton.x && mx <= polygonButton.x + polygonButton.w && my >= polygonButton.y 
			&& my <= polygonButton.y + polygonButton.h) {
                shapeMode = SHAPE_POLYGON;
                polygonCreationActive = true;
                return;
            }
        }

        // Handle Polygon Creation Confirmation
        if (polygonCreationActive) {
            if (mx >= confirmPolygonButton.x && mx <= confirmPolygonButton.x + confirmPolygonButton.w &&
                my >= confirmPolygonButton.y && my <= confirmPolygonButton.y + confirmPolygonButton.h) {
                FinalizePolygonCreation();
                return;
            }
            if (mx >= cancelPolygonButton.x && mx <= cancelPolygonButton.x + cancelPolygonButton.w &&
                my >= cancelPolygonButton.y && my <= cancelPolygonButton.y + cancelPolygonButton.h) {
                polygonPointCount = 0;
                polygonCreationActive = false;
                return;
            }
        }

        // **Prevent accidental shape creation when clicking buttons**
        if (!clickedButton && addModeActive && !polygonCreationActive) {
            if (shapeMode == SHAPE_CIRCLE) {
                AddSceneObject(OBJECT_CIRCLE, worldX, worldY, 25, 0, NULL, 0);
            } else if (shapeMode == SHAPE_SQUARE) {
                AddSceneObject(OBJECT_POLYGON, worldX, worldY, 50, 50, NULL, 4);
            }
            return;
        }

        // Call CheckObjectClick to handle object interaction
        if (CheckObjectClick(worldX, worldY)) {
            selectedAssetIndex = -1; // only one selection active
            if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneSettings.objectCount) {
                selectedMaterialIndex = sceneSettings.sceneObjects[selectedObjectIndex].material_id;
            }
            return;
        }

        if (!clickedButton &&
            !addModeActive &&
            !deleteModeActive &&
            !polygonCreationActive &&
            selectedObjectIndex == -1) {
            viewportPanDragging = true;
            viewportPanLastMouseX = mx;
            viewportPanLastMouseY = my;
        }
    }
}

void HandleObjectEditorMouseDrag(SDL_Event* event) {
    if (event->motion.state & SDL_BUTTON_LMASK) {
        int mx = event->motion.x;
        int my = event->motion.y;
        Camera previewCam = BuildObjectEditorCamera();
        CameraPoint worldPoint = ScreenToWorldObjectEditor(&previewCam, mx, my);
        double worldX = worldPoint.x;
        double worldY = worldPoint.y;

        if (viewportPanDragging &&
            selectedObjectIndex == -1 &&
            !draggingRotationHandle) {
            PanViewportObjectByScreenDelta(viewportPanLastMouseX,
                                           viewportPanLastMouseY,
                                           mx,
                                           my);
            viewportPanLastMouseX = mx;
            viewportPanLastMouseY = my;
        } else if (selectedObjectIndex != -1) {
            SceneObject* obj = &sceneSettings.sceneObjects[selectedObjectIndex];

            if (draggingRotationHandle) {
                // Compute new rotation based on cursor position
                double newAngle = atan2(worldY - obj->y, worldX - obj->x);
                obj->rotation = newAngle;

                // Compute new distance from object center to handle position
                double newDistance = sqrt((worldX - obj->x) * (worldX - obj->x) +
                                          (worldY - obj->y) * (worldY - obj->y));

                // Adjust scaling based on new distance, ensuring uniform scaling
                double initialDistance = obj->radius;
                if (initialDistance > 0) {
                    obj->scale = newDistance / initialDistance;
                }

                printf("Updated Rotation: %.2f radians, Scale: %.2f\n", obj->rotation, obj->scale);

            } else {
                // Move the object naturally based on cursor movement
                int dx = (int)lround(worldX - lastWorldX);
                int dy = (int)lround(worldY - lastWorldY);
                MoveObject(obj, dx, dy);
            }

            MarkObjectDirty(obj);
        }

        lastWorldX = worldX;
        lastWorldY = worldY;
    }
}


void HandleObjectEditorMouseRelease(SDL_Event* event) {
    if (event->button.button == SDL_BUTTON_LEFT) {
        viewportPanDragging = false;
    }
}


static void ClearSelections(void) {
    selectedAssetIndex = -1;
    selectedObjectIndex = -1;
}

static void DeleteSelected(void) {
    if (selectedAssetIndex >= 0 && !showImports) {
        if (selectedAssetIndex < (int)assetLib.count && assetLib.assets[selectedAssetIndex].name) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s.asset.json", ShapeAssetDir(), assetLib.assets[selectedAssetIndex].name);
            remove(path);
            RefreshAssetLibrary();
            selectedAssetIndex = -1;
        }
        return;
    }
    if (selectedObjectIndex != -1) {
        printf("Deleting Object %d\n", selectedObjectIndex);
        RemoveSceneObject(selectedObjectIndex);
        selectedObjectIndex = -1;
    }
}

void HandleObjectEditorKeyPress(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_DELETE || key == SDLK_BACKSPACE || key == SDLK_KP_PERIOD) {
            DeleteSelected();
            return;
        }
        switch (key) {
            case SDLK_a:
                ClearSelections();
                addModeActive = !addModeActive;
                deleteModeActive = false;
                printf("Add Mode: %s\n", addModeActive ? "ON" : "OFF");
                break;

            case SDLK_d:
                ClearSelections();
                deleteModeActive = !deleteModeActive;
                addModeActive = false;
                printf("Delete Mode: %s\n", deleteModeActive ? "ON" : "OFF");
                break;
    
            case SDLK_t: // Toggle between cubic and quadratic Bézier paths
                ToggleBezierPathMode(&sceneSettings.bezierPath);
                break;
        }
    }
}
