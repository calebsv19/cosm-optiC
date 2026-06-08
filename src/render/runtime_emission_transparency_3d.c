#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_emission_transparency_3d_internal.h"

#include <math.h>

#include "render/runtime_dielectric_transport_3d.h"
#include "render/material_bsdf.h"
#include "render/runtime_emissive_direct_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_visibility_3d.h"

static bool runtime_emission_transparency_3d_shade_hit_recursive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DPathState path_state,
    RuntimeEmissionTransparency3DResult* out_result);

static bool runtime_emission_transparency_3d_shade_pixel_recursive(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* out_result);

static void runtime_emission_transparency_3d_copy_material_result(
    const RuntimeMaterialResponse3DResult* material_result,
    const RuntimeMaterialPayload3D* payload,
    RuntimeEmissionTransparency3DResult* out_result);

static void runtime_emission_transparency_3d_apply_transparency(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    RuntimeEmissionTransparency3DPathState path_state,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result);

static bool runtime_emission_transparency_3d_can_skip_emission_support(
    const RuntimeScene3D* scene,
    const RuntimeMaterialPayload3D* payload) {
    if (!scene || !payload || !payload->valid) return false;
    if (!scene->capabilities.valid) return false;
    if (!scene->capabilities.canSkipEmissionSupport ||
        scene->capabilities.hasEmissiveSurfaces ||
        !scene->capabilities.canSkipTransparencySupport) {
        return false;
    }
    return !(payload->emissive > 1e-6) && !(payload->transparency > 1e-6);
}

static bool runtime_emission_transparency_3d_can_skip_transparency_support(
    const RuntimeScene3D* scene,
    const RuntimeMaterialPayload3D* payload) {
    if (!scene || !payload || !payload->valid) return false;
    if (!scene->capabilities.valid) return false;
    if (!scene->capabilities.canSkipTransparencySupport ||
        scene->capabilities.hasTransparentSurfaces ||
        scene->capabilities.hasTransmissionSurfaces) {
        return false;
    }
    return !(payload->transparency > 1e-6);
}

static double runtime_emission_transparency_3d_first_hit_emissive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 view_dir,
    double* out_r,
    double* out_g,
    double* out_b) {
    double facing = 0.0;
    double energy_scale = 1.0;
    double radiance = 0.0;
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;

    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!scene || !hit || !payload || !payload->valid || !(payload->emissive > 0.0)) {
        return 0.0;
    }

    facing = fmax(0.0, vec3_dot(hit->normal, vec3_normalize(view_dir)));
    energy_scale = runtime_emission_transparency_3d_clamp(scene->light.intensity * 0.15, 0.5, 2.0);
    radiance = payload->emissive * facing * energy_scale;
    runtime_emission_transparency_3d_resolve_payload_tint(payload, &tint_r, &tint_g, &tint_b);
    if (out_r) *out_r = radiance * tint_r;
    if (out_g) *out_g = radiance * tint_g;
    if (out_b) *out_b = radiance * tint_b;
    return radiance;
}

static double runtime_emission_transparency_3d_secondary_emissive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result,
    double* out_r,
    double* out_g,
    double* out_b) {
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double accumulated = 0.0;
    double accumulated_r = 0.0;
    double accumulated_g = 0.0;
    double accumulated_b = 0.0;
    int sample_count = 0;

    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!scene || !hit || !io_result) return 0.0;

    runtime_emission_transparency_3d_build_basis(hit->normal, &tangent, &bitangent);
    sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();

    for (int i = 0; i < sample_count; ++i) {
        HitInfo3D secondary_hit = {0};
        RuntimeMaterialPayload3D secondary_payload = {0};
        Vec3 sample_dir = runtime_emission_transparency_3d_sample_direction(hit,
                                                                            hit->normal,
                                                                            tangent,
                                                                            bitangent,
                                                                            sampling,
                                                                            sample_count,
                                                                            i);
        Ray3D bounce_ray = RuntimeRay3D_MakeOffset(hit->position,
                                                   hit->normal,
                                                   sample_dir,
                                                   kRuntimeEmissionTransparency3DEpsilon);
        double secondary_facing = 0.0;
        double segment_distance = 0.0;
        double sample_energy = 0.0;
        double tint_r = 1.0;
        double tint_g = 1.0;
        double tint_b = 1.0;

        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &bounce_ray,
                                             kRuntimeEmissionTransparency3DEpsilon,
                                             kRuntimeEmissionTransparency3DMaxDistance,
                                             &secondary_hit)) {
            continue;
        }
        io_result->secondaryHitCount += 1;
        if (secondary_hit.triangleIndex == hit->triangleIndex) {
            continue;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&secondary_hit, &secondary_payload)) {
            continue;
        }
        if (!(secondary_payload.emissive > 0.0)) {
            continue;
        }
        io_result->secondaryContributingHitCount += 1;

        secondary_facing = fabs(vec3_dot(secondary_hit.normal, vec3_scale(sample_dir, -1.0)));
        segment_distance = vec3_length(vec3_sub(secondary_hit.position, hit->position));
        sample_energy = secondary_payload.emissive *
                        secondary_facing *
                        runtime_emission_transparency_3d_distance_decay(segment_distance) *
                        kRuntimeEmissionTransparency3DEnergyScale;
        accumulated += sample_energy;
        runtime_emission_transparency_3d_resolve_payload_tint(&secondary_payload,
                                                              &tint_r,
                                                              &tint_g,
                                                              &tint_b);
        accumulated_r += sample_energy * tint_r;
        accumulated_g += sample_energy * tint_g;
        accumulated_b += sample_energy * tint_b;
    }

    if (out_r) *out_r = accumulated_r / (double)sample_count;
    if (out_g) *out_g = accumulated_g / (double)sample_count;
    if (out_b) *out_b = accumulated_b / (double)sample_count;
    return accumulated / (double)sample_count;
}

