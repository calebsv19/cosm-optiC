#include "render/runtime_principled_bsdf_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef INV_PI
#define INV_PI (1.0 / M_PI)
#endif

static double runtime_principled_bsdf_3d_clamp(double value,
                                               double min_value,
                                               double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_principled_bsdf_3d_clamp01(double value) {
    return runtime_principled_bsdf_3d_clamp(value, 0.0, 1.0);
}

static double runtime_principled_bsdf_3d_lerp(double a, double b, double t) {
    t = runtime_principled_bsdf_3d_clamp01(t);
    return a + ((b - a) * t);
}

static double runtime_principled_bsdf_3d_luma(double r, double g, double b) {
    return runtime_principled_bsdf_3d_clamp01((0.2126 * r) + (0.7152 * g) + (0.0722 * b));
}

static void runtime_principled_bsdf_3d_apply_legacy_transparency_bridge(
    RuntimePrincipledBSDF3D* bsdf,
    double transparency) {
    double transmission_weight = runtime_principled_bsdf_3d_clamp01(transparency);
    if (!bsdf) return;
    bsdf->opacity = 1.0 - transmission_weight;
    bsdf->transmissionWeight = transmission_weight;
}

static void runtime_principled_bsdf_3d_apply_legacy_reflectivity_f0_floor(
    RuntimePrincipledBSDF3D* bsdf) {
    if (!bsdf) return;
    if (bsdf->reflectivity > bsdf->dielectricF0) {
        bsdf->dielectricF0 = bsdf->reflectivity;
    }
}

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_Default(void) {
    RuntimePrincipledBSDF3D bsdf;
    memset(&bsdf, 0, sizeof(bsdf));
    bsdf.valid = true;
    bsdf.baseColorR = 1.0;
    bsdf.baseColorG = 1.0;
    bsdf.baseColorB = 1.0;
    bsdf.roughness = 0.5;
    bsdf.specularWeight = 0.5;
    bsdf.diffuseWeight = 1.0;
    bsdf.ior = 1.5;
    bsdf.opacity = 1.0;
    return RuntimePrincipledBSDF3D_Normalize(bsdf);
}

double RuntimePrincipledBSDF3D_DielectricF0FromIor(double ior) {
    double eta = runtime_principled_bsdf_3d_clamp(ior, 1.0, 4.0);
    double ratio = (eta - 1.0) / (eta + 1.0);
    return runtime_principled_bsdf_3d_clamp01(ratio * ratio);
}

double RuntimePrincipledBSDF3D_FresnelSchlick(double cos_theta, double f0) {
    double clamped = runtime_principled_bsdf_3d_clamp01(cos_theta);
    double base = 1.0 - clamped;
    double base5 = base * base * base * base * base;
    return runtime_principled_bsdf_3d_clamp01(f0 + ((1.0 - f0) * base5));
}

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_Normalize(
    RuntimePrincipledBSDF3D bsdf) {
    double weight_sum = 0.0;
    double dielectric_f0 = 0.0;
    if (!bsdf.valid) {
        bsdf = RuntimePrincipledBSDF3D_Default();
    }
    bsdf.baseColorR = runtime_principled_bsdf_3d_clamp01(bsdf.baseColorR);
    bsdf.baseColorG = runtime_principled_bsdf_3d_clamp01(bsdf.baseColorG);
    bsdf.baseColorB = runtime_principled_bsdf_3d_clamp01(bsdf.baseColorB);
    bsdf.metallic = runtime_principled_bsdf_3d_clamp01(bsdf.metallic);
    bsdf.roughness = runtime_principled_bsdf_3d_clamp(bsdf.roughness, 0.02, 1.0);
    bsdf.specularWeight = runtime_principled_bsdf_3d_clamp01(bsdf.specularWeight);
    bsdf.diffuseWeight = runtime_principled_bsdf_3d_clamp01(bsdf.diffuseWeight);
    bsdf.reflectivity = runtime_principled_bsdf_3d_clamp01(bsdf.reflectivity);
    bsdf.ior = runtime_principled_bsdf_3d_clamp(bsdf.ior, 1.0, 4.0);
    bsdf.opacity = runtime_principled_bsdf_3d_clamp01(bsdf.opacity);
    bsdf.transmissionWeight = runtime_principled_bsdf_3d_clamp01(bsdf.transmissionWeight);
    bsdf.emissiveR = runtime_principled_bsdf_3d_clamp01(bsdf.emissiveR);
    bsdf.emissiveG = runtime_principled_bsdf_3d_clamp01(bsdf.emissiveG);
    bsdf.emissiveB = runtime_principled_bsdf_3d_clamp01(bsdf.emissiveB);
    bsdf.emissiveStrength = runtime_principled_bsdf_3d_clamp01(bsdf.emissiveStrength);

    dielectric_f0 = RuntimePrincipledBSDF3D_DielectricF0FromIor(bsdf.ior);
    if (bsdf.dielectricF0 <= 0.0) {
        bsdf.dielectricF0 = dielectric_f0;
    }
    bsdf.dielectricF0 = runtime_principled_bsdf_3d_clamp01(bsdf.dielectricF0);
    runtime_principled_bsdf_3d_apply_legacy_reflectivity_f0_floor(&bsdf);

    bsdf.diffuseWeight *= (1.0 - bsdf.metallic) * (1.0 - bsdf.transmissionWeight);
    weight_sum = bsdf.diffuseWeight + bsdf.specularWeight;
    if (weight_sum > 1.0) {
        bsdf.diffuseWeight /= weight_sum;
        bsdf.specularWeight /= weight_sum;
    }
    if (bsdf.diffuseWeight + bsdf.specularWeight <= 1e-6) {
        bsdf.diffuseWeight = (bsdf.transmissionWeight < 1.0) ? 1.0 : 0.0;
    }

    bsdf.specularF0R = runtime_principled_bsdf_3d_lerp(bsdf.dielectricF0,
                                                       bsdf.baseColorR,
                                                       bsdf.metallic) *
                       bsdf.specularWeight;
    bsdf.specularF0G = runtime_principled_bsdf_3d_lerp(bsdf.dielectricF0,
                                                       bsdf.baseColorG,
                                                       bsdf.metallic) *
                       bsdf.specularWeight;
    bsdf.specularF0B = runtime_principled_bsdf_3d_lerp(bsdf.dielectricF0,
                                                       bsdf.baseColorB,
                                                       bsdf.metallic) *
                       bsdf.specularWeight;
    bsdf.specularF0R = runtime_principled_bsdf_3d_clamp01(bsdf.specularF0R);
    bsdf.specularF0G = runtime_principled_bsdf_3d_clamp01(bsdf.specularF0G);
    bsdf.specularF0B = runtime_principled_bsdf_3d_clamp01(bsdf.specularF0B);
    bsdf.valid = true;
    return bsdf;
}

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromMaterialBSDF(
    const MaterialBSDF* material) {
    RuntimePrincipledBSDF3D bsdf = RuntimePrincipledBSDF3D_Default();
    if (!material) return bsdf;
    bsdf.baseColorR = material->baseColorR;
    bsdf.baseColorG = material->baseColorG;
    bsdf.baseColorB = material->baseColorB;
    bsdf.roughness = material->roughness;
    bsdf.specularWeight = material->specWeight;
    bsdf.diffuseWeight = material->diffuseWeight;
    bsdf.reflectivity = material->reflectivity;
    bsdf.ior = material->ior;
    bsdf.opacity = material->opacity;
    bsdf.emissiveR = material->baseColorR;
    bsdf.emissiveG = material->baseColorG;
    bsdf.emissiveB = material->baseColorB;
    bsdf.emissiveStrength = material->emissive;
    return RuntimePrincipledBSDF3D_Normalize(bsdf);
}

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromMaterialPayload(
    const RuntimeMaterialPayload3D* payload) {
    RuntimePrincipledBSDF3D bsdf;
    if (!payload || !payload->valid) {
        return RuntimePrincipledBSDF3D_Default();
    }
    bsdf = RuntimePrincipledBSDF3D_FromMaterialBSDF(&payload->bsdf);
    bsdf.baseColorR = payload->baseColorR;
    bsdf.baseColorG = payload->baseColorG;
    bsdf.baseColorB = payload->baseColorB;
    bsdf.ior = payload->opticalIor > 0.0 ? payload->opticalIor : bsdf.ior;
    bsdf.metallic = 0.0;
    bsdf.dielectricF0 = 0.0;
    runtime_principled_bsdf_3d_apply_legacy_transparency_bridge(&bsdf, payload->transparency);
    bsdf.emissiveR = payload->baseColorR;
    bsdf.emissiveG = payload->baseColorG;
    bsdf.emissiveB = payload->baseColorB;
    bsdf.emissiveStrength = payload->emissive;
    return RuntimePrincipledBSDF3D_Normalize(bsdf);
}

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromSurfaceEval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    double ior,
    double emissive_strength) {
    RuntimePrincipledBSDF3D bsdf = RuntimePrincipledBSDF3D_Default();
    if (!surface_eval) return bsdf;
    bsdf.baseColorR = surface_eval->colorR;
    bsdf.baseColorG = surface_eval->colorG;
    bsdf.baseColorB = surface_eval->colorB;
    bsdf.roughness = surface_eval->roughness;
    bsdf.reflectivity = surface_eval->reflectivity;
    bsdf.specularWeight = surface_eval->specWeight;
    bsdf.diffuseWeight = surface_eval->diffuseWeight;
    bsdf.ior = ior;
    bsdf.metallic = 0.0;
    bsdf.dielectricF0 = 0.0;
    runtime_principled_bsdf_3d_apply_legacy_transparency_bridge(&bsdf,
                                                                surface_eval->transparency);
    bsdf.emissiveR = surface_eval->colorR;
    bsdf.emissiveG = surface_eval->colorG;
    bsdf.emissiveB = surface_eval->colorB;
    bsdf.emissiveStrength = emissive_strength;
    return RuntimePrincipledBSDF3D_Normalize(bsdf);
}

