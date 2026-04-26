#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/ray_tracing_mode_backend.h"

#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.03)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (4.0)

static bool scene_editor_digest_overlay_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

bool SceneEditorDigestOverlayResolve(RuntimeSceneBridge3DDigestState* out_digest) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!RayTracingModeBackend_IsControlled3D(&route)) {
        if (out_digest) {
            memset(out_digest, 0, sizeof(*out_digest));
        }
        return false;
    }
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    if (out_digest) {
        *out_digest = digest;
    }
    return digest.valid;
}

bool SceneEditorDigestOverlayResolveExtents(const RuntimeSceneBridge3DDigestState* digest,
                                            double* out_min_x,
                                            double* out_min_y,
                                            double* out_min_z,
                                            double* out_max_x,
                                            double* out_max_y,
                                            double* out_max_z,
                                            double* out_span_max) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;
    int i = 0;
    double span_x = 0.0;
    double span_y = 0.0;
    double span_z = 0.0;
    double span_max = 0.0;
    if (!digest) return false;
    if (!digest->valid) return false;

    if (digest->has_scene_bounds) {
        min_x = digest->bounds_min_x;
        min_y = digest->bounds_min_y;
        min_z = digest->bounds_min_z;
        max_x = digest->bounds_max_x;
        max_y = digest->bounds_max_y;
        max_z = digest->bounds_max_z;
        seeded = true;
    }

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        double half_w = primitive->has_dimensions ? fabs(primitive->width) * 0.5 : 0.0;
        double half_h = primitive->has_dimensions ? fabs(primitive->height) * 0.5 : 0.0;
        double half_d = primitive->has_dimensions ? fabs(primitive->depth) * 0.5 : 0.0;
        double p_min_x = primitive->origin_x - half_w;
        double p_max_x = primitive->origin_x + half_w;
        double p_min_y = primitive->origin_y - half_h;
        double p_max_y = primitive->origin_y + half_h;
        double p_min_z = primitive->origin_z - half_d;
        double p_max_z = primitive->origin_z + half_d;
        if (!seeded) {
            min_x = p_min_x;
            max_x = p_max_x;
            min_y = p_min_y;
            max_y = p_max_y;
            min_z = p_min_z;
            max_z = p_max_z;
            seeded = true;
        } else {
            if (p_min_x < min_x) min_x = p_min_x;
            if (p_max_x > max_x) max_x = p_max_x;
            if (p_min_y < min_y) min_y = p_min_y;
            if (p_max_y > max_y) max_y = p_max_y;
            if (p_min_z < min_z) min_z = p_min_z;
            if (p_max_z > max_z) max_z = p_max_z;
        }
    }

    if (!seeded) {
        return false;
    }

    span_x = fmax(1.0, max_x - min_x);
    span_y = fmax(1.0, max_y - min_y);
    span_z = fmax(1.0, max_z - min_z);
    span_max = fmax(span_x, fmax(span_y, span_z));

    if (out_min_x) *out_min_x = min_x;
    if (out_min_y) *out_min_y = min_y;
    if (out_min_z) *out_min_z = min_z;
    if (out_max_x) *out_max_x = max_x;
    if (out_max_y) *out_max_y = max_y;
    if (out_max_z) *out_max_z = max_z;
    if (out_span_max) *out_span_max = span_max;
    return true;
}