static bool runtime_emission_transparency_3d_trace_support(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    Vec3 support_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DPathState path_state,
    bool transmission_branch,
    RuntimeEmissionTransparency3DTransmissionResult* out_result,
    double* out_source_segment_distance) {
    RuntimePathDepthPolicy3D path_policy = RuntimePathDepthPolicy3D_Resolve();
    Ray3D support_ray;
    HitInfo3D transmitted_hit = {0};
    RuntimeLightEmitterHit3DResult emitter_hit = {0};
    RuntimeEmissionTransparency3DTransmissionResult result = {0};
    double remaining_distance = kRuntimeEmissionTransparency3DTransmissionMaxDistance;
    double source_segment_distance = 0.0;
    int skip_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (out_source_segment_distance) *out_source_segment_distance = 1.0;
    HitInfo3D_Reset(&transmitted_hit);

    support_ray = RuntimeRay3D_MakeOffset(hit->position,
                                          hit->normal,
                                          support_dir,
                                          kRuntimeEmissionTransparency3DEpsilon);
    while (skip_count < kRuntimeEmissionTransparency3DMaxTransmissionSurfaceSkips &&
           remaining_distance > kRuntimeEmissionTransparency3DEpsilon) {
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &support_ray,
                                             kRuntimeEmissionTransparency3DEpsilon,
                                             remaining_distance,
                                             &transmitted_hit)) {
            break;
        }
        if (transmitted_hit.sceneObjectIndex != hit->sceneObjectIndex &&
            transmitted_hit.triangleIndex != hit->triangleIndex) {
            break;
        }
        source_segment_distance += transmitted_hit.t;
        remaining_distance -= transmitted_hit.t;
        support_ray = RuntimeRay3D_MakeOffset(transmitted_hit.position,
                                              transmitted_hit.normal,
                                              support_dir,
                                              kRuntimeEmissionTransparency3DEpsilon);
        skip_count += 1;
        HitInfo3D_Reset(&transmitted_hit);
    }
    if (transmitted_hit.triangleIndex < 0) {
        remaining_distance = kRuntimeEmissionTransparency3DTransmissionMaxDistance;
    } else {
        remaining_distance = transmitted_hit.t;
    }

    if (RuntimeLightEmitter3D_IntersectRay(scene,
                                           &support_ray,
                                           kRuntimeEmissionTransparency3DEpsilon,
                                           remaining_distance,
                                           &emitter_hit) &&
        (transmitted_hit.triangleIndex < 0 || emitter_hit.t < transmitted_hit.t)) {
        result.hit = true;
        result.emitterWins = true;
        result.emitterHit = emitter_hit;
        if (out_source_segment_distance) {
            *out_source_segment_distance = fmax(source_segment_distance, 1.0);
        }
        *out_result = result;
        return true;
    }

    if (transmitted_hit.triangleIndex < 0) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&transmitted_hit, &result.materialPayload)) {
        return false;
    }
    if (result.materialPayload.transparency > 0.0 &&
        ((transmission_branch &&
          path_state.transmissionDepth < path_policy.transmissionDepth) ||
         (!transmission_branch &&
          path_state.specularDepth < path_policy.specularDepth))) {
        RuntimeEmissionTransparency3DPathState next_state = path_state;
        next_state.incidentDir = support_dir;
        if (transmission_branch) {
            next_state.transmissionDepth += 1;
        } else {
            next_state.specularDepth += 1;
        }
        if (!runtime_emission_transparency_3d_shade_hit_recursive(scene,
                                                                  &transmitted_hit,
                                                                  sampling,
                                                                  next_state,
                                                                  &result.transparencyResult)) {
            return false;
        }
        result.usedTransparencyTier = true;
    } else {
        if (!RuntimeMaterialResponse3D_ShadeHit(scene,
                                               &transmitted_hit,
                                               sampling,
                                               &result.materialResult)) {
            return false;
        }
    }

    result.hit = true;
    if (out_source_segment_distance) {
        *out_source_segment_distance = fmax(source_segment_distance, 1.0);
    }
    *out_result = result;
    return true;
}

