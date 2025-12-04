#include "editor/object_editor.h"
#include "scene/object_manager.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "camera/camera.h"
#include "geo/shape_adapter.h"
#include "geo/shape_library.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"
#include <dirent.h>
#include <stdio.h>
#include <math.h>

#define MAX_POLYGON_POINTS 10  // Limit for custom polygons

int width, height;

// **Tracks which object is currently selected**
static int selectedObjectIndex = -1;
static int handleRadius = 5;
static bool draggingRotationHandle = false;  // ✅ Tracks whether the handle is being moved
static double lastWorldX = 0.0;
static double lastWorldY = 0.0;
static bool renderHandles = true;
#define MAX_ASSET_LIST 128
#define ASSET_PANEL_WIDTH 220
#define ASSET_ROW_HEIGHT 22


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

static SDL_Rect assetPanelRect;
static SDL_Rect assetToggleRect;
static bool showImports = false;
static int selectedAssetIndex = -1;
static ShapeAssetLibrary assetLib = {0};
static char importNames[MAX_ASSET_LIST][256];
static int importCount = 0;

static Camera BuildObjectEditorCamera(void) {
    double margin = GetCurrentMarginPixels();
    return CameraBuildPreviewCamera(&sceneSettings.camera,
                                    margin,
                                    sceneSettings.windowWidth,
                                    sceneSettings.windowHeight);
}

static CameraPoint ScreenToWorldObjectEditor(const Camera* camera, int sx, int sy) {
    return CameraScreenToWorld(camera,
                               sx,
                               sy,
                               sceneSettings.windowWidth,
                               sceneSettings.windowHeight);
}

static SDL_Point WorldToScreenObjectEditor(const Camera* camera, double wx, double wy) {
    CameraPoint screen = CameraWorldToScreen(camera,
                                             wx,
                                             wy,
                                             sceneSettings.windowWidth,
                                             sceneSettings.windowHeight);
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
    shape_library_load_dir("Configs/objects", &assetLib);
    if (selectedAssetIndex >= (int)assetLib.count) selectedAssetIndex = -1;
}

static void RefreshImportList(void) {
    importCount = 0;
    DIR* dir = opendir("import");
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

static void DrawAssetList(SDL_Renderer* renderer) {
    SDL_Rect panel = assetPanelRect;
    SDL_BlendMode prevMode;
    SDL_GetRenderDrawBlendMode(renderer, &prevMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 30);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &assetToggleRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &assetToggleRect);
    RenderButtonText(renderer, assetToggleRect, showImports ? "Imports" : "Assets");

    int listY = assetToggleRect.y + assetToggleRect.h + 6;
    int visible = showImports ? importCount : (int)assetLib.count;
    for (int i = 0; i < visible; ++i) {
        SDL_Rect row = {panel.x + 6, listY + i * ASSET_ROW_HEIGHT, panel.w - 12, ASSET_ROW_HEIGHT - 2};
        bool selected = (!showImports && i == selectedAssetIndex);
        SDL_SetRenderDrawColor(renderer, selected ? 80 : 25, selected ? 160 : 25, selected ? 240 : 25, 200);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &row);

        const char* label = "";
        char buffer[256];
        if (showImports) {
            label = importNames[i];
        } else if (assetLib.assets && assetLib.assets[i].name) {
            label = assetLib.assets[i].name;
        } else {
            snprintf(buffer, sizeof(buffer), "asset_%d", i);
            label = buffer;
        }
        RenderButtonText(renderer, row, label);
    }

    SDL_SetRenderDrawBlendMode(renderer, prevMode);
}

