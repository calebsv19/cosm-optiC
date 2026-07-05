#include "render/runtime_visibility_3d.h"

#include <math.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_volume_3d_integrate.h"

static const double kRuntimeVisibility3DEpsilon = 1e-4;
static const double kRuntimeVisibility3DMinimumTransmittance = 1e-4;
static const int kRuntimeVisibility3DMaxTransparentSurfaceSkips = 24;

static double runtime_visibility_3d_clamp(double value,
                                          double min_value,
                                          double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_visibility_3d_luminance(double r, double g, double b) {
    return runtime_visibility_3d_clamp(0.2126 * r + 0.7152 * g + 0.0722 * b, 0.0, 1.0);
}

static RuntimeVisibility3DTransmittance runtime_visibility_3d_zero_transmittance(void) {
    RuntimeVisibility3DTransmittance zero = {0};
    return zero;
}

static void runtime_visibility_3d_multiply_transmittance(
    RuntimeVisibility3DTransmittance* io_transmittance,
    const RuntimeVisibility3DTransmittance* segment_transmittance) {
    if (!io_transmittance || !segment_transmittance) return;

    io_transmittance->r *= segment_transmittance->r;
    io_transmittance->g *= segment_transmittance->g;
    io_transmittance->b *= segment_transmittance->b;
    io_transmittance->luma = runtime_visibility_3d_luminance(io_transmittance->r,
                                                             io_transmittance->g,
                                                             io_transmittance->b);
}

RuntimeVisibility3DTransmittance RuntimeVisibility3D_UnitTransmittance(void) {
    RuntimeVisibility3DTransmittance result = {1.0, 1.0, 1.0, 1.0};
    return result;
}

void RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(
    const RuntimeMaterialPayload3D* payload,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double segment_distance,
    RuntimeVisibility3DTransmittance* io_transmittance) {
    double transparency = 0.0;
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double absorption_distance =
        1.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double effective_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double zero_length = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;
    double distance_ratio = 1.0;
    double segment_r = 1.0;
    double segment_g = 1.0;
    double segment_b = 1.0;

    if (!payload || !payload->valid || !io_transmittance) return;

    transparency = runtime_visibility_3d_clamp(payload->transparency, 0.0, 1.0);
    if (!(transparency > 0.0)) {
        io_transmittance->luma = 0.0;
        io_transmittance->r = 0.0;
        io_transmittance->g = 0.0;
        io_transmittance->b = 0.0;
        return;
    }

    tint_r = runtime_visibility_3d_clamp(payload->baseColorR, 0.0, 1.0);
    tint_g = runtime_visibility_3d_clamp(payload->baseColorG, 0.0, 1.0);
    tint_b = runtime_visibility_3d_clamp(payload->baseColorB, 0.0, 1.0);
    absorption_distance =
        fmax(payload->absorptionDistance, length_epsilon);
    effective_distance = payload->thinWalled
                             ? absorption_distance
                             : fmax(segment_distance, zero_length);
    distance_ratio = fmax(effective_distance / absorption_distance, 0.0);

    /* Beer-Lambert form with authored baseColor interpreted as the
     * transmittance color at one absorption-distance unit. */
    segment_r = transparency * pow(fmax(tint_r, kRuntimeVisibility3DMinimumTransmittance), distance_ratio);
    segment_g = transparency * pow(fmax(tint_g, kRuntimeVisibility3DMinimumTransmittance), distance_ratio);
    segment_b = transparency * pow(fmax(tint_b, kRuntimeVisibility3DMinimumTransmittance), distance_ratio);

    io_transmittance->r *= runtime_visibility_3d_clamp(segment_r,
                                                       kRuntimeVisibility3DMinimumTransmittance,
                                                       1.0);
    io_transmittance->g *= runtime_visibility_3d_clamp(segment_g,
                                                       kRuntimeVisibility3DMinimumTransmittance,
                                                       1.0);
    io_transmittance->b *= runtime_visibility_3d_clamp(segment_b,
                                                       kRuntimeVisibility3DMinimumTransmittance,
                                                       1.0);
    io_transmittance->luma = runtime_visibility_3d_luminance(io_transmittance->r,
                                                             io_transmittance->g,
                                                             io_transmittance->b);
}

static bool runtime_visibility_3d_hit_matches_source(const HitInfo3D* a,
                                                     const HitInfo3D* b) {
    if (!a || !b) return false;
    return a->sceneObjectIndex == b->sceneObjectIndex &&
           a->triangleIndex == b->triangleIndex;
}

static bool runtime_visibility_3d_hit_matches_target(const HitInfo3D* hit,
                                                     int scene_object_index,
                                                     int triangle_index) {
    if (!hit || scene_object_index < 0) return false;
    if (hit->sceneObjectIndex != scene_object_index) return false;
    if (triangle_index < 0) return true;
    return hit->triangleIndex == triangle_index;
}

static bool runtime_visibility_3d_can_use_opaque_fast_path(const RuntimeScene3D* scene) {
    if (!scene) return false;
    if (scene->capabilities.valid) {
        return scene->capabilities.canUseOpaqueNoVolumeVisibilityFastPath &&
               !RuntimeVolume3D_HasActiveExtinction(&scene->volume);
    }
    if (!scene->materialFlags.valid) return false;
    if (scene->materialFlags.hasTransparentSurfaces ||
        scene->materialFlags.hasUnresolvedSurfaces) {
        return false;
    }
    return !RuntimeVolume3D_HasActiveExtinction(&scene->volume);
}

bool RuntimeVisibility3D_CanUseOpaqueNoVolumeFastPath(const RuntimeScene3D* scene) {
    return runtime_visibility_3d_can_use_opaque_fast_path(scene);
}

static RuntimeVisibility3DTransmittance runtime_visibility_3d_trace_opaque_fast_path(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    Vec3 ray_origin,
    Vec3 ray_normal,
    Vec3 ray_dir,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double ray_length,
    int target_scene_object_index,
    int target_triangle_index) {
    Ray3D current_ray = RuntimeRay3D_MakeOffset(ray_origin,
                                                ray_normal,
                                                ray_dir,
                                                kRuntimeVisibility3DEpsilon);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double remaining_distance =
        ray_length;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;
    int skip_count = 0;

    while (skip_count < kRuntimeVisibility3DMaxTransparentSurfaceSkips &&
           remaining_distance > length_epsilon) {
        HitInfo3D blocker_hit = {0};
        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
            RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY,
            skip_count);
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &current_ray,
                                             length_epsilon,
                                             remaining_distance,
                                             &blocker_hit)) {
            return RuntimeVisibility3D_UnitTransmittance();
        }
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&blocker_hit);
        if (source_hit && runtime_visibility_3d_hit_matches_source(&blocker_hit, source_hit)) {
            remaining_distance -= blocker_hit.t;
            current_ray = RuntimeRay3D_MakeOffset(blocker_hit.position,
                                                  blocker_hit.normal,
                                                  ray_dir,
                                                  length_epsilon);
            skip_count += 1;
            continue;
        }
        if (runtime_visibility_3d_hit_matches_target(&blocker_hit,
                                                     target_scene_object_index,
                                                     target_triangle_index)) {
            return RuntimeVisibility3D_UnitTransmittance();
        }
        return runtime_visibility_3d_zero_transmittance();
    }

    return RuntimeVisibility3D_UnitTransmittance();
}

