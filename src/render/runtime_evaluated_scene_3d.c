#include "render/runtime_evaluated_scene_3d.h"

#include <string.h>

static Vec3 runtime_evaluated_scene_vec3(TimelineVec3 value) {
    return vec3(value.x, value.y, value.z);
}

static RuntimeLightSource3D* runtime_evaluated_scene_find_light(
    RuntimeLightSet3D* set,
    const RayEvaluatedLight* evaluated,
    RayEvaluatedSceneSource source) {
    RuntimeLightSource3D* fallback = NULL;
    if (!set || !evaluated) return NULL;
    for (int i = 0; i < set->lightCount; ++i) {
        RuntimeLightSource3D* light = &set->lights[i];
        if (!fallback && light->enabled) fallback = light;
        if (evaluated->runtime_light_id[0] &&
            strcmp(light->id, evaluated->runtime_light_id) == 0) {
            return light;
        }
    }
    return source == RAY_EVALUATED_SCENE_SOURCE_LEGACY_PREVIEW_FALLBACK
               ? fallback
               : NULL;
}

static void runtime_evaluated_scene_refresh_enabled_count(
    RuntimeLightSet3D* set) {
    int enabled_count = 0;
    if (!set) return;
    for (int i = 0; i < set->lightCount; ++i) {
        if (set->lights[i].enabled) enabled_count += 1;
    }
    set->enabledCount = enabled_count;
}

bool RuntimeEvaluatedScene3DApply(
    RuntimeScene3D* copied_scene,
    const RayEvaluatedSceneSnapshot* snapshot) {
    RuntimeLightSource3D* light = NULL;
    const RuntimeLightSource3D* first_enabled = NULL;
    if (!copied_scene || !snapshot ||
        RayEvaluatedSceneSnapshotValidate(snapshot) != TIMELINE_STATUS_OK) {
        return false;
    }
    light = runtime_evaluated_scene_find_light(&copied_scene->lightSet,
                                               &snapshot->light,
                                               snapshot->source);
    if (!light) return false;
    light->position = runtime_evaluated_scene_vec3(snapshot->light.position);
    if (snapshot->source == RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE) {
        light->enabled = snapshot->light.enabled;
        light->kind = (RuntimeLightSource3DKind)snapshot->light.kind;
        light->origin = (RuntimeLightSource3DOrigin)snapshot->light.origin;
        light->emissionProfile =
            (RuntimeLightSource3DEmissionProfile)snapshot->light.emission_profile;
        light->axisU = runtime_evaluated_scene_vec3(snapshot->light.axis_u);
        light->axisV = runtime_evaluated_scene_vec3(snapshot->light.axis_v);
        light->normal = runtime_evaluated_scene_vec3(snapshot->light.normal);
        light->radius = snapshot->light.radius;
        light->width = snapshot->light.width;
        light->height = snapshot->light.height;
        light->color = runtime_evaluated_scene_vec3(snapshot->light.color);
        light->intensity = snapshot->light.intensity;
        light->radiometryMode =
            (RuntimeLightRadiometryMode3D)snapshot->light.radiometry_mode;
        light->radiance = snapshot->light.radiance;
        light->falloffDistance = snapshot->light.falloff_distance;
        light->falloffMode = (ForwardFalloffMode)snapshot->light.falloff_mode;
    }
    runtime_evaluated_scene_refresh_enabled_count(&copied_scene->lightSet);

    first_enabled = RuntimeLightSet3D_GetEnabled(&copied_scene->lightSet, 0);
    if (!first_enabled && copied_scene->lightSet.lightCount > 0) {
        first_enabled = &copied_scene->lightSet.lights[0];
    }
    if (!first_enabled) return false;
    copied_scene->light.position = first_enabled->position;
    copied_scene->light.radius = first_enabled->radius;
    copied_scene->light.intensity = first_enabled->intensity;
    copied_scene->light.falloffDistance = first_enabled->falloffDistance;
    copied_scene->light.falloffMode = first_enabled->falloffMode;
    copied_scene->hasLight = first_enabled->enabled;

    copied_scene->camera.position =
        runtime_evaluated_scene_vec3(snapshot->camera.position);
    copied_scene->camera.rotation = snapshot->camera.yaw_radians;
    copied_scene->camera.lookPitch = snapshot->camera.pitch_radians;
    copied_scene->camera.zoom = snapshot->camera.zoom;
    copied_scene->hasCamera = snapshot->camera.valid;
    return copied_scene->hasLight && copied_scene->hasCamera;
}
