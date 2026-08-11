#include "procedural/procedural_surface_material_runtime_adapter.h"

#include <math.h>

static bool unit_value(double value) {
    return isfinite(value) && value >= 0.0 && value <= 1.0;
}

static bool sample_valid(const ProceduralSurfaceMaterialSample *sample) {
    return sample &&
           unit_value(sample->final_color_r) &&
           unit_value(sample->final_color_g) &&
           unit_value(sample->final_color_b) &&
           unit_value(sample->final_roughness) &&
           unit_value(sample->snow_likelihood);
}

bool ProceduralSurfaceMaterial_ToSurfaceEval(
    const ProceduralSurfaceMaterialSample *sample,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval) {
    RuntimeMaterialSurfaceEval eval;
    if (!sample_valid(sample) || !base_eval || !out_eval || !base_eval->active) {
        return false;
    }
    eval = *base_eval;
    eval.active = true;
    eval.colorR = sample->final_color_r;
    eval.colorG = sample->final_color_g;
    eval.colorB = sample->final_color_b;
    eval.roughness = sample->final_roughness;
    eval.reflectivity = 0.035 + (0.025 * (1.0 - sample->snow_likelihood));
    eval.specWeight = 0.10 + (0.08 * sample->snow_likelihood);
    eval.diffuseWeight = 0.90 - (0.08 * sample->snow_likelihood);
    eval.transparency = 0.0;
    eval.textureMask = 1.0;
    eval.textureU = 0.0;
    eval.textureV = 0.0;
    if (fabs(eval.roughness - base_eval->roughness) <= 1e-9 &&
        fabs(eval.colorR - base_eval->colorR) <= 1e-9 &&
        fabs(eval.colorG - base_eval->colorG) <= 1e-9 &&
        fabs(eval.colorB - base_eval->colorB) <= 1e-9) {
        return false;
    }
    *out_eval = eval;
    return true;
}
