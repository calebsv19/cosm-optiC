#include "render/runtime_material_response_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_mirror_composition_3d.h"
#include "render/runtime_specular_reflection_3d.h"

static bool runtime_material_response_3d_shade_hit_with_payload_depth(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    int specular_depth_remaining,
    RuntimeMaterialResponse3DResult* out_result);

static double runtime_material_response_3d_clamp(double value,
                                                 double min_value,
                                                 double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 runtime_material_response_3d_default_view_dir(const HitInfo3D* hit) {
    if (!hit) return vec3(0.0, 0.0, 1.0);
    return vec3_normalize(hit->normal);
}

static double runtime_material_response_3d_direct_scale(const RuntimeScene3D* scene,
                                                        const HitInfo3D* hit,
                                                        const RuntimeMaterialPayload3D* payload,
                                                        Vec3 view_dir) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;
    double bsdf_eval = 0.0;
    double reference = 0.25;
    double scale = 1.0;

    if (!scene || !hit || !payload || !payload->valid) return 1.0;

    to_light = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(to_light);
    if (!(light_distance > 1e-9)) {
        return 1.0;
    }
    to_light = vec3_scale(to_light, 1.0 / light_distance);

    bsdf_eval = MaterialBSDFEvaluateCos3(&payload->bsdf,
                                         hit->normal.x,
                                         hit->normal.y,
                                         hit->normal.z,
                                         to_light.x,
                                         to_light.y,
                                         to_light.z,
                                         view_dir.x,
                                         view_dir.y,
                                         view_dir.z);
    scale = bsdf_eval / reference;

    return runtime_material_response_3d_clamp(scale, 0.15, 2.0);
}

static double runtime_material_response_3d_bounce_scale(const RuntimeMaterialPayload3D* payload) {
    double bounce_weight = 0.0;
    double normalized = 1.0;

    if (!payload || !payload->valid) return 1.0;

    bounce_weight = payload->bsdf.albedo *
                    fmax(payload->bsdf.diffuseWeight, 0.15) *
                    (0.7 + (0.3 * payload->bsdf.roughness));
    normalized = bounce_weight / 0.35;
    return runtime_material_response_3d_clamp(normalized, 0.05, 1.75);
}

static double runtime_material_response_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static void runtime_material_response_3d_apply_transmittance(
    const RuntimeVisibility3DTransmittance* transmittance,
    RuntimeMaterialResponse3DResult* io_result) {
    if (!transmittance || !io_result) return;

    io_result->directRadianceR *= transmittance->r;
    io_result->directRadianceG *= transmittance->g;
    io_result->directRadianceB *= transmittance->b;
    io_result->bounceRadianceR *= transmittance->r;
    io_result->bounceRadianceG *= transmittance->g;
    io_result->bounceRadianceB *= transmittance->b;
    io_result->specularRadianceR *= transmittance->r;
    io_result->specularRadianceG *= transmittance->g;
    io_result->specularRadianceB *= transmittance->b;
    io_result->mirrorBaseRadianceBeforeAttenuation *= transmittance->luma;
    io_result->mirrorBaseRadianceAfterAttenuation *= transmittance->luma;
    io_result->directRadiance = runtime_material_response_3d_peak(io_result->directRadianceR,
                                                                  io_result->directRadianceG,
                                                                  io_result->directRadianceB);
    io_result->bounceRadiance = runtime_material_response_3d_peak(io_result->bounceRadianceR,
                                                                  io_result->bounceRadianceG,
                                                                  io_result->bounceRadianceB);
    io_result->specularRadiance = runtime_material_response_3d_peak(io_result->specularRadianceR,
                                                                    io_result->specularRadianceG,
                                                                    io_result->specularRadianceB);
    io_result->radianceR = io_result->directRadianceR + io_result->bounceRadianceR;
    io_result->radianceG = io_result->directRadianceG + io_result->bounceRadianceG;
    io_result->radianceB = io_result->directRadianceB + io_result->bounceRadianceB;
    io_result->radianceR += io_result->specularRadianceR;
    io_result->radianceG += io_result->specularRadianceG;
    io_result->radianceB += io_result->specularRadianceB;
    io_result->radiance = io_result->directRadiance +
                           io_result->bounceRadiance +
                           io_result->specularRadiance;
    if (!(transmittance->luma > 1e-9)) {
        io_result->visible = false;
    }
}

