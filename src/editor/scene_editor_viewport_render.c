#include "editor/scene_editor_viewport_render.h"

#include <math.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/object_editor.h"
#include "editor/scene_editor.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/fluid/fluid_state.h"

typedef enum SceneEditorViewportRenderLane {
    SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D = 0,
    SCENE_EDITOR_VIEWPORT_RENDER_LANE_DIGEST_3D = 1
} SceneEditorViewportRenderLane;

static void scene_editor_viewport_render_fluid_bounds(SDL_Renderer* renderer) {
    Camera cam = {0};
    SpaceModeViewContext view_ctx = {0};
    CameraPoint minS = {0};
    CameraPoint maxS = {0};
    SDL_Rect rect = {0};
    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;

    if (!renderer || !g_fluidGrid.valid) return;

    cam = CameraBuildPreviewCamera(&sceneSettings.camera,
                                   GetCurrentMarginPixels(),
                                   sceneSettings.windowWidth,
                                   sceneSettings.windowHeight);
    view_ctx = EditorModeRouter_BuildViewContext(&cam,
                                                 sceneSettings.windowWidth,
                                                 sceneSettings.windowHeight);
    minS = SpaceModeAdapter_WorldToScreen(&view_ctx, g_fluidGrid.min_x, g_fluidGrid.min_y);
    maxS = SpaceModeAdapter_WorldToScreen(&view_ctx, g_fluidGrid.max_x, g_fluidGrid.max_y);
    x0 = (int)lrint(fmin(minS.x, maxS.x));
    x1 = (int)lrint(fmax(minS.x, maxS.x));
    y0 = (int)lrint(fmin(minS.y, maxS.y));
    y1 = (int)lrint(fmax(minS.y, maxS.y));
    rect = (SDL_Rect){x0, y0, x1 - x0, y1 - y0};

    SDL_SetRenderDrawColor(renderer, 120, 200, 255, 180);
    SDL_RenderDrawRect(renderer, &rect);
}

static SceneEditorViewportRenderLane scene_editor_viewport_render_resolve_lane(void) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    if (RayTracingModeBackend_IsControlled3D(&route)) {
        return SCENE_EDITOR_VIEWPORT_RENDER_LANE_DIGEST_3D;
    }
    return SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D;
}

static void scene_editor_viewport_render_active_mode_layer(SDL_Renderer* renderer,
                                                           int current_mode) {
    if (!renderer) return;

    switch (current_mode) {
        case 0:
            RenderBezierEditor(renderer);
            break;
        case 1:
            RenderObjectEditor(renderer);
            break;
        case 2:
            RenderCameraEditor(renderer);
            break;
        default:
            break;
    }
}

void SceneEditorViewportRenderDraw(SDL_Renderer* renderer,
                                   int current_mode,
                                   SceneEditorViewportDigestRenderFn digest_render) {
    SceneEditorViewportRenderLane lane = scene_editor_viewport_render_resolve_lane();
    SceneEditorPaneLayout pane_layout = {0};
    SDL_Rect previous_clip = {0};
    bool had_clip = false;
    bool use_clip = false;

    if (!renderer) return;
    had_clip = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    if (had_clip) {
        SDL_RenderGetClipRect(renderer, &previous_clip);
    }
    if (SceneEditorGetPaneLayout(&pane_layout)) {
        SDL_RenderSetClipRect(renderer, &pane_layout.viewport_rect);
        use_clip = true;
    }

    if (lane == SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D) {
        scene_editor_viewport_render_active_mode_layer(renderer, current_mode);
        scene_editor_viewport_render_fluid_bounds(renderer);
    } else {
        if (digest_render) {
            digest_render(renderer);
        }
    }

    if (use_clip) {
        if (had_clip) {
            SDL_RenderSetClipRect(renderer, &previous_clip);
        } else {
            SDL_RenderSetClipRect(renderer, NULL);
        }
    }
}