static bool runtime_emission_transparency_3d_shade_primary_hit_recursive(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* resolved_payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DPathState path_state = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);
    double emissive_direct = 0.0;
    double emissive_bounce = 0.0;
    double emissive_direct_r = 0.0;
    double emissive_direct_g = 0.0;
    double emissive_direct_b = 0.0;
    double emissive_bounce_r = 0.0;
    double emissive_bounce_g = 0.0;
    double emissive_bounce_b = 0.0;
    RuntimeEmissiveDirect3DResult emissive_direct_result = {0};
    int secondary_sample_count = 0;

    if (!scene || !primary_hit || !out_result) return false;
    if (!RuntimeMaterialResponse3D_ShadePrimaryHitWithPayload(scene,
                                                              primary_hit,
                                                              resolved_payload,
                                                              sampling,
                                                              &material_result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }
    if (resolved_payload && resolved_payload->valid) {
        result.payload = *resolved_payload;
    } else if (!RuntimeMaterialPayload3D_ResolveFromHit(&material_result.hitInfo, &result.payload)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }

    runtime_emission_transparency_3d_copy_material_result(&material_result,
                                                          &result.payload,
                                                          &result);
    path_state.incidentDir = material_result.primaryRay.direction;
    path_state.specularDepth = 0;
    path_state.transmissionDepth = 0;
    if (!runtime_emission_transparency_3d_can_skip_emission_support(scene, &result.payload)) {
        secondary_sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();
        result.secondaryRayCount = secondary_sample_count;
        view_dir = vec3_scale(material_result.primaryRay.direction, -1.0);
        emissive_direct = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                              &result.hitInfo,
                                                                              &result.payload,
                                                                              view_dir,
                                                                              &emissive_direct_r,
                                                                              &emissive_direct_g,
                                                                              &emissive_direct_b);
        emissive_bounce = runtime_emission_transparency_3d_secondary_emissive(scene,
                                                                               &result.hitInfo,
                                                                               sampling,
                                                                               &result,
                                                                               &emissive_bounce_r,
                                                                               &emissive_bounce_g,
                                                                               &emissive_bounce_b);
        if (RuntimeEmissiveDirect3D_ShadeHit(scene,
                                             &result.hitInfo,
                                             sampling,
                                             &emissive_direct_result)) {
            emissive_direct += emissive_direct_result.directRadiance;
            emissive_direct_r += emissive_direct_result.directRadianceR;
            emissive_direct_g += emissive_direct_result.directRadianceG;
            emissive_direct_b += emissive_direct_result.directRadianceB;
        }
    }
    result.emissiveDirectRadiance = emissive_direct;
    result.emissiveDirectRadianceR = emissive_direct_r;
    result.emissiveDirectRadianceG = emissive_direct_g;
    result.emissiveDirectRadianceB = emissive_direct_b;
    result.emissiveBounceRadiance = emissive_bounce;
    result.emissiveBounceRadianceR = emissive_bounce_r;
    result.emissiveBounceRadianceG = emissive_bounce_g;
    result.emissiveBounceRadianceB = emissive_bounce_b;
    result.directRadiance += emissive_direct;
    result.directRadianceR += emissive_direct_r;
    result.directRadianceG += emissive_direct_g;
    result.directRadianceB += emissive_direct_b;
    result.bounceRadiance += emissive_bounce;
    result.bounceRadianceR += emissive_bounce_r;
    result.bounceRadianceG += emissive_bounce_g;
    result.bounceRadianceB += emissive_bounce_b;
    result.radiance = result.directRadiance + result.bounceRadiance;
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    if (!runtime_emission_transparency_3d_can_skip_transparency_support(scene,
                                                                        &result.payload)) {
        runtime_emission_transparency_3d_apply_transparency(scene,
                                                            &result.hitInfo,
                                                            &result.payload,
                                                            path_state,
                                                            sampling,
                                                            &result);
    }
    *out_result = result;
    return true;
}

