#include "render/runtime_material_payload_3d.h"

#include <math.h>

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

bool RuntimeMaterialPayload3D_ApplySurfaceEval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    RuntimeMaterialPayload3D* payload) {
    double weight_sum;
    if (!surface_eval || !payload || !payload->valid || !surface_eval->active) {
        return false;
    }
    payload->textureMask = clamp01(surface_eval->textureMask);
    payload->textureU = surface_eval->textureU;
    payload->textureV = surface_eval->textureV;
    payload->hasMicrodetailNormal = surface_eval->microdetailNormalActive;
    payload->microdetailHeight = surface_eval->microdetailHeight;
    payload->microdetailSlopeU = surface_eval->microdetailSlopeU;
    payload->microdetailSlopeV = surface_eval->microdetailSlopeV;
    payload->baseColorR = clamp01(surface_eval->colorR);
    payload->baseColorG = clamp01(surface_eval->colorG);
    payload->baseColorB = clamp01(surface_eval->colorB);
    payload->bsdf.baseColorR = payload->baseColorR;
    payload->bsdf.baseColorG = payload->baseColorG;
    payload->bsdf.baseColorB = payload->baseColorB;
    payload->bsdf.albedo = clamp01(
        (0.2126 * payload->baseColorR) +
        (0.7152 * payload->baseColorG) +
        (0.0722 * payload->baseColorB));
    payload->bsdf.reflectivity = clamp01(surface_eval->reflectivity);
    payload->bsdf.roughness = clamp01(surface_eval->roughness);
    if (payload->bsdf.roughness < 0.02) payload->bsdf.roughness = 0.02;
    payload->bsdf.specWeight = clamp01(surface_eval->specWeight);
    payload->bsdf.diffuseWeight = clamp01(surface_eval->diffuseWeight);
    weight_sum = payload->bsdf.specWeight + payload->bsdf.diffuseWeight;
    if (weight_sum > 1.0) {
        payload->bsdf.specWeight /= weight_sum;
        payload->bsdf.diffuseWeight /= weight_sum;
    }
    payload->bsdf.weightSum =
        payload->bsdf.specWeight + payload->bsdf.diffuseWeight;
    if (payload->bsdf.weightSum <= 1e-4) {
        payload->bsdf.diffuseWeight = 1.0;
        payload->bsdf.weightSum = 1.0;
    }
    payload->transparency = clamp01(surface_eval->transparency);
    return true;
}

bool RuntimeMaterialPayload3D_ResolveShadingNormal(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* io_payload) {
    Vec3 normal;
    Vec3 helper;
    Vec3 tangent;
    Vec3 bitangent;
    Vec3 perturbed;
    if (!hit || !io_payload || !io_payload->valid ||
        !io_payload->hasMicrodetailNormal ||
        !isfinite(io_payload->microdetailSlopeU) ||
        !isfinite(io_payload->microdetailSlopeV)) {
        return false;
    }
    normal = hit->shadingNormal;
    if (!(vec3_length(normal) > 1e-9)) normal = hit->normal;
    if (!(vec3_length(normal) > 1e-9)) normal = hit->geometricNormal;
    if (!(vec3_length(normal) > 1e-9)) return false;
    normal = vec3_normalize(normal);
    helper = fabs(normal.z) < 0.999
                 ? vec3(0.0, 0.0, 1.0)
                 : vec3(0.0, 1.0, 0.0);
    tangent = vec3_normalize(vec3_cross(helper, normal));
    bitangent = vec3_normalize(vec3_cross(normal, tangent));
    perturbed = vec3_sub(
        normal,
        vec3_add(vec3_scale(tangent, io_payload->microdetailSlopeU),
                 vec3_scale(bitangent, io_payload->microdetailSlopeV)));
    if (!(vec3_length(perturbed) > 1e-9)) return false;
    perturbed = vec3_normalize(perturbed);
    if (vec3_dot(perturbed, hit->geometricNormal) <= 1e-6) {
        perturbed = normal;
    }
    io_payload->microdetailShadingNormal = perturbed;
    return true;
}

bool RuntimeMaterialPayload3D_ApplyShadingNormal(
    const RuntimeMaterialPayload3D* payload,
    HitInfo3D* io_hit) {
    if (!payload || !io_hit || !payload->valid ||
        !payload->hasMicrodetailNormal ||
        !(vec3_length(payload->microdetailShadingNormal) > 1e-9)) {
        return false;
    }
    io_hit->shadingNormal = payload->microdetailShadingNormal;
    io_hit->normal = payload->microdetailShadingNormal;
    return true;
}
