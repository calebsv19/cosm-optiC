#include "editor/scene_editor_surface_render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "config/config_manager.h"
#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"
#include "editor/object_editor.h"
#include "editor/object_editor_panels.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_mesh_preview_render.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "editor/scene_editor_tool_state.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/render_helper.h"

static const char* SceneEditorSurfaceModeLabel(int mode) {
    switch (mode) {
        case EDITOR_MODE_PATH: return "Bezier";
        case EDITOR_MODE_OBJECT: return "Objects";
        case EDITOR_MODE_CAMERA: return "Camera";
        case EDITOR_MODE_MATERIAL: return "Material";
        default: return "Mode";
    }
}

static int SceneEditorSurfaceRenderFlowLine(SDL_Renderer* renderer,
                                            SDL_Rect bounds,
                                            int cursor_y,
                                            int bottom_y,
                                            const char* text,
                                            SDL_Color color,
                                            bool wrapped,
                                            int gap) {
    SDL_Rect line_rect = {0, 0, 0, 0};
    int used_height = 0;
    if (!renderer || !text || !text[0]) return cursor_y;
    if (cursor_y >= bottom_y) return cursor_y;
    line_rect.x = bounds.x;
    line_rect.y = cursor_y;
    line_rect.w = bounds.w;
    line_rect.h = bottom_y - cursor_y;
    if (line_rect.w <= 0 || line_rect.h <= 0) return cursor_y;
    used_height = wrapped
                      ? RenderLabelTextWrappedLeft(renderer, line_rect, text, color)
                      : RenderLabelTextLeft(renderer, line_rect, text, color);
    if (used_height < 1) used_height = 18;
    return cursor_y + used_height + gap;
}

static const RayTracingRuntimeMeshAssetInstance* SceneEditorSurfaceFindLoadedMeshPreview(
    int scene_object_index) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!mesh_assets || scene_object_index < 0) return NULL;
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        if (mesh_assets->instances[i].scene_object_index == scene_object_index) {
            return &mesh_assets->instances[i];
        }
    }
    return NULL;
}

static const RayTracingRuntimeMeshAssetSkippedInstance*
SceneEditorSurfaceFindSkippedMeshPreview(int scene_object_index) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!mesh_assets || scene_object_index < 0) return NULL;
    for (int i = 0; i < mesh_assets->skipped_instance_count; ++i) {
        if (mesh_assets->skipped_instances[i].scene_object_index == scene_object_index) {
            return &mesh_assets->skipped_instances[i];
        }
    }
    return NULL;
}

static const RuntimeSceneBridgePrimitiveDigest* SceneEditorSurfaceFindPrimitiveDigest(
    const RuntimeSceneBridge3DDigestState* digest,
    int scene_object_index) {
    if (!digest || !digest->valid || scene_object_index < 0) return NULL;
    for (int i = 0; i < digest->primitive_count; ++i) {
        if (digest->primitives[i].scene_object_index == scene_object_index) {
            return &digest->primitives[i];
        }
    }
    return NULL;
}

static const char* SceneEditorSurfacePrimitiveLabel(RuntimeSceneBridgePrimitiveKind kind) {
    switch (kind) {
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE: return "plane";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM: return "prism";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX: return "box";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_TRIANGLE_MESH: return "tri mesh";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN:
        default: return "primitive";
    }
}

static const char* SceneEditorSurfaceShortObjectId(int scene_object_index,
                                                   char* buffer,
                                                   size_t buffer_size) {
    const char* id = NULL;
    const char* prefix = NULL;
    if (!buffer || buffer_size == 0u) return "";
    buffer[0] = '\0';
    if (!runtime_scene_bridge_get_last_object_id_for_scene_index(scene_object_index,
                                                                 buffer,
                                                                 buffer_size)) {
        return "";
    }
    id = buffer;
    prefix = strrchr(id, '_');
    return (prefix && prefix[1]) ? prefix + 1 : id;
}

