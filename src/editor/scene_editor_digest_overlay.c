#include "editor/scene_editor_digest_overlay.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "render/ray_tracing_mode_backend.h"
#include "scene/object_manager.h"

#define SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX 16.0
#define SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.03)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (4.0)

static bool scene_editor_digest_overlay_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              double ax,
                                              double ay,
                                              double az,
                                              double bx,
                                              double by,
                                              double bz,
                                              SDL_Color color);
static SDL_Color SceneEditorDigestOverlayResolvePrimitiveColor(int primitive_index);
bool SceneEditorDigestOverlayResolve(RuntimeSceneBridge3DDigestState* out_digest) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!RayTracingModeBackend_IsCompat3DFallback(&route)) {
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

int SceneEditorDigestOverlayPickBezierPointIndex(const SceneEditorDigestOverlayProjector* projector,
                                                 const Path* path,
                                                 const CameraPath3D* path3d,
                                                 double plane_z,
                                                 int screen_x,
                                                 int screen_y) {
    int pick_index = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !path) return -1;
    for (i = 0; i < path->numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  path->points[i].x,
                                                  path->points[i].y,
                                                  path3d ? path3d->point_z[i] : plane_z,
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (pick_index < 0 || dist2 < best_dist2) {
                pick_index = i;
                best_dist2 = dist2;
            }
        }
    }
    return pick_index;
}

static bool SceneEditorDigestOverlayBezierHandleWorldPosition(const Path* path,
                                                              const CameraPath3D* path3d,
                                                              int segment_index,
                                                              int handle_index,
                                                              double* out_x,
                                                              double* out_y,
                                                              double* out_z,
                                                              double* out_anchor_x,
                                                              double* out_anchor_y,
                                                              double* out_anchor_z,
                                                              double plane_z) {
    int point_index = 0;
    (void)plane_z;
    if (!path || !path3d) return false;
    if (segment_index < 0 || segment_index >= path->numPoints - 1) return false;
    if (!(handle_index == 0 || handle_index == 1)) return false;
    point_index = (handle_index == 0) ? segment_index : (segment_index + 1);
    if (out_anchor_x) *out_anchor_x = path->points[point_index].x;
    if (out_anchor_y) *out_anchor_y = path->points[point_index].y;
    if (out_anchor_z) *out_anchor_z = path3d->point_z[point_index];
    if (out_x) *out_x = path->points[point_index].x + path->handles[segment_index][handle_index].vx;
    if (out_y) *out_y = path->points[point_index].y + path->handles[segment_index][handle_index].vy;
    if (out_z) *out_z = path3d->point_z[point_index] + path3d->handles_vz[segment_index][handle_index];
    return true;
}

static bool SceneEditorDigestOverlayGetBezierSelectionWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!projector || !digest) return false;
    if (!BezierEditorGetSelectionWorldPosition3D(&world_x, &world_y, &world_z)) return false;
    if (out_x) *out_x = world_x;
    if (out_y) *out_y = world_y;
    if (out_z) *out_z = world_z;
    return true;
}

int SceneEditorDigestOverlayPickBezierHandle(const SceneEditorDigestOverlayProjector* projector,
                                             const Path* path,
                                             const CameraPath3D* path3d,
                                             double plane_z,
                                             int screen_x,
                                             int screen_y,
                                             int* out_segment_index,
                                             int* out_handle_index) {
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    int segment = 0;
    if (!projector || !path || !path3d) return -1;
    for (segment = 0; segment < path->numPoints - 1; ++segment) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(path,
                                                                   path3d,
                                                                   segment,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   plane_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment >= 0) {
        if (out_segment_index) *out_segment_index = picked_segment;
        if (out_handle_index) *out_handle_index = picked_handle;
    }
    return picked_segment;
}

static bool SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
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

static bool SceneEditorDigestOverlayProjectBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit) {
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    if (!projector) return false;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 &base_x,
                                                                 &base_y,
                                                                 &base_z)) {
        return false;
    }
    return SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                base_x,
                                                                base_y,
                                                                base_z,
                                                                axis,
                                                                world_length,
                                                                out_anchor_x,
                                                                out_anchor_y,
                                                                out_end_x,
                                                                out_end_y,
                                                                out_pixels_per_unit);
}

SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (BezierEditorGetSelectionKind() == BEZIER_EDITOR_SELECTION_NONE) {
        return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int gx = 0;
        int gy = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (SceneEditorDigestOverlayBezierGizmoAxisLocked(axis)) {
            continue;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
                                                            axis,
                                                            metrics.gizmo_world_length,
                                                            NULL,
                                                            NULL,
                                                            &gx,
                                                            &gy,
                                                            NULL)) {
            continue;
        }
        dx = (double)screen_x - (double)gx;
        dy = (double)screen_y - (double)gy;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX) {
            if (best_axis == SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE || dist2 < best_dist2) {
                best_axis = axis;
                best_dist2 = dist2;
            }
        }
    }
    return best_axis;
}

bool SceneEditorDigestOverlayBezierGizmoAxisLocked(SceneEditorBezier3DGizmoAxis axis) {
    (void)axis;
    return false;
}

static void SceneEditorDigestOverlayDrawBezierGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest,
                                                    int mouse_x,
                                                    int mouse_y,
                                                    const SceneEditorBezier3DGizmoState* gizmo_state) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int anchor_x = 0;
    int anchor_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL)) {
        return;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    hover_axis = SceneEditorDigestOverlayPickBezierGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool locked = SceneEditorDigestOverlayBezierGizmoAxisLocked(axis);
        bool active = (gizmo_state->dragging && gizmo_state->drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
                                                            axis,
                                                            metrics.gizmo_world_length,
                                                            &anchor_x,
                                                            &anchor_y,
                                                            &end_x,
                                                            &end_y,
                                                            NULL)) {
            continue;
        }
        if (locked) {
            axis_color.a = 110;
        } else if (active || hovered) {
            axis_color.a = 255;
        } else {
            axis_color.a = 220;
        }
        if (active) {
            knob_half = 7;
        } else if (hovered) {
            knob_half = 6;
        }
        SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
        SDL_RenderDrawLine(renderer, anchor_x, anchor_y, end_x, end_y);
        knob = (SDL_Rect){end_x - knob_half, end_y - knob_half, knob_half * 2, knob_half * 2};
        if (locked) {
            SDL_RenderDrawRect(renderer, &knob);
        } else {
            SDL_RenderFillRect(renderer, &knob);
        }
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawPathPassive3D(SDL_Renderer* renderer,
                                                      const SceneEditorDigestOverlayProjector* projector,
                                                      const Path* path,
                                                      const CameraPath3D* path3d,
                                                      SDL_Color curve_color,
                                                      bool reverse_endpoints) {
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int i = 0;
    if (!renderer || !projector || !path || !path3d || path->numPoints < 2) return;

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
        if (reverse_endpoints) {
            color = (i == 0) ? end_color : start_color;
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

static void SceneEditorDigestOverlayDrawBezier3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest,
                                                 int mouse_x,
                                                 int mouse_y,
                                                 const SceneEditorBezier3DGizmoState* gizmo_state) {
    const double plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(digest, projector);
    SDL_Color curve_color = {128, 214, 255, 235};
    SDL_Color point_color = {218, 232, 246, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color handle_color = {236, 120, 120, 210};
    SDL_Color selected_handle_color = {255, 196, 98, 255};
    SDL_Color hover_handle_color = {255, 226, 138, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int selected_segment = -1;
    int selected_handle = -1;
    int hover_segment = -1;
    int hover_handle = -1;
    int hover_point = -1;
    BezierEditorSelectionKind selection_kind = BezierEditorGetSelectionKind();
    int i = 0;
    if (!renderer || !projector || !gizmo_state) return;
    hover_point = SceneEditorDigestOverlayPickBezierPointIndex(projector,
                                                               &sceneSettings.bezierPath,
                                                               &sceneSettings.bezierPath3D,
                                                               plane_z,
                                                               mouse_x,
                                                               mouse_y);
    if (SceneEditorDigestOverlayPickBezierHandle(projector,
                                                 &sceneSettings.bezierPath,
                                                 &sceneSettings.bezierPath3D,
                                                 plane_z,
                                                 mouse_x,
                                                 mouse_y,
                                                 &hover_segment,
                                                 &hover_handle) >= 0) {
        hover_point = -1;
    } else {
        hover_segment = -1;
        hover_handle = -1;
    }
    if (sceneSettings.bezierPath.numPoints >= 2) {
        Point previous = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                &sceneSettings.bezierPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   t);
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
    if (BezierEditorGetSelectedHandle(&selected_segment, &selected_handle)) {
        selection_kind = BEZIER_EDITOR_SELECTION_HANDLE;
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            int ax = 0;
            int ay = 0;
            SDL_Rect knob = {0, 0, 0, 0};
            SDL_Color knob_color = handle_color;
            int knob_half = 5;
            bool selected = (selection_kind == BEZIER_EDITOR_SELECTION_HANDLE &&
                             i == selected_segment &&
                             handle_index == selected_handle);
            bool hovered = (i == hover_segment && handle_index == hover_handle);
            if (selected) {
                knob_color = selected_handle_color;
                knob_half = 6;
            } else if (hovered) {
                knob_color = hover_handle_color;
                knob_half = 6;
            }
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   i,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   &anchor_x,
                                                                   &anchor_y,
                                                                   &anchor_z,
                                                                   plane_z)) {
                continue;
            }
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              anchor_x,
                                              anchor_y,
                                              anchor_z,
                                              hx,
                                              hy,
                                              hz,
                                              (SDL_Color){handle_color.r, handle_color.g, handle_color.b, 120});
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, anchor_x, anchor_y, anchor_z, &ax, &ay)) {
                continue;
            }
            knob = (SDL_Rect){px - knob_half, py - knob_half, knob_half * 2, knob_half * 2};
            SDL_SetRenderDrawColor(renderer, knob_color.r, knob_color.g, knob_color.b, knob_color.a);
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (selection_kind == BEZIER_EDITOR_SELECTION_POINT &&
                               i == BezierEditorGetSelectedPointIndex());
        bool point_hovered = (i == hover_point);
        int radius = point_selected ? 6 : (point_hovered ? 5 : 4);
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = point_selected ? selected_color : (point_hovered ? hover_point_color : point_color);
        if (!point_selected && !point_hovered && i == 0) {
            color = start_color;
        } else if (!point_selected && !point_hovered && i == sceneSettings.bezierPath.numPoints - 1) {
            color = end_color;
        }
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.bezierPath.points[i].x,
                                                  sceneSettings.bezierPath.points[i].y,
                                                  sceneSettings.bezierPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);
    }
    if (selection_kind != BEZIER_EDITOR_SELECTION_NONE) {
        SceneEditorDigestOverlayDrawBezierGizmo(renderer, projector, digest, mouse_x, mouse_y, gizmo_state);
    }
}