static bool runtime_emission_transparency_3d_sample_transmission(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 transmission_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DPathState path_state,
    RuntimeEmissionTransparency3DResult* io_result,
    double* out_direct,
    double* out_direct_r,
    double* out_direct_g,
    double* out_direct_b,
    double* out_bounce,
    double* out_bounce_r,
    double* out_bounce_g,
    double* out_bounce_b) {
    RuntimeEmissionTransparency3DTransmissionResult transmitted = {0};
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double transparency = 0.0;
    double cone_radius = 0.0;
    double total_direct = 0.0;
    double total_direct_r = 0.0;
    double total_direct_g = 0.0;
    double total_direct_b = 0.0;
    double total_bounce = 0.0;
    double total_bounce_r = 0.0;
    double total_bounce_g = 0.0;
    double total_bounce_b = 0.0;
    int contributing_samples = 0;
    int sample_count = 0;
    double first_hit_emissive = 0.0;
    double first_hit_emissive_r = 0.0;
    double first_hit_emissive_g = 0.0;
    double first_hit_emissive_b = 0.0;
    double source_segment_distance = 1.0;

    if (out_direct) *out_direct = 0.0;
    if (out_direct_r) *out_direct_r = 0.0;
    if (out_direct_g) *out_direct_g = 0.0;
    if (out_direct_b) *out_direct_b = 0.0;
    if (out_bounce) *out_bounce = 0.0;
    if (out_bounce_r) *out_bounce_r = 0.0;
    if (out_bounce_g) *out_bounce_g = 0.0;
    if (out_bounce_b) *out_bounce_b = 0.0;
    if (!scene || !hit || !payload || !io_result || !out_direct || !out_direct_r ||
        !out_direct_g || !out_direct_b || !out_bounce || !out_bounce_r || !out_bounce_g ||
        !out_bounce_b) {
        return false;
    }

    transparency = runtime_emission_transparency_3d_clamp(payload->transparency, 0.0, 1.0);
    runtime_emission_transparency_3d_build_basis(transmission_dir, &tangent, &bitangent);
    cone_radius = runtime_emission_transparency_3d_transmission_cone_radius(payload, transparency);
    sample_count = runtime_emission_transparency_3d_resolve_transmission_sample_count();
    io_result->secondaryRayCount += sample_count;

    for (int i = 0; i < sample_count; ++i) {
        Vec3 sample_dir = runtime_emission_transparency_3d_sample_transmission_direction(hit,
                                                                                         transmission_dir,
                                                                                         tangent,
                                                                                         bitangent,
                                                                                         cone_radius,
                                                                                         sampling,
                                                                                         sample_count,
                                                                                         i);
        RuntimeVisibility3DTransmittance source_filter = RuntimeVisibility3D_UnitTransmittance();
        if (!runtime_emission_transparency_3d_trace_support(scene,
                                                            hit,
                                                            sample_dir,
                                                            sampling,
                                                            path_state,
                                                            true,
                                                            &transmitted,
                                                            &source_segment_distance)) {
            continue;
        }
        RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(payload,
                                                              source_segment_distance,
                                                              &source_filter);
        contributing_samples += 1;
        if (transmitted.emitterWins) {
            total_direct += transmitted.emitterHit.radiance * source_filter.luma;
            total_direct_r += transmitted.emitterHit.radiance * source_filter.r;
            total_direct_g += transmitted.emitterHit.radiance * source_filter.g;
            total_direct_b += transmitted.emitterHit.radiance * source_filter.b;
            continue;
        }

        if (transmitted.usedTransparencyTier) {
            total_direct += transmitted.transparencyResult.directRadiance * source_filter.luma;
            total_direct_r += transmitted.transparencyResult.directRadianceR * source_filter.r;
            total_direct_g += transmitted.transparencyResult.directRadianceG * source_filter.g;
            total_direct_b += transmitted.transparencyResult.directRadianceB * source_filter.b;
            total_bounce += transmitted.transparencyResult.bounceRadiance * source_filter.luma;
            total_bounce_r += transmitted.transparencyResult.bounceRadianceR * source_filter.r;
            total_bounce_g += transmitted.transparencyResult.bounceRadianceG * source_filter.g;
            total_bounce_b += transmitted.transparencyResult.bounceRadianceB * source_filter.b;
            io_result->secondaryRayCount += transmitted.transparencyResult.secondaryRayCount;
            io_result->secondaryHitCount += transmitted.transparencyResult.secondaryHitCount;
            io_result->secondaryContributingHitCount +=
                transmitted.transparencyResult.secondaryContributingHitCount;
        } else {
            first_hit_emissive = runtime_emission_transparency_3d_first_hit_emissive(
                scene,
                &transmitted.materialResult.hitInfo,
                &transmitted.materialPayload,
                vec3_scale(sample_dir, -1.0),
                &first_hit_emissive_r,
                &first_hit_emissive_g,
                &first_hit_emissive_b);
            total_direct += (transmitted.materialResult.directRadiance + first_hit_emissive) *
                            source_filter.luma;
            total_direct_r +=
                (transmitted.materialResult.directRadianceR + first_hit_emissive_r) *
                source_filter.r;
            total_direct_g +=
                (transmitted.materialResult.directRadianceG + first_hit_emissive_g) *
                source_filter.g;
            total_direct_b +=
                (transmitted.materialResult.directRadianceB + first_hit_emissive_b) *
                source_filter.b;
            total_bounce += transmitted.materialResult.bounceRadiance * source_filter.luma;
            total_bounce_r += transmitted.materialResult.bounceRadianceR * source_filter.r;
            total_bounce_g += transmitted.materialResult.bounceRadianceG * source_filter.g;
            total_bounce_b += transmitted.materialResult.bounceRadianceB * source_filter.b;
            io_result->secondaryRayCount += transmitted.materialResult.secondaryRayCount;
            io_result->secondaryHitCount += transmitted.materialResult.secondaryHitCount;
            io_result->secondaryContributingHitCount +=
                transmitted.materialResult.secondaryContributingHitCount;
        }
    }

    if (contributing_samples <= 0) {
        return false;
    }

    *out_direct = total_direct / (double)contributing_samples;
    *out_direct_r = total_direct_r / (double)contributing_samples;
    *out_direct_g = total_direct_g / (double)contributing_samples;
    *out_direct_b = total_direct_b / (double)contributing_samples;
    *out_bounce = total_bounce / (double)contributing_samples;
    *out_bounce_r = total_bounce_r / (double)contributing_samples;
    *out_bounce_g = total_bounce_g / (double)contributing_samples;
    *out_bounce_b = total_bounce_b / (double)contributing_samples;
    return true;
}