static RuntimeVisibility3DTransmittance runtime_visibility_3d_trace_transmittance_rgb(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    Vec3 ray_origin,
    Vec3 ray_normal,
    Vec3 ray_dir,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double ray_length,
    int target_scene_object_index,
    int target_triangle_index) {
    Ray3D current_ray = {0};
    Ray3D segment_ray = {0};
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double remaining_distance = ray_length;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double unit_length = 1.0;
    RuntimeVisibility3DTransmittance transmittance = RuntimeVisibility3D_UnitTransmittance();
    RuntimeVisibility3DTransmittance volume_transmittance = RuntimeVisibility3D_UnitTransmittance();
    int skip_count = 0;

    if (!scene) {
        return runtime_visibility_3d_zero_transmittance();
    }
    if (!(ray_length > length_epsilon)) return transmittance;
    if (runtime_visibility_3d_can_use_opaque_fast_path(scene)) {
        return runtime_visibility_3d_trace_opaque_fast_path(scene,
                                                            source_hit,
                                                            ray_origin,
                                                            ray_normal,
                                                            ray_dir,
                                                            ray_length,
                                                            target_scene_object_index,
                                                            target_triangle_index);
    }

    segment_ray = RuntimeRay3D_Make(ray_origin, ray_dir);
    volume_transmittance = RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                                    &segment_ray,
                                                                    length_epsilon,
                                                                    ray_length);
    runtime_visibility_3d_multiply_transmittance(&transmittance, &volume_transmittance);
    if (!(transmittance.luma > kRuntimeVisibility3DMinimumTransmittance)) {
        return runtime_visibility_3d_zero_transmittance();
    }

    current_ray = RuntimeRay3D_MakeOffset(ray_origin,
                                          ray_normal,
                                          ray_dir,
                                          kRuntimeVisibility3DEpsilon);
    while (skip_count < kRuntimeVisibility3DMaxTransparentSurfaceSkips &&
           remaining_distance > length_epsilon) {
        HitInfo3D blocker_hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        int transparent_object_index = -1;
        HitInfo3D current_surface = {0};
        [[fisics::dim(length)]] [[fisics::unit(meter)]] double segment_distance =
            0.0;

        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
            RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY,
            skip_count);
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &current_ray,
                                             length_epsilon,
                                             remaining_distance,
                                             &blocker_hit)) {
            return transmittance;
        }
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&blocker_hit);
        if (source_hit && runtime_visibility_3d_hit_matches_source(&blocker_hit, source_hit)) {
            remaining_distance -= blocker_hit.t;
            current_ray = RuntimeRay3D_MakeOffset(blocker_hit.position,
                                                  blocker_hit.normal,
                                                  ray_dir,
                                                  length_epsilon);
            skip_count += 1;
            continue;
        }
        if (runtime_visibility_3d_hit_matches_target(&blocker_hit,
                                                     target_scene_object_index,
                                                     target_triangle_index)) {
            return transmittance;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&blocker_hit, &payload)) {
            return runtime_visibility_3d_zero_transmittance();
        }
        if (!(payload.transparency > 0.0)) {
            return runtime_visibility_3d_zero_transmittance();
        }

        transparent_object_index = blocker_hit.sceneObjectIndex;
        current_surface = blocker_hit;
        for (;;) {
            remaining_distance -= current_surface.t;
            current_ray = RuntimeRay3D_MakeOffset(current_surface.position,
                                                  current_surface.normal,
                                                  ray_dir,
                                                  length_epsilon);
            skip_count += 1;
            if (skip_count >= kRuntimeVisibility3DMaxTransparentSurfaceSkips ||
                !(remaining_distance > length_epsilon)) {
                RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload,
                                                                      fmax(segment_distance,
                                                                           unit_length),
                                                                      &transmittance);
                return transmittance;
            }
            RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
                RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY,
                skip_count);
            if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                                 &current_ray,
                                                 length_epsilon,
                                                 remaining_distance,
                                                 &blocker_hit)) {
                RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload,
                                                                      fmax(segment_distance,
                                                                           unit_length),
                                                                      &transmittance);
                return transmittance;
            }
            RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&blocker_hit);
            if (runtime_visibility_3d_hit_matches_target(&blocker_hit,
                                                         target_scene_object_index,
                                                         target_triangle_index)) {
                RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload,
                                                                      fmax(segment_distance,
                                                                           unit_length),
                                                                      &transmittance);
                return transmittance;
            }
            if (blocker_hit.sceneObjectIndex != transparent_object_index) {
                break;
            }
            segment_distance += blocker_hit.t;
            current_surface = blocker_hit;
        }

        RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload,
                                                              fmax(segment_distance,
                                                                   unit_length),
                                                              &transmittance);
        if (!(transmittance.luma > kRuntimeVisibility3DMinimumTransmittance)) {
            return runtime_visibility_3d_zero_transmittance();
        }

        /* The current ray has already been advanced beyond the transparent
         * shell, so continue tracing from there to find the next blocker. */
    }

    return transmittance;
}

