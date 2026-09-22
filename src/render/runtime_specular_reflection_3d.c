#include "render/runtime_principled_bsdf_3d.h"
#include "render/runtime_specular_reflection_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_surface_sampling.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

static const double kRuntimeSpecularReflection3DEpsilon = 1e-4;
static const double kRuntimeSpecularReflection3DMinWeight = 1e-4;

static double runtime_specular_reflection_3d_clamp(double value,
                                                   double min_value,
                                                   double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 runtime_specular_reflection_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    const double ndoti = vec3_dot(normal, incident_dir);
    return vec3_normalize(vec3_sub(incident_dir, vec3_scale(normal, 2.0 * ndoti)));
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
    Vec3 reflection_normal = vec3(0.0, 0.0, 0.0);
    Vec3 reflection_dir = vec3(0.0, 0.0, 0.0);


    (void)sampling;
    if (!out_result) return false;
    memset(out_result, 0, sizeof(*out_result));
    if (!scene || !hit || !payload || !payload->valid) return false;

    incident_dir = vec3_scale(vec3_normalize(view_dir), -1.0);
    reflection_normal = HitInfo3D_ShadingNormalForReflection(hit, view_dir);
    if (payload->transparency > 1e-6 || payload->bsdf.reflectivity <= 0.05 || payload->bsdf.specWeight <= 0.01)
        return false;
    RuntimePrincipledBSDF3D bsdf = RuntimePrincipledBSDF3D_FromMaterialPayload(payload);
    double cosine = runtime_specular_reflection_3d_clamp(vec3_dot(reflection_normal, vec3_normalize(view_dir)), 0.0, 1.0);
    double r = RuntimePrincipledBSDF3D_FresnelSchlick(cosine, bsdf.specularF0R);
    double g = RuntimePrincipledBSDF3D_FresnelSchlick(cosine, bsdf.specularF0G);
    double b = RuntimePrincipledBSDF3D_FresnelSchlick(cosine, bsdf.specularF0B);
    result.weight = fmax(r, fmax(g, b));
    if (!(result.weight > kRuntimeSpecularReflection3DMinWeight)) return false;
    result.tintR = r / result.weight;
    result.tintG = g / result.weight;
    result.tintB = b / result.weight;

    reflection_dir = runtime_specular_reflection_3d_reflect(incident_dir, reflection_normal);
    if (!(vec3_length(reflection_dir) > 1e-9)) {
        *out_result = result;
        return false;
    }

    result.ray = RuntimeRay3D_MakeOffset(hit->position,
                                         HitInfo3D_OffsetNormal(hit),
                                         reflection_dir,
                                         kRuntimeSpecularReflection3DEpsilon);
    if(payload->bsdf.roughness<=0.08 && !payload->hasMicrodetailNormal &&
       !RuntimeSurfaceSamplingNormalResponseActive(hit->sceneObjectIndex))
        (void)RuntimeRay3D_TransportIdealFootprint(hit,reflection_normal,
            RUNTIME_RAY_IDEAL_REFLECTION,1,1,&result.ray);
    result.traced = true;
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
        RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR,
        1);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &result.ray,
                                               kRuntimeSpecularReflection3DEpsilon,
                                               RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE,
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
