#include "editor/scene_editor_digest_overlay.h"

#include <math.h>

#include "import/runtime_mesh_asset_loader.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.00005)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (240.0)

static void scene_editor_digest_overlay_accumulate_extents(double x,
                                                           double y,
                                                           double z,
                                                           bool* seeded,
                                                           double* min_x,
                                                           double* min_y,
                                                           double* min_z,
                                                           double* max_x,
                                                           double* max_y,
                                                           double* max_z) {
    if (!seeded || !min_x || !min_y || !min_z || !max_x || !max_y || !max_z) return;
    if (!*seeded) {
        *min_x = *max_x = x;
        *min_y = *max_y = y;
        *min_z = *max_z = z;
        *seeded = true;
        return;
    }
    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (y < *min_y) *min_y = y;
    if (y > *max_y) *max_y = y;
    if (z < *min_z) *min_z = z;
    if (z > *max_z) *max_z = z;
}

static void scene_editor_digest_overlay_accumulate_plane_seed_extents(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    bool* seeded,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z) {
    double half_w = 0.0;
    double half_h = 0.0;
    int sx = 0;
    int sy = 0;
    if (!primitive || !primitive->has_dimensions) return;
    half_w = fabs(primitive->width) * 0.5;
    half_h = fabs(primitive->height) * 0.5;
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            double x = primitive->origin_x +
                       primitive->axis_u_x * half_w * (double)sx +
                       primitive->axis_v_x * half_h * (double)sy;
            double y = primitive->origin_y +
                       primitive->axis_u_y * half_w * (double)sx +
                       primitive->axis_v_y * half_h * (double)sy;
            double z = primitive->origin_z +
                       primitive->axis_u_z * half_w * (double)sx +
                       primitive->axis_v_z * half_h * (double)sy;
            scene_editor_digest_overlay_accumulate_extents(x,
                                                           y,
                                                           z,
                                                           seeded,
                                                           min_x,
                                                           min_y,
                                                           min_z,
                                                           max_x,
                                                           max_y,
                                                           max_z);
        }
    }
}

static void scene_editor_digest_overlay_accumulate_prism_seed_extents(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    bool* seeded,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z) {
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    int sx = 0;
    int sy = 0;
    int sz = 0;
    if (!primitive || !primitive->has_dimensions) return;
    half_w = fabs(primitive->width) * 0.5;
    half_h = fabs(primitive->height) * 0.5;
    half_d = fabs(primitive->depth) * 0.5;
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            for (sz = -1; sz <= 1; sz += 2) {
                double x = primitive->origin_x +
                           primitive->axis_u_x * half_w * (double)sx +
                           primitive->axis_v_x * half_h * (double)sy +
                           primitive->normal_x * half_d * (double)sz;
                double y = primitive->origin_y +
                           primitive->axis_u_y * half_w * (double)sx +
                           primitive->axis_v_y * half_h * (double)sy +
                           primitive->normal_y * half_d * (double)sz;
                double z = primitive->origin_z +
                           primitive->axis_u_z * half_w * (double)sx +
                           primitive->axis_v_z * half_h * (double)sy +
                           primitive->normal_z * half_d * (double)sz;
                scene_editor_digest_overlay_accumulate_extents(x,
                                                               y,
                                                               z,
                                                               seeded,
                                                               min_x,
                                                               min_y,
                                                               min_z,
                                                               max_x,
                                                               max_y,
                                                               max_z);
            }
        }
    }
}

static bool scene_editor_digest_overlay_resolve_seed_extents(double* out_min_x,
                                                             double* out_min_y,
                                                             double* out_min_z,
                                                             double* out_max_x,
                                                             double* out_max_y,
                                                             double* out_max_z) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    bool seeded = false;
    int i = 0;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    if (!seeds.valid || seeds.primitive_count <= 0) {
        return false;
    }
    for (i = 0; i < seeds.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
            scene_editor_digest_overlay_accumulate_plane_seed_extents(primitive,
                                                                      &seeded,
                                                                      out_min_x,
                                                                      out_min_y,
                                                                      out_min_z,
                                                                      out_max_x,
                                                                      out_max_y,
                                                                      out_max_z);
        } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) {
            scene_editor_digest_overlay_accumulate_prism_seed_extents(primitive,
                                                                      &seeded,
                                                                      out_min_x,
                                                                      out_min_y,
                                                                      out_min_z,
                                                                      out_max_x,
                                                                      out_max_y,
                                                                      out_max_z);
        } else {
            scene_editor_digest_overlay_accumulate_extents(primitive->origin_x,
                                                           primitive->origin_y,
                                                           primitive->origin_z,
                                                           &seeded,
                                                           out_min_x,
                                                           out_min_y,
                                                           out_min_z,
                                                           out_max_x,
                                                           out_max_y,
                                                           out_max_z);
        }
    }
    return seeded;
}