MaterialBSDF RuntimePrincipledBSDF3D_ToMaterialBSDF(
    const RuntimePrincipledBSDF3D* principled) {
    RuntimePrincipledBSDF3D normalized =
        principled ? RuntimePrincipledBSDF3D_Normalize(*principled)
                   : RuntimePrincipledBSDF3D_Default();
    MaterialBSDF material;
    memset(&material, 0, sizeof(material));
    material.baseColorR = normalized.baseColorR;
    material.baseColorG = normalized.baseColorG;
    material.baseColorB = normalized.baseColorB;
    material.albedo = runtime_principled_bsdf_3d_luma(normalized.baseColorR,
                                                      normalized.baseColorG,
                                                      normalized.baseColorB);
    material.opacity = normalized.opacity;
    material.reflectivity = runtime_principled_bsdf_3d_luma(normalized.specularF0R,
                                                            normalized.specularF0G,
                                                            normalized.specularF0B);
    material.roughness = normalized.roughness;
    material.ior = normalized.ior;
    material.model = normalized.specularWeight > 0.05 ? MATERIAL_BSDF_GGX
                                                      : MATERIAL_BSDF_LAMBERT;
    material.diffuseWeight = normalized.diffuseWeight;
    material.specWeight = normalized.specularWeight;
    material.weightSum = material.diffuseWeight + material.specWeight;
    if (material.weightSum <= 1e-6) {
        material.diffuseWeight = 1.0;
        material.weightSum = 1.0;
    }
    material.emissive = normalized.emissiveStrength;
    return material;
}