int SceneEditorDigestOverlayPickCameraPointIndex(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y) {
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector) return -1;
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

bool SceneEditorDigestOverlayPickCameraBezierHandle(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y,
    int* out_segment_index,
    int* out_handle_index) {
    int segment_index = 0;
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    if (!projector) return false;
    for (segment_index = 0; segment_index < sceneSettings.cameraPath.numPoints - 1; ++segment_index) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double handle_x = 0.0;
            double handle_y = 0.0;
            double handle_z = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     segment_index,
                                                     handle_index,
                                                     &handle_x,
                                                     &handle_y,
                                                     &handle_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      handle_x,
                                                      handle_y,
                                                      handle_z,
                                                      &px,
                                                      &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment_index;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment < 0) return false;
    if (out_segment_index) *out_segment_index = picked_segment;
    if (out_handle_index) *out_handle_index = picked_handle;
    return true;
}

int SceneEditorDigestOverlayPickCameraRotationHandle(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !digest) return -1;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = metrics.gizmo_world_length * 0.95;
        double horizontal_len = cos(pitch) * direction_len;
        double end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        double end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        double end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector, end_x, end_y, end_z, &px, &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

bool SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    CameraEditorSelectionKind selection_kind = CAMERA_EDITOR_SELECTION_NONE;
    int point_index = -1;
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double rotation = 0.0;
    double draw_angle = 0.0;
    double direction_len = 0.0;
    if (!projector || !digest) return false;
    selection_kind = CameraEditorGetSelectionKind();
    if (selection_kind != CAMERA_EDITOR_SELECTION_ROTATION_HANDLE) {
        return CameraEditorGetSelectedGizmoWorldPosition(out_x, out_y, out_z);
    }
    point_index = CameraEditorGetSelectedPointIndex();
    if (point_index < 0 || point_index >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    rotation = CameraEditorGetPointRotation(point_index);
    {
        double pitch = CameraEditorGetPointPitch(point_index);
        double horizontal_len = cos(pitch) * (metrics.gizmo_world_length * 0.95);
        direction_len = metrics.gizmo_world_length * 0.95;
        draw_angle = rotation - (M_PI * 0.5);
        if (out_x) {
            *out_x = sceneSettings.cameraPath.points[point_index].x + cos(draw_angle) * horizontal_len;
        }
        if (out_y) {
            *out_y = sceneSettings.cameraPath.points[point_index].y + sin(draw_angle) * horizontal_len;
        }
        if (out_z) {
            *out_z = sceneSettings.cameraPath3D.point_z[point_index] + sin(pitch) * direction_len;
        }
    }
    return true;
}

SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickCameraGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) {
        return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int gx = 0;
        int gy = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  NULL,
                                                                  NULL,
                                                                  &gx,
                                                                  &gy,
                                                                  NULL)) {
            continue;
        }
        dx = (double)screen_x - (double)gx;
        dy = (double)screen_y - (double)gy;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX) {
            if (best_axis == SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE || dist2 < best_dist2) {
                best_axis = axis;
                best_dist2 = dist2;
            }
        }
    }
    return best_axis;
}

static void SceneEditorDigestOverlayDrawCameraGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest,
                                                    int mouse_x,
                                                    int mouse_y,
                                                    const SceneEditorCamera3DGizmoState* gizmo_state) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int anchor_x = 0;
    int anchor_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    hover_axis = SceneEditorDigestOverlayPickCameraGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool active = (gizmo_state->dragging && gizmo_state->drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  &anchor_x,
                                                                  &anchor_y,
                                                                  &end_x,
                                                                  &end_y,
                                                                  NULL)) {
            continue;
        }
        if (active || hovered) {
            axis_color.a = 255;
        } else {
            axis_color.a = 220;
        }
        if (active) {
            knob_half = 7;
        } else if (hovered) {
            knob_half = 6;
        }
        SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
        SDL_RenderDrawLine(renderer, anchor_x, anchor_y, end_x, end_y);
        knob = (SDL_Rect){end_x - knob_half, end_y - knob_half, knob_half * 2, knob_half * 2};
        SDL_RenderFillRect(renderer, &knob);
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawCamera3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest,
                                                 int mouse_x,
                                                 int mouse_y,
                                                 const SceneEditorCamera3DGizmoState* gizmo_state) {
    SDL_Color curve_color = {210, 168, 255, 230};
    SDL_Color point_color = {212, 238, 255, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color handle_color = {255, 186, 104, 200};
    SDL_Color direction_color = {200, 170, 255, 220};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int selected_point = CameraEditorGetSelectedPointIndex();
    int hover_point = -1;
    int i = 0;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    hover_point = SceneEditorDigestOverlayPickCameraPointIndex(projector, mouse_x, mouse_y);
    for (i = 0; i < sceneSettings.cameraPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            double end_x = 0.0;
            double end_y = 0.0;
            double end_z = 0.0;
            int anchor_px = 0;
            int anchor_py = 0;
            int end_px = 0;
            int end_py = 0;
            int knob_half = 4;
            SDL_Rect knob = {0, 0, 0, 0};
            bool related_selected = (selected_point == i) || (selected_point == i + 1);
            SDL_Color draw_color = handle_color;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     i,
                                                     handle_index,
                                                     &end_x,
                                                     &end_y,
                                                     &end_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      anchor_x,
                                                      anchor_y,
                                                      anchor_z,
                                                      &anchor_px,
                                                      &anchor_py) ||
                !SceneEditorDigestOverlayProjectPoint(projector,
                                                      end_x,
                                                      end_y,
                                                      end_z,
                                                      &end_px,
                                                      &end_py)) {
                continue;
            }
            if (related_selected) {
                draw_color.a = 255;
                knob_half = 5;
            }
            SDL_SetRenderDrawColor(renderer, draw_color.r, draw_color.g, draw_color.b, draw_color.a);
            SDL_RenderDrawLine(renderer, anchor_px, anchor_py, end_px, end_py);
            knob = (SDL_Rect){end_px - knob_half, end_py - knob_half, knob_half * 2, knob_half * 2};
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    if (sceneSettings.cameraPath.numPoints >= 2) {
        Point previous_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                &sceneSettings.cameraPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                   &sceneSettings.cameraPath3D,
                                                                   t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous_xy.x,
                                              previous_xy.y,
                                              previous_z,
                                              current_xy.x,
                                              current_xy.y,
                                              current_z,
                                              curve_color);
            previous_xy = current_xy;
            previous_z = current_z;
        }
    }
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (i == selected_point);
        bool point_hovered = (i == hover_point);
        int radius = point_selected ? 6 : (point_hovered ? 5 : 4);
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = point_selected ? selected_color : (point_hovered ? hover_point_color : point_color);
        if (!point_selected && !point_hovered && i == 0) {
            color = end_color;
        } else if (!point_selected && !point_hovered && i == sceneSettings.cameraPath.numPoints - 1) {
            color = start_color;
        }
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = 0.0;
        double horizontal_len = 0.0;
        double dir_end_x = 0.0;
        double dir_end_y = 0.0;
        double dir_end_z = 0.0;
        int dir_end_px = 0;
        int dir_end_py = 0;
        SDL_Color dir_color = direction_color;
        SDL_Rect dir_knob = {0, 0, 0, 0};
        int dir_knob_half = point_selected ? 5 : 4;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);

        direction_len = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector).gizmo_world_length * 0.95;
        horizontal_len = cos(pitch) * direction_len;
        dir_end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        dir_end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        dir_end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        if (SceneEditorDigestOverlayProjectPoint(projector,
                                                 dir_end_x,
                                                 dir_end_y,
                                                 dir_end_z,
                                                 &dir_end_px,
                                                 &dir_end_py)) {
            if (point_selected) {
                dir_color.a = 255;
            }
            SDL_SetRenderDrawColor(renderer, dir_color.r, dir_color.g, dir_color.b, dir_color.a);
            SDL_RenderDrawLine(renderer, px, py, dir_end_px, dir_end_py);
            dir_knob = (SDL_Rect){dir_end_px - dir_knob_half,
                                  dir_end_py - dir_knob_half,
                                  dir_knob_half * 2,
                                  dir_knob_half * 2};
            SDL_RenderFillRect(renderer, &dir_knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &dir_knob);
        }
    }
    if (selected_point >= 0 && selected_point < sceneSettings.cameraPath.numPoints) {
        SceneEditorDigestOverlayDrawCameraGizmo(renderer, projector, digest, mouse_x, mouse_y, gizmo_state);
    }
}