static int SceneEditorSurfaceRenderObjectList(SDL_Renderer* renderer,
                                              SDL_Rect bounds,
                                              int cursor_y,
                                              int bottom_y,
                                              int selected_index,
                                              SDL_Color title_color,
                                              SDL_Color body_color) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    const int row_h = 24;
    const int gap = 4;
    int preview_mesh_instances = SceneEditorMeshPreviewStoreInstanceCount();
    int max_rows = 0;
    char line[160];
    if (!renderer || bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;
    ObjectEditorClearObjectListRows();

    snprintf(line,
             sizeof(line),
             "Scene Objects  %d   Mesh Preview %d",
             sceneSettings.objectCount,
             preview_mesh_instances);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                bottom_y,
                                                line,
                                                title_color,
                                                false,
                                                6);

    max_rows = (bottom_y - cursor_y) / (row_h + gap);
    if (max_rows > 6) max_rows = 6;
    if (max_rows < 1) max_rows = 1;
    if (max_rows > sceneSettings.objectCount) max_rows = sceneSettings.objectCount;

    for (int i = 0; i < max_rows; ++i) {
        const SceneObject* obj = &sceneSettings.sceneObjects[i];
        RuntimeSceneBridge3DDigestState digest = {0};
        const RayTracingRuntimeMeshAssetInstance* loaded_mesh =
            SceneEditorSurfaceFindLoadedMeshPreview(i);
        const RayTracingRuntimeMeshAssetSkippedInstance* skipped_mesh =
            SceneEditorSurfaceFindSkippedMeshPreview(i);
        const RuntimeSceneBridgePrimitiveDigest* primitive = NULL;
        char object_id_label[64];
        const char* short_id = SceneEditorSurfaceShortObjectId(i,
                                                               object_id_label,
                                                               sizeof(object_id_label));
        const char* role = NULL;
        bool selected = i == selected_index;
        SDL_Rect row = {bounds.x, cursor_y, bounds.w, row_h};
        SDL_Color fill = selected ? (SDL_Color){96, 104, 112, 220}
                                  : (SDL_Color){20, 23, 26, 210};
        SDL_Color border = selected ? (SDL_Color){188, 198, 208, 255}
                                    : (SDL_Color){48, 54, 60, 220};
        runtime_scene_bridge_get_last_3d_digest_state(&digest);
        primitive = SceneEditorSurfaceFindPrimitiveDigest(&digest, i);
        if (loaded_mesh) {
            role = "mesh loaded";
        } else if (skipped_mesh && SceneEditorMeshPreviewStoreHasSceneObject(i)) {
            role = "mesh preview";
        } else if (skipped_mesh) {
            role = "mesh skipped";
        } else if (primitive) {
            role = SceneEditorSurfacePrimitiveLabel(primitive->kind);
        } else {
            role = obj->type[0] ? obj->type : "object";
        }
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &row);
        ObjectEditorRegisterObjectListRow(i, row);
        if (skipped_mesh && SceneEditorMeshPreviewStoreHasSceneObject(i)) {
            snprintf(line,
                     sizeof(line),
                     "#%d  %s  %s  LOD from %.1f MB",
                     i,
                     role,
                     short_id,
                     (double)skipped_mesh->file_size_bytes / (1024.0 * 1024.0));
        } else if (skipped_mesh) {
            snprintf(line,
                     sizeof(line),
                     "#%d  %s  %s  %.1f/%.1f MB",
                     i,
                     role,
                     short_id,
                     (double)skipped_mesh->file_size_bytes / (1024.0 * 1024.0),
                     (double)skipped_mesh->max_file_size_bytes / (1024.0 * 1024.0));
        } else if (loaded_mesh) {
            snprintf(line,
                     sizeof(line),
                     "#%d  %s  %s  z %.1f",
                     i,
                     role,
                     short_id,
                     obj->z);
        } else {
            snprintf(line,
                     sizeof(line),
                     "#%d  %s%s  %s  z %.1f",
                     i,
                     role,
                     SceneObjectIsGuideOnly(obj) ? " guide" : "",
                     short_id,
                     obj->z);
        }
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){row.x + 8, row.y + 2, row.w - 16, row.h - 4},
                            line,
                            body_color);
        cursor_y += row_h + gap;
    }
    if (sceneSettings.objectCount > max_rows) {
        snprintf(line, sizeof(line), "+%d more", sceneSettings.objectCount - max_rows);
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                    bounds,
                                                    cursor_y,
                                                    bottom_y,
                                                    line,
                                                    body_color,
                                                    false,
                                                    6);
    }
    return cursor_y + 4;
}