bool SceneEditorDigestOverlayBuildProjectorWithView(const RuntimeSceneBridge3DDigestState* digest,
                                                    const SDL_Rect* viewport,
                                                    double yaw_deg,
                                                    double pitch_deg,
                                                    double zoom,
                                                    SceneEditorDigestOverlayProjector* out_projector) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    double span_max = 0.0;
    int viewport_w = 0;
    int viewport_h = 0;
    if (!digest || !viewport || !out_projector) return false;
    if (!SceneEditorDigestOverlayResolveExtents(digest,
                                                &min_x,
                                                &min_y,
                                                &min_z,
                                                &max_x,
                                                &max_y,
                                                &max_z,
                                                &span_max)) {
        return false;
    }
    if (zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    if (zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;

    viewport_w = viewport->w;
    viewport_h = viewport->h;

    *out_projector = (SceneEditorDigestOverlayProjector){
        .viewport = *viewport,
        .center_x = (min_x + max_x) * 0.5,
        .center_y = (min_y + max_y) * 0.5,
        .center_z = (min_z + max_z) * 0.5,
        .yaw_rad = yaw_deg * (M_PI / 180.0),
        .pitch_rad = pitch_deg * (M_PI / 180.0),
        .distance = span_max * 3.4,
        .scale = (double)fmin(viewport_w, viewport_h) * zoom,
        .span_max = span_max
    };
    if (out_projector->distance < 8.0) out_projector->distance = 8.0;
    if (viewport_w <= 0 || viewport_h <= 0) return false;
    if (out_projector->scale < 1.0) out_projector->scale = 1.0;
    return true;
}

bool SceneEditorDigestOverlayBuildProjector(const RuntimeSceneBridge3DDigestState* digest,
                                            const SDL_Rect* viewport,
                                            const SceneEditorDigestOverlayNavState* nav_state,
                                            SceneEditorDigestOverlayProjector* out_projector) {
    if (!nav_state) return false;
    return SceneEditorDigestOverlayBuildProjectorWithView(digest,
                                                          viewport,
                                                          nav_state->orbit_yaw_deg,
                                                          nav_state->orbit_pitch_deg,
                                                          nav_state->overlay_zoom,
                                                          out_projector);
}

static double SceneEditorResolveNiceStepFloor(double raw_step) {
    static const double buckets[] = {10.0, 5.0, 2.5, 2.0, 1.0};
    double magnitude = 1.0;
    double normalized = 0.0;
    size_t i = 0;
    if (!(raw_step > 0.0) || !isfinite(raw_step)) return 1.0;
    magnitude = pow(10.0, floor(log10(raw_step)));
    normalized = raw_step / magnitude;
    for (i = 0; i < sizeof(buckets) / sizeof(buckets[0]); ++i) {
        if (normalized >= buckets[i]) {
            return buckets[i] * magnitude;
        }
    }
    return magnitude;
}

double SceneEditorDigestOverlayQuantizeWorldValue(double value, double step) {
    if (!(step > 0.0) || !isfinite(step)) return value;
    return nearbyint(value / step) * step;
}

SceneEditorBezier3DInteractionMetrics SceneEditorDigestOverlayResolveBezierMetrics(
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorDigestOverlayProjector* projector) {
    SceneEditorBezier3DInteractionMetrics metrics = {1.0, 6.0, 4.0};
    double span = 0.0;
    double raw_step = 0.0;
    if (projector && projector->span_max > 0.0) {
        span = projector->span_max;
    } else if (digest && digest->has_scene_bounds) {
        double span_x = fabs(digest->bounds_max_x - digest->bounds_min_x);
        double span_y = fabs(digest->bounds_max_y - digest->bounds_min_y);
        double span_z = fabs(digest->bounds_max_z - digest->bounds_min_z);
        span = fmax(span_x, fmax(span_y, span_z));
    }
    if (!(span > 0.0) || !isfinite(span)) {
        span = 10.0;
    }
    raw_step = span / 40.0;
    metrics.snap_step = SceneEditorResolveNiceStepFloor(raw_step);
    if (metrics.snap_step < 0.01) {
        metrics.snap_step = 0.01;
    }
    metrics.gizmo_world_length = fmax(metrics.snap_step * 6.0, span * 0.125);
    if (metrics.gizmo_world_length > span * 0.25) {
        metrics.gizmo_world_length = span * 0.25;
    }
    if (metrics.gizmo_world_length < metrics.snap_step * 4.0) {
        metrics.gizmo_world_length = metrics.snap_step * 4.0;
    }
    metrics.default_handle_length = fmax(metrics.snap_step * 6.0, span * 0.12);
    if (metrics.default_handle_length > span * 0.20) {
        metrics.default_handle_length = span * 0.20;
    }
    if (metrics.default_handle_length > metrics.gizmo_world_length * 1.15) {
        metrics.default_handle_length = metrics.gizmo_world_length * 1.15;
    }
    return metrics;
}

bool SceneEditorDigestOverlayProjectPoint(const SceneEditorDigestOverlayProjector* projector,
                                          double world_x,
                                          double world_y,
                                          double world_z,
                                          int* out_x,
                                          int* out_y) {
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double yaw_x = 0.0;
    double yaw_y = 0.0;
    double yaw_z = 0.0;
    double pitch_y = 0.0;
    double screen_x = 0.0;
    double screen_y = 0.0;
    if (!projector || !out_x || !out_y) return false;
    px = world_x - projector->center_x;
    py = world_y - projector->center_y;
    pz = world_z - projector->center_z;

    yaw_x = cos(projector->yaw_rad) * px - sin(projector->yaw_rad) * py;
    yaw_y = sin(projector->yaw_rad) * px + cos(projector->yaw_rad) * py;
    yaw_z = pz;

    pitch_y = cos(projector->pitch_rad) * yaw_y - sin(projector->pitch_rad) * yaw_z;
    screen_x = (double)projector->viewport.x + (double)projector->viewport.w * 0.5 +
               yaw_x * projector->scale;
    screen_y = (double)projector->viewport.y + (double)projector->viewport.h * 0.5 +
               pitch_y * projector->scale;
    *out_x = (int)lround(screen_x);
    *out_y = (int)lround(screen_y);
    return true;
}

static void SceneEditorDigestOverlayRotateCameraToWorld(const SceneEditorDigestOverlayProjector* projector,
                                                        double cam_x,
                                                        double cam_y,
                                                        double cam_z,
                                                        double* out_world_x,
                                                        double* out_world_y,
                                                        double* out_world_z) {
    double yaw_x = 0.0;
    double yaw_y = 0.0;
    double yaw_z = 0.0;
    if (!projector || !out_world_x || !out_world_y || !out_world_z) return;
    yaw_x = cam_x;
    yaw_y = cos(projector->pitch_rad) * cam_y + sin(projector->pitch_rad) * cam_z;
    yaw_z = -sin(projector->pitch_rad) * cam_y + cos(projector->pitch_rad) * cam_z;
    *out_world_x = cos(projector->yaw_rad) * yaw_x + sin(projector->yaw_rad) * yaw_y;
    *out_world_y = -sin(projector->yaw_rad) * yaw_x + cos(projector->yaw_rad) * yaw_y;
    *out_world_z = yaw_z;
}

double SceneEditorDigestOverlayResolveEditPlaneZ(const RuntimeSceneBridge3DDigestState* digest,
                                                 const SceneEditorDigestOverlayProjector* projector) {
    if (digest && digest->has_construction_plane) {
        return digest->construction_plane_offset;
    }
    if (projector) {
        return projector->center_z;
    }
    return 0.0;
}

bool SceneEditorDigestOverlayScreenRayToPlanePoint(const SceneEditorDigestOverlayProjector* projector,
                                                   int screen_x,
                                                   int screen_y,
                                                   double plane_z,
                                                   double* out_world_x,
                                                   double* out_world_y,
                                                   double* out_world_z) {
    double viewport_cx = 0.0;
    double viewport_cy = 0.0;
    double cam_plane_x = 0.0;
    double cam_plane_y = 0.0;
    double origin_rel_x = 0.0;
    double origin_rel_y = 0.0;
    double origin_rel_z = 0.0;
    double dir_rel_x = 0.0;
    double dir_rel_y = 0.0;
    double dir_rel_z = 0.0;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double origin_z = 0.0;
    double dir_x = 0.0;
    double dir_y = 0.0;
    double dir_z = 0.0;
    double t = 0.0;
    if (!projector || !out_world_x || !out_world_y || !out_world_z) return false;
    if (projector->scale <= 1e-6) return false;
    viewport_cx = (double)projector->viewport.x + (double)projector->viewport.w * 0.5;
    viewport_cy = (double)projector->viewport.y + (double)projector->viewport.h * 0.5;
    cam_plane_x = ((double)screen_x - viewport_cx) / projector->scale;
    cam_plane_y = ((double)screen_y - viewport_cy) / projector->scale;
    SceneEditorDigestOverlayRotateCameraToWorld(projector,
                                                cam_plane_x,
                                                cam_plane_y,
                                                projector->distance,
                                                &origin_rel_x,
                                                &origin_rel_y,
                                                &origin_rel_z);
    SceneEditorDigestOverlayRotateCameraToWorld(projector,
                                                0.0,
                                                0.0,
                                                -1.0,
                                                &dir_rel_x,
                                                &dir_rel_y,
                                                &dir_rel_z);
    origin_x = projector->center_x + origin_rel_x;
    origin_y = projector->center_y + origin_rel_y;
    origin_z = projector->center_z + origin_rel_z;
    dir_x = dir_rel_x;
    dir_y = dir_rel_y;
    dir_z = dir_rel_z;
    if (fabs(dir_z) < 1e-6) return false;
    t = (plane_z - origin_z) / dir_z;
    if (t < 0.0) return false;
    *out_world_x = origin_x + dir_x * t;
    *out_world_y = origin_y + dir_y * t;
    *out_world_z = plane_z;
    return true;
}

bool SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
    const SceneEditorDigestOverlayProjector* projector,
    double base_x,
    double base_y,
    double base_z,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit) {
    double target_x = 0.0;
    double target_y = 0.0;
    double target_z = 0.0;
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    double dx = 0.0;
    double dy = 0.0;
    if (!projector) return false;
    target_x = base_x;
    target_y = base_y;
    target_z = base_z;
    switch (axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            target_x += world_length;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            target_y += world_length;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            target_z += world_length;
            break;
        default:
            return false;
    }
    if (!SceneEditorDigestOverlayProjectPoint(projector, base_x, base_y, base_z, &ax, &ay)) return false;
    if (!SceneEditorDigestOverlayProjectPoint(projector, target_x, target_y, target_z, &bx, &by)) return false;
    dx = (double)bx - (double)ax;
    dy = (double)by - (double)ay;
    if (fabs(dx) < 1e-6 && fabs(dy) < 1e-6) return false;
    if (out_anchor_x) *out_anchor_x = ax;
    if (out_anchor_y) *out_anchor_y = ay;
    if (out_end_x) *out_end_x = bx;
    if (out_end_y) *out_end_y = by;
    if (out_pixels_per_unit) {
        *out_pixels_per_unit = sqrt(dx * dx + dy * dy) / world_length;
    }
    return true;
}

