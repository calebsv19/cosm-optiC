#include "editor/scene_editor_viewport_nav.h"

#include <math.h>

#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"
#include "render/ray_tracing_mode_backend.h"

#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG (-35.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG (24.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.00005)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (240.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR (0.72)
#define SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MIN_FACTOR (0.10)
#define SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MAX_FACTOR (20.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MIN_FACTOR (0.02)
#define SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MAX_FACTOR (500.0)

static bool scene_editor_viewport_nav_resolve_digest(RuntimeSceneBridge3DDigestState* out_digest) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!RayTracingModeBackend_IsControlled3D(&route)) {
        if (out_digest) *out_digest = digest;
        return false;
    }
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    if (out_digest) *out_digest = digest;
    return digest.valid;
}

static bool scene_editor_viewport_nav_target_is_focused_material(int active_mode,
                                                                 int selected_object_index) {
    return active_mode == EDITOR_MODE_MATERIAL &&
           selected_object_index >= 0 &&
           MaterialEditorGetViewMode() == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN;
}

static bool scene_editor_viewport_nav_resolve_target_extents(
    const RuntimeSceneBridge3DDigestState* digest,
    int active_mode,
    int selected_object_index,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z,
    double* span_max) {
    if (scene_editor_viewport_nav_target_is_focused_material(active_mode, selected_object_index) &&
        SceneEditorDigestOverlayResolveObjectExtents(digest,
                                                     selected_object_index,
                                                     min_x,
                                                     min_y,
                                                     min_z,
                                                     max_x,
                                                     max_y,
                                                     max_z,
                                                     span_max)) {
        return true;
    }
    return SceneEditorDigestOverlayResolveExtents(digest,
                                                  min_x,
                                                  min_y,
                                                  min_z,
                                                  max_x,
                                                  max_y,
                                                  max_z,
                                                  span_max);
}