void SceneEditorSurfaceRenderLeftPaneContent(SDL_Renderer* renderer,
                                             const SceneEditorPaneLayout* layout,
                                             const SceneEditorControlSurfaceContract* contract,
                                             SDL_Color title_color,
                                             SDL_Color body_color) {
    SDL_Rect bounds = {0, 0, 0, 0};
    SDL_Rect object_panel_region = {0, 0, 0, 0};
    int cursor_y = 0;
    int bottom_y = 0;
    char line[256];
    int selected_index = -1;
    int selected_bezier_point = -1;
    if (!renderer || !layout || !contract) return;
    bounds = layout->left_content_rect;
    if (bounds.w <= 0 || bounds.h <= 0) return;
    cursor_y = bounds.y + 2;
    bottom_y = bounds.y + bounds.h;
    if (selectButton.h > 0 && selectButton.y > bounds.y) {
        bottom_y = selectButton.y - 10;
    }

    if (contract->activeMode == EDITOR_MODE_MATERIAL) {
        MaterialEditorRenderPaneControls(renderer, bounds, cursor_y, bottom_y);
        return;
    }

    snprintf(line,
             sizeof(line),
             "%s  |  %s",
             SceneEditorSurfaceModeLabel(contract->activeMode),
             SceneEditorToolStateToolLabel(SceneEditorToolStateGetActive()));
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, title_color, false, 8);

    if (contract->activeMode == EDITOR_MODE_OBJECT) {
        selected_index = ObjectEditorGetSelectedObjectIndex();
        snprintf(line,
                 sizeof(line),
                 "Mesh display: %s  |  stable LOD",
                 SceneEditorMeshDisplayModeName(SceneEditorMeshPreviewModeGet()));
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                    bounds,
                                                    cursor_y,
                                                    bottom_y,
                                                    line,
                                                    body_color,
                                                    false,
                                                    6);
        cursor_y = SceneEditorSurfaceRenderObjectList(renderer,
                                                      bounds,
                                                      cursor_y,
                                                      bottom_y,
                                                      selected_index,
                                                      title_color,
                                                      body_color);
        if (selected_index >= 0 && selected_index < sceneSettings.objectCount) {
            SceneObject* obj = &sceneSettings.sceneObjects[selected_index];
            const char* type = (obj->type[0] ? obj->type : "unknown");
            snprintf(line, sizeof(line), "Selected #%d  %s  mat %d", selected_index, type, obj->material_id);
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, true, 4);
            if (SceneObjectIsGuideOnly(obj)) {
                snprintf(line,
                         sizeof(line),
                         "Pos %.2f, %.2f, %.2f  helper #%06X",
                         obj->x,
                         obj->y,
                         obj->z,
                         (obj->color & 0xFFFFFF));
            } else {
                snprintf(line,
                         sizeof(line),
                         "Pos %.2f, %.2f, %.2f  color #%06X",
                         obj->x,
                         obj->y,
                         obj->z,
                         (obj->color & 0xFFFFFF));
            }
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, true, 8);
        } else {
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                        bounds,
                                                        cursor_y,
                                                        bottom_y,
                                                        "No object selected. Click an object in the viewport.",
                                                        body_color,
                                                        true,
                                                        8);
        }
        cursor_y = ObjectEditorRenderPaneControls(renderer, bounds, cursor_y, bottom_y);
        object_panel_region = bounds;
        object_panel_region.y = cursor_y + 8;
        object_panel_region.h = bottom_y - object_panel_region.y;
        if (object_panel_region.h > 0) {
            ObjectEditorPanels_UpdateLayoutForRegion(&object_panel_region);
            ObjectEditorPanels_DrawAssetList(renderer);
            ObjectEditorPanels_DrawMaterialList(renderer);
        }
        return;
    }

    if (contract->activeMode == EDITOR_MODE_PATH) {
        int handle_segment = -1;
        int handle_index = -1;
        selected_bezier_point = BezierEditorGetSelectedPointIndex();
        snprintf(line, sizeof(line), "Bezier Points: %d", sceneSettings.bezierPath.numPoints);
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 4);
        if (BezierEditorGetSelectedHandle(&handle_segment, &handle_index)) {
            snprintf(line,
                     sizeof(line),
                     "Selected Handle: seg=%d %s",
                     handle_segment,
                     (handle_index == 0) ? "out" : "in");
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 4);
        } else if (selected_bezier_point >= 0 && selected_bezier_point < sceneSettings.bezierPath.numPoints) {
            snprintf(line, sizeof(line), "Selected Point: #%d", selected_bezier_point);
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 4);
        }
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                    bounds,
                                                    cursor_y,
                                                    bottom_y,
                                                    contract->lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D
                                                        ? "3D lane: active tool controls point insert/delete while gizmo handles selection and movement."
                                                        : "2D lane: active tool controls point insert/delete; Shift temporarily overrides Select to Add.",
                                                    body_color,
                                                    true,
                                                    4);
        BezierEditorRenderPaneControls(renderer, bounds, cursor_y, bottom_y);
        return;
    }

    snprintf(line,
             sizeof(line),
             "Camera: x=%.2f y=%.2f z=%.2f zoom=%.2f rot=%.1f",
             sceneSettings.camera.x,
             sceneSettings.camera.y,
             sceneSettings.cameraZ,
             sceneSettings.camera.zoom,
             sceneSettings.camera.rotation * (180.0 / M_PI));
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, true, 4);
    snprintf(line, sizeof(line), "Camera Path Points: %d", sceneSettings.cameraPath.numPoints);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 4);
    if (CameraEditorGetSelectedPointIndex() >= 0 &&
        CameraEditorGetSelectedPointIndex() < sceneSettings.cameraPath.numPoints) {
        int selected_index = CameraEditorGetSelectedPointIndex();
        snprintf(line,
                 sizeof(line),
                 "Selected Camera Point: #%d z=%.2f",
                 selected_index,
                 sceneSettings.cameraPath3D.point_z[selected_index]);
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, true, 4);
    }
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                bottom_y,
                                                contract->lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D
                                                    ? "3D lane: active tool controls point insert/delete; gizmo moves selected camera points in X/Y/Z."
                                                    : "2D lane: active tool controls point insert/delete; Shift temporarily overrides Select to Add.",
                                                body_color,
                                                true,
                                                4);
    CameraEditorRenderPaneControls(renderer, bounds, cursor_y, bottom_y);
}

