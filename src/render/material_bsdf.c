#include "render/material_bsdf.h"
#include "material/material_manager.h"

#include <float.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef INV_PI
#define INV_PI (1.0 / M_PI)
#endif

static double Clamp(double v, double minV, double maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

static double Clamp01(double v) {
    return Clamp(v, 0.0, 1.0);
}

static double ExtractBaseAlbedo(const SceneObject* obj) {
    double r = (double)((obj->color >> 16) & 0xFF) / 255.0;
    double g = (double)((obj->color >> 8) & 0xFF) / 255.0;
    double b = (double)(obj->color & 0xFF) / 255.0;
    double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    return Clamp01(luma);
}

static MaterialBSDFModel SelectModel(const SceneObject* obj) {
    double reflectivity = Clamp01(obj->reflectivity);
    return (reflectivity > 0.05) ? MATERIAL_BSDF_GGX : MATERIAL_BSDF_LAMBERT;
}

static void BuildTangent(double nx, double ny, double* tx, double* ty) {
    *tx = -ny;
    *ty = nx;
}

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static void CosineSampleHemisphere2D(double nx,
                                     double ny,
                                     double u1,
                                     double u2,
                                     double* dirX,
                                     double* dirY,
                                     double* pdf) {
    double tx, ty;
    BuildTangent(nx, ny, &tx, &ty);

    double cosTheta = sqrt(fmax(0.0, 1.0 - u1));
    double sinTheta = sqrt(fmax(0.0, 1.0 - cosTheta * cosTheta));
    double tangentSign = (u2 < 0.5) ? -1.0 : 1.0;

    *dirX = tx * (sinTheta * tangentSign) + nx * cosTheta;
    *dirY = ty * (sinTheta * tangentSign) + ny * cosTheta;
    Normalize(dirX, dirY);

    if (pdf) {
        *pdf = cosTheta * INV_PI;
    }
}

static double LambertEvaluate(const MaterialBSDF* material, double cosTheta) {
    if (!material) return 0.0;
    double clamped = fmax(0.0, cosTheta);
    return material->diffuseWeight * material->albedo * INV_PI * clamped;
}

static double LambertPdf(double cosTheta) {
    if (cosTheta <= 0.0) return 0.0;
    return cosTheta * INV_PI;
}

double FresnelSchlick(double cosTheta, double f0) {
    double clamped = Clamp01(cosTheta);
    double base = 1.0 - clamped;
    double base5 = base * base * base * base * base;
    return f0 + (1.0 - f0) * base5;
}

double GGXDistribution(double alpha, double cosThetaH) {
    double clamped = Clamp01(cosThetaH);
    double alpha2 = alpha * alpha;
    double denom = (clamped * clamped) * (alpha2 - 1.0) + 1.0;
    denom = M_PI * denom * denom;
    if (denom <= 1e-12) return 0.0;
    return alpha2 / denom;
}

double GGXSmithG1(double alpha, double cosThetaV) {
    double clamped = Clamp01(cosThetaV);
    double tanTheta = sqrt(fmax(1.0 - clamped * clamped, 0.0)) / fmax(clamped, 1e-6);
    double a = alpha * tanTheta;
    double lambda = (-1.0 + sqrt(1.0 + a * a)) * 0.5;
    return 1.0 / (1.0 + lambda);
}

double GGXSmithGeometry(double alpha, double cosThetaI, double cosThetaO) {
    double g1i = GGXSmithG1(alpha, cosThetaI);
    double g1o = GGXSmithG1(alpha, cosThetaO);
    return g1i * g1o;
}

static double GGXPdf(double alpha,
                     double cosThetaH,
                     double dotIH) {
    if (cosThetaH <= 0.0 || fabs(dotIH) <= 1e-8) return 0.0;
    double D = GGXDistribution(alpha, cosThetaH);
    return (D * cosThetaH) / fmax(2.0 * fabs(dotIH), 1e-8);
}

static double GGXEvaluate(const MaterialBSDF* material,
                          double cosThetaI,
                          double cosThetaO,
                          double cosThetaH) {
    if (!material) return 0.0;
    double alpha = fmax(material->roughness * material->roughness, 1e-3);
    double D = GGXDistribution(alpha, cosThetaH);
    double G = GGXSmithGeometry(alpha, cosThetaI, cosThetaO);
    double F = FresnelSchlick(fmax(0.0, cosThetaI), material->reflectivity);
    double denom = 4.0 * fmax(1e-4, cosThetaI) * fmax(1e-4, cosThetaO);
    return (D * G * F) / denom;
}

static bool BSDFSampleLambert(const MaterialBSDF* material,
                              double nx,
                              double ny,
                              double u1,
                              double u2,
                              BSDFSample* out) {
    if (!material || !out) return false;
    double dirX, dirY, pdf;
    CosineSampleHemisphere2D(nx, ny, u1, u2, &dirX, &dirY, &pdf);
    if (pdf <= 0.0) return false;
    double cosTheta = dirX * nx + dirY * ny;
    out->dirX = dirX;
    out->dirY = dirY;
    out->pdf = pdf;
    out->weight = LambertEvaluate(material, cosTheta);
    out->specular = false;
    return true;
}

static bool BSDFSampleGGX(const MaterialBSDF* material,
                          double nx,
                          double ny,
                          double inDirX,
                          double inDirY,
                          double u1,
                          double u2,
                          BSDFSample* out) {
    if (!material || !out) return false;
    double alpha = fmax(material->roughness * material->roughness, 1e-3);
    double tx, ty;
    BuildTangent(nx, ny, &tx, &ty);

    double u = Clamp(u1, 1e-6, 1.0 - 1e-6);
    double tan2Theta = (alpha * alpha * u) / fmax(1.0 - u, 1e-6);
    double cosThetaH = 1.0 / sqrt(1.0 + tan2Theta);
    double sinThetaH = sqrt(fmax(0.0, 1.0 - cosThetaH * cosThetaH));
    double tangentSign = (u2 < 0.5) ? -1.0 : 1.0;

    double hx = tx * (sinThetaH * tangentSign) + nx * cosThetaH;
    double hy = ty * (sinThetaH * tangentSign) + ny * cosThetaH;
    Normalize(&hx, &hy);

    double dotIH = inDirX * hx + inDirY * hy;
    if (dotIH <= 0.0) {
        return false;
    }

    double outX = 2.0 * dotIH * hx - inDirX;
    double outY = 2.0 * dotIH * hy - inDirY;
    Normalize(&outX, &outY);

    double cosThetaI = fmax(0.0, inDirX * nx + inDirY * ny);
    double cosThetaO = fmax(0.0, outX * nx + outY * ny);
    double cosThetaHalf = fmax(0.0, hx * nx + hy * ny);
    if (cosThetaI <= 0.0 || cosThetaO <= 0.0 || cosThetaHalf <= 0.0) {
        return false;
    }

    double brdf = GGXEvaluate(material, cosThetaI, cosThetaO, cosThetaHalf);
    double pdf = GGXPdf(alpha, cosThetaHalf, dotIH);

    out->dirX = outX;
    out->dirY = outY;
    out->pdf = pdf;
    out->weight = brdf * cosThetaO;
    out->specular = true;
    return true;
}

void MaterialBSDFInitFromSceneObject(const SceneObject* obj, MaterialBSDF* material) {
    if (!obj || !material) return;
    const Material* preset = MaterialManagerGet(obj->material_id);
    memset(material, 0, sizeof(*material));
    double objLuma = ExtractBaseAlbedo(obj);
    double presetLuma = 1.0;
    if (preset) {
        presetLuma = Clamp01(0.2126 * preset->base_color.x + 0.7152 * preset->base_color.y + 0.0722 * preset->base_color.z);
    }
    material->albedo = Clamp01(objLuma * presetLuma);
    material->opacity = Clamp01(obj->opacity);
    material->reflectivity = Clamp01(preset ? preset->reflectivity : obj->reflectivity);
    material->roughness = Clamp(preset ? preset->roughness : obj->roughness, 0.02, 1.0);
    material->ior = (material->reflectivity > 0.0) ? 1.45 : 1.0;
    material->textureId = obj->textureId;
    material->model = (material->reflectivity > 0.05) ? MATERIAL_BSDF_GGX : MATERIAL_BSDF_LAMBERT;
    material->specWeight = (preset ? preset->specular : 0.0) + material->reflectivity;
    material->diffuseWeight = preset ? preset->diffuse : Clamp01(1.0 - material->reflectivity);

    // Enforce energy conservation: diffuse + spec <= 1.0
    double total = material->diffuseWeight + material->specWeight;
    if (total > 1.0) {
        material->diffuseWeight /= total;
        material->specWeight /= total;
    }
    material->weightSum = material->diffuseWeight + material->specWeight;
    if (material->weightSum <= 1e-4) {
        material->diffuseWeight = 1.0;
        material->weightSum = 1.0;
    }
}

double MaterialBSDFDiffuseProbability(const MaterialBSDF* material) {
    if (!material || material->weightSum <= 0.0) return 1.0;
    return material->diffuseWeight / material->weightSum;
}

double MaterialBSDFSpecProbability(const MaterialBSDF* material) {
    if (!material || material->weightSum <= 0.0) return 0.0;
    return material->specWeight / material->weightSum;
}

bool MaterialBSDFSample(const MaterialBSDF* material,
                        double nx,
                        double ny,
                        double inDirX,
                        double inDirY,
                        FastRNG* rng,
                        BSDFSample* out) {
    if (!material || !rng || !out) return false;
    double diffuseProb = MaterialBSDFDiffuseProbability(material);
    double specProb = MaterialBSDFSpecProbability(material);
    double choice = FastRNGNextDouble(rng);
    bool pickSpec = (specProb > 0.0) && (choice > diffuseProb || diffuseProb <= 0.0);
    bool sampled = false;
    if (pickSpec) {
        sampled = BSDFSampleGGX(material, nx, ny, inDirX, inDirY, FastRNGNextDouble(rng), FastRNGNextDouble(rng), out);
        if (sampled) {
            double jitter = 0.1 * Clamp(material->roughness, 0.0, 1.0);
            double offsetX = (FastRNGNextDouble(rng) * 2.0 - 1.0) * jitter;
            double offsetY = (FastRNGNextDouble(rng) * 2.0 - 1.0) * jitter;
            out->dirX += offsetX;
            out->dirY += offsetY;
            Normalize(&out->dirX, &out->dirY);
            out->pdf *= specProb;
            return true;
        }
        // Fallback to diffuse if spec fails
    }
    sampled = BSDFSampleLambert(material, nx, ny, FastRNGNextDouble(rng), FastRNGNextDouble(rng), out);
    if (sampled) {
        out->pdf *= diffuseProb > 0.0 ? diffuseProb : 1.0;
    }
    return sampled;
}

double MaterialBSDFEvaluateCos(const MaterialBSDF* material,
                               double nx,
                               double ny,
                               double inDirX,
                               double inDirY,
                               double outDirX,
                               double outDirY) {
    if (!material) return 0.0;
    double cosThetaI = fmax(0.0, inDirX * nx + inDirY * ny);
    double cosThetaO = fmax(0.0, outDirX * nx + outDirY * ny);
    if (cosThetaI <= 0.0 || cosThetaO <= 0.0) return 0.0;
    double value = LambertEvaluate(material, cosThetaO);
    if (material->specWeight > 0.0) {
        double hx = inDirX + outDirX;
        double hy = inDirY + outDirY;
        Normalize(&hx, &hy);
        double cosThetaH = fmax(0.0, hx * nx + hy * ny);
        double brdf = GGXEvaluate(material, cosThetaI, cosThetaO, cosThetaH);
        value += brdf * cosThetaO;
    }
    return value;
}

double MaterialBSDFAngularPdf(const MaterialBSDF* material,
                              double nx,
                              double ny,
                              double inDirX,
                              double inDirY,
                              double outDirX,
                              double outDirY) {
    if (!material) return 0.0;
    double cosThetaO = fmax(0.0, outDirX * nx + outDirY * ny);
    if (cosThetaO <= 0.0) return 0.0;
    double pdf = 0.0;
    double diffuseProb = MaterialBSDFDiffuseProbability(material);
    double specProb = MaterialBSDFSpecProbability(material);
    if (diffuseProb > 0.0) {
        pdf += diffuseProb * LambertPdf(cosThetaO);
    }
    if (specProb > 0.0) {
        double hx = inDirX + outDirX;
        double hy = inDirY + outDirY;
        Normalize(&hx, &hy);
        double cosThetaH = fmax(0.0, hx * nx + hy * ny);
        double dotIH = fmax(0.0, inDirX * hx + inDirY * hy);
        if (cosThetaH > 0.0 && dotIH > 0.0) {
            double alpha = fmax(material->roughness * material->roughness, 1e-3);
            pdf += specProb * GGXPdf(alpha, cosThetaH, dotIH);
        }
    }
    return pdf;
}