static void runtime_material_response_3d_apply_weights(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 view_dir,
    const RuntimeDiffuseBounce3DResult* diffuse_result,
    RuntimeMaterialResponse3DResult* out_result) {
    double direct_scale = 1.0;
    double bounce_scale = 1.0;

    if (!diffuse_result || !out_result) return;

    direct_scale = runtime_material_response_3d_direct_scale(scene, hit, payload, view_dir);
    bounce_scale = runtime_material_response_3d_bounce_scale(payload);

    out_result->directRadiance = diffuse_result->directRadiance * direct_scale;
    out_result->directRadianceR = diffuse_result->directRadianceR * direct_scale;
    out_result->directRadianceG = diffuse_result->directRadianceG * direct_scale;
    out_result->directRadianceB = diffuse_result->directRadianceB * direct_scale;
    out_result->bounceRadiance = diffuse_result->bounceRadiance * bounce_scale;
    out_result->bounceRadianceR = diffuse_result->bounceRadianceR * bounce_scale;
    out_result->bounceRadianceG = diffuse_result->bounceRadianceG * bounce_scale;
    out_result->bounceRadianceB = diffuse_result->bounceRadianceB * bounce_scale;
    out_result->radianceR = out_result->directRadianceR + out_result->bounceRadianceR;
    out_result->radianceG = out_result->directRadianceG + out_result->bounceRadianceG;
    out_result->radianceB = out_result->directRadianceB + out_result->bounceRadianceB;
    out_result->radiance = out_result->directRadiance + out_result->bounceRadiance;
}

static void runtime_material_response_3d_apply_mirror_composition(
    const RuntimeMaterialPayload3D* payload,
    RuntimeMaterialResponse3DResult* io_result) {
    RuntimeMirrorComposition3DPolicy policy = RuntimeMirrorComposition3D_Evaluate(payload);
    double before_r = 0.0;
    double before_g = 0.0;
    double before_b = 0.0;

    if (!io_result) return;

    before_r = io_result->directRadianceR + io_result->bounceRadianceR;
    before_g = io_result->directRadianceG + io_result->bounceRadianceG;
    before_b = io_result->directRadianceB + io_result->bounceRadianceB;
    io_result->mirrorDominance = policy.dominance;
    io_result->mirrorBaseAttenuation = policy.baseAttenuation;
    io_result->mirrorBaseRadianceBeforeAttenuation =
        runtime_material_response_3d_peak(before_r, before_g, before_b);

    if (policy.active) {
        io_result->directRadianceR *= policy.baseAttenuation;
        io_result->directRadianceG *= policy.baseAttenuation;
        io_result->directRadianceB *= policy.baseAttenuation;
        io_result->bounceRadianceR *= policy.baseAttenuation;
        io_result->bounceRadianceG *= policy.baseAttenuation;
        io_result->bounceRadianceB *= policy.baseAttenuation;
        io_result->directRadiance =
            runtime_material_response_3d_peak(io_result->directRadianceR,
                                              io_result->directRadianceG,
                                              io_result->directRadianceB);
        io_result->bounceRadiance =
            runtime_material_response_3d_peak(io_result->bounceRadianceR,
                                              io_result->bounceRadianceG,
                                              io_result->bounceRadianceB);
        io_result->radianceR = io_result->directRadianceR + io_result->bounceRadianceR;
        io_result->radianceG = io_result->directRadianceG + io_result->bounceRadianceG;
        io_result->radianceB = io_result->directRadianceB + io_result->bounceRadianceB;
        io_result->radiance = io_result->directRadiance + io_result->bounceRadiance;
    }

    io_result->mirrorBaseRadianceAfterAttenuation =
        runtime_material_response_3d_peak(io_result->directRadianceR + io_result->bounceRadianceR,
                                          io_result->directRadianceG + io_result->bounceRadianceG,
                                          io_result->directRadianceB + io_result->bounceRadianceB);
}

