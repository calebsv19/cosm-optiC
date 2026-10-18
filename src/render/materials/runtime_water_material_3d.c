#include "render/runtime_water_material_3d.h"

#include <math.h>
#include <string.h>

#include "scene/object_manager.h"

static RuntimeWaterMaterial3DOverride gWaterMaterialOverrides[MAX_OBJECTS];

static double runtime_water_material_3d_clamp(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_water_material_3d_positive_or(double value, double fallback) {
    if (isfinite(value) && value > 1e-6) {
        return value;
    }
    return fallback;
}

static double runtime_water_material_3d_nonnegative_or(double value, double fallback) {
    if (isfinite(value) && value >= 0.0) {
        return value;
    }
    return fallback;
}

void RuntimeWaterMaterial3D_ClearAll(void) {
    memset(gWaterMaterialOverrides, 0, sizeof(gWaterMaterialOverrides));
}

void RuntimeWaterMaterial3D_Clear(int scene_object_index) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return;
    }
    memset(&gWaterMaterialOverrides[scene_object_index],
           0,
           sizeof(gWaterMaterialOverrides[scene_object_index]));
}

bool RuntimeWaterMaterial3D_Set(int scene_object_index,
                                const RuntimeWaterMaterial3DOverride* override) {
    RuntimeWaterMaterial3DOverride normalized = {0};
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS || !override) {
        return false;
    }

    normalized.valid = true;
    normalized.wetContactContainerIndexPlusOne = override->wetContactContainerIndexPlusOne;
    normalized.ior = runtime_water_material_3d_clamp(
        runtime_water_material_3d_positive_or(override->ior, 1.333),
        1.0,
        4.0);
    normalized.absorptionDistance =
        runtime_water_material_3d_positive_or(override->absorptionDistance, 4.0);
    normalized.absorptionR = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(override->absorptionR, 0.10),
        0.0,
        64.0);
    normalized.absorptionG = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(override->absorptionG, 0.035),
        0.0,
        64.0);
    normalized.absorptionB = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(override->absorptionB, 0.015),
        0.0,
        64.0);
    normalized.reflectivity = runtime_water_material_3d_clamp(override->reflectivity, 0.0, 1.0);
    normalized.roughness = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(override->roughness, 0.02),
        0.0,
        1.0);

    gWaterMaterialOverrides[scene_object_index] = normalized;
    return true;
}

bool RuntimeWaterMaterial3D_Get(int scene_object_index,
                                RuntimeWaterMaterial3DOverride* out_override) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return false;
    }
    if (!gWaterMaterialOverrides[scene_object_index].valid) {
        return false;
    }
    if (out_override) {
        *out_override = gWaterMaterialOverrides[scene_object_index];
    }
    return true;
}

void RuntimeWaterMaterial3D_ComputeTransmittanceTint(double absorption_distance,
                                                     double absorption_r,
                                                     double absorption_g,
                                                     double absorption_b,
                                                     double* out_r,
                                                     double* out_g,
                                                     double* out_b) {
    const double distance = runtime_water_material_3d_positive_or(absorption_distance, 4.0);
    const double coeff_r = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(absorption_r, 0.10),
        0.0,
        64.0);
    const double coeff_g = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(absorption_g, 0.035),
        0.0,
        64.0);
    const double coeff_b = runtime_water_material_3d_clamp(
        runtime_water_material_3d_nonnegative_or(absorption_b, 0.015),
        0.0,
        64.0);

    if (out_r) *out_r = runtime_water_material_3d_clamp(exp(-coeff_r * distance), 0.0, 1.0);
    if (out_g) *out_g = runtime_water_material_3d_clamp(exp(-coeff_g * distance), 0.0, 1.0);
    if (out_b) *out_b = runtime_water_material_3d_clamp(exp(-coeff_b * distance), 0.0, 1.0);
}

bool RuntimeWaterMaterial3D_ApplyToPayload(int scene_object_index,
                                           RuntimeMaterialPayload3D* payload) {
    RuntimeWaterMaterial3DOverride override = {0};
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    if (!payload || !payload->valid ||
        !RuntimeWaterMaterial3D_Get(scene_object_index, &override)) {
        return false;
    }

    RuntimeWaterMaterial3D_ComputeTransmittanceTint(override.absorptionDistance,
                                                    override.absorptionR,
                                                    override.absorptionG,
                                                    override.absorptionB,
                                                    &tint_r,
                                                    &tint_g,
                                                    &tint_b);
    /* Clear water is a dielectric interface plus a participating medium.
     * Reference-distance transmittance must never become an interface tint,
     * albedo, or an implicit opaque/diffuse mixture. */
    payload->hasGlassTransportOverride = true;
    payload->wetContactContainerIndexPlusOne = override.wetContactContainerIndexPlusOne;
    payload->hasGlassInterfaceTintOverride = true;
    payload->glassInterfaceTintR = payload->glassInterfaceTintG = payload->glassInterfaceTintB = 1.0;
    payload->hasGlassAbsorptionColorOverride = true;
    payload->glassAbsorptionColorR = tint_r;
    payload->glassAbsorptionColorG = tint_g;
    payload->glassAbsorptionColorB = tint_b;
    payload->baseColorR = payload->baseColorG = payload->baseColorB = 1.0;
    payload->transparency = 1.0; /* Fresnel still splits reflected/transmitted energy. */
    payload->opticalIor = override.ior;
    payload->absorptionDistance = override.absorptionDistance;
    payload->thinWalled = false;
    payload->bsdf.baseColorR = payload->bsdf.baseColorG = payload->bsdf.baseColorB = 1.0;
    payload->bsdf.albedo = 1.0;
    payload->bsdf.opacity = 0.0;
    payload->bsdf.ior = override.ior;
    const double ratio = (override.ior - 1.0) / (override.ior + 1.0);
    payload->bsdf.reflectivity = ratio * ratio;
    payload->bsdf.roughness = override.roughness;
    payload->bsdf.model = MATERIAL_BSDF_GGX;
    payload->bsdf.diffuseWeight = 0.0;
    payload->bsdf.specWeight = 1.0;
    payload->bsdf.weightSum = 1.0;
    return true;
}