static bool scene_editor_viewport_nav_resolve_fit_zoom(SceneEditorDigestOverlayNavState* nav_state,
                                                       const SDL_Rect* viewport_rect,
                                                       bool reset_angles,
                                                       int active_mode,
                                                       int selected_object_index,
                                                       double* out_fit_zoom) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    double span_max = 0.0;
    double projected_min_x = 0.0;
    double projected_min_y = 0.0;
    double projected_max_x = 0.0;
    double projected_max_y = 0.0;
    double corners[8][3];
    double available_w = 0.0;
    double available_h = 0.0;
    double projected_w = 0.0;
    double projected_h = 0.0;
    double fit_zoom = 1.0;
    int sx = 0;
    int sy = 0;
    int i = 0;
    if (!nav_state || !viewport_rect || !out_fit_zoom) {
        return false;
    }
    if (!scene_editor_viewport_nav_resolve_digest(&digest)) {
        return false;
    }
    if (reset_angles) {
        nav_state->orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
        nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
    }
    if (!scene_editor_viewport_nav_resolve_target_extents(&digest,
                                                          active_mode,
                                                          selected_object_index,
                                                          &min_x,
                                                          &min_y,
                                                          &min_z,
                                                          &max_x,
                                                          &max_y,
                                                          &max_z,
                                                          &span_max)) {
        nav_state->orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
        nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
        nav_state->overlay_zoom = 1.0;
        return true;
    }

    if (!SceneEditorDigestOverlayBuildProjectorWithView(&digest,
                                                        viewport_rect,
                                                        nav_state->orbit_yaw_deg,
                                                        nav_state->orbit_pitch_deg,
                                                        1.0,
                                                        &projector)) {
        return false;
    }

    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = min_x; corners[1][1] = min_y; corners[1][2] = max_z;
    corners[2][0] = min_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = max_z;
    corners[4][0] = max_x; corners[4][1] = min_y; corners[4][2] = min_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = min_z;
    corners[7][0] = max_x; corners[7][1] = max_y; corners[7][2] = max_z;

    for (i = 0; i < 8; ++i) {
        if (!SceneEditorDigestOverlayProjectPoint(&projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (i == 0) {
            projected_min_x = projected_max_x = (double)sx;
            projected_min_y = projected_max_y = (double)sy;
        } else {
            if ((double)sx < projected_min_x) projected_min_x = (double)sx;
            if ((double)sx > projected_max_x) projected_max_x = (double)sx;
            if ((double)sy < projected_min_y) projected_min_y = (double)sy;
            if ((double)sy > projected_max_y) projected_max_y = (double)sy;
        }
    }

    available_w = (double)projector.viewport.w * SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR;
    available_h = (double)projector.viewport.h * SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR;
    projected_w = fmax(1.0, projected_max_x - projected_min_x);
    projected_h = fmax(1.0, projected_max_y - projected_min_y);
    if (available_w > 1.0 && available_h > 1.0) {
        double zoom_x = available_w / projected_w;
        double zoom_y = available_h / projected_h;
        fit_zoom = fmin(zoom_x, zoom_y);
    }
    if (fit_zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) {
        fit_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    }
    if (fit_zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) {
        fit_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;
    }
    *out_fit_zoom = fit_zoom;
    return true;
}

bool SceneEditorViewportNavFitDigestOverlayForTarget(SceneEditorDigestOverlayNavState* nav_state,
                                                     const SDL_Rect* viewport_rect,
                                                     bool reset_angles,
                                                     int active_mode,
                                                     int selected_object_index) {
    double fit_zoom = 1.0;
    if (!scene_editor_viewport_nav_resolve_fit_zoom(nav_state,
                                                   viewport_rect,
                                                   reset_angles,
                                                   active_mode,
                                                   selected_object_index,
                                                   &fit_zoom)) {
        return false;
    }
    nav_state->overlay_zoom = fit_zoom;
    return true;
}

bool SceneEditorViewportNavApplyDigestWheelZoom(SceneEditorDigestOverlayNavState* nav_state,
                                                const SDL_Rect* viewport_rect,
                                                int wheel_y,
                                                int active_mode,
                                                int selected_object_index) {
    RuntimeSceneBridge3DDigestState digest = {0};
    double fit_zoom = 1.0;
    double min_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    double max_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;
    bool material_focus = scene_editor_viewport_nav_target_is_focused_material(active_mode,
                                                                               selected_object_index);
    if (!nav_state || wheel_y == 0) {
        return false;
    }
    if (!scene_editor_viewport_nav_resolve_digest(&digest)) {
        return false;
    }
    if (viewport_rect &&
        scene_editor_viewport_nav_resolve_fit_zoom(nav_state,
                                                   viewport_rect,
                                                   false,
                                                   active_mode,
                                                   selected_object_index,
                                                   &fit_zoom)) {
        double min_factor = material_focus ? SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MIN_FACTOR
                                           : SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MIN_FACTOR;
        double max_factor = material_focus ? SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MAX_FACTOR
                                           : SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MAX_FACTOR;
        min_zoom = fmax(SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM, fit_zoom * min_factor);
        max_zoom = fmin(SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM, fit_zoom * max_factor);
        if (max_zoom < min_zoom) {
            max_zoom = min_zoom;
        }
        if (!(nav_state->overlay_zoom > 0.0) || !isfinite(nav_state->overlay_zoom)) {
            nav_state->overlay_zoom = fit_zoom;
        }
    }
    if (wheel_y > 0) {
        double factor = material_focus ? 1.25 : 1.12;
        nav_state->overlay_zoom *= pow(factor, (double)wheel_y);
    } else {
        double factor = material_focus ? 0.80 : 0.90;
        nav_state->overlay_zoom *= pow(factor, (double)(-wheel_y));
    }
    if (nav_state->overlay_zoom < min_zoom) nav_state->overlay_zoom = min_zoom;
    if (nav_state->overlay_zoom > max_zoom) nav_state->overlay_zoom = max_zoom;
    return true;
}