bool SceneEditorDigestOverlayApplyCameraGizmoDrag(const SceneEditorDigestOverlayProjector* projector,
                                                  const RuntimeSceneBridge3DDigestState* digest,
                                                  const SceneEditorCamera3DGizmoState* gizmo_state,
                                                  int mouse_x,
                                                  int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int axis_anchor_x = 0;
    int axis_anchor_y = 0;
    int axis_end_x = 0;
    int axis_end_y = 0;
    double pixels_per_unit = 0.0;
    double axis_dx = 0.0;
    double axis_dy = 0.0;
    double axis_len = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double projected_px = 0.0;
    double world_delta = 0.0;
    if (!projector || !digest || !gizmo_state || !gizmo_state->dragging) return false;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                              gizmo_state->drag_start_world_x,
                                                              gizmo_state->drag_start_world_y,
                                                              gizmo_state->drag_start_world_z,
                                                              gizmo_state->drag_axis,
                                                              metrics.gizmo_world_length,
                                                              &axis_anchor_x,
                                                              &axis_anchor_y,
                                                              &axis_end_x,
                                                              &axis_end_y,
                                                              &pixels_per_unit)) {
        return false;
    }
    axis_dx = (double)axis_end_x - (double)axis_anchor_x;
    axis_dy = (double)axis_end_y - (double)axis_anchor_y;
    axis_len = sqrt(axis_dx * axis_dx + axis_dy * axis_dy);
    if (axis_len <= 1e-6 || pixels_per_unit <= 1e-6) return false;
    mouse_dx = (double)mouse_x - (double)gizmo_state->drag_start_mouse_x;
    mouse_dy = (double)mouse_y - (double)gizmo_state->drag_start_mouse_y;
    projected_px = mouse_dx * (axis_dx / axis_len) + mouse_dy * (axis_dy / axis_len);
    world_delta = projected_px / pixels_per_unit;
    if (!gizmo_state->smooth_drag) {
        world_delta = SceneEditorDigestOverlayQuantizeWorldValue(world_delta, metrics.snap_step);
    }
    switch (gizmo_state->drag_axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x + world_delta,
                                                   gizmo_state->drag_start_world_y,
                                                   gizmo_state->drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x,
                                                   gizmo_state->drag_start_world_y + world_delta,
                                                   gizmo_state->drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x,
                                                   gizmo_state->drag_start_world_y,
                                                   gizmo_state->drag_start_world_z + world_delta);
        default:
            return false;
    }
}

