#include "editor/object_editor_internal.h"

#include "app/data_paths.h"
#include "editor/object_editor_motion.h"
#include "editor/object_editor_object_ops.h"
#include "editor/scene_editor_tool_state.h"
#include "geo/shape_adapter.h"
#include "geo/shape_asset.h"
#include "import/shape_import.h"
#include "material/material_manager.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

static bool IsClickingButton(int mx, int my) {
    if (SceneEditorIsPaneToolButton(mx, my)) {
        return true;
    }

    if (ObjectEditorPointInRect(mx, my, &objectHandlesButton)) {
        return true;
    }

    if (ObjectEditorObjectListIndexAtPoint(mx, my) >= 0) {
        return true;
    }

    if (ObjectEditorPointInRect(mx, my, &circleButton) ||
        ObjectEditorPointInRect(mx, my, &squareButton) ||
        ObjectEditorPointInRect(mx, my, &polygonButton)) {
        return true;
    }

    if (polygonCreationActive) {
        if (ObjectEditorPointInRect(mx, my, &confirmPolygonButton) ||
            ObjectEditorPointInRect(mx, my, &cancelPolygonButton)) {
            return true;
        }
    }

    return false;
}

bool CheckObjectClick(double mx, double my) {
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];

        double handleDistance = obj->radius * obj->scale;
        double handleX = obj->x + cos(obj->rotation) * handleDistance;
        double handleY = obj->y + sin(obj->rotation) * handleDistance;

        double dx = mx - handleX;
        double dy = my - handleY;

        if ((dx * dx + dy * dy) <= (handleRadius * handleRadius)) {
            ObjectEditorSetSelectedObjectIndex(i);
            draggingRotationHandle = true;
            printf("Dragging Rotation Handle for Object %d\n", i);
            return true;
        }

        if (IsInsideObject((int)mx, (int)my, obj)) {
            if (ObjectEditorAddToolActive()) {
                printf("Add Mode Active - Adding New Object\n");
                AddSceneObject(OBJECT_POLYGON, mx, my, 50, 50, NULL, 4);
                return true;
            }
            if (ObjectEditorDeleteToolActive()) {
                printf("Delete Mode Active - Removing Object %d\n", i);
                ObjectEditorDeleteObjectIndex(i);
                return true;
            }

            ObjectEditorSetSelectedObjectIndex(i);
            printf("Selected Object %d\n", i);
            return true;
        }
    }
    return false;
}

static void ClearSelections(void) {
    selectedAssetIndex = -1;
    ObjectEditorSetSelectedObjectIndex(-1);
    activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
}

