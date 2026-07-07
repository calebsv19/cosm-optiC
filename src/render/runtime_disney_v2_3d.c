#include "render/runtime_disney_v2_internal_3d.h"

double runtime_disney_v2_3d_clamp(double value,
                                         double min_value,
                                         double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

double runtime_disney_v2_3d_clamp01(double value) {
    return runtime_disney_v2_3d_clamp(value, 0.0, 1.0);
}

double runtime_disney_v2_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static bool runtime_disney_v2_3d_resolve_payload(
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    RuntimeMaterialPayload3D* out_payload) {
    if (!out_payload) return false;
    memset(out_payload, 0, sizeof(*out_payload));
    if (payload && payload->valid) {
        *out_payload = *payload;
        return true;
    }
    if (!hit) {
        return false;
    }
    return RuntimeMaterialPayload3D_ResolveFromHit(hit, out_payload) && out_payload->valid;
}

static void runtime_disney_v2_3d_apply_mirror_composition(
    const RuntimeMaterialPayload3D* payload,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeMirrorComposition3DPolicy policy = RuntimeMirrorComposition3D_Evaluate(payload);
    double before_r = 0.0;
    double before_g = 0.0;
    double before_b = 0.0;

    if (!io_result) return;

    before_r = io_result->directRadianceR + io_result->diffuseRadianceR;
    before_g = io_result->directRadianceG + io_result->diffuseRadianceG;
    before_b = io_result->directRadianceB + io_result->diffuseRadianceB;
    io_result->mirrorDominance = policy.dominance;
    io_result->mirrorBaseAttenuation = policy.baseAttenuation;
    io_result->mirrorBaseRadianceBeforeAttenuation =
        runtime_disney_v2_3d_peak(before_r, before_g, before_b);

    if (policy.active) {
        io_result->directRadianceR *= policy.baseAttenuation;
        io_result->directRadianceG *= policy.baseAttenuation;
        io_result->directRadianceB *= policy.baseAttenuation;
        io_result->diffuseRadianceR *= policy.baseAttenuation;
        io_result->diffuseRadianceG *= policy.baseAttenuation;
        io_result->diffuseRadianceB *= policy.baseAttenuation;
    }

    io_result->mirrorBaseRadianceAfterAttenuation =
        runtime_disney_v2_3d_peak(io_result->directRadianceR + io_result->diffuseRadianceR,
                                  io_result->directRadianceG + io_result->diffuseRadianceG,
                                  io_result->directRadianceB + io_result->diffuseRadianceB);
}

static void runtime_disney_v2_3d_apply_specular_reflection(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeSpecularReflection3DResult reflection = {0};
    RuntimeMaterialPayload3D reflected_payload = {0};
    RuntimeDirectLight3DResult reflected_direct = {0};
    double reflected_r = 0.0;
    double reflected_g = 0.0;
    double reflected_b = 0.0;

    if (!scene || !hit || !io_result || !io_result->payloadResolved ||
        !RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                              RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR,
                                              1)) {
        return;
    }
    if (!RuntimeSpecularReflection3D_Trace(scene,
                                           hit,
                                           &io_result->payload,
                                           view_dir,
                                           sampling,
                                           &reflection)) {
        return;
    }
    if (reflection.traced) {
        io_result->specularReflectionRayCount += 1;
    }
    if (reflection.emitterWins) {
        io_result->specularReflectionHitCount += 1;
        io_result->specularReflectionEmitterHitCount += 1;
        reflected_r = reflection.emitterHitInfo.radiance;
        reflected_g = reflection.emitterHitInfo.radiance;
        reflected_b = reflection.emitterHitInfo.radiance;
    } else if (reflection.geometryHit && reflection.hitInfo.triangleIndex >= 0 &&
               reflection.hitInfo.triangleIndex != hit->triangleIndex) {
        io_result->specularReflectionHitCount += 1;
        io_result->specularReflectionGeometryHitCount += 1;
        (void)RuntimeDisneyV2_3D_ApplySpecularReflectionRecursion(scene,
                                                                  hit,
                                                                  &reflection,
                                                                  sampling,
                                                                  io_result);
        if (RuntimeMaterialPayload3D_ResolveFromHit(&reflection.hitInfo, &reflected_payload) &&
            reflected_payload.valid &&
            RuntimeDirectLight3D_ShadeHitWithPayload(scene,
                                                     &reflection.hitInfo,
                                                     &reflected_payload,
                                                     sampling,
                                                     &reflected_direct)) {
            reflected_r = reflected_direct.radianceR;
            reflected_g = reflected_direct.radianceG;
            reflected_b = reflected_direct.radianceB;
        }
    } else if (reflection.traced) {
        io_result->specularReflectionNoHitCount += 1;
    }

    reflected_r *= reflection.weight * reflection.tintR;
    reflected_g *= reflection.weight * reflection.tintG;
    reflected_b *= reflection.weight * reflection.tintB;
    if (runtime_disney_v2_3d_peak(reflected_r, reflected_g, reflected_b) <= 1e-9) {
        return;
    }

    io_result->specularReflectionContributingHitCount += 1;
    io_result->specularReflectionRadianceR += reflected_r;
    io_result->specularReflectionRadianceG += reflected_g;
    io_result->specularReflectionRadianceB += reflected_b;
    io_result->specularRadianceR += reflected_r;
    io_result->specularRadianceG += reflected_g;
    io_result->specularRadianceB += reflected_b;
}