static bool runtime_emission_transparency_3d_sample_reflection(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    Vec3 reflection_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DPathState path_state,
    RuntimeEmissionTransparency3DResult* io_result,
    double* out_direct,
    double* out_direct_r,
    double* out_direct_g,
    double* out_direct_b,
    double* out_bounce,
    double* out_bounce_r,
    double* out_bounce_g,
    double* out_bounce_b) {
    RuntimeEmissionTransparency3DTransmissionResult reflected = {0};
    double first_hit_emissive = 0.0;
    double first_hit_emissive_r = 0.0;
    double first_hit_emissive_g = 0.0;
    double first_hit_emissive_b = 0.0;
    double source_segment_distance = 1.0;

    if (out_direct) *out_direct = 0.0;
    if (out_direct_r) *out_direct_r = 0.0;
    if (out_direct_g) *out_direct_g = 0.0;
    if (out_direct_b) *out_direct_b = 0.0;
    if (out_bounce) *out_bounce = 0.0;
    if (out_bounce_r) *out_bounce_r = 0.0;
    if (out_bounce_g) *out_bounce_g = 0.0;
    if (out_bounce_b) *out_bounce_b = 0.0;
    if (!scene || !hit || !io_result || !out_direct || !out_direct_r || !out_direct_g ||
        !out_direct_b || !out_bounce || !out_bounce_r || !out_bounce_g || !out_bounce_b) {
        return false;
    }

    io_result->secondaryRayCount += 1;
    if (!runtime_emission_transparency_3d_trace_support(scene,
                                                        hit,
                                                        reflection_dir,
                                                        sampling,
                                                        path_state,
                                                        false,
                                                        &reflected,
                                                        &source_segment_distance)) {
        return false;
    }
    (void)source_segment_distance;
    if (reflected.emitterWins) {
        *out_direct = reflected.emitterHit.radiance;
        *out_direct_r = reflected.emitterHit.radiance;
        *out_direct_g = reflected.emitterHit.radiance;
        *out_direct_b = reflected.emitterHit.radiance;
        return true;
    }

    if (reflected.usedTransparencyTier) {
        *out_direct = reflected.transparencyResult.directRadiance;
        *out_direct_r = reflected.transparencyResult.directRadianceR;
        *out_direct_g = reflected.transparencyResult.directRadianceG;
        *out_direct_b = reflected.transparencyResult.directRadianceB;
        *out_bounce = reflected.transparencyResult.bounceRadiance;
        *out_bounce_r = reflected.transparencyResult.bounceRadianceR;
        *out_bounce_g = reflected.transparencyResult.bounceRadianceG;
        *out_bounce_b = reflected.transparencyResult.bounceRadianceB;
        io_result->secondaryRayCount += reflected.transparencyResult.secondaryRayCount;
        io_result->secondaryHitCount += reflected.transparencyResult.secondaryHitCount;
        io_result->secondaryContributingHitCount +=
            reflected.transparencyResult.secondaryContributingHitCount;
        return true;
    }

    first_hit_emissive = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                              &reflected.materialResult.hitInfo,
                                                                              &reflected.materialPayload,
                                                                              vec3_scale(reflection_dir, -1.0),
                                                                              &first_hit_emissive_r,
                                                                              &first_hit_emissive_g,
                                                                              &first_hit_emissive_b);
    *out_direct = reflected.materialResult.directRadiance + first_hit_emissive;
    *out_direct_r = reflected.materialResult.directRadianceR + first_hit_emissive_r;
    *out_direct_g = reflected.materialResult.directRadianceG + first_hit_emissive_g;
    *out_direct_b = reflected.materialResult.directRadianceB + first_hit_emissive_b;
    *out_bounce = reflected.materialResult.bounceRadiance;
    *out_bounce_r = reflected.materialResult.bounceRadianceR;
    *out_bounce_g = reflected.materialResult.bounceRadianceG;
    *out_bounce_b = reflected.materialResult.bounceRadianceB;
    io_result->secondaryRayCount += reflected.materialResult.secondaryRayCount;
    io_result->secondaryHitCount += reflected.materialResult.secondaryHitCount;
    io_result->secondaryContributingHitCount +=
        reflected.materialResult.secondaryContributingHitCount;
    return true;
}

