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

static double clamp_wood_grain_slope(double value) {
    const double maximum = 0.25;
    if (value < -maximum) return -maximum;
    if (value > maximum) return maximum;
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
    ProceduralSolidAuthoredMaterialSurfaceV1 wood_surface;
    ProceduralSolidMaterialRuntimeSampleV1 runtime_sample;
    ProceduralSolidMaterialGraphReport graph_report;
    ProceduralSolidMaterialWeightedTextureV1 fallback_texture;
    const ProceduralSolidMaterialWeightedTextureV1 *textures = NULL;
    size_t texture_count = 0u;
    RuntimeMaterialSurfaceEval base_eval;
    RuntimeMaterialSurfaceEval final_eval;
    ProceduralSurfaceFeatureCurveSampleV1 curve_feature = {0};
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
        curve_feature = runtime_sample.curve_feature;
        if (runtime_sample.wood_grain_valid) {
            wood_surface = *surface;
            wood_surface.base_color_r = runtime_sample.wood_grain.color[0];
            wood_surface.base_color_g = runtime_sample.wood_grain.color[1];
            wood_surface.base_color_b = runtime_sample.wood_grain.color[2];
            wood_surface.roughness = clamp01(surface->roughness +
                runtime_sample.wood_grain.roughness_delta);
            surface = &wood_surface;
        }
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
    if (curve_feature.coverage > 0.0) {
        /* The gradient is evaluated in object space.  Convert it through the
         * same interpolated frame that ResolveShadingNormal will use below;
         * mixing a per-triangle geometric frame here with the shading frame
         * there makes a continuous grain field break into triangle dots. */
        Vec3 normal = hit->shadingNormal;
        Vec3 helper;
        Vec3 tangent;
        Vec3 bitangent;
        Vec3 direction = vec3(curve_feature.direction.x, curve_feature.direction.y,
                              curve_feature.direction.z);
        double scale = fmin(0.35, fabs(curve_feature.depth_slope));
        if (vec3_length(normal) <= 1e-9) normal = hit->normal;
        if (vec3_length(normal) > 1e-9 && vec3_length(direction) > 1e-9 && scale > 0.0) {
            normal = vec3_normalize(normal);
            helper = fabs(normal.z) < 0.999 ? vec3(0.0,0.0,1.0) : vec3(0.0,1.0,0.0);
            tangent = vec3_normalize(vec3_cross(helper, normal));
            bitangent = vec3_normalize(vec3_cross(normal, tangent));
            payload->hasMicrodetailNormal = true;
            payload->microdetailHeight = scale;
            direction = vec3_normalize(vec3_cross(normal, direction));
            if (curve_feature.depth_slope < 0.0)
                direction = vec3_scale(direction, -1.0);
            payload->microdetailSlopeU = scale * vec3_dot(direction, tangent);
            payload->microdetailSlopeV = scale * vec3_dot(direction, bitangent);
        }
    }
    if (runtime_sample.wood_grain_valid) {
        Vec3 normal = hit->geometricNormal;
        Vec3 helper;
        Vec3 tangent;
        Vec3 bitangent;
        Vec3 gradient = vec3(runtime_sample.wood_grain.slope_x, 0.0,
                             runtime_sample.wood_grain.slope_z);
        if (vec3_length(normal) <= 1e-9) normal = hit->normal;
        if (vec3_length(normal) <= 1e-9) return false;
        normal = vec3_normalize(normal);
        helper = fabs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0)
                                        : vec3(0.0, 1.0, 0.0);
        tangent = vec3_normalize(vec3_cross(helper, normal));
        bitangent = vec3_normalize(vec3_cross(normal, tangent));
        payload->hasMicrodetailNormal = true;
        payload->microdetailHeight = 0.0;
        /* Grain is a shading-normal microdetail, not a physical groove.  The
         * bounded tangent slopes keep high-frequency fields readable under a
         * grazing key light instead of producing channel-like banding. */
        payload->microdetailSlopeU = clamp_wood_grain_slope(
            vec3_dot(gradient, tangent));
        payload->microdetailSlopeV = clamp_wood_grain_slope(
            vec3_dot(gradient, bitangent));
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