bool RuntimeVisibility3D_TraceToLight(const RuntimeScene3D* scene,
                                      Vec3 surface_position,
                                      Vec3 surface_normal,
                                      Vec3 light_position,
                                      HitInfo3D* out_blocker_hit,
                                      [[fisics::dim(length)]] [[fisics::unit(meter)]] double* out_light_distance) {
    Vec3 to_light;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;
    Ray3D shadow_ray = {0};
    HitInfo3D blocker_hit = {0};
    bool blocked = false;

    if (!scene) return false;

    to_light = vec3_sub(light_position, surface_position);
    light_distance = vec3_length(to_light);
    if (out_light_distance) {
        *out_light_distance = light_distance;
    }
    if (light_distance <= length_epsilon) {
        if (out_blocker_hit) {
            HitInfo3D_Reset(out_blocker_hit);
        }
        return false;
    }

    shadow_ray = RuntimeRay3D_MakeOffset(surface_position,
                                         surface_normal,
                                         to_light,
                                         length_epsilon);
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
        RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY,
        0);
    blocked = RuntimeRay3D_TraceSceneFirstHit(scene,
                                              &shadow_ray,
                                              length_epsilon,
                                              fmax(light_distance - length_epsilon,
                                                   length_epsilon),
                                              &blocker_hit);
    if (blocked) {
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&blocker_hit);
    }
    if (out_blocker_hit) {
        if (blocked) {
            *out_blocker_hit = blocker_hit;
        } else {
            HitInfo3D_Reset(out_blocker_hit);
        }
    }
    return blocked;
}

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceToLightRGB(
    const RuntimeScene3D* scene,
    Vec3 surface_position,
    Vec3 surface_normal,
    Vec3 light_position) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene) {
        RuntimeVisibility3DTransmittance zero = {0};
        return zero;
    }

    to_light = vec3_sub(light_position, surface_position);
    light_distance = vec3_length(to_light);
    if (light_distance <= length_epsilon) {
        return RuntimeVisibility3D_UnitTransmittance();
    }

    return runtime_visibility_3d_trace_transmittance_rgb(scene,
                                                         NULL,
                                                         surface_position,
                                                         surface_normal,
                                                         vec3_scale(to_light, 1.0 / light_distance),
                                                         light_distance,
                                                         -1,
                                                         -1);
}