static void runtime_emission_transparency_3d_apply_transparency(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    RuntimeEmissionTransparency3DPathState path_state,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result) {
    double transparency = 0.0;
    double base_front_weight = 1.0;
    double reflection_weight = 0.0;
    double transmission_weight = 0.0;
    RuntimeDielectricTransport3D dielectric = {0};
    double reflected_direct = 0.0;
    double reflected_direct_r = 0.0;
    double reflected_direct_g = 0.0;
    double reflected_direct_b = 0.0;
    double reflected_bounce = 0.0;
    double reflected_bounce_r = 0.0;
    double reflected_bounce_g = 0.0;
    double reflected_bounce_b = 0.0;
    double transmitted_direct = 0.0;
    double transmitted_direct_r = 0.0;
    double transmitted_direct_g = 0.0;
    double transmitted_direct_b = 0.0;
    double transmitted_bounce = 0.0;
    double transmitted_bounce_r = 0.0;
    double transmitted_bounce_g = 0.0;
    double transmitted_bounce_b = 0.0;

    if (!scene || !hit || !payload || !io_result) return;
    transparency = runtime_emission_transparency_3d_clamp(payload->transparency, 0.0, 1.0);
    if (!(transparency > 0.0)) {
        return;
    }

    (void)RuntimeDielectricTransport3D_Resolve(payload,
                                               hit->normal,
                                               path_state.incidentDir,
                                               &dielectric);
    runtime_emission_transparency_3d_resolve_mix_weights(payload,
                                                         &dielectric,
                                                         &base_front_weight,
                                                         &reflection_weight,
                                                         &transmission_weight);
    if (reflection_weight > 1e-9) {
        (void)runtime_emission_transparency_3d_sample_reflection(scene,
                                                                 hit,
                                                                 dielectric.reflectionDir,
                                                                 sampling,
                                                                 path_state,
                                                                 io_result,
                                                                 &reflected_direct,
                                                                 &reflected_direct_r,
                                                                 &reflected_direct_g,
                                                                 &reflected_direct_b,
                                                                 &reflected_bounce,
                                                                 &reflected_bounce_r,
                                                                 &reflected_bounce_g,
                                                                 &reflected_bounce_b);
    }
    if (transmission_weight > 1e-9 && dielectric.hasRefraction) {
        (void)runtime_emission_transparency_3d_sample_transmission(scene,
                                                                   hit,
                                                                   payload,
                                                                   dielectric.refractionDir,
                                                                   sampling,
                                                                   path_state,
                                                                   io_result,
                                                                   &transmitted_direct,
                                                                   &transmitted_direct_r,
                                                                   &transmitted_direct_g,
                                                                   &transmitted_direct_b,
                                                                   &transmitted_bounce,
                                                                   &transmitted_bounce_r,
                                                                   &transmitted_bounce_g,
                                                                   &transmitted_bounce_b);
    }

    io_result->emissiveDirectRadiance *= base_front_weight;
    io_result->emissiveDirectRadianceR *= base_front_weight;
    io_result->emissiveDirectRadianceG *= base_front_weight;
    io_result->emissiveDirectRadianceB *= base_front_weight;
    io_result->emissiveBounceRadiance *= base_front_weight;
    io_result->emissiveBounceRadianceR *= base_front_weight;
    io_result->emissiveBounceRadianceG *= base_front_weight;
    io_result->emissiveBounceRadianceB *= base_front_weight;
    io_result->reflectedDirectRadiance = reflected_direct * reflection_weight;
    io_result->reflectedDirectRadianceR = reflected_direct_r * reflection_weight;
    io_result->reflectedDirectRadianceG = reflected_direct_g * reflection_weight;
    io_result->reflectedDirectRadianceB = reflected_direct_b * reflection_weight;
    io_result->reflectedBounceRadiance = reflected_bounce * reflection_weight;
    io_result->reflectedBounceRadianceR = reflected_bounce_r * reflection_weight;
    io_result->reflectedBounceRadianceG = reflected_bounce_g * reflection_weight;
    io_result->reflectedBounceRadianceB = reflected_bounce_b * reflection_weight;
    io_result->transmittedDirectRadiance = transmitted_direct * transmission_weight;
    io_result->transmittedDirectRadianceR = transmitted_direct_r * transmission_weight;
    io_result->transmittedDirectRadianceG = transmitted_direct_g * transmission_weight;
    io_result->transmittedDirectRadianceB = transmitted_direct_b * transmission_weight;
    io_result->transmittedBounceRadiance = transmitted_bounce * transmission_weight;
    io_result->transmittedBounceRadianceR = transmitted_bounce_r * transmission_weight;
    io_result->transmittedBounceRadianceG = transmitted_bounce_g * transmission_weight;
    io_result->transmittedBounceRadianceB = transmitted_bounce_b * transmission_weight;
    io_result->directRadiance = (io_result->directRadiance * base_front_weight) +
                                io_result->reflectedDirectRadiance +
                                io_result->transmittedDirectRadiance;
    io_result->directRadianceR = (io_result->directRadianceR * base_front_weight) +
                                 io_result->reflectedDirectRadianceR +
                                 io_result->transmittedDirectRadianceR;
    io_result->directRadianceG = (io_result->directRadianceG * base_front_weight) +
                                 io_result->reflectedDirectRadianceG +
                                 io_result->transmittedDirectRadianceG;
    io_result->directRadianceB = (io_result->directRadianceB * base_front_weight) +
                                 io_result->reflectedDirectRadianceB +
                                 io_result->transmittedDirectRadianceB;
    io_result->bounceRadiance = (io_result->bounceRadiance * base_front_weight) +
                                io_result->reflectedBounceRadiance +
                                io_result->transmittedBounceRadiance;
    io_result->bounceRadianceR = (io_result->bounceRadianceR * base_front_weight) +
                                 io_result->reflectedBounceRadianceR +
                                 io_result->transmittedBounceRadianceR;
    io_result->bounceRadianceG = (io_result->bounceRadianceG * base_front_weight) +
                                 io_result->reflectedBounceRadianceG +
                                 io_result->transmittedBounceRadianceG;
    io_result->bounceRadianceB = (io_result->bounceRadianceB * base_front_weight) +
                                 io_result->reflectedBounceRadianceB +
                                 io_result->transmittedBounceRadianceB;
    io_result->radiance = io_result->directRadiance + io_result->bounceRadiance;
    io_result->radianceR = io_result->directRadianceR + io_result->bounceRadianceR;
    io_result->radianceG = io_result->directRadianceG + io_result->bounceRadianceG;
    io_result->radianceB = io_result->directRadianceB + io_result->bounceRadianceB;
}

