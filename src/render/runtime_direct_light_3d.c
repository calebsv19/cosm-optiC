#include "render/runtime_direct_light_internal_3d.h"

#include <math.h>

#include "config/config_manager.h"
#include "render/runtime_light_set_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_volume_3d_integrate.h"

static const double kRuntimeDirectLight3DTopFillIntensityScale = 0.08;
static const double kRuntimeDirectLight3DTopFillIntensityMax = 0.75;

double runtime_direct_light_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static double runtime_direct_light_3d_clamp(double value,
                                            double min_value,
                                            double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool runtime_direct_light_3d_payload_is_transparent(
    const RuntimeMaterialPayload3D* payload) {
    return payload && payload->valid &&
           (payload->materialId == MATERIAL_PRESET_TRANSPARENT ||
            payload->transparency > 1.0e-6);
}

static void runtime_direct_light_3d_apply_transmittance(
    const RuntimeVisibility3DTransmittance* transmittance,
    RuntimeDirectLight3DResult* io_result) {
    if (!transmittance || !io_result) return;

    io_result->radianceR *= transmittance->r;
    io_result->radianceG *= transmittance->g;
    io_result->radianceB *= transmittance->b;
    io_result->radiance = runtime_direct_light_3d_peak(io_result->radianceR,
                                                       io_result->radianceG,
                                                       io_result->radianceB);
    if (!(transmittance->luma > 1e-9)) {
        io_result->visible = false;
    }
}

static double runtime_direct_light_3d_top_fill_radiance(const RuntimeScene3D* scene,
                                                        const HitInfo3D* hit) {
    double up_facing = 0.0;
    double top_fill_intensity = 0.0;

    if (!scene || !hit || scene->environment.lightMode != ENVIRONMENT_LIGHT_MODE_TOP_FILL) {
        return 0.0;
    }

    up_facing = runtime_direct_light_3d_clamp(hit->normal.z, 0.0, 1.0);
    if (!(up_facing > 0.0)) return 0.0;

    top_fill_intensity =
        runtime_direct_light_3d_clamp(scene->environment.topFillIntensity *
                                          kRuntimeDirectLight3DTopFillIntensityScale,
                                      0.0,
                                      kRuntimeDirectLight3DTopFillIntensityMax);
    return top_fill_intensity * up_facing;
}

static void runtime_direct_light_3d_resolve_hit_tint(const HitInfo3D* hit,
                                                     const RuntimeMaterialPayload3D* resolved_payload,
                                                     double* out_r,
                                                     double* out_g,
                                                     double* out_b) {
    RuntimeMaterialPayload3D payload = {0};
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (resolved_payload && resolved_payload->valid) {
        r = resolved_payload->baseColorR;
        g = resolved_payload->baseColorG;
        b = resolved_payload->baseColorB;
    } else if (hit &&
        hit->sceneObjectIndex >= 0 &&
        hit->sceneObjectIndex < sceneSettings.objectCount &&
        RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload) &&
        payload.valid) {
        r = payload.baseColorR;
        g = payload.baseColorG;
        b = payload.baseColorB;
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

static bool runtime_direct_light_3d_shade_hit_with_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    RuntimeDirectLight3DResult* out_result);

static bool runtime_direct_light_3d_shade_hit_with_light_set_and_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_TracePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimePrimaryHit3DResult* out_result) {
    RuntimePrimaryHit3DResult result = {0};

    if (!scene || !projector || !out_result) return false;

    result.primaryRay = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    result.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(RUNTIME_RENDER_TRACE_COST_RAY_PRIMARY, 0);
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &result.primaryRay,
                                         projector->nearPlane,
                                         HUGE_VAL,
                                         &result.hitInfo)) {
        *out_result = result;
        return false;
    }
    RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&result.hitInfo);

    result.hit = true;
    result.primaryTransmittance =
        RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                 &result.primaryRay,
                                                 projector->nearPlane,
                                                 result.hitInfo.t);
    *out_result = result;
    return true;
}

bool RuntimeDirectLight3D_ShadeHit(const RuntimeScene3D* scene,
                                   const HitInfo3D* hit,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDirectLight3DResult* out_result) {
    return RuntimeDirectLight3D_ShadeHitWithPayload(scene, hit, NULL, sampling, out_result);
}

bool RuntimeDirectLight3D_ShadeHitWithPayload(const RuntimeScene3D* scene,
                                              const HitInfo3D* hit,
                                              const RuntimeMaterialPayload3D* payload,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeDirectLight3DResult* out_result) {
    return runtime_direct_light_3d_shade_hit_with_payload(
        scene,
        hit,
        payload,
        sampling,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_SHADED_HIT,
        out_result);
}