double RuntimeVisibility3D_TransmittanceToLight(const RuntimeScene3D* scene,
                                                Vec3 surface_position,
                                                Vec3 surface_normal,
                                                Vec3 light_position) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene) return 0.0;

    to_light = vec3_sub(light_position, surface_position);
    light_distance = vec3_length(to_light);
    if (light_distance <= length_epsilon) {
        return 1.0;
    }

    return RuntimeVisibility3D_TransmittanceToLightRGB(scene,
                                                       surface_position,
                                                       surface_normal,
                                                       light_position)
        .luma;
}

bool RuntimeVisibility3D_HasLineOfSightToLight(const RuntimeScene3D* scene,
                                               Vec3 surface_position,
                                               Vec3 surface_normal,
                                               Vec3 light_position) {
    return RuntimeVisibility3D_TransmittanceToLight(scene,
                                                    surface_position,
                                                    surface_normal,
                                                    light_position) > kRuntimeVisibility3DMinimumTransmittance;
}

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceFromHitRGB(
    const RuntimeScene3D* scene,
    const HitInfo3D* surface_hit,
    const RuntimeLight3D* light) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene || !surface_hit || !light) {
        RuntimeVisibility3DTransmittance zero = {0};
        return zero;
    }

    to_light = vec3_sub(light->position, surface_hit->position);
    light_distance = vec3_length(to_light);
    if (light_distance <= length_epsilon) {
        return RuntimeVisibility3D_UnitTransmittance();
    }

    return runtime_visibility_3d_trace_transmittance_rgb(scene,
                                                         surface_hit,
                                                         surface_hit->position,
                                                         surface_hit->normal,
                                                         vec3_scale(to_light, 1.0 / light_distance),
                                                         light_distance,
                                                         -1,
                                                         -1);
}

double RuntimeVisibility3D_TransmittanceFromHit(const RuntimeScene3D* scene,
                                                const HitInfo3D* surface_hit,
                                                const RuntimeLight3D* light) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene || !surface_hit || !light) return 0.0;

    to_light = vec3_sub(light->position, surface_hit->position);
    light_distance = vec3_length(to_light);
    if (light_distance <= length_epsilon) {
        return 1.0;
    }

    return RuntimeVisibility3D_TransmittanceFromHitRGB(scene, surface_hit, light).luma;
}

RuntimeVisibility3DTransmittance RuntimeVisibility3D_TransmittanceFromHitToPointRGB(
    const RuntimeScene3D* scene,
    const HitInfo3D* surface_hit,
    Vec3 target_position,
    int target_scene_object_index,
    int target_triangle_index) {
    Vec3 to_target = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double target_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene || !surface_hit) {
        RuntimeVisibility3DTransmittance zero = {0};
        return zero;
    }

    to_target = vec3_sub(target_position, surface_hit->position);
    target_distance = vec3_length(to_target);
    if (target_distance <= length_epsilon) {
        return RuntimeVisibility3D_UnitTransmittance();
    }

    return runtime_visibility_3d_trace_transmittance_rgb(scene,
                                                         surface_hit,
                                                         surface_hit->position,
                                                         surface_hit->normal,
                                                         vec3_scale(to_target, 1.0 / target_distance),
                                                         target_distance,
                                                         target_scene_object_index,
                                                         target_triangle_index);
}

double RuntimeVisibility3D_TransmittanceFromHitToPoint(const RuntimeScene3D* scene,
                                                       const HitInfo3D* surface_hit,
                                                       Vec3 target_position,
                                                       int target_scene_object_index,
                                                       int target_triangle_index) {
    Vec3 to_target = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double target_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon =
        kRuntimeVisibility3DEpsilon;

    if (!scene || !surface_hit) return 0.0;

    to_target = vec3_sub(target_position, surface_hit->position);
    target_distance = vec3_length(to_target);
    if (target_distance <= length_epsilon) {
        return 1.0;
    }

    return RuntimeVisibility3D_TransmittanceFromHitToPointRGB(scene,
                                                              surface_hit,
                                                              target_position,
                                                              target_scene_object_index,
                                                              target_triangle_index)
        .luma;
}

bool RuntimeVisibility3D_HasLineOfSightFromHit(const RuntimeScene3D* scene,
                                               const HitInfo3D* surface_hit,
                                               const RuntimeLight3D* light) {
    return RuntimeVisibility3D_TransmittanceFromHit(scene,
                                                    surface_hit,
                                                    light) > kRuntimeVisibility3DMinimumTransmittance;
}