void InitializeObjectEditor(void) {
    // **Initialize Button Positions Based on Window Size**
    width = sceneSettings.windowWidth;
    height = sceneSettings.windowHeight;	 
     int buttonWidth = 50;
    int sceneButtonWidth = 70;
    // Add mode buttons (appear left of the Add button)
    width -= sceneButtonWidth;
    circleButton = (SDL_Rect){width - (buttonWidth + 10), 20, buttonWidth, 35};
    squareButton = (SDL_Rect){width - (buttonWidth + 10) * 2, 20, buttonWidth, 35};
    polygonButton = (SDL_Rect){width - (buttonWidth + 10) * 3, 20, buttonWidth, 35};

    // Polygon creation buttons (appear left of Polygon button)
    confirmPolygonButton = (SDL_Rect){width - buttonWidth - 5, 60, buttonWidth - 10, 30};
    cancelPolygonButton = (SDL_Rect){width - buttonWidth - 5,  95, buttonWidth - 10, 30};

    assetPanelRect = (SDL_Rect){20, 60, ASSET_PANEL_WIDTH, 260};
    assetToggleRect = (SDL_Rect){assetPanelRect.x + 6, assetPanelRect.y + 6, assetPanelRect.w - 12, 24};
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
    // Check if click is within main buttons
    if ((mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y && my <= addButton.y + addButton.h) 
||
        (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y && my <= deleteButton.y 
+ deleteButton.h) ||
        (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w && my >= toggleButton.y && my <= toggleButton.y 
+ toggleButton.h)) {
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
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderClear(renderer);

    Camera preview = BuildObjectEditorCamera();
    Camera original = sceneSettings.camera;
    sceneSettings.camera = preview;

    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        RenderSceneObject(renderer, obj, true);

        if (i == selectedObjectIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            RenderSceneObject(renderer, obj, false);
            RenderHandles(renderer, obj, &preview);
        } else if (renderHandles) {
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
            RenderHandles(renderer, obj, &preview);
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Color pathColor = {100, 120, 100, 140};
    SDL_Color handleColor = {255, 80, 80, 80};
    SDL_Color selectColor = {255, 255, 160, 255};
    RenderBezierPathCameraStyled(renderer,
                                 &sceneSettings.bezierPath,
                                 false,
                                 &preview,
                                 pathColor,
                                 handleColor,
                                 -1,
                                 selectColor,
                                 4);

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

    DrawAssetList(renderer);
}


void HandleObjectEditorEvents(SDL_Event* event) {
    if (event->type == SDL_QUIT) {  
	SaveAllSettings();
        sceneEditorExitFlag = true;  // ✅ Ensure clicking "X" closes the editor properly
        printf("Window closed manually. Exiting Object Editor.\n");
        return;
    }

    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        SaveAllSettings();
	sceneEditorExitFlag = true;  // ✅ Also close with ESC
        return;
    }
	
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (showImports || selectedAssetIndex >= 0) {
                // allow selection without toggling add mode
            }
            HandleObjectEditorMouseClick(event);
            break;
        case SDL_MOUSEBUTTONUP:
            HandleObjectEditorMouseRelease(event);
            break;
        case SDL_MOUSEMOTION:
            HandleObjectEditorMouseDrag(event);
            break;
        case SDL_KEYDOWN:
            HandleObjectEditorKeyPress(event);
            break;
    }
}

void HandleObjectEditorMouseClick(SDL_Event* event) {
    if (event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        if (mx >= assetPanelRect.x && mx <= assetPanelRect.x + assetPanelRect.w &&
            my >= assetPanelRect.y && my <= assetPanelRect.y + assetPanelRect.h) {
            // Toggle button
            if (mx >= assetToggleRect.x && mx <= assetToggleRect.x + assetToggleRect.w &&
                my >= assetToggleRect.y && my <= assetToggleRect.y + assetToggleRect.h) {
                showImports = !showImports;
                selectedAssetIndex = -1;
                return;
            }
            int listY = assetToggleRect.y + assetToggleRect.h + 6;
            int idx = (my - listY) / ASSET_ROW_HEIGHT;
            if (idx >= 0) {
                if (showImports) {
                    if (idx < importCount) {
                        if (event->button.clicks >= 2) {
                            // Convert import to asset on double-click
                            char path[256];
                            snprintf(path, sizeof(path), "import/%s", importNames[idx]);
                            ShapeDocument doc = {0};
                            if (shape_import_load(path, &doc) && doc.shapeCount > 0) {
                                ShapeAsset asset = {0};
                                if (shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset)) {
                                    const char* base = importNames[idx];
                                    const char* dot = strrchr(base, '.');
                                    size_t len = dot ? (size_t)(dot - base) : strlen(base);
                                    char outPath[256];
                                    snprintf(outPath, sizeof(outPath), "Configs/objects/%.*s.asset.json", (int)len, base);
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
        bool clickedButton = IsClickingButtonMain(mx, my);
	if (!clickedButton){
		clickedButton = IsClickingButton(mx, my);
	}
	

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
            return;
        }

        // Handle Apply Button Click
        if (mx >= applyButton.x && mx <= applyButton.x + applyButton.w &&
            my >= applyButton.y && my <= applyButton.y + applyButton.h) {
            SaveSceneConfig();
            sceneEditorExitFlag = true;
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

        if (selectedObjectIndex != -1) {
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
        // No special action for now, but useful if we add drag snapping later
    }
}


static void ClearSelections(void) {
    selectedAssetIndex = -1;
    selectedObjectIndex = -1;
}

static void DeleteSelected(void) {
    if (selectedAssetIndex >= 0 && !showImports) {
        if (selectedAssetIndex < (int)assetLib.count && assetLib.assets[selectedAssetIndex].name) {
            char path[256];
            snprintf(path, sizeof(path), "Configs/objects/%s.asset.json", assetLib.assets[selectedAssetIndex].name);
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