static bool runtime_direct_light_3d_shade_hit_with_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    RuntimeDirectLight3DResult* out_result) {
    RuntimeLightSet3D light_set;
    const RuntimeLightSet3D* active_light_set = NULL;
    bool ok = false;

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight && scene->lightSet.lightCount <= 0) return false;

    if (scene->lightSet.lightCount > 0) {
        active_light_set = &scene->lightSet;
        ok = true;
    } else {
        RuntimeLightSet3D_Init(&light_set);
        ok = RuntimeLightSet3D_BuildFromCompatibilityLight(&light_set,
                                                           &scene->light,
                                                           scene->hasLight);
        active_light_set = &light_set;
    }
    if (ok) {
        ok = runtime_direct_light_3d_shade_hit_with_light_set_and_payload(scene,
                                                                          hit,
                                                                          active_light_set,
                                                                          payload,
                                                                          sampling,
                                                                          caller,
                                                                          out_result);
    }
    if (active_light_set == &light_set) {
        RuntimeLightSet3D_Free(&light_set);
    }
    return ok;
}

bool RuntimeDirectLight3D_ShadeHitWithLightSet(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result) {
    return RuntimeDirectLight3D_ShadeHitWithLightSetAndPayload(scene,
                                                               hit,
                                                               light_set,
                                                               NULL,
                                                               sampling,
                                                               out_result);
}

bool RuntimeDirectLight3D_ShadeHitWithLightSetAndPayload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result) {
    return runtime_direct_light_3d_shade_hit_with_light_set_and_payload(
        scene,
        hit,
        light_set,
        payload,
        sampling,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_LIGHT_SET,
        out_result);
}

static bool runtime_direct_light_3d_shade_hit_with_light_set_and_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    double light_r = 0.0;
    double light_g = 0.0;
    double light_b = 0.0;
    double top_fill = 0.0;
    bool any_light_sample_visible = false;
    const bool receiver_is_transparent =
        runtime_direct_light_3d_payload_is_transparent(payload);

    if (!scene || !hit || !out_result) return false;
    if (!light_set) return false;
    if (hit->triangleIndex < 0) return false;

    result.hit = true;
    result.hitInfo = *hit;
    runtime_direct_light_3d_resolve_hit_tint(hit, payload, &tint_r, &tint_g, &tint_b);
    top_fill = runtime_direct_light_3d_top_fill_radiance(scene, hit);

    for (int i = 0; i < RuntimeLightSet3D_EnabledCount(light_set); ++i) {
        const RuntimeLightSource3D* source = RuntimeLightSet3D_GetEnabled(light_set, i);
        runtime_direct_light_3d_accumulate_source(scene,
                                                  hit,
                                                  source,
                                                  receiver_is_transparent,
                                                  caller,
                                                  sampling,
                                                  &result,
                                                  &light_r,
                                                  &light_g,
                                                  &light_b,
                                                  &any_light_sample_visible);
    }
    if (result.rectSampleCount > 0) {
        result.rectReceiverCosAvg =
            result.rectReceiverCosSum / (double)result.rectSampleCount;
        result.rectEmitterCosAvg =
            result.rectEmitterCosSum / (double)result.rectSampleCount;
    }
    result.visible = any_light_sample_visible || top_fill > 1e-6;
    result.radianceR = (light_r + top_fill) * tint_r;
    result.radianceG = (light_g + top_fill) * tint_g;
    result.radianceB = (light_b + top_fill) * tint_b;
    result.radiance = runtime_direct_light_3d_peak(result.radianceR,
                                                   result.radianceG,
                                                   result.radianceB);

    *out_result = result;
    return true;
}

bool RuntimeDirectLight3D_ShadePixel(const RuntimeScene3D* scene,
                                     const RuntimeCameraProjector3D* projector,
                                     double pixel_x,
                                     double pixel_y,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};

    if (!scene || !projector || !out_result) return false;
    if (!scene->hasLight) return false;

    if (!RuntimeDirectLight3D_TracePrimaryHit(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              &primary_hit)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    return RuntimeDirectLight3D_ShadePrimaryHit(scene, &primary_hit, sampling, out_result);
}

bool RuntimeDirectLight3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimePrimaryHit3DResult* primary_hit,
                                          const RuntimeNative3DSamplingContext* sampling,
                                          RuntimeDirectLight3DResult* out_result) {
    return RuntimeDirectLight3D_ShadePrimaryHitWithPayload(scene,
                                                           primary_hit,
                                                           NULL,
                                                           sampling,
                                                           out_result);
}

bool RuntimeDirectLight3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};

    if (!scene || !primary_hit || !out_result) return false;
    if (!primary_hit->hit) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    if (!runtime_direct_light_3d_shade_hit_with_payload(
            scene,
            &primary_hit->hitInfo,
            payload,
            sampling,
            RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_PRIMARY_HIT,
            &result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    runtime_direct_light_3d_apply_transmittance(&primary_hit->primaryTransmittance, &result);
    result.primaryRay = primary_hit->primaryRay;
    *out_result = result;
    return true;
}
