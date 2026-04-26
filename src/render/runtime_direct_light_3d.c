#include "render/runtime_direct_light_3d.h"

#include <math.h>
#include <string.h>

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

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight) return false;
    if (hit->triangleIndex < 0) return false;

    result.hit = true;
    result.hitInfo = *hit;
    to_light = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(to_light);
    result.lightDistance = light_distance;
    if (light_distance <= 1e-9) {
        result.visible = true;
        result.ndotl = 1.0;
        result.attenuation = 1.0;
        result.radiance = scene->light.intensity;
        *out_result = result;
        return true;
    }

    to_light = vec3_scale(to_light, 1.0 / light_distance);
    result.ndotl = fmax(0.0, vec3_dot(hit->normal, to_light));
    result.visible = RuntimeVisibility3D_HasLineOfSightFromHit(scene, hit, &scene->light);
    attenuation = runtime_direct_light_3d_attenuation(&scene->light, light_distance);
    result.attenuation = attenuation;
    if (result.visible && result.ndotl > 0.0) {
        result.radiance = scene->light.intensity * attenuation * result.ndotl;
    }

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
