#include "render/runtime_light_set_3d.h"
#include "render/runtime_scene_3d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double runtime_light_set_3d_nonnegative(double value) {
    if (!isfinite(value) || value < 0.0) return 0.0;
    return value;
}

static Vec3 runtime_light_set_3d_default_color(Vec3 color) {
    if (!isfinite(color.x) || !isfinite(color.y) || !isfinite(color.z)) {
        return vec3(1.0, 1.0, 1.0);
    }
    return color;
}

static void runtime_light_set_3d_recount(RuntimeLightSet3D* set) {
    int enabled_count = 0;
    if (!set) return;
    for (int i = 0; i < set->lightCount; ++i) {
        if (set->lights[i].enabled) {
            ++enabled_count;
        }
    }
    set->enabledCount = enabled_count;
}

void RuntimeLightSource3D_Init(RuntimeLightSource3D* light) {
    if (!light) return;
    memset(light, 0, sizeof(*light));
    light->kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light->origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light->emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI;
    light->enabled = true;
    light->axisU = vec3(1.0, 0.0, 0.0);
    light->axisV = vec3(0.0, 0.0, 1.0);
    light->normal = vec3(0.0, -1.0, 0.0);
    light->color = vec3(1.0, 1.0, 1.0);
    light->intensity = 1.0;
    light->falloffDistance = 1.0;
    light->falloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    light->emissiveAverageNormal = vec3(0.0, 1.0, 0.0);
    light->sourceSceneObjectIndex = -1;
    light->sourcePrimitiveIndex = -1;
    light->sourceTriangleIndex = -1;
}