double RuntimePrincipledBSDF3D_DiffuseProbability(
    const RuntimePrincipledBSDF3D* bsdf) {
    double total = 0.0;
    if (!bsdf) return 1.0;
    total = bsdf->diffuseWeight + bsdf->specularWeight;
    if (total <= 1e-9) return 1.0;
    return runtime_principled_bsdf_3d_clamp01(bsdf->diffuseWeight / total);
}

double RuntimePrincipledBSDF3D_SpecularProbability(
    const RuntimePrincipledBSDF3D* bsdf) {
    double total = 0.0;
    if (!bsdf) return 0.0;
    total = bsdf->diffuseWeight + bsdf->specularWeight;
    if (total <= 1e-9) return 0.0;
    return runtime_principled_bsdf_3d_clamp01(bsdf->specularWeight / total);
}

double RuntimePrincipledBSDF3D_EvaluateDiffuseCos(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_o) {
    double color_luma = 0.0;
    if (!bsdf || cos_theta_o <= 0.0) return 0.0;
    color_luma = runtime_principled_bsdf_3d_luma(bsdf->baseColorR,
                                                 bsdf->baseColorG,
                                                 bsdf->baseColorB);
    return bsdf->diffuseWeight * color_luma * INV_PI * cos_theta_o;
}

static double runtime_principled_bsdf_3d_ggx_distribution(double alpha,
                                                          double cos_theta_h) {
    double clamped = runtime_principled_bsdf_3d_clamp01(cos_theta_h);
    double alpha2 = alpha * alpha;
    double denom = (clamped * clamped * (alpha2 - 1.0)) + 1.0;
    denom = M_PI * denom * denom;
    if (denom <= 1e-12) return 0.0;
    return alpha2 / denom;
}

