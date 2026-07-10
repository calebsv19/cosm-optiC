#include "render/runtime_emission_transparency_3d_internal.h"

bool runtime_emission_transparency_3d_can_skip_emission_support(
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

bool runtime_emission_transparency_3d_can_skip_transparency_support(
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

void runtime_emission_transparency_3d_copy_material_result(
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
    out_result->directRadiance = source->directRadiance + source->specularRadiance;
    out_result->directRadianceR = source->directRadianceR + source->specularRadianceR;
    out_result->directRadianceG = source->directRadianceG + source->specularRadianceG;
    out_result->directRadianceB = source->directRadianceB + source->specularRadianceB;
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
