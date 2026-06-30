#ifndef RENDER_RUNTIME_LIGHT_SET_3D_H
#define RENDER_RUNTIME_LIGHT_SET_3D_H

#include <stdbool.h>

#include "config/config_manager.h"
#include "math/vec3.h"

#define RUNTIME_LIGHT_SOURCE_3D_MAX_ID 64

typedef struct RuntimeLight3D RuntimeLight3D;

typedef enum {
    RUNTIME_LIGHT_SOURCE_3D_KIND_POINT = 0,
    RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE = 1,
    RUNTIME_LIGHT_SOURCE_3D_KIND_DISK = 2,
    RUNTIME_LIGHT_SOURCE_3D_KIND_RECT = 3,
    RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE = 4
} RuntimeLightSource3DKind;

typedef enum {
    RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT = 0,
    RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT = 1,
    RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER = 2
} RuntimeLightSource3DOrigin;

typedef struct {
    char id[RUNTIME_LIGHT_SOURCE_3D_MAX_ID];
    RuntimeLightSource3DKind kind;
    RuntimeLightSource3DOrigin origin;
    bool enabled;
    Vec3 position;
    Vec3 axisU;
    Vec3 axisV;
    Vec3 normal;
    double radius;
    double width;
    double height;
    Vec3 color;
    double intensity;
    double falloffDistance;
    ForwardFalloffMode falloffMode;
    int emissiveCandidateCount;
    double emissiveArea;
    double emissiveWeight;
    Vec3 emissiveCentroid;
    Vec3 emissiveAverageNormal;
    double emissiveProxyRadius;
    bool meshAreaSamplerOnly;
} RuntimeLightSource3D;

typedef struct {
    RuntimeLightSource3D* lights;
    int lightCount;
    int lightCapacity;
    int enabledCount;
} RuntimeLightSet3D;

void RuntimeLightSource3D_Init(RuntimeLightSource3D* light);
void RuntimeLightSet3D_Init(RuntimeLightSet3D* set);
void RuntimeLightSet3D_Reset(RuntimeLightSet3D* set);
void RuntimeLightSet3D_Free(RuntimeLightSet3D* set);
bool RuntimeLightSet3D_Reserve(RuntimeLightSet3D* set, int capacity);
bool RuntimeLightSet3D_Append(RuntimeLightSet3D* set,
                              const RuntimeLightSource3D* light,
                              int* out_index);
void RuntimeLightSet3D_RemoveOrigin(RuntimeLightSet3D* set,
                                    RuntimeLightSource3DOrigin origin);
bool RuntimeLightSet3D_CopyFrom(RuntimeLightSet3D* dst, const RuntimeLightSet3D* src);
bool RuntimeLightSet3D_BuildFromCompatibilityLight(RuntimeLightSet3D* set,
                                                   const RuntimeLight3D* light,
                                                   bool has_light);
bool RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(RuntimeLightSet3D* set,
                                                                const RuntimeLight3D* light);
int RuntimeLightSet3D_EnabledCount(const RuntimeLightSet3D* set);
const RuntimeLightSource3D* RuntimeLightSet3D_GetEnabled(const RuntimeLightSet3D* set,
                                                         int enabled_index);

#endif
