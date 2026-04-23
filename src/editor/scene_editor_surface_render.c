#include "editor/scene_editor_surface_render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "editor/object_editor.h"
#include "editor/object_editor_panels.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_tool_state.h"
#include "render/render_helper.h"

static const char* SceneEditorSurfaceModeLabel(int mode) {
    switch (mode) {
        case 0: return "Bezier";
        case 1: return "Objects";
        case 2: return "Camera";
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
    SceneEditorTool active_tool = SCENE_EDITOR_TOOL_SELECT;
    if (!renderer || !layout || !contract) return;
    bounds = layout->left_content_rect;
    if (bounds.w <= 0 || bounds.h <= 0) return;
    cursor_y = bounds.y + 2;
    bottom_y = bounds.y + bounds.h;
    if (selectButton.h > 0 && selectButton.y > bounds.y) {
        bottom_y = selectButton.y - 10;
    }

    snprintf(line, sizeof(line), "Mode: %s", SceneEditorSurfaceModeLabel(contract->activeMode));
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, title_color, false, 6);
    active_tool = SceneEditorToolStateGetActive();
    snprintf(line, sizeof(line), "Tool: %s", SceneEditorToolStateToolLabel(active_tool));
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 6);
    cursor_y = SceneEditorSurfaceRenderFlowLine(renderer,
                                                bounds,
                                                cursor_y,
                                                bottom_y,
                                                "Tool buttons in this pane are authoritative for Select, Add, and Delete.",
                                                body_color,
                                                true,
                                                8);

    if (contract->activeMode == 1) {
        selected_index = ObjectEditorGetSelectedObjectIndex();
        snprintf(line, sizeof(line), "Objects: %d", sceneSettings.objectCount);
        cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, false, 4);
        if (selected_index >= 0 && selected_index < sceneSettings.objectCount) {
            SceneObject* obj = &sceneSettings.sceneObjects[selected_index];
            const char* type = (obj->type[0] ? obj->type : "unknown");
            snprintf(line, sizeof(line), "Selected #%d type=%s mat=%d", selected_index, type, obj->material_id);
            cursor_y = SceneEditorSurfaceRenderFlowLine(renderer, bounds, cursor_y, bottom_y, line, body_color, true, 4);
            snprintf(line,
                     sizeof(line),
                     "Pos %.2f, %.2f, %.2f  Color #%06X",
                     obj->x,
                     obj->y,
                     obj->z,
                     (obj->color & 0xFFFFFF));
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

    if (contract->activeMode == 0) {
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