static void scene_editor_digest_overlay_rotate_mesh_point(
    double* io_x,
    double* io_y,
    double* io_z,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    double x = io_x ? *io_x : 0.0;
    double y = io_y ? *io_y : 0.0;
    double z = io_z ? *io_z : 0.0;
    double cx = 0.0;
    double sx = 0.0;
    double cy = 0.0;
    double sy = 0.0;
    double cz = 0.0;
    double sz = 0.0;
    double tx = 0.0;
    double ty = 0.0;
    double tz = 0.0;
    if (!io_x || !io_y || !io_z || !instance) return;

    cx = cos(instance->rotation_x);
    sx = sin(instance->rotation_x);
    cy = cos(instance->rotation_y);
    sy = sin(instance->rotation_y);
    cz = cos(instance->rotation_z);
    sz = sin(instance->rotation_z);

    ty = y * cx - z * sx;
    tz = y * sx + z * cx;
    y = ty;
    z = tz;

    tx = x * cy + z * sy;
    tz = -x * sy + z * cy;
    x = tx;
    z = tz;

    tx = x * cz - y * sz;
    ty = x * sz + y * cz;
    x = tx;
    y = ty;

    *io_x = x;
    *io_y = y;
    *io_z = z;
}

static void scene_editor_digest_overlay_accumulate_mesh_asset_extents(
    bool* seeded,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!mesh_assets || !seeded || !min_x || !min_y || !min_z || !max_x || !max_y || !max_z) {
        return;
    }
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        const CoreMeshAssetRuntimeDocument* document = NULL;
        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            continue;
        }
        document = &mesh_assets->assets[instance->asset_index].document;
        for (size_t j = 0; j < document->vertex_count; ++j) {
            double x = document->vertices[j].position.x * instance->scale_x;
            double y = document->vertices[j].position.y * instance->scale_y;
            double z = document->vertices[j].position.z * instance->scale_z;
            scene_editor_digest_overlay_rotate_mesh_point(&x, &y, &z, instance);
            x += instance->position_x;
            y += instance->position_y;
            z += instance->position_z;
            scene_editor_digest_overlay_accumulate_extents(x,
                                                           y,
                                                           z,
                                                           seeded,
                                                           min_x,
                                                           min_y,
                                                           min_z,
                                                           max_x,
                                                           max_y,
                                                           max_z);
        }
    }
}

static bool scene_editor_digest_overlay_seed_matches_object(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    int scene_object_index) {
    return primitive && scene_object_index >= 0 &&
           primitive->scene_object_index == scene_object_index;
}

static bool scene_editor_digest_overlay_digest_matches_object(
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    int scene_object_index) {
    return primitive && scene_object_index >= 0 &&
           primitive->scene_object_index == scene_object_index;
}

static void scene_editor_digest_overlay_accumulate_digest_extents(
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    bool* seeded,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z) {
    double half_w = primitive && primitive->has_dimensions ? fabs(primitive->width) * 0.5 : 0.0;
    double half_h = primitive && primitive->has_dimensions ? fabs(primitive->height) * 0.5 : 0.0;
    double half_d = primitive && primitive->has_dimensions ? fabs(primitive->depth) * 0.5 : 0.0;
    if (!primitive) return;
    scene_editor_digest_overlay_accumulate_extents(primitive->origin_x - half_w,
                                                   primitive->origin_y - half_h,
                                                   primitive->origin_z - half_d,
                                                   seeded,
                                                   min_x,
                                                   min_y,
                                                   min_z,
                                                   max_x,
                                                   max_y,
                                                   max_z);
    scene_editor_digest_overlay_accumulate_extents(primitive->origin_x + half_w,
                                                   primitive->origin_y + half_h,
                                                   primitive->origin_z + half_d,
                                                   seeded,
                                                   min_x,
                                                   min_y,
                                                   min_z,
                                                   max_x,
                                                   max_y,
                                                   max_z);
}

