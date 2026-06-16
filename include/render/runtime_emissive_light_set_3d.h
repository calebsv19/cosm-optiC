#ifndef RENDER_RUNTIME_EMISSIVE_LIGHT_SET_3D_H
#define RENDER_RUNTIME_EMISSIVE_LIGHT_SET_3D_H

#include <stdbool.h>

#include "math/vec3.h"

struct RuntimeScene3D;

typedef struct {
    int triangleIndex;
    int sceneObjectIndex;
    int primitiveIndex;
    int localTriangleIndex;
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

void RuntimeEmissiveLightSet3D_Init(RuntimeEmissiveLightSet3D* set);
void RuntimeEmissiveLightSet3D_Free(RuntimeEmissiveLightSet3D* set);
bool RuntimeEmissiveLightSet3D_CopyFrom(RuntimeEmissiveLightSet3D* dst,
                                        const RuntimeEmissiveLightSet3D* src);
bool RuntimeEmissiveLightSet3D_BuildForScene(RuntimeEmissiveLightSet3D* set,
                                             const struct RuntimeScene3D* scene);

#endif