static void runtime_material_response_3d_add_specular_rgb(
    RuntimeMaterialResponse3DResult* io_result,
    double r,
    double g,
    double b) {
    if (!io_result) return;
    io_result->specularRadianceR += r;
    io_result->specularRadianceG += g;
    io_result->specularRadianceB += b;
    io_result->specularRadiance = runtime_material_response_3d_peak(io_result->specularRadianceR,
                                                                    io_result->specularRadianceG,
                                                                    io_result->specularRadianceB);
    io_result->radianceR += r;
    io_result->radianceG += g;
    io_result->radianceB += b;
    io_result->radiance = io_result->directRadiance +
                          io_result->bounceRadiance +
                          io_result->specularRadiance;
    if (io_result->specularRadiance > 1e-9) {
        io_result->visible = true;
    }
}

static void runtime_material_response_3d_apply_specular_reflection(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    int specular_depth_remaining,
    RuntimeMaterialResponse3DResult* io_result) {
    RuntimeSpecularReflection3DResult reflection = {0};
    RuntimeMaterialResponse3DResult reflected_material = {0};
    RuntimeMaterialPayload3D reflected_payload = {0};
    double reflected_r = 0.0;
    double reflected_g = 0.0;
    double reflected_b = 0.0;

    if (!scene || !hit || !payload || !io_result || specular_depth_remaining <= 0) return;
    if (!RuntimeSpecularReflection3D_Trace(scene,
                                           hit,
                                           payload,
                                           view_dir,
                                           sampling,
                                           &reflection)) {
        return;
    }
    if (reflection.traced) {
        io_result->specularRayCount += 1;
    }
    if (reflection.emitterWins) {
        io_result->specularHitCount += 1;
        reflected_r = reflection.emitterHitInfo.radiance;
        reflected_g = reflection.emitterHitInfo.radiance;
        reflected_b = reflection.emitterHitInfo.radiance;
    } else if (reflection.geometryHit &&
               reflection.hitInfo.triangleIndex >= 0 &&
               reflection.hitInfo.triangleIndex != hit->triangleIndex &&
               RuntimeMaterialPayload3D_ResolveFromHit(&reflection.hitInfo, &reflected_payload) &&
               runtime_material_response_3d_shade_hit_with_payload_depth(
                   scene,
                   &reflection.hitInfo,
                   &reflected_payload,
                   sampling,
                   vec3_scale(reflection.ray.direction, -1.0),
                   specular_depth_remaining - 1,
                   &reflected_material)) {
        io_result->specularHitCount += 1;
        io_result->specularRayCount += reflected_material.specularRayCount;
        io_result->specularHitCount += reflected_material.specularHitCount;
        io_result->specularContributingHitCount +=
            reflected_material.specularContributingHitCount;
        reflected_r = reflected_material.radianceR;
        reflected_g = reflected_material.radianceG;
        reflected_b = reflected_material.radianceB;
    }

    reflected_r *= reflection.weight * reflection.tintR;
    reflected_g *= reflection.weight * reflection.tintG;
    reflected_b *= reflection.weight * reflection.tintB;
    if (runtime_material_response_3d_peak(reflected_r, reflected_g, reflected_b) > 1e-9) {
        io_result->specularContributingHitCount += 1;
        runtime_material_response_3d_add_specular_rgb(io_result,
                                                      reflected_r,
                                                      reflected_g,
                                                      reflected_b);
    }
}

bool RuntimeMaterialResponse3D_ShadeHit(const RuntimeScene3D* scene,
                                        const HitInfo3D* hit,
                                        const RuntimeNative3DSamplingContext* sampling,
                                        RuntimeMaterialResponse3DResult* out_result) {
    return RuntimeMaterialResponse3D_ShadeHitWithPayload(scene, hit, NULL, sampling, out_result);
}

bool RuntimeMaterialResponse3D_ShadeHitWithPayload(const RuntimeScene3D* scene,
                                                   const HitInfo3D* hit,
                                                   const RuntimeMaterialPayload3D* payload,
                                                   const RuntimeNative3DSamplingContext* sampling,
                                                   RuntimeMaterialResponse3DResult* out_result) {
    RuntimePathDepthPolicy3D path_policy = RuntimePathDepthPolicy3D_Resolve();
    return runtime_material_response_3d_shade_hit_with_payload_depth(
        scene,
        hit,
        payload,
        sampling,
        runtime_material_response_3d_default_view_dir(hit),
        path_policy.specularDepth,
        out_result);
}