static void scene_editor_digest_overlay_accumulate_seed_for_object(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    bool* seeded,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z) {
    if (!primitive) return;
    if (primitive->has_dimensions &&
        primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        scene_editor_digest_overlay_accumulate_plane_seed_extents(primitive,
                                                                  seeded,
                                                                  min_x,
                                                                  min_y,
                                                                  min_z,
                                                                  max_x,
                                                                  max_y,
                                                                  max_z);
        return;
    }
    if (primitive->has_dimensions &&
        (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
         primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) {
        scene_editor_digest_overlay_accumulate_prism_seed_extents(primitive,
                                                                  seeded,
                                                                  min_x,
                                                                  min_y,
                                                                  min_z,
                                                                  max_x,
                                                                  max_y,
                                                                  max_z);
        return;
    }
    scene_editor_digest_overlay_accumulate_extents(primitive->origin_x,
                                                   primitive->origin_y,
                                                   primitive->origin_z,
                                                   seeded,
                                                   min_x,
                                                   min_y,
                                                   min_z,
                                                   max_x,
                                                   max_y,
                                                   max_z);
}

static bool scene_editor_digest_overlay_resolve_object_seed_extents(
    int scene_object_index,
    double* out_min_x,
    double* out_min_y,
    double* out_min_z,
    double* out_max_x,
    double* out_max_y,
    double* out_max_z) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    bool seeded = false;
    int i = 0;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    if (!seeds.valid || seeds.primitive_count <= 0 || scene_object_index < 0) {
        return false;
    }
    for (i = 0; i < seeds.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
        if (!scene_editor_digest_overlay_seed_matches_object(primitive, scene_object_index)) continue;
        scene_editor_digest_overlay_accumulate_seed_for_object(primitive,
                                                               &seeded,
                                                               out_min_x,
                                                               out_min_y,
                                                               out_min_z,
                                                               out_max_x,
                                                               out_max_y,
                                                               out_max_z);
    }
    return seeded;
}

static bool scene_editor_digest_overlay_resolve_object_digest_extents(
    const RuntimeSceneBridge3DDigestState* digest,
    int scene_object_index,
    double* out_min_x,
    double* out_min_y,
    double* out_min_z,
    double* out_max_x,
    double* out_max_y,
    double* out_max_z) {
    bool seeded = false;
    int i = 0;
    if (!digest || !digest->valid || scene_object_index < 0) return false;
    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        if (!scene_editor_digest_overlay_digest_matches_object(primitive, scene_object_index)) continue;
        scene_editor_digest_overlay_accumulate_digest_extents(primitive,
                                                              &seeded,
                                                              out_min_x,
                                                              out_min_y,
                                                              out_min_z,
                                                              out_max_x,
                                                              out_max_y,
                                                              out_max_z);
    }
    return seeded;
}