bool SceneEditorDigestOverlayApplyBezierGizmoDrag(const SceneEditorDigestOverlayProjector* projector,
                                                  const RuntimeSceneBridge3DDigestState* digest,
                                                  const SceneEditorBezier3DGizmoState* gizmo_state,
                                                  int mouse_x,
                                                  int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int axis_anchor_x = 0;
    int axis_anchor_y = 0;
    int axis_end_x = 0;
    int axis_end_y = 0;
    double pixels_per_unit = 0.0;
    double axis_dx = 0.0;
    double axis_dy = 0.0;
    double axis_len = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double projected_px = 0.0;
    double world_delta = 0.0;
    double new_x = 0.0;
    double new_y = 0.0;
    double new_z = 0.0;
    if (!projector || !digest || !gizmo_state) return false;
    if (!gizmo_state->dragging) return false;
    new_x = gizmo_state->drag_start_world_x;
    new_y = gizmo_state->drag_start_world_y;
    new_z = gizmo_state->drag_start_world_z;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                        digest,
                                                        gizmo_state->drag_axis,
                                                        metrics.gizmo_world_length,
                                                        &axis_anchor_x,
                                                        &axis_anchor_y,
                                                        &axis_end_x,
                                                        &axis_end_y,
                                                        &pixels_per_unit)) {
        return false;
    }
    axis_dx = (double)axis_end_x - (double)axis_anchor_x;
    axis_dy = (double)axis_end_y - (double)axis_anchor_y;
    axis_len = sqrt(axis_dx * axis_dx + axis_dy * axis_dy);
    if (axis_len <= 1e-6 || pixels_per_unit <= 1e-6) return false;
    mouse_dx = (double)mouse_x - (double)gizmo_state->drag_start_mouse_x;
    mouse_dy = (double)mouse_y - (double)gizmo_state->drag_start_mouse_y;
    projected_px = mouse_dx * (axis_dx / axis_len) + mouse_dy * (axis_dy / axis_len);
    world_delta = projected_px / pixels_per_unit;
    if (!gizmo_state->smooth_drag) {
        world_delta = SceneEditorDigestOverlayQuantizeWorldValue(world_delta, metrics.snap_step);
    }
    switch (gizmo_state->drag_axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            new_x += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            new_y += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            new_z += world_delta;
            break;
        default:
            return false;
    }
    return BezierEditorMoveSelectionTo3D(new_x, new_y, new_z);
}

static bool SceneEditorDigestOverlayPrimitiveScreenRect(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Rect* out_rect,
    SDL_Point* out_center) {
    double corners[8][3] = {{0.0}};
    int corner_count = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int center_x = 0;
    int center_y = 0;
    int i = 0;
    bool seeded = false;

    if (!projector || !primitive || !out_rect || !out_center) return false;

    if (!primitive->has_dimensions) {
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  primitive->origin_x,
                                                  primitive->origin_y,
                                                  primitive->origin_z,
                                                  &center_x,
                                                  &center_y)) {
            return false;
        }
        out_rect->x = center_x - 14;
        out_rect->y = center_y - 14;
        out_rect->w = 28;
        out_rect->h = 28;
        out_center->x = center_x;
        out_center->y = center_y;
        return true;
    }

    if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z;
        corner_count = 4;
    } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
               primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        double half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z - half_d;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z - half_d;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z - half_d;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z - half_d;
        corners[4][0] = primitive->origin_x - half_w; corners[4][1] = primitive->origin_y - half_h; corners[4][2] = primitive->origin_z + half_d;
        corners[5][0] = primitive->origin_x + half_w; corners[5][1] = primitive->origin_y - half_h; corners[5][2] = primitive->origin_z + half_d;
        corners[6][0] = primitive->origin_x + half_w; corners[6][1] = primitive->origin_y + half_h; corners[6][2] = primitive->origin_z + half_d;
        corners[7][0] = primitive->origin_x - half_w; corners[7][1] = primitive->origin_y + half_h; corners[7][2] = primitive->origin_z + half_d;
        corner_count = 8;
    } else {
        return false;
    }

    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &center_x,
                                              &center_y)) {
        center_x = projector->viewport.x + projector->viewport.w / 2;
        center_y = projector->viewport.y + projector->viewport.h / 2;
    }
    for (i = 0; i < corner_count; ++i) {
        int sx = 0;
        int sy = 0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (!seeded) {
            min_x = max_x = sx;
            min_y = max_y = sy;
            seeded = true;
        } else {
            if (sx < min_x) min_x = sx;
            if (sx > max_x) max_x = sx;
            if (sy < min_y) min_y = sy;
            if (sy > max_y) max_y = sy;
        }
    }
    if (!seeded) return false;

    out_rect->x = min_x;
    out_rect->y = min_y;
    out_rect->w = (max_x - min_x) + 1;
    out_rect->h = (max_y - min_y) + 1;
    out_center->x = center_x;
    out_center->y = center_y;
    return true;
}