static bool runtime_material_response_3d_shade_hit_with_payload_depth(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    int specular_depth_remaining,
    RuntimeMaterialResponse3DResult* out_result) {
    RuntimeMaterialResponse3DResult result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};

    if (!scene || !hit || !out_result) return false;
    if (payload && payload->valid) {
        result.payload = *payload;
    } else if (!RuntimeMaterialPayload3D_ResolveFromHit(hit, &result.payload)) {
        return false;
    }
    if (!RuntimeDiffuseBounce3D_ShadeHitWithPayload(scene,
                                                    hit,
                                                    &result.payload,
                                                    sampling,
                                                    &diffuse_result)) {
        return false;
    }

    result.hit = diffuse_result.hit;
    result.visible = diffuse_result.visible;
    result.materialResolved = result.payload.valid;
    result.hitInfo = diffuse_result.hitInfo;
    runtime_material_response_3d_apply_weights(scene,
                                               hit,
                                               &result.payload,
                                               view_dir,
                                               &diffuse_result,
                                               &result);
    runtime_material_response_3d_apply_mirror_composition(&result.payload, &result);
    result.secondaryRayCount = diffuse_result.secondaryRayCount;
    result.secondaryHitCount = diffuse_result.secondaryHitCount;
    result.secondaryContributingHitCount = diffuse_result.secondaryContributingHitCount;
    runtime_material_response_3d_apply_specular_reflection(scene,
                                                           hit,
                                                           &result.payload,
                                                           sampling,
                                                           view_dir,
                                                           specular_depth_remaining,
                                                           &result);
    *out_result = result;
    return true;
}

bool RuntimeMaterialResponse3D_ShadePixel(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          const RuntimeNative3DSamplingContext* sampling,
                                          RuntimeMaterialResponse3DResult* out_result) {
    RuntimeMaterialResponse3DResult result = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};

    if (!scene || !projector || !out_result) return false;

    if (!RuntimeDirectLight3D_TracePrimaryHit(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              &primary_hit)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    return RuntimeMaterialResponse3D_ShadePrimaryHit(scene, &primary_hit, sampling, out_result);
}

bool RuntimeMaterialResponse3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                               const RuntimePrimaryHit3DResult* primary_hit,
                                               const RuntimeNative3DSamplingContext* sampling,
                                               RuntimeMaterialResponse3DResult* out_result) {
    return RuntimeMaterialResponse3D_ShadePrimaryHitWithPayload(scene,
                                                                primary_hit,
                                                                NULL,
                                                                sampling,
                                                                out_result);
}

bool RuntimeMaterialResponse3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeMaterialResponse3DResult* out_result) {
    RuntimeMaterialResponse3DResult result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    RuntimePathDepthPolicy3D path_policy = RuntimePathDepthPolicy3D_Resolve();
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);

    if (!scene || !primary_hit || !out_result) return false;
    if (!primary_hit->hit) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    if (payload && payload->valid) {
        result.payload = *payload;
    } else if (!RuntimeMaterialPayload3D_ResolveFromHit(&primary_hit->hitInfo, &result.payload)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }
    if (!RuntimeDiffuseBounce3D_ShadeHitWithPayload(scene,
                                                    &primary_hit->hitInfo,
                                                    &result.payload,
                                                    sampling,
                                                    &diffuse_result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    result.hit = diffuse_result.hit;
    result.visible = diffuse_result.visible;
    result.materialResolved = result.payload.valid;
    result.hitInfo = diffuse_result.hitInfo;
    view_dir = vec3_scale(primary_hit->primaryRay.direction, -1.0);
    runtime_material_response_3d_apply_weights(scene,
                                               &result.hitInfo,
                                               &result.payload,
                                               view_dir,
                                               &diffuse_result,
                                               &result);
    runtime_material_response_3d_apply_mirror_composition(&result.payload, &result);
    runtime_material_response_3d_apply_transmittance(&primary_hit->primaryTransmittance, &result);
    result.secondaryRayCount = diffuse_result.secondaryRayCount;
    result.secondaryHitCount = diffuse_result.secondaryHitCount;
    result.secondaryContributingHitCount = diffuse_result.secondaryContributingHitCount;
    result.primaryRay = primary_hit->primaryRay;
    runtime_material_response_3d_apply_specular_reflection(scene,
                                                           &result.hitInfo,
                                                           &result.payload,
                                                           sampling,
                                                           view_dir,
                                                           path_policy.specularDepth,
                                                           &result);
    *out_result = result;
    return true;
}
