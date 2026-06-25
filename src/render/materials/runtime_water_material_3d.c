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

static double runtime_water_material_3d_luma(double r, double g, double b) {
    return runtime_water_material_3d_clamp((0.2126 * r) + (0.7152 * g) + (0.0722 * b),
                                          0.0,
                                          1.0);
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
    normalized.transparency = runtime_water_material_3d_clamp(
        runtime_water_material_3d_positive_or(override->transparency, 0.92),
        0.0,
        1.0);
    normalized.reflectivity = runtime_water_material_3d_clamp(override->reflectivity, 0.0, 1.0);
    normalized.roughness = runtime_water_material_3d_clamp(
        runtime_water_material_3d_positive_or(override->roughness, 0.02),
        0.02,
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
    double tint_luma = 0.0;
    double spec_weight = 0.0;
    double diffuse_weight = 0.0;
    double weight_sum = 0.0;
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
    tint_luma = runtime_water_material_3d_luma(tint_r, tint_g, tint_b);

    payload->baseColorR = tint_r;
    payload->baseColorG = tint_g;
    payload->baseColorB = tint_b;
    payload->transparency = override.transparency;
    payload->opticalIor = override.ior;
    payload->absorptionDistance = override.absorptionDistance;
    payload->thinWalled = false;
    payload->bsdf.baseColorR = tint_r;
    payload->bsdf.baseColorG = tint_g;
    payload->bsdf.baseColorB = tint_b;
    payload->bsdf.albedo = tint_luma;
    payload->bsdf.ior = override.ior;
    payload->bsdf.reflectivity = override.reflectivity;
    payload->bsdf.roughness = override.roughness;
    payload->bsdf.model = override.reflectivity > 0.05 ? MATERIAL_BSDF_GGX
                                                       : MATERIAL_BSDF_LAMBERT;
    spec_weight = runtime_water_material_3d_clamp(
        fmax(payload->bsdf.specWeight, override.reflectivity),
        0.0,
        1.0);
    diffuse_weight = runtime_water_material_3d_clamp(
        fmin(payload->bsdf.diffuseWeight, 1.0 - spec_weight),
        0.0,
        1.0);
    weight_sum = diffuse_weight + spec_weight;
    if (weight_sum <= 1e-4) {
        diffuse_weight = 1.0;
        weight_sum = 1.0;
    }
    payload->bsdf.diffuseWeight = diffuse_weight;
    payload->bsdf.specWeight = spec_weight;
    payload->bsdf.weightSum = weight_sum;
    return true;
}