int SceneEditorDigestOverlayPickObjectIndex(const SceneEditorDigestOverlayProjector* projector,
                                            const RuntimeSceneBridge3DDigestState* digest,
                                            int mx,
                                            int my) {
    int pick_index = -1;
    double pick_area = 0.0;
    double fallback_dist2 = 0.0;
    int i = 0;

    if (!projector || !digest) return -1;

    for (i = 0; i < digest->primitive_count && i < sceneSettings.objectCount; ++i) {
        SDL_Rect rect = {0, 0, 0, 0};
        SDL_Point center = {0, 0};
        SDL_Rect expanded = {0, 0, 0, 0};
        double area = 0.0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        const int pad = 6;

        if (!SceneEditorDigestOverlayPrimitiveScreenRect(projector, &digest->primitives[i], &rect, &center)) {
            continue;
        }

        expanded.x = rect.x - pad;
        expanded.y = rect.y - pad;
        expanded.w = rect.w + pad * 2;
        expanded.h = rect.h + pad * 2;
        area = (double)expanded.w * (double)expanded.h;
        if (scene_editor_digest_overlay_point_in_rect(mx, my, &expanded)) {
            if (pick_index < 0 || area < pick_area) {
                pick_index = i;
                pick_area = area;
            }
            continue;
        }

        dx = (double)mx - (double)center.x;
        dy = (double)my - (double)center.y;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= 26.0 * 26.0) {
            if (pick_index < 0 || dist2 < fallback_dist2) {
                pick_index = i;
                fallback_dist2 = dist2;
            }
        }
    }

    return pick_index;
}

static SDL_Color SceneEditorColorFromPackedRGB(int packed, Uint8 alpha) {
    SDL_Color out;
    out.r = (Uint8)((packed >> 16) & 0xFF);
    out.g = (Uint8)((packed >> 8) & 0xFF);
    out.b = (Uint8)(packed & 0xFF);
    out.a = alpha;
    return out;
}

static SDL_Color SceneEditorDigestOverlayResolvePrimitiveColor(int primitive_index) {
    SDL_Color color = {220, 200, 88, 240};
    if (primitive_index >= 0 && primitive_index < sceneSettings.objectCount) {
        SceneObject* obj = &sceneSettings.sceneObjects[primitive_index];
        if (obj->color != 0) {
            color = SceneEditorColorFromPackedRGB(obj->color, 240);
        }
    }
    return color;
}

static void SceneEditorDigestOverlayDrawSelectionMarker(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Color color) {
    int cx = 0;
    int cy = 0;
    const int outer_r = 5;
    const int inner_r = 2;
    SDL_Rect outer_box = {0, 0, 0, 0};
    SDL_Rect inner_box = {0, 0, 0, 0};
    if (!renderer || !projector || !primitive) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &cx,
                                              &cy)) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    outer_box = (SDL_Rect){cx - outer_r, cy - outer_r, outer_r * 2, outer_r * 2};
    inner_box = (SDL_Rect){cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2};
    SDL_RenderDrawRect(renderer, &outer_box);
    SDL_RenderFillRect(renderer, &inner_box);
}

static void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
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

static void SceneEditorDigestOverlayDrawBox(SDL_Renderer* renderer,
                                            const SceneEditorDigestOverlayProjector* projector,
                                            double min_x,
                                            double min_y,
                                            double min_z,
                                            double max_x,
                                            double max_y,
                                            double max_z,
                                            SDL_Color color) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3];
    int i = 0;
    if (!renderer || !projector) return;
    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = max_x; corners[1][1] = min_y; corners[1][2] = min_z;
    corners[2][0] = max_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = min_z;
    corners[4][0] = min_x; corners[4][1] = min_y; corners[4][2] = max_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = max_z;
    corners[7][0] = min_x; corners[7][1] = max_y; corners[7][2] = max_z;
    for (i = 0; i < 12; ++i) {
        int a = k_edges[i][0];
        int b = k_edges[i][1];
        SceneEditorDigestOverlayDrawLine3(renderer,
                                          projector,
                                          corners[a][0], corners[a][1], corners[a][2],
                                          corners[b][0], corners[b][1], corners[b][2],
                                          color);
    }
}

