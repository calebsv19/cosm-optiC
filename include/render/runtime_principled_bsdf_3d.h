#ifndef RENDER_RUNTIME_PRINCIPLED_BSDF_3D_H
#define RENDER_RUNTIME_PRINCIPLED_BSDF_3D_H

#include <stdbool.h>

#include "render/material_bsdf.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

typedef struct RuntimePrincipledBSDF3D {
    bool valid;
    double baseColorR;
    double baseColorG;
    double baseColorB;
    double metallic;
    double roughness;
    double specularWeight;
    double diffuseWeight;
    double reflectivity;
    double ior;
    double dielectricF0;
    double specularF0R;
    double specularF0G;
    double specularF0B;
    double opacity;
    double transmissionWeight;
    double emissiveR;
    double emissiveG;
    double emissiveB;
    double emissiveStrength;
} RuntimePrincipledBSDF3D;

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_Default(void);
RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_Normalize(
    RuntimePrincipledBSDF3D bsdf);

double RuntimePrincipledBSDF3D_DielectricF0FromIor(double ior);
double RuntimePrincipledBSDF3D_FresnelSchlick(double cos_theta, double f0);

RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromMaterialBSDF(
    const MaterialBSDF* material);
RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromMaterialPayload(
    const RuntimeMaterialPayload3D* payload);
RuntimePrincipledBSDF3D RuntimePrincipledBSDF3D_FromSurfaceEval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    double ior,
    double emissive_strength);
MaterialBSDF RuntimePrincipledBSDF3D_ToMaterialBSDF(
    const RuntimePrincipledBSDF3D* principled);

double RuntimePrincipledBSDF3D_DiffuseProbability(
    const RuntimePrincipledBSDF3D* bsdf);
double RuntimePrincipledBSDF3D_SpecularProbability(
    const RuntimePrincipledBSDF3D* bsdf);

double RuntimePrincipledBSDF3D_EvaluateDiffuseCos(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_o);
double RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_i,
    double cos_theta_o,
    double cos_theta_h);
double RuntimePrincipledBSDF3D_GGXHalfVectorPdf(
    const RuntimePrincipledBSDF3D* bsdf,
    double cos_theta_h,
    double dot_i_h);

#endif