static double runtime_principled_bsdf_3d_ggx_smith_g1(double alpha,
                                                      double cos_theta_v) {
    double clamped = runtime_principled_bsdf_3d_clamp(cos_theta_v, 1e-6, 1.0);
    double tan_theta =
        sqrt(fmax(1.0 - (clamped * clamped), 0.0)) / fmax(clamped, 1e-6);
    double a = alpha * tan_theta;
    double lambda = (-1.0 + sqrt(1.0 + (a * a))) * 0.5;
    return 1.0 / (1.0 + lambda);
}

double RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_i,
    double cos_theta_o,
    double cos_theta_h) {
    double alpha = 0.0;
    double d = 0.0;
    double g = 0.0;
    double f = 0.0;
    double denom = 0.0;
    if (!bsdf || cos_theta_i <= 0.0 || cos_theta_o <= 0.0 || cos_theta_h <= 0.0) {
        return 0.0;
    }
    alpha = fmax(bsdf->roughness * bsdf->roughness, 1e-3);
    d = runtime_principled_bsdf_3d_ggx_distribution(alpha, cos_theta_h);
    g = runtime_principled_bsdf_3d_ggx_smith_g1(alpha, cos_theta_i) *
        runtime_principled_bsdf_3d_ggx_smith_g1(alpha, cos_theta_o);
    f = RuntimePrincipledBSDF3D_FresnelSchlick(
        cos_theta_i,
        runtime_principled_bsdf_3d_luma(bsdf->specularF0R,
                                        bsdf->specularF0G,
                                        bsdf->specularF0B));
    denom = 4.0 * fmax(cos_theta_i, 1e-4) * fmax(cos_theta_o, 1e-4);
    return ((d * g * f) / denom) * cos_theta_o;
}

double RuntimePrincipledBSDF3D_GGXHalfVectorPdf(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_h,
    double dot_i_h) {
    double alpha = 0.0;
    double d = 0.0;
    if (!bsdf || cos_theta_h <= 0.0 || fabs(dot_i_h) <= 1e-8) return 0.0;
    alpha = fmax(bsdf->roughness * bsdf->roughness, 1e-3);
    d = runtime_principled_bsdf_3d_ggx_distribution(alpha, cos_theta_h);
    return (d * cos_theta_h) / fmax(2.0 * fabs(dot_i_h), 1e-8);
}