static bool runtime_disney_v2_3d_shade_hit_with_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
    RuntimeDirectLight3DResult direct = {0};
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    Vec3 half_vec = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;
    double cos_theta_h = 0.0;
    double dot_i_h = 0.0;
    double diffuse_scale = 0.0;
    double specular_scale = 0.0;
    double transmission_scale = 0.0;

    if (!scene || !hit || !out_result) return false;
    result.hit = true;
    result.hitInfo = *hit;
    result.primaryRay.direction = vec3_scale(view_dir, -1.0);
    result.payloadResolved = runtime_disney_v2_3d_resolve_payload(hit, payload, &result.payload);
    if (!result.payloadResolved) {
        *out_result = result;
        return false;
    }

    result.principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&result.payload);
    result.pathPolicy = RuntimePathDepthPolicy3D_Resolve();
    result.pathPolicyResolved = true;
    result.diffuseProbability =
        RuntimePrincipledBSDF3D_DiffuseProbability(&result.principled);
    result.specularProbability =
        RuntimePrincipledBSDF3D_SpecularProbability(&result.principled);
    result.transmissionProbability =
        runtime_disney_v2_3d_clamp01(result.principled.transmissionWeight);
    result.pathDepth = 1;

    view_dir = vec3_normalize(view_dir);
    result.ndotv = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, view_dir));
    light_dir = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(light_dir);
    if (light_distance > 1e-9) {
        light_dir = vec3_scale(light_dir, 1.0 / light_distance);
    } else {
        light_dir = vec3_normalize(hit->normal);
    }
    result.ndotl = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, light_dir));
    half_vec = vec3_normalize(vec3_add(light_dir, view_dir));
    cos_theta_h = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, half_vec));
    dot_i_h = runtime_disney_v2_3d_clamp01(vec3_dot(light_dir, half_vec));
    result.fresnelWeight =
        RuntimePrincipledBSDF3D_FresnelSchlick(dot_i_h, result.principled.dielectricF0);
    result.diffuseBsdfCos =
        RuntimePrincipledBSDF3D_EvaluateDiffuseCos(&result.principled, result.ndotl);
    result.specularBsdfCos =
        RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(&result.principled,
                                                       result.ndotl,
                                                       result.ndotv,
                                                       cos_theta_h);
    result.specularHalfPdf =
        RuntimePrincipledBSDF3D_GGXHalfVectorPdf(&result.principled, cos_theta_h, dot_i_h);

    if (scene->hasLight &&
        RuntimeDirectLight3D_ShadeHitWithPayload(scene, hit, &result.payload, sampling, &direct)) {
        result.directSampleCount = scene->hasLight ? 1 : 0;
        result.directRadianceR = direct.radianceR;
        result.directRadianceG = direct.radianceG;
        result.directRadianceB = direct.radianceB;
        result.visible = direct.visible;
    }

    diffuse_scale = runtime_disney_v2_3d_clamp(result.diffuseProbability *
                                                  (0.25 + result.diffuseBsdfCos),
                                              0.0,
                                              1.25);
    specular_scale = runtime_disney_v2_3d_clamp(result.specularProbability *
                                                   (result.fresnelWeight + result.specularBsdfCos),
                                               0.0,
                                               1.75);
    transmission_scale = runtime_disney_v2_3d_clamp(result.transmissionProbability *
                                                       (1.0 - result.diffuseProbability),
                                                   0.0,
                                                   1.0);

    result.diffuseRadianceR = result.directRadianceR * diffuse_scale;
    result.diffuseRadianceG = result.directRadianceG * diffuse_scale;
    result.diffuseRadianceB = result.directRadianceB * diffuse_scale;
    result.specularRadianceR = result.directRadianceR * specular_scale;
    result.specularRadianceG = result.directRadianceG * specular_scale;
    result.specularRadianceB = result.directRadianceB * specular_scale;
    result.transmissionRadianceR = result.directRadianceR * transmission_scale;
    result.transmissionRadianceG = result.directRadianceG * transmission_scale;
    result.transmissionRadianceB = result.directRadianceB * transmission_scale;
    result.emissionRadianceR = result.principled.emissiveR * result.principled.emissiveStrength;
    result.emissionRadianceG = result.principled.emissiveG * result.principled.emissiveStrength;
    result.emissionRadianceB = result.principled.emissiveB * result.principled.emissiveStrength;

    runtime_disney_v2_3d_apply_mirror_composition(&result.payload, &result);
    runtime_disney_v2_3d_apply_specular_reflection(scene, hit, sampling, view_dir, &result);
    runtime_disney_v2_3d_apply_stochastic_transport(scene, hit, sampling, view_dir, &result);
    runtime_disney_v2_3d_refresh_peaks(&result);
    *out_result = result;
    return true;
}

