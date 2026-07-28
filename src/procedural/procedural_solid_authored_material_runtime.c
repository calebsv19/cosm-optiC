#include "procedural/procedural_solid_authored_material_runtime.h"
#include "procedural/procedural_solid_material_runtime_program.h"
#include "procedural/procedural_solid_material_texture_runtime.h"

#include <math.h>
#include <string.h>

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool resolve_uv(const HitInfo3D *hit, double *out_u, double *out_v) {
    double ax;
    double ay;
    double az;
    if (!hit || !out_u || !out_v) return false;
    if (!hit->hasObjectTextureCoord) {
        *out_u = hit->baryV;
        *out_v = hit->baryW;
        return true;
    }
    ax = fabs(hit->normal.x);
    ay = fabs(hit->normal.y);
    az = fabs(hit->normal.z);
    if (az >= ax && az >= ay) {
        *out_u = hit->objectTextureCoord.x;
        *out_v = hit->objectTextureCoord.y;
    } else if (ay >= ax) {
        *out_u = hit->objectTextureCoord.x;
        *out_v = hit->objectTextureCoord.z;
    } else {
        *out_u = hit->objectTextureCoord.y;
        *out_v = hit->objectTextureCoord.z;
    }
    return true;
}

bool ProceduralSolidAuthoredMaterial_ApplyHitToPayload(
    const SceneObject *object,
    const HitInfo3D *hit,
    RuntimeMaterialPayload3D *payload) {
    const ProceduralSolidAuthoredMaterialSurfaceV1 *surface;
    ProceduralSolidMaterialRuntimeSampleV1 runtime_sample;
    ProceduralSolidMaterialGraphReport graph_report;
    ProceduralSolidMaterialWeightedTextureV1 fallback_texture;
    const ProceduralSolidMaterialWeightedTextureV1 *textures = NULL;
    size_t texture_count = 0u;
    RuntimeMaterialSurfaceEval base_eval;
    RuntimeMaterialSurfaceEval final_eval;
    double u = 0.0;
    double v = 0.0;
    double metallic;
    if (!object || !hit || !payload || !payload->valid ||
        !hit->hasRegionAuthoredMaterial) {
        return false;
    }
    surface = &hit->regionAuthoredMaterial;
    if (hit->proceduralSolidMaterialRuntimeProgram &&
        ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
            hit->proceduralSolidMaterialRuntimeProgram,
            (size_t)hit->localTriangleIndex,
            hit->baryU, hit->baryV, hit->baryW,
            &runtime_sample, &graph_report)) {
        surface = &runtime_sample.surface;
        textures = runtime_sample.textures;
        texture_count = runtime_sample.texture_count;
    } else if (surface->texture.enabled) {
        memset(&fallback_texture, 0, sizeof(fallback_texture));
        fallback_texture.texture = surface->texture;
        fallback_texture.weight = 1.0;
        textures = &fallback_texture;
        texture_count = 1u;
    }
    metallic = clamp01(surface->metallic);
    base_eval = RuntimeMaterialSurfaceEvalMakeBase(
        surface->base_color_r,
        surface->base_color_g,
        surface->base_color_b,
        surface->roughness,
        fmax(surface->reflectivity, metallic * 0.85),
        fmax(surface->specular, metallic),
        1.0 - metallic,
        surface->transparency);
    base_eval.active = true;
    final_eval = base_eval;

    if (texture_count > 0u) {
        if (!resolve_uv(hit, &u, &v) ||
            !ProceduralSolidMaterialWeightedTextures_EvaluateMicrodetailPlacedUV(
                textures, texture_count, object, u, v,
                hit->triangleIndex + 1, &base_eval, &final_eval)) {
            return false;
        }
    }
    if (!RuntimeMaterialPayload3D_ApplySurfaceEval(&final_eval, payload)) {
        return false;
    }
    (void)RuntimeMaterialPayload3D_ResolveShadingNormal(hit, payload);
    payload->emissive = fmax(
        0.0,
        surface->emission_strength *
            ((0.2126 * surface->emission_color_r) +
             (0.7152 * surface->emission_color_g) +
             (0.0722 * surface->emission_color_b)));
    payload->bsdf.emissive = payload->emissive;
    if (surface->ior > 1e-6) payload->opticalIor = surface->ior;
    return true;
}