static void runtime_emission_transparency_3d_copy_material_result(
    const RuntimeMaterialResponse3DResult* source,
    const RuntimeMaterialPayload3D* payload,
    RuntimeEmissionTransparency3DResult* out_result) {
    if (!source || !payload || !out_result) return;

    out_result->hit = source->hit;
    out_result->visible = source->visible;
    out_result->payloadResolved = payload->valid;
    out_result->primaryRay = source->primaryRay;
    out_result->hitInfo = source->hitInfo;
    out_result->payload = *payload;
    out_result->directRadiance = source->directRadiance;
    out_result->directRadianceR = source->directRadianceR;
    out_result->directRadianceG = source->directRadianceG;
    out_result->directRadianceB = source->directRadianceB;
    out_result->bounceRadiance = source->bounceRadiance;
    out_result->bounceRadianceR = source->bounceRadianceR;
    out_result->bounceRadianceG = source->bounceRadianceG;
    out_result->bounceRadianceB = source->bounceRadianceB;
    out_result->emissiveDirectRadiance = 0.0;
    out_result->emissiveDirectRadianceR = 0.0;
    out_result->emissiveDirectRadianceG = 0.0;
    out_result->emissiveDirectRadianceB = 0.0;
    out_result->emissiveBounceRadiance = 0.0;
    out_result->emissiveBounceRadianceR = 0.0;
    out_result->emissiveBounceRadianceG = 0.0;
    out_result->emissiveBounceRadianceB = 0.0;
    out_result->reflectedDirectRadiance = 0.0;
    out_result->reflectedDirectRadianceR = 0.0;
    out_result->reflectedDirectRadianceG = 0.0;
    out_result->reflectedDirectRadianceB = 0.0;
    out_result->reflectedBounceRadiance = 0.0;
    out_result->reflectedBounceRadianceR = 0.0;
    out_result->reflectedBounceRadianceG = 0.0;
    out_result->reflectedBounceRadianceB = 0.0;
    out_result->transmittedDirectRadiance = 0.0;
    out_result->transmittedDirectRadianceR = 0.0;
    out_result->transmittedDirectRadianceG = 0.0;
    out_result->transmittedDirectRadianceB = 0.0;
    out_result->transmittedBounceRadiance = 0.0;
    out_result->transmittedBounceRadianceR = 0.0;
    out_result->transmittedBounceRadianceG = 0.0;
    out_result->transmittedBounceRadianceB = 0.0;
    out_result->radiance = source->radiance;
    out_result->radianceR = source->radianceR;
    out_result->radianceG = source->radianceG;
    out_result->radianceB = source->radianceB;
    out_result->secondaryRayCount = source->secondaryRayCount;
    out_result->secondaryHitCount = source->secondaryHitCount;
    out_result->secondaryContributingHitCount = source->secondaryContributingHitCount;
}

static bool runtime_emission_transparency_3d_shade_hit_recursive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DPathState path_state,
    RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);
    double emissive_direct = 0.0;
    double emissive_bounce = 0.0;
    double emissive_direct_r = 0.0;
    double emissive_direct_g = 0.0;
    double emissive_direct_b = 0.0;
    double emissive_bounce_r = 0.0;
    double emissive_bounce_g = 0.0;
    double emissive_bounce_b = 0.0;
    RuntimeEmissiveDirect3DResult emissive_direct_result = {0};
    int secondary_sample_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (!RuntimeMaterialPayload3D_ResolveFromHit(hit, &result.payload)) {
        *out_result = result;
        return false;
    }
    if (!RuntimeMaterialResponse3D_ShadeHit(scene, hit, sampling, &material_result)) {
        *out_result = result;
        return false;
    }

    runtime_emission_transparency_3d_copy_material_result(&material_result,
                                                          &result.payload,
                                                          &result);
    if (!runtime_emission_transparency_3d_can_skip_emission_support(scene, &result.payload)) {
        secondary_sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();
        result.secondaryRayCount = secondary_sample_count;
        view_dir = vec3_scale(path_state.incidentDir, -1.0);
        emissive_direct = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                              hit,
                                                                              &result.payload,
                                                                              view_dir,
                                                                              &emissive_direct_r,
                                                                              &emissive_direct_g,
                                                                              &emissive_direct_b);
        emissive_bounce = runtime_emission_transparency_3d_secondary_emissive(scene,
                                                                              hit,
                                                                              sampling,
                                                                              &result,
                                                                              &emissive_bounce_r,
                                                                              &emissive_bounce_g,
                                                                              &emissive_bounce_b);
        if (RuntimeEmissiveDirect3D_ShadeHit(scene, hit, sampling, &emissive_direct_result)) {
            emissive_direct += emissive_direct_result.directRadiance;
            emissive_direct_r += emissive_direct_result.directRadianceR;
            emissive_direct_g += emissive_direct_result.directRadianceG;
            emissive_direct_b += emissive_direct_result.directRadianceB;
        }
    }
    result.emissiveDirectRadiance = emissive_direct;
    result.emissiveDirectRadianceR = emissive_direct_r;
    result.emissiveDirectRadianceG = emissive_direct_g;
    result.emissiveDirectRadianceB = emissive_direct_b;
    result.emissiveBounceRadiance = emissive_bounce;
    result.emissiveBounceRadianceR = emissive_bounce_r;
    result.emissiveBounceRadianceG = emissive_bounce_g;
    result.emissiveBounceRadianceB = emissive_bounce_b;
    result.directRadiance += emissive_direct;
    result.directRadianceR += emissive_direct_r;
    result.directRadianceG += emissive_direct_g;
    result.directRadianceB += emissive_direct_b;
    result.bounceRadiance += emissive_bounce;
    result.bounceRadianceR += emissive_bounce_r;
    result.bounceRadianceG += emissive_bounce_g;
    result.bounceRadianceB += emissive_bounce_b;
    result.radiance = result.directRadiance + result.bounceRadiance;
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    runtime_emission_transparency_3d_apply_transparency(scene,
                                                        hit,
                                                        &result.payload,
                                                        path_state,
                                                        sampling,
                                                        &result);
    *out_result = result;
    return true;
}