bool SceneEditorDigestOverlayResolveObjectExtents(const RuntimeSceneBridge3DDigestState* digest,
                                                  int scene_object_index,
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
    double span_x = 0.0;
    double span_y = 0.0;
    double span_z = 0.0;
    double span_max = 0.0;
    bool seeded = false;
    seeded = scene_editor_digest_overlay_resolve_object_seed_extents(scene_object_index,
                                                                     &min_x,
                                                                     &min_y,
                                                                     &min_z,
                                                                     &max_x,
                                                                     &max_y,
                                                                     &max_z);
    if (!seeded) {
        seeded = scene_editor_digest_overlay_resolve_object_digest_extents(digest,
                                                                          scene_object_index,
                                                                          &min_x,
                                                                          &min_y,
                                                                          &min_z,
                                                                          &max_x,
                                                                          &max_y,
                                                                          &max_z);
    }
    if (!seeded) return false;

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

bool SceneEditorDigestOverlayProjectPointF(const SceneEditorDigestOverlayProjector* projector,
                                           double world_x,
                                           double world_y,
                                           double world_z,
                                           double* out_x,
                                           double* out_y) {
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
    *out_x = screen_x;
    *out_y = screen_y;
    return true;
}

double SceneEditorDigestOverlayViewDepth(const SceneEditorDigestOverlayProjector* projector,
                                         double world_x,
                                         double world_y,
                                         double world_z) {
    double yaw_y = 0.0;
    if (!projector) return 0.0;
    yaw_y = sin(projector->yaw_rad) * (world_x - projector->center_x) +
            cos(projector->yaw_rad) * (world_y - projector->center_y);
    return sin(projector->pitch_rad) * yaw_y +
           cos(projector->pitch_rad) * (world_z - projector->center_z);
}

bool SceneEditorDigestOverlayProjectPoint(const SceneEditorDigestOverlayProjector* projector,
                                          double world_x,
                                          double world_y,
                                          double world_z,
                                          int* out_x,
                                          int* out_y) {
    double screen_x = 0.0;
    double screen_y = 0.0;
    if (!out_x || !out_y ||
        !SceneEditorDigestOverlayProjectPointF(projector,
                                               world_x,
                                               world_y,
                                               world_z,
                                               &screen_x,
                                               &screen_y)) {
        return false;
    }
    *out_x = (int)lround(screen_x);
    *out_y = (int)lround(screen_y);
    return true;
}

static bool scene_editor_digest_overlay_projector_from_extents(
    const SDL_Rect* viewport,
    double yaw_deg,
    double pitch_deg,
    double zoom,
    double min_x,
    double min_y,
    double min_z,
    double max_x,
    double max_y,
    double max_z,
    SceneEditorDigestOverlayProjector* out_projector) {
    double span_x = fmax(1.0, max_x - min_x);
    double span_y = fmax(1.0, max_y - min_y);
    double span_z = fmax(1.0, max_z - min_z);
    double span_max = fmax(span_x, fmax(span_y, span_z));
    int viewport_w = 0;
    int viewport_h = 0;
    if (!viewport || !out_projector) return false;
    if (zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    if (zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;

    viewport_w = viewport->w;
    viewport_h = viewport->h;
    if (viewport_w <= 0 || viewport_h <= 0) return false;

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
    if (out_projector->scale < 1.0) out_projector->scale = 1.0;
    return true;
}

static void scene_editor_digest_overlay_projector_apply_navigation_target(
    const SceneEditorDigestOverlayNavState* nav_state,
    SceneEditorDigestOverlayProjector* projector) {
    if (!nav_state || !projector || !nav_state->target_valid) return;
    if (!isfinite(nav_state->target_x) ||
        !isfinite(nav_state->target_y) ||
        !isfinite(nav_state->target_z)) {
        return;
    }
    projector->center_x = nav_state->target_x;
    projector->center_y = nav_state->target_y;
    projector->center_z = nav_state->target_z;
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

    seeded = scene_editor_digest_overlay_resolve_seed_extents(&min_x,
                                                              &min_y,
                                                              &min_z,
                                                              &max_x,
                                                              &max_y,
                                                              &max_z);
    scene_editor_digest_overlay_accumulate_mesh_asset_extents(&seeded,
                                                              &min_x,
                                                              &min_y,
                                                              &min_z,
                                                              &max_x,
                                                              &max_y,
                                                              &max_z);

    for (i = 0; !seeded && i < digest->primitive_count; ++i) {
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

    if (!seeded && digest->has_scene_bounds) {
        min_x = digest->bounds_min_x;
        min_y = digest->bounds_min_y;
        min_z = digest->bounds_min_z;
        max_x = digest->bounds_max_x;
        max_y = digest->bounds_max_y;
        max_z = digest->bounds_max_z;
        seeded = true;
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
    (void)span_max;
    return scene_editor_digest_overlay_projector_from_extents(viewport,
                                                              yaw_deg,
                                                              pitch_deg,
                                                              zoom,
                                                              min_x,
                                                              min_y,
                                                              min_z,
                                                              max_x,
                                                              max_y,
                                                              max_z,
                                                              out_projector);
}

bool SceneEditorDigestOverlayBuildProjector(const RuntimeSceneBridge3DDigestState* digest,
                                            const SDL_Rect* viewport,
                                            const SceneEditorDigestOverlayNavState* nav_state,
                                            SceneEditorDigestOverlayProjector* out_projector) {
    if (!nav_state) return false;
    if (!SceneEditorDigestOverlayBuildProjectorWithView(digest,
                                                        viewport,
                                                        nav_state->orbit_yaw_deg,
                                                        nav_state->orbit_pitch_deg,
                                                        nav_state->overlay_zoom,
                                                        out_projector)) {
        return false;
    }
    scene_editor_digest_overlay_projector_apply_navigation_target(nav_state, out_projector);
    return true;
}

bool SceneEditorDigestOverlayBuildObjectProjector(const RuntimeSceneBridge3DDigestState* digest,
                                                  const SDL_Rect* viewport,
                                                  const SceneEditorDigestOverlayNavState* nav_state,
                                                  int scene_object_index,
                                                  bool focused_origin,
                                                  SceneEditorDigestOverlayProjector* out_projector) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    if (!nav_state) return false;
    if (!focused_origin) {
        return SceneEditorDigestOverlayBuildProjector(digest, viewport, nav_state, out_projector);
    }
    if (!SceneEditorDigestOverlayResolveObjectExtents(digest,
                                                      scene_object_index,
                                                      &min_x,
                                                      &min_y,
                                                      &min_z,
                                                      &max_x,
                                                      &max_y,
                                                      &max_z,
                                                      NULL)) {
        return SceneEditorDigestOverlayBuildProjector(digest, viewport, nav_state, out_projector);
    }
    if (!scene_editor_digest_overlay_projector_from_extents(viewport,
                                                            nav_state->orbit_yaw_deg,
                                                            nav_state->orbit_pitch_deg,
                                                            nav_state->overlay_zoom,
                                                            min_x,
                                                            min_y,
                                                            min_z,
                                                            max_x,
                                                            max_y,
                                                            max_z,
                                                            out_projector)) {
        return false;
    }
    scene_editor_digest_overlay_projector_apply_navigation_target(nav_state, out_projector);
    return true;
}
