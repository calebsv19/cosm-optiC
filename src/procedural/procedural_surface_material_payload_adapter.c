#include "procedural/procedural_surface_material_runtime_adapter.h"

bool ProceduralSurfaceMaterial_ApplyToPayload(
    const ProceduralSurfaceMaterialSample *sample,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialPayload3D *payload) {
    RuntimeMaterialSurfaceEval eval;
    if (!ProceduralSurfaceMaterial_ToSurfaceEval(sample, base_eval, &eval)) {
        return false;
    }
    return RuntimeMaterialPayload3D_ApplySurfaceEval(&eval, payload);
}

bool ProceduralSurfaceMaterial_ApplyHitToPayload(
    const HitInfo3D *hit,
    RuntimeMaterialPayload3D *payload) {
    ProceduralSurfaceMaterialSample sample = {0};
    RuntimeMaterialSurfaceEval base_eval;
    if (!hit || !payload || !payload->valid ||
        !hit->hasProceduralSurfaceMaterial) {
        return false;
    }
    sample.final_color_r = hit->proceduralSurfaceMaterial.colorR;
    sample.final_color_g = hit->proceduralSurfaceMaterial.colorG;
    sample.final_color_b = hit->proceduralSurfaceMaterial.colorB;
    sample.final_roughness = hit->proceduralSurfaceMaterial.roughness;
    sample.snow_likelihood =
        hit->proceduralSurfaceMaterial.snowLikelihood;
    base_eval.colorR = payload->baseColorR;
    base_eval.colorG = payload->baseColorG;
    base_eval.colorB = payload->baseColorB;
    base_eval.roughness = payload->bsdf.roughness;
    base_eval.reflectivity = payload->bsdf.reflectivity;
    base_eval.specWeight = payload->bsdf.specWeight;
    base_eval.diffuseWeight = payload->bsdf.diffuseWeight;
    base_eval.transparency = payload->transparency;
    base_eval.active = true;
    return ProceduralSurfaceMaterial_ApplyToPayload(
        &sample, &base_eval, payload);
}