static bool runtime_emission_transparency_3d_shade_pixel_recursive(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DPathState path_state = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);
    double emissive_direct = 0.0;
    double emissive_bounce = 0.0;
    double emissive_direct_r = 0.0;
    double emissive_direct_g = 0.0;
    double emissive_direct_b = 0.0;
    double emissive_bounce_r = 0.0;
    double emissive_bounce_g = 0.0;
    double emissive_bounce_b = 0.0;
    RuntimeEmissiveDirect3DResult emissive_direct_result = {0};
    int secondary_sample_count = 0;

    if (!scene || !projector || !out_result) return false;
    if (!RuntimeMaterialResponse3D_ShadePixel(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              sampling,
                                              &material_result)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&material_result.hitInfo, &result.payload)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }

    runtime_emission_transparency_3d_copy_material_result(&material_result,
                                                          &result.payload,
                                                          &result);
    path_state.incidentDir = material_result.primaryRay.direction;
    path_state.specularDepth = 0;
    path_state.transmissionDepth = 0;
    if (!runtime_emission_transparency_3d_can_skip_emission_support(scene, &result.payload)) {
        secondary_sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();
        result.secondaryRayCount = secondary_sample_count;
        view_dir = vec3_scale(material_result.primaryRay.direction, -1.0);
        emissive_direct = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                              &result.hitInfo,
                                                                              &result.payload,
                                                                              view_dir,
                                                                              &emissive_direct_r,
                                                                              &emissive_direct_g,
                                                                              &emissive_direct_b);
        emissive_bounce = runtime_emission_transparency_3d_secondary_emissive(scene,
                                                                               &result.hitInfo,
                                                                               sampling,
                                                                               &result,
                                                                               &emissive_bounce_r,
                                                                               &emissive_bounce_g,
                                                                               &emissive_bounce_b);
        if (RuntimeEmissiveDirect3D_ShadeHit(scene,
                                             &result.hitInfo,
                                             sampling,
                                             &emissive_direct_result)) {
            emissive_direct += emissive_direct_result.directRadiance;
            emissive_direct_r += emissive_direct_result.directRadianceR;
            emissive_direct_g += emissive_direct_result.directRadianceG;
            emissive_direct_b += emissive_direct_result.directRadianceB;
        }
    }
    result.emissiveDirectRadiance = emissive_direct;
    result.emissiveDirectRadianceR = emissive_direct_r;
    result.emissiveDirectRadianceG = emissive_direct_g;
    result.emissiveDirectRadianceB = emissive_direct_b;
    result.emissiveBounceRadiance = emissive_bounce;
    result.emissiveBounceRadianceR = emissive_bounce_r;
    result.emissiveBounceRadianceG = emissive_bounce_g;
    result.emissiveBounceRadianceB = emissive_bounce_b;
    result.directRadiance += emissive_direct;
    result.directRadianceR += emissive_direct_r;
    result.directRadianceG += emissive_direct_g;
    result.directRadianceB += emissive_direct_b;
    result.bounceRadiance += emissive_bounce;
    result.bounceRadianceR += emissive_bounce_r;
    result.bounceRadianceG += emissive_bounce_g;
    result.bounceRadianceB += emissive_bounce_b;
    result.radiance = result.directRadiance + result.bounceRadiance;
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    runtime_emission_transparency_3d_apply_transparency(scene,
                                                        &result.hitInfo,
                                                        &result.payload,
                                                        path_state,
                                                        sampling,
                                                        &result);
    *out_result = result;
    return true;
}

bool RuntimeEmissionTransparency3D_ShadeHit(const RuntimeScene3D* scene,
                                            const HitInfo3D* hit,
                                            const RuntimeNative3DSamplingContext* sampling,
                                            RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DPathState path_state = {0};
    path_state.incidentDir = vec3_scale(hit->normal, -1.0);
    path_state.specularDepth = 0;
    path_state.transmissionDepth = 0;
    return runtime_emission_transparency_3d_shade_hit_recursive(
        scene, hit, sampling, path_state, out_result);
}

bool RuntimeEmissionTransparency3D_ShadePixel(const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector,
                                              double pixel_x,
                                              double pixel_y,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeEmissionTransparency3DResult* out_result) {
    return runtime_emission_transparency_3d_shade_pixel_recursive(
        scene, projector, pixel_x, pixel_y, sampling, out_result);
}

bool RuntimeEmissionTransparency3D_ShadePrimaryHit(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* out_result) {
    return runtime_emission_transparency_3d_shade_primary_hit_recursive(
        scene, primary_hit, NULL, sampling, out_result);
}

bool RuntimeEmissionTransparency3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* out_result) {
    return runtime_emission_transparency_3d_shade_primary_hit_recursive(
        scene, primary_hit, payload, sampling, out_result);
}