void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
                                       const SceneEditorDigestOverlayProjector* projector,
                                       double ax,
                                       double ay,
                                       double az,
                                       double bx,
                                       double by,
                                       double bz,
                                       SDL_Color color) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!renderer || !projector) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, ax, ay, az, &x0, &y0)) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, bx, by, bz, &x1, &y1)) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void SceneEditorDigestOverlayDrawPathPassive3D(SDL_Renderer* renderer,
                                                      const SceneEditorDigestOverlayProjector* projector,
                                                      const Path* path,
                                                      const CameraPath3D* path3d,
                                                      SDL_Color curve_color) {
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    PathTraversalEndpoints endpoints = {0};
    bool have_endpoints = false;
    int i = 0;
    if (!renderer || !projector || !path || !path3d || path->numPoints < 2) return;
    have_endpoints = PathResolveTraversalEndpoints(path, path3d, &endpoints);

    {
        Point previous = GetPositionAlongPathNormalized((Path*)path, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(path, path3d, 0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current = GetPositionAlongPathNormalized((Path*)path, t);
            double current_z = CameraPath3D_GetPositionZNormalized(path, path3d, t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous.x,
                                              previous.y,
                                              previous_z,
                                              current.x,
                                              current.y,
                                              current_z,
                                              curve_color);
            previous = current;
            previous_z = current_z;
        }
    }

    for (i = 0; i < path->numPoints; ++i) {
        int px = 0;
        int py = 0;
        int radius = 0;
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = curve_color;
        if (i != 0 && i != path->numPoints - 1) {
            continue;
        }
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  path->points[i].x,
                                                  path->points[i].y,
                                                  path3d->point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        radius = 5;
        if (have_endpoints) {
            if (i == endpoints.start_point_index) {
                color = start_color;
            } else if (i == endpoints.end_point_index) {
                color = end_color;
            }
        } else {
            color = (i == 0) ? start_color : end_color;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);
    }
}

int SceneEditorDigestOverlayRender(SDL_Renderer* renderer,
                                   const SDL_Rect* viewport,
                                   const SceneEditorDigestOverlayNavState* nav_state,
                                   int active_mode,
                                   int selected_object_index,
                                   int mouse_x,
                                   int mouse_y,
                                   const SceneEditorBezier3DGizmoState* bezier_gizmo_state,
                                   const SceneEditorCamera3DGizmoState* camera_gizmo_state) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    int hover_object_index = -1;
    if (!renderer || !viewport || !nav_state || !bezier_gizmo_state || !camera_gizmo_state) return -1;
    if (!SceneEditorDigestOverlayResolve(&digest)) return -1;
    if (!SceneEditorDigestOverlayBuildProjector(&digest, viewport, nav_state, &projector)) return -1;

    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &projector.viewport);
    if (active_mode == 1 && scene_editor_digest_overlay_point_in_rect(mouse_x, mouse_y, &projector.viewport)) {
        hover_object_index = SceneEditorDigestOverlayPickObjectIndex(&projector, &digest, mouse_x, mouse_y);
    }

    SceneEditorDigestOverlayRenderObjectLayer(renderer,
                                              &projector,
                                              &digest,
                                              active_mode,
                                              selected_object_index,
                                              hover_object_index);

    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.bezierPath,
                                              &sceneSettings.bezierPath3D,
                                              (SDL_Color){128, 214, 255, 210});
    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.cameraPath,
                                              &sceneSettings.cameraPath3D,
                                              (SDL_Color){210, 168, 255, 210});

    if (active_mode == 0) {
        SceneEditorDigestOverlayRenderBezierLayer(renderer,
                                                  &projector,
                                                  &digest,
                                                  mouse_x,
                                                  mouse_y,
                                                  bezier_gizmo_state);
    } else if (active_mode == 2) {
        SceneEditorDigestOverlayRenderCameraLayer(renderer,
                                                  &projector,
                                                  &digest,
                                                  mouse_x,
                                                  mouse_y,
                                                  camera_gizmo_state);
    }

    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x + projector.span_max * 0.15,
                                      projector.center_y,
                                      projector.center_z,
                                      (SDL_Color){230, 110, 110, 240});
    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x,
                                      projector.center_y + projector.span_max * 0.15,
                                      projector.center_z,
                                      (SDL_Color){110, 220, 140, 240});
    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z + projector.span_max * 0.15,
                                      (SDL_Color){120, 170, 240, 240});

    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
    return hover_object_index;
}