void RuntimeLightSet3D_Init(RuntimeLightSet3D* set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

void RuntimeLightSet3D_Reset(RuntimeLightSet3D* set) {
    if (!set) return;
    set->lightCount = 0;
    set->enabledCount = 0;
}

void RuntimeLightSet3D_Free(RuntimeLightSet3D* set) {
    if (!set) return;
    free(set->lights);
    set->lights = NULL;
    set->lightCount = 0;
    set->lightCapacity = 0;
    set->enabledCount = 0;
}

bool RuntimeLightSet3D_Reserve(RuntimeLightSet3D* set, int capacity) {
    RuntimeLightSource3D* lights = NULL;
    if (!set) return false;
    if (capacity <= set->lightCapacity) return true;
    if (capacity <= 0) return true;

    lights = (RuntimeLightSource3D*)realloc(set->lights,
                                           sizeof(*set->lights) * (size_t)capacity);
    if (!lights) return false;

    set->lights = lights;
    set->lightCapacity = capacity;
    return true;
}

bool RuntimeLightSet3D_Append(RuntimeLightSet3D* set,
                              const RuntimeLightSource3D* light,
                              int* out_index) {
    RuntimeLightSource3D stored;
    int next_capacity = 0;

    if (out_index) *out_index = -1;
    if (!set || !light) return false;

    if (set->lightCount >= set->lightCapacity) {
        next_capacity = set->lightCapacity > 0 ? set->lightCapacity * 2 : 4;
        if (!RuntimeLightSet3D_Reserve(set, next_capacity)) {
            return false;
        }
    }

    stored = *light;
    stored.radius = runtime_light_set_3d_nonnegative(stored.radius);
    stored.width = runtime_light_set_3d_nonnegative(stored.width);
    stored.height = runtime_light_set_3d_nonnegative(stored.height);
    stored.intensity = runtime_light_set_3d_nonnegative(stored.intensity);
    stored.falloffDistance = runtime_light_set_3d_nonnegative(stored.falloffDistance);
    stored.color = runtime_light_set_3d_default_color(stored.color);
    if (stored.emissionProfile < RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI ||
        stored.emissionProfile > RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED) {
        stored.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI;
    }
    if (stored.emissiveCandidateCount < 0) stored.emissiveCandidateCount = 0;
    stored.emissiveArea = runtime_light_set_3d_nonnegative(stored.emissiveArea);
    stored.emissiveWeight = runtime_light_set_3d_nonnegative(stored.emissiveWeight);
    stored.emissiveProxyRadius =
        runtime_light_set_3d_nonnegative(stored.emissiveProxyRadius);
    if (stored.sourceSceneObjectIndex < 0) {
        stored.sourcePrimitiveIndex = -1;
        stored.sourceTriangleIndex = -1;
    }

    set->lights[set->lightCount] = stored;
    if (out_index) *out_index = set->lightCount;
    ++set->lightCount;
    if (stored.enabled) {
        ++set->enabledCount;
    }
    return true;
}

void RuntimeLightSet3D_RemoveOrigin(RuntimeLightSet3D* set,
                                    RuntimeLightSource3DOrigin origin) {
    int write_index = 0;
    if (!set || !set->lights || set->lightCount <= 0) return;

    for (int read_index = 0; read_index < set->lightCount; ++read_index) {
        if (set->lights[read_index].origin == origin) {
            continue;
        }
        if (write_index != read_index) {
            set->lights[write_index] = set->lights[read_index];
        }
        ++write_index;
    }
    set->lightCount = write_index;
    runtime_light_set_3d_recount(set);
}

bool RuntimeLightSet3D_CopyFrom(RuntimeLightSet3D* dst, const RuntimeLightSet3D* src) {
    if (!dst || !src) return false;

    RuntimeLightSet3D_Free(dst);
    RuntimeLightSet3D_Init(dst);
    if (src->lightCount > 0) {
        if (!RuntimeLightSet3D_Reserve(dst, src->lightCount)) {
            RuntimeLightSet3D_Free(dst);
            return false;
        }
        memcpy(dst->lights, src->lights, sizeof(*dst->lights) * (size_t)src->lightCount);
        dst->lightCount = src->lightCount;
        dst->lightCapacity = src->lightCount;
        runtime_light_set_3d_recount(dst);
    }
    return true;
}

bool RuntimeLightSet3D_BuildFromCompatibilityLight(RuntimeLightSet3D* set,
                                                   const RuntimeLight3D* light,
                                                   bool has_light) {
    RuntimeLightSource3D source;

    if (!set) return false;
    RuntimeLightSet3D_Reset(set);
    if (!has_light || !light) {
        return true;
    }

    RuntimeLightSource3D_Init(&source);
    snprintf(source.id, sizeof(source.id), "%s", "compat_scene_light");
    source.kind = light->radius > 0.0 ? RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE
                                      : RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    source.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT;
    source.position = light->position;
    source.radius = light->radius;
    source.color = vec3(1.0, 1.0, 1.0);
    source.intensity = light->intensity;
    source.falloffDistance = light->falloffDistance;
    source.falloffMode = light->falloffMode;
    source.enabled = true;
    return RuntimeLightSet3D_Append(set, &source, NULL);
}

bool RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(RuntimeLightSet3D* set,
                                                                const RuntimeLight3D* light) {
    RuntimeLightSource3D* target = NULL;
    if (!set || !light || set->lightCount <= 0) return false;

    for (int i = 0; i < set->lightCount; ++i) {
        if (set->lights[i].enabled) {
            target = &set->lights[i];
            break;
        }
    }
    if (!target) {
        target = &set->lights[0];
    }

    target->position = light->position;
    target->radius = runtime_light_set_3d_nonnegative(light->radius);
    target->intensity = runtime_light_set_3d_nonnegative(light->intensity);
    target->falloffDistance = runtime_light_set_3d_nonnegative(light->falloffDistance);
    target->falloffMode = light->falloffMode;
    if (target->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_POINT ||
        target->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE) {
        target->kind = target->radius > 0.0 ? RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE
                                            : RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    }
    return true;
}

int RuntimeLightSet3D_EnabledCount(const RuntimeLightSet3D* set) {
    return set ? set->enabledCount : 0;
}

const RuntimeLightSource3D* RuntimeLightSet3D_GetEnabled(const RuntimeLightSet3D* set,
                                                         int enabled_index) {
    int seen = 0;
    if (!set || enabled_index < 0) return NULL;
    for (int i = 0; i < set->lightCount; ++i) {
        if (!set->lights[i].enabled) {
            continue;
        }
        if (seen == enabled_index) {
            return &set->lights[i];
        }
        ++seen;
    }
    return NULL;
}