static void DeleteSelected(void) {
    if (selectedAssetIndex >= 0 && !showImports) {
        if (selectedAssetIndex < (int)assetLib.count && assetLib.assets[selectedAssetIndex].name) {
            char path[PATH_MAX];
            snprintf(path,
                     sizeof(path),
                     "%s/%s.asset.json",
                     ShapeAssetDir(),
                     assetLib.assets[selectedAssetIndex].name);
            remove(path);
            RefreshAssetLibrary();
            selectedAssetIndex = -1;
        }
        return;
    }
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_index != -1) {
        printf("Deleting Object %d\n", selected_index);
        ObjectEditorDeleteObjectIndex(selected_index);
        activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
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

void HandleObjectEditorEvents(SDL_Event* event) {
    ObjectEditorAction action = ResolveObjectEditorAction(event);
    switch (action) {
        case OBJECT_EDITOR_ACTION_QUIT:
            sceneEditorExitFlag = true;
            printf("Window closed manually. Exiting Object Editor.\n");
            return;
        case OBJECT_EDITOR_ACTION_ESCAPE:
            sceneEditorExitFlag = true;
            return;
        case OBJECT_EDITOR_ACTION_MOUSE_DOWN:
            if (showImports || selectedAssetIndex >= 0) {
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
                int maxScroll = ObjectEditorPanels_AssetMaxScroll();
                assetScroll += scrollDir;
                if (assetScroll < 0) assetScroll = 0;
                if (assetScroll > maxScroll) assetScroll = maxScroll;
            }
            if (mx >= materialPanelRect.x && mx <= materialPanelRect.x + materialPanelRect.w &&
                my >= materialPanelRect.y && my <= materialPanelRect.y + materialPanelRect.h &&
                !materialsCollapsed) {
                int maxScroll = ObjectEditorPanels_MaterialMaxScroll();
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
        int object_list_index = -1;
        viewportPanDragging = false;
        object_list_index = ObjectEditorObjectListIndexAtPoint(mx, my);
        if (object_list_index >= 0) {
            ObjectEditorSetSelectedObjectIndex(object_list_index);
            selectedAssetIndex = -1;
            activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
            int selected_index = ObjectEditorGetSelectedObjectIndex();
            if (selected_index >= 0 && selected_index < sceneSettings.objectCount) {
                ObjectEditorSetSelectedMaterialIndex(sceneSettings.sceneObjects[selected_index].material_id);
            }
            return;
        }
        if (mx >= assetPanelRect.x && mx <= assetPanelRect.x + assetPanelRect.w &&
            my >= assetPanelRect.y && my <= assetPanelRect.y + assetPanelRect.h) {
            if (mx >= assetCollapseRect.x && mx <= assetCollapseRect.x + assetCollapseRect.w &&
                my >= assetCollapseRect.y && my <= assetCollapseRect.y + assetCollapseRect.h) {
                assetsCollapsed = !assetsCollapsed;
                return;
            }
            if (mx >= assetToggleRect.x && mx <= assetToggleRect.x + assetToggleRect.w &&
                my >= assetToggleRect.y && my <= assetToggleRect.y + assetToggleRect.h) {
                showImports = !showImports;
                selectedAssetIndex = -1;
                return;
            }
            if (!assetsCollapsed) {
                int idx = ObjectEditorPanels_AssetIndexAtPoint(mx, my);
                if (idx >= 0) {
                    if (showImports) {
                        if (idx < importCount) {
                            if (event->button.clicks >= 2) {
                                char path[PATH_MAX];
                                char import_root_buf[PATH_MAX];
                                const char *import_root =
                                    ray_tracing_resolve_import_dir(import_root_buf,
                                                                   sizeof(import_root_buf));
                                snprintf(path, sizeof(path), "%s/%s", import_root, importNames[idx]);
                                ShapeDocument doc = {0};
                                if (shape_import_load(path, &doc) && doc.shapeCount > 0) {
                                    ShapeAsset asset = {0};
                                    if (shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset)) {
                                        const char* base = importNames[idx];
                                        const char* dot = strrchr(base, '.');
                                        size_t len = dot ? (size_t)(dot - base) : strlen(base);
                                        char outPath[PATH_MAX];
                                        snprintf(outPath,
                                                 sizeof(outPath),
                                                 "%s/%.*s.asset.json",
                                                 ShapeAssetDir(),
                                                 (int)len,
                                                 base);
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
                            ObjectEditorSetSelectedObjectIndex(-1);
                        }
                    }
                }
                return;
            }
        }

        if (mx >= materialPanelRect.x && mx <= materialPanelRect.x + materialPanelRect.w &&
            my >= materialPanelRect.y && my <= materialPanelRect.y + materialPanelRect.h) {
            if (mx >= materialCollapseRect.x && mx <= materialCollapseRect.x + materialCollapseRect.w &&
                my >= materialCollapseRect.y && my <= materialCollapseRect.y + materialCollapseRect.h) {
                materialsCollapsed = !materialsCollapsed;
                return;
            }
            if (!materialsCollapsed) {
                ObjectEditorPanelMotionAction motion_action =
                    OBJECT_EDITOR_PANEL_MOTION_ACTION_NONE;
                if (ObjectEditorPanels_MotionActionAtPoint(mx, my, &motion_action)) {
                    int selected_index = ObjectEditorGetSelectedObjectIndex();
                    activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
                    if (motion_action == OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC) {
                        if (!ObjectEditorMotionSetSelectedObjectStatic(selected_index)) {
                            printf("Motion Static unavailable for selected object.\n");
                        }
                    } else if (motion_action == OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED) {
                        if (!ObjectEditorMotionSetSelectedObjectAuthored(selected_index)) {
                            printf("Motion Path unavailable for selected object.\n");
                        }
                    } else if (motion_action ==
                               OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED) {
                        printf("Motion Physics is reserved for a future solver handoff.\n");
                    }
                    return;
                }
                int idx = ObjectEditorPanels_MaterialIndexAtPoint(mx, my);
                if (idx >= 0 && idx < MaterialManagerCount()) {
                    ObjectEditorSetSelectedMaterialIndex(idx);
                    ObjectEditorAssignMaterialToSelected(idx);
                    return;
                }
                {
                    ObjectEditorPanelSliderKind slider_kind = OBJECT_EDITOR_PANEL_SLIDER_NONE;
                    double slider_value = 0.0;
                    if (ObjectEditorPanels_SliderValueAtPoint(mx,
                                                              my,
                                                              &slider_kind,
                                                              &slider_value)) {
                        activeMaterialSlider = slider_kind;
                        ObjectEditorApplySliderValueToSelected(slider_kind, slider_value);
                        return;
                    }
                }
                return;
            }
        }

        Camera previewCam = BuildObjectEditorCamera();
        CameraPoint worldPoint = ScreenToWorldObjectEditor(&previewCam, mx, my);
        double worldX = worldPoint.x;
        double worldY = worldPoint.y;
        lastWorldX = worldX;
        lastWorldY = worldY;

        ObjectEditorSetSelectedObjectIndex(-1);
        activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
        draggingRotationHandle = false;

        if (SceneEditorToolStateGetEffective(SDL_GetModState()) == SCENE_EDITOR_TOOL_ADD &&
            selectedAssetIndex >= 0 &&
            !polygonCreationActive) {
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
            SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
            selectedAssetIndex = -1;
            return;
        }

        bool clickedButton = IsClickingButtonMain(mx, my) || IsClickingButton(mx, my);

        if (mx >= selectButton.x && mx <= selectButton.x + selectButton.w &&
            my >= selectButton.y && my <= selectButton.y + selectButton.h) {
            polygonCreationActive = false;
            SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_SELECT);
            return;
        }
        if (mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y &&
            my <= addButton.y + addButton.h) {
            SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
            polygonCreationActive = false;
            return;
        }
        if (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y &&
            my <= deleteButton.y + deleteButton.h) {
            polygonCreationActive = false;
            SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
            return;
        }
        if (ObjectEditorPointInRect(mx, my, &objectHandlesButton)) {
            renderHandles = !renderHandles;
            return;
        }
        if (ObjectEditorPointInRect(mx, my, &circleButton)) {
            polygonCreationActive = false;
            shapeMode = SHAPE_CIRCLE;
            return;
        }
        if (ObjectEditorPointInRect(mx, my, &squareButton)) {
            polygonCreationActive = false;
            shapeMode = SHAPE_SQUARE;
            return;
        }
        if (ObjectEditorPointInRect(mx, my, &polygonButton)) {
            shapeMode = SHAPE_POLYGON;
            polygonCreationActive = true;
            polygonPointCount = 0;
            return;
        }

        if (polygonCreationActive) {
            if (ObjectEditorPointInRect(mx, my, &confirmPolygonButton)) {
                FinalizePolygonCreation();
                return;
            }
            if (ObjectEditorPointInRect(mx, my, &cancelPolygonButton)) {
                polygonPointCount = 0;
                polygonCreationActive = false;
                return;
            }
        }

        if (!clickedButton &&
            SceneEditorToolStateGetEffective(SDL_GetModState()) == SCENE_EDITOR_TOOL_ADD &&
            !polygonCreationActive) {
            if (shapeMode == SHAPE_CIRCLE) {
                AddSceneObject(OBJECT_CIRCLE, worldX, worldY, 25, 0, NULL, 0);
            } else if (shapeMode == SHAPE_SQUARE) {
                AddSceneObject(OBJECT_POLYGON, worldX, worldY, 50, 50, NULL, 4);
            }
            return;
        }

        if (CheckObjectClick(worldX, worldY)) {
            selectedAssetIndex = -1;
            int selected_index = ObjectEditorGetSelectedObjectIndex();
            if (selected_index >= 0 && selected_index < sceneSettings.objectCount) {
                ObjectEditorSetSelectedMaterialIndex(sceneSettings.sceneObjects[selected_index].material_id);
            }
            return;
        }

        if (!clickedButton &&
            !ObjectEditorAddToolActive() &&
            !ObjectEditorDeleteToolActive() &&
            !polygonCreationActive &&
            ObjectEditorGetSelectedObjectIndex() == -1) {
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
            ObjectEditorGetSelectedObjectIndex() == -1 &&
            !draggingRotationHandle) {
            PanViewportObjectByScreenDelta(viewportPanLastMouseX,
                                           viewportPanLastMouseY,
                                           mx,
                                           my);
            viewportPanLastMouseX = mx;
            viewportPanLastMouseY = my;
        } else if (activeMaterialSlider != OBJECT_EDITOR_PANEL_SLIDER_NONE) {
            double slider_value = 0.0;
            if (ObjectEditorPanels_SliderValueForKindAtX(activeMaterialSlider,
                                                         mx,
                                                         &slider_value)) {
                ObjectEditorApplySliderValueToSelected(activeMaterialSlider,
                                                       slider_value);
            }
        } else {
            int selected_index = ObjectEditorGetSelectedObjectIndex();
            if (selected_index == -1) {
                lastWorldX = worldX;
                lastWorldY = worldY;
                return;
            }
            SceneObject* obj = &sceneSettings.sceneObjects[selected_index];

            if (draggingRotationHandle) {
                double newAngle = atan2(worldY - obj->y, worldX - obj->x);
                obj->rotation = newAngle;

                double newDistance = sqrt((worldX - obj->x) * (worldX - obj->x) +
                                          (worldY - obj->y) * (worldY - obj->y));

                double initialDistance = obj->radius;
                if (initialDistance > 0) {
                    obj->scale = newDistance / initialDistance;
                }

                printf("Updated Rotation: %.2f radians, Scale: %.2f\n", obj->rotation, obj->scale);
            } else {
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
        draggingRotationHandle = false;
        activeMaterialSlider = OBJECT_EDITOR_PANEL_SLIDER_NONE;
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
                SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
                printf("Add Mode: %s\n", ObjectEditorAddToolActive() ? "ON" : "OFF");
                break;
            case SDLK_d:
                ClearSelections();
                SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
                printf("Delete Mode: %s\n", ObjectEditorDeleteToolActive() ? "ON" : "OFF");
                break;
            case SDLK_t:
                ToggleBezierPathMode(&sceneSettings.bezierPath);
                break;
        }
    }
}