void SceneEditorSurfaceRenderRightPaneStatus(SDL_Renderer* renderer,
                                             const SceneEditorPaneLayout* layout,
                                             const SceneEditorControlSurfaceContract* contract,
                                             int status_bottom,
                                             SDL_Color title_color,
                                             SDL_Color body_color) {
    SDL_Rect bounds = {0, 0, 0, 0};
    int cursor_y = 0;
    if (!renderer || !layout || !contract) return;
    bounds = layout->right_content_rect;
    if (bounds.w <= 0 || bounds.h <= 0) return;
    if (status_bottom <= bounds.y + 30) {
        status_bottom = bounds.y + bounds.h;
    }
    if (status_bottom > bounds.y + bounds.h) {
        status_bottom = bounds.y + bounds.h;
    }
    cursor_y = bounds.y + 2;
    if (contract->activeMode == EDITOR_MODE_MATERIAL) {
        int preview_bottom = MaterialEditorRenderRightPanePreview(renderer,
                                                                  bounds,
                                                                  cursor_y,
                                                                  status_bottom);
        if (preview_bottom > cursor_y) {
            cursor_y = preview_bottom + 8;
        }
    }
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusTitle,
                                                title_color,
                                                false,
                                                6);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusSource,
                                                body_color,
                                                true,
                                                4);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusPath,
                                                body_color,
                                                true,
                                                4);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusObjects,
                                                body_color,
                                                true,
                                                4);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusRoute,
                                                body_color,
                                                true,
                                                4);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusDigest,
                                                body_color,
                                                true,
                                                4);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                status_bottom,
                                                contract->statusRuntime,
                                                body_color,
                                                true,
                                                4);
    SceneEditorSurfaceRenderFlowLine(renderer,
                                     bounds,
                                     cursor_y,
                                     status_bottom,
                                     contract->statusControls,
                                     body_color,
                                     true,
                                     4);
}
