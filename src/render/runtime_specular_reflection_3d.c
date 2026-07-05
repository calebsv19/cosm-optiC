#include "render/runtime_specular_reflection_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

static const double kRuntimeSpecularReflection3DEpsilon = 1e-4;
static const double kRuntimeSpecularReflection3DMaxDistance = 32.0;
static const double kRuntimeSpecularReflection3DMinWeight = 1e-4;

static double runtime_specular_reflection_3d_clamp(double value,
                                                   double min_value,
                                                   double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_specular_reflection_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

static Vec3 runtime_specular_reflection_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    const double ndoti = vec3_dot(normal, incident_dir);
    return vec3_normalize(vec3_sub(incident_dir, vec3_scale(normal, 2.0 * ndoti)));
}

static double runtime_specular_reflection_3d_weight(const RuntimeMaterialPayload3D* payload,
                                                    Vec3 view_dir,
                                                    Vec3 normal) {
    double reflectivity = 0.0;
    double spec_weight = 0.0;
    double roughness = 1.0;
    double view_facing = 0.0;
    double f0 = 0.04;
    double fresnel = 0.0;
    double roughness_focus = 0.0;
    double weight = 0.0;

    if (!payload || !payload->valid) return 0.0;
    if (payload->transparency > 1e-6) return 0.0;

    reflectivity = runtime_specular_reflection_3d_clamp(payload->bsdf.reflectivity, 0.0, 1.0);
    spec_weight = runtime_specular_reflection_3d_clamp(payload->bsdf.specWeight, 0.0, 1.0);
    roughness = runtime_specular_reflection_3d_clamp(payload->bsdf.roughness, 0.0, 1.0);
    if (!(reflectivity > 0.05) || !(spec_weight > 0.01)) return 0.0;

    view_facing = runtime_specular_reflection_3d_clamp(vec3_dot(normal, vec3_normalize(view_dir)),
                                                       0.0,
                                                       1.0);
    f0 = runtime_specular_reflection_3d_clamp(0.04 + (reflectivity * 0.96), 0.04, 1.0);
    fresnel = FresnelSchlick(view_facing, f0);
    roughness_focus = runtime_specular_reflection_3d_clamp(1.0 - (roughness * 0.75), 0.15, 1.0);
    weight = reflectivity * spec_weight * fresnel * roughness_focus;
    return runtime_specular_reflection_3d_clamp(weight, 0.0, 1.0);
}

bool RuntimeSpecularReflection3D_Trace(const RuntimeScene3D* scene,
                                       const HitInfo3D* hit,
                                       const RuntimeMaterialPayload3D* payload,
                                       Vec3 view_dir,
                                       const RuntimeNative3DSamplingContext* sampling,
                                       RuntimeSpecularReflection3DResult* out_result) {
    RuntimeSpecularReflection3DResult result = {0};
    RuntimeLightEmitterTrace3DResult trace = {0};
    Vec3 incident_dir = vec3(0.0, 0.0, 0.0);
    Vec3 reflection_dir = vec3(0.0, 0.0, 0.0);
    double tint_luma = 1.0;

    (void)sampling;
    if (!out_result) return false;
    memset(out_result, 0, sizeof(*out_result));
    if (!scene || !hit || !payload || !payload->valid) return false;

    result.weight = runtime_specular_reflection_3d_weight(payload, view_dir, hit->normal);
    if (!(result.weight > kRuntimeSpecularReflection3DMinWeight)) {
        *out_result = result;
        return false;
    }

    result.tintR = runtime_specular_reflection_3d_clamp(payload->baseColorR, 0.0, 1.0);
    result.tintG = runtime_specular_reflection_3d_clamp(payload->baseColorG, 0.0, 1.0);
    result.tintB = runtime_specular_reflection_3d_clamp(payload->baseColorB, 0.0, 1.0);
    tint_luma = runtime_specular_reflection_3d_luma(result.tintR, result.tintG, result.tintB);
    if (tint_luma > 1e-6) {
        result.tintR /= tint_luma;
        result.tintG /= tint_luma;
        result.tintB /= tint_luma;
    }

    incident_dir = vec3_scale(vec3_normalize(view_dir), -1.0);
    reflection_dir = runtime_specular_reflection_3d_reflect(incident_dir, hit->normal);
    if (!(vec3_length(reflection_dir) > 1e-9)) {
        *out_result = result;
        return false;
    }

    result.ray = RuntimeRay3D_MakeOffset(hit->position,
                                         hit->normal,
                                         reflection_dir,
                                         kRuntimeSpecularReflection3DEpsilon);
    result.traced = true;
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
        RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR,
        1);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &result.ray,
                                               kRuntimeSpecularReflection3DEpsilon,
                                               kRuntimeSpecularReflection3DMaxDistance,
                                               &trace)) {
        *out_result = result;
        return true;
    }
    if (trace.geometryHit) {
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&trace.geometryHitInfo);
    }

    result.geometryHit = trace.geometryHit;
    result.emitterHit = trace.emitterHit;
    result.emitterWins = trace.emitterWins;
    result.hitInfo = trace.geometryHitInfo;
    result.emitterHitInfo = trace.emitterHitInfo;
    *out_result = result;
    return true;
}
