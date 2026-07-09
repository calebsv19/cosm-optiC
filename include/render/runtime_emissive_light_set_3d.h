#ifndef RENDER_RUNTIME_EMISSIVE_LIGHT_SET_3D_H
#define RENDER_RUNTIME_EMISSIVE_LIGHT_SET_3D_H

#include <stdbool.h>

#include "math/vec3.h"
#include "render/runtime_light_set_3d.h"

struct RuntimeScene3D;

typedef struct {
    int triangleIndex;
    int sceneObjectIndex;
    int primitiveIndex;
    int localTriangleIndex;
    int primitiveKind;
    Vec3 primitiveOrigin;
    Vec3 primitiveAxisU;
    Vec3 primitiveAxisV;
    double primitiveWidth;
    double primitiveHeight;
    Vec3 centroid;
    Vec3 normal;
    double area;
    double emissive;
    double baseColorR;
    double baseColorG;
    double baseColorB;
    double weight;
    double cumulativeWeight;
} RuntimeEmissiveLightCandidate3D;

typedef struct {
    RuntimeEmissiveLightCandidate3D* candidates;
    int candidateCount;
    int candidateCapacity;
    double totalWeight;
    bool valid;
} RuntimeEmissiveLightSet3D;

typedef enum {
    RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_DIAGNOSTICS_ONLY = 0,
    RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_ENABLE_SIMPLE_PROXIES = 1
} RuntimeEmissiveLightRegistryMode3D;

void RuntimeEmissiveLightSet3D_Init(RuntimeEmissiveLightSet3D* set);
void RuntimeEmissiveLightSet3D_Free(RuntimeEmissiveLightSet3D* set);
bool RuntimeEmissiveLightSet3D_CopyFrom(RuntimeEmissiveLightSet3D* dst,
                                        const RuntimeEmissiveLightSet3D* src);
bool RuntimeEmissiveLightSet3D_BuildForScene(RuntimeEmissiveLightSet3D* set,
                                             const struct RuntimeScene3D* scene);
bool RuntimeEmissiveLightSet3D_AppendRegistryEntries(
    const RuntimeEmissiveLightSet3D* set,
    RuntimeLightSet3D* registry,
    RuntimeEmissiveLightRegistryMode3D mode);
bool RuntimeEmissiveLightSet3D_AppendRegistryDiagnostics(const RuntimeEmissiveLightSet3D* set,
                                                         RuntimeLightSet3D* registry);

#endif