static void SceneEditorDigestOverlayDrawConstructionPlane(SDL_Renderer* renderer,
                                                          const SceneEditorDigestOverlayProjector* projector,
                                                          const RuntimeSceneBridge3DDigestState* digest,
                                                          SDL_Color color) {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double plane_z = 0.0;
    if (!renderer || !projector || !digest) return;
    if (!digest->has_scene_bounds || !digest->has_construction_plane) return;
    min_x = digest->bounds_min_x;
    max_x = digest->bounds_max_x;
    min_y = digest->bounds_min_y;
    max_y = digest->bounds_max_y;
    plane_z = digest->construction_plane_offset;
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, min_y, plane_z, max_x, min_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, min_y, plane_z, max_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, max_y, plane_z, min_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, max_y, plane_z, min_x, min_y, plane_z, color);
}

static void SceneEditorDigestOverlayDrawPrism(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              const RuntimeSceneBridgePrimitiveDigest* primitive,
                                              SDL_Color color) {
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    if (!renderer || !projector || !primitive) return;
    if (!(primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
          primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) {
        return;
    }
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    SceneEditorDigestOverlayDrawBox(renderer,
                                    projector,
                                    primitive->origin_x - half_w,
                                    primitive->origin_y - half_h,
                                    primitive->origin_z - half_d,
                                    primitive->origin_x + half_w,
                                    primitive->origin_y + half_h,
                                    primitive->origin_z + half_d,
                                    color);
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
    int i = 0;
    if (!renderer || !viewport || !nav_state || !bezier_gizmo_state || !camera_gizmo_state) return -1;
    if (!SceneEditorDigestOverlayResolve(&digest)) return -1;
    if (!SceneEditorDigestOverlayBuildProjector(&digest, viewport, nav_state, &projector)) return -1;

    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &projector.viewport);
    if (active_mode == 1 && scene_editor_digest_overlay_point_in_rect(mouse_x, mouse_y, &projector.viewport)) {
        hover_object_index = SceneEditorDigestOverlayPickObjectIndex(&projector, &digest, mouse_x, mouse_y);
    }

    if (digest.has_scene_bounds) {
        SDL_Color bounds_color = digest.bounds_enabled
                                     ? (SDL_Color){90, 130, 190, 220}
                                     : (SDL_Color){70, 84, 102, 170};
        SceneEditorDigestOverlayDrawBox(renderer,
                                        &projector,
                                        digest.bounds_min_x,
                                        digest.bounds_min_y,
                                        digest.bounds_min_z,
                                        digest.bounds_max_x,
                                        digest.bounds_max_y,
                                        digest.bounds_max_z,
                                        bounds_color);
    }

    SceneEditorDigestOverlayDrawConstructionPlane(renderer,
                                                  &projector,
                                                  &digest,
                                                  (SDL_Color){205, 176, 106, 220});

    for (i = 0; i < digest.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest.primitives[i];
        SDL_Color primitive_color = SceneEditorDigestOverlayResolvePrimitiveColor(i);
        bool is_selected = (active_mode == 1 && selected_object_index == i);
        bool is_hover = (active_mode == 1 && hover_object_index == i);
        SDL_Color highlight_color = is_selected
                                        ? (SDL_Color){255, 120, 70, 255}
                                        : (SDL_Color){84, 224, 255, 245};
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive_color);
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, &projector, primitive, highlight_color);
            }
        } else {
            SceneEditorDigestOverlayDrawPrism(renderer,
                                              &projector,
                                              primitive,
                                              primitive_color);
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, &projector, primitive, highlight_color);
            }
        }
    }

    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.bezierPath,
                                              &sceneSettings.bezierPath3D,
                                              (SDL_Color){128, 214, 255, 210},
                                              false);
    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.cameraPath,
                                              &sceneSettings.cameraPath3D,
                                              (SDL_Color){210, 168, 255, 210},
                                              true);

    if (active_mode == 0) {
        SceneEditorDigestOverlayDrawBezier3D(renderer,
                                             &projector,
                                             &digest,
                                             mouse_x,
                                             mouse_y,
                                             bezier_gizmo_state);
    } else if (active_mode == 2) {
        SceneEditorDigestOverlayDrawCamera3D(renderer,
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
