#include "render/runtime_direct_light_3d.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/runtime_material_payload_3d.h"

static const double kRuntimeDirectLight3DTopFillIntensityScale = 0.08;
static const double kRuntimeDirectLight3DTopFillIntensityMax = 0.75;

static double runtime_direct_light_3d_attenuation(const RuntimeLight3D* light,
                                                  double light_distance) {
    double falloff = 1.0;
    double normalized = 0.0;
    if (!light) return 0.0;

    falloff = light->falloffDistance;
    if (!(falloff > 0.0)) {
        falloff = 1.0;
    }
    normalized = light_distance / falloff;

    switch (light->falloffMode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            return 1.0 / (1.0 + normalized);
        case FORWARD_FALLOFF_MODE_NONE:
            return 1.0;
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            return 1.0 / (1.0 + normalized * normalized);
    }
}

static double runtime_direct_light_3d_peak(double r, double g, double b) {
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

static double runtime_direct_light_3d_top_fill_radiance(const HitInfo3D* hit,
                                                        const RuntimeLight3D* light) {
    double up_facing = 0.0;
    double top_fill_intensity = 0.0;

    if (!animSettings.topFillLightEnabled || !hit || !light) return 0.0;

    up_facing = runtime_direct_light_3d_clamp(hit->normal.z, 0.0, 1.0);
    if (!(up_facing > 0.0)) return 0.0;

    top_fill_intensity =
        runtime_direct_light_3d_clamp(light->intensity * kRuntimeDirectLight3DTopFillIntensityScale,
                                      0.0,
                                      kRuntimeDirectLight3DTopFillIntensityMax);
    return top_fill_intensity * up_facing;
}

static void runtime_direct_light_3d_resolve_hit_tint(const HitInfo3D* hit,
                                                     double* out_r,
                                                     double* out_g,
                                                     double* out_b) {
    RuntimeMaterialPayload3D payload = {0};
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (hit &&
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

bool RuntimeDirectLight3D_TracePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimePrimaryHit3DResult* out_result) {
    RuntimePrimaryHit3DResult result = {0};

    if (!scene || !projector || !out_result) return false;

    result.primaryRay = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &result.primaryRay,
                                         projector->nearPlane,
                                         HUGE_VAL,
                                         &result.hitInfo)) {
        *out_result = result;
        return false;
    }

    result.hit = true;
    *out_result = result;
    return true;
}

bool RuntimeDirectLight3D_ShadeHit(const RuntimeScene3D* scene,
                                   const HitInfo3D* hit,
                                   RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;
    double attenuation = 0.0;
    RuntimeVisibility3DTransmittance transmittance = {0};
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    double light_r = 0.0;
    double light_g = 0.0;
    double light_b = 0.0;
    double top_fill = 0.0;

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight) return false;
    if (hit->triangleIndex < 0) return false;

    result.hit = true;
    result.hitInfo = *hit;
    runtime_direct_light_3d_resolve_hit_tint(hit, &tint_r, &tint_g, &tint_b);
    top_fill = runtime_direct_light_3d_top_fill_radiance(hit, &scene->light);
    to_light = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(to_light);
    result.lightDistance = light_distance;
    if (light_distance <= 1e-9) {
        result.visible = true;
        result.ndotl = 1.0;
        result.attenuation = 1.0;
        result.radiance = scene->light.intensity;
        result.radianceR = result.radiance * tint_r;
        result.radianceG = result.radiance * tint_g;
        result.radianceB = result.radiance * tint_b;
        *out_result = result;
        return true;
    }

    to_light = vec3_scale(to_light, 1.0 / light_distance);
    result.ndotl = fmax(0.0, vec3_dot(hit->normal, to_light));
    transmittance = RuntimeVisibility3D_TransmittanceFromHitRGB(scene, hit, &scene->light);
    result.visible = transmittance.luma > 1e-6 || top_fill > 1e-6;
    attenuation = runtime_direct_light_3d_attenuation(&scene->light, light_distance);
    result.attenuation = attenuation;
    if (result.visible && result.ndotl > 0.0) {
        light_r = scene->light.intensity * attenuation * result.ndotl * transmittance.r;
        light_g = scene->light.intensity * attenuation * result.ndotl * transmittance.g;
        light_b = scene->light.intensity * attenuation * result.ndotl * transmittance.b;
    }
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

    if (!RuntimeDirectLight3D_ShadeHit(scene, &primary_hit.hitInfo, &result)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }
    result.primaryRay = primary_hit.primaryRay;
    *out_result = result;
    return true;
}