bool RuntimeDisneyV2_3D_ShadeHit(const RuntimeScene3D* scene,
                                 const HitInfo3D* hit,
                                 const RuntimeNative3DSamplingContext* sampling,
                                 RuntimeDisneyV2_3DResult* out_result) {
    return runtime_disney_v2_3d_shade_hit_with_payload(scene,
                                                       hit,
                                                       NULL,
                                                       sampling,
                                                       runtime_disney_v2_3d_default_view_dir(hit),
                                                       out_result);
}

bool RuntimeDisneyV2_3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                        const RuntimePrimaryHit3DResult* primary_hit,
                                        const RuntimeNative3DSamplingContext* sampling,
                                        RuntimeDisneyV2_3DResult* out_result) {
    return RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(scene,
                                                         primary_hit,
                                                         NULL,
                                                         sampling,
                                                         out_result);
}

bool RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 1.0);

    if (!scene || !primary_hit || !out_result) return false;
    if (!primary_hit->hit) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    view_dir = vec3_scale(primary_hit->primaryRay.direction, -1.0);
    if (!runtime_disney_v2_3d_shade_hit_with_payload(scene,
                                                     &primary_hit->hitInfo,
                                                     payload,
                                                     sampling,
                                                     view_dir,
                                                     &result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }
    result.primaryRay = primary_hit->primaryRay;
    runtime_disney_v2_3d_apply_transmittance(&primary_hit->primaryTransmittance, &result);
    (void)RuntimeDisneyV2_3D_ApplyPrimaryTransmissionContinuation(scene,
                                                                  primary_hit,
                                                                  sampling,
                                                                  &result);
    runtime_disney_v2_3d_refresh_peaks(&result);
    *out_result = result;
    return true;
}

bool RuntimeDisneyV2_3D_ShadePixel(const RuntimeScene3D* scene,
                                   const RuntimeCameraProjector3D* projector,
                                   double pixel_x,
                                   double pixel_y,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
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

    return RuntimeDisneyV2_3D_ShadePrimaryHit(scene, &primary_hit, sampling, out_result);
}
