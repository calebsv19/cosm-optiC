#include "render/runtime_emissive_light_set_3d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_scene_3d.h"

static double runtime_emissive_light_set_3d_clamp(double value,
                                                  double min_value,
                                                  double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_emissive_light_set_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

static double runtime_emissive_light_set_3d_triangle_area(
    const RuntimeTriangle3D* triangle) {
    Vec3 edge_a = vec3(0.0, 0.0, 0.0);
    Vec3 edge_b = vec3(0.0, 0.0, 0.0);
    Vec3 cross = vec3(0.0, 0.0, 0.0);

    if (!triangle) return 0.0;

    edge_a = vec3_sub(triangle->p1, triangle->p0);
    edge_b = vec3_sub(triangle->p2, triangle->p0);
    cross = vec3_cross(edge_a, edge_b);
    return 0.5 * vec3_length(cross);
}

void RuntimeEmissiveLightSet3D_Init(RuntimeEmissiveLightSet3D* set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

void RuntimeEmissiveLightSet3D_Free(RuntimeEmissiveLightSet3D* set) {
    if (!set) return;
    free(set->candidates);
    set->candidates = NULL;
    set->candidateCount = 0;
    set->candidateCapacity = 0;
    set->totalWeight = 0.0;
    set->valid = false;
}

bool RuntimeEmissiveLightSet3D_CopyFrom(RuntimeEmissiveLightSet3D* dst,
                                        const RuntimeEmissiveLightSet3D* src) {
    if (!dst || !src) return false;

    RuntimeEmissiveLightSet3D_Free(dst);
    RuntimeEmissiveLightSet3D_Init(dst);
    if (src->candidateCount > 0) {
        dst->candidates = (RuntimeEmissiveLightCandidate3D*)malloc(
            sizeof(*dst->candidates) * (size_t)src->candidateCount);
        if (!dst->candidates) {
            RuntimeEmissiveLightSet3D_Free(dst);
            return false;
        }
        memcpy(dst->candidates,
               src->candidates,
               sizeof(*dst->candidates) * (size_t)src->candidateCount);
        dst->candidateCount = src->candidateCount;
        dst->candidateCapacity = src->candidateCount;
    }
    dst->totalWeight = src->totalWeight;
    dst->valid = src->valid;
    return true;
}

static bool runtime_emissive_light_set_3d_reserve(RuntimeEmissiveLightSet3D* set,
                                                  int capacity) {
    RuntimeEmissiveLightCandidate3D* candidates = NULL;
    if (!set) return false;
    if (capacity <= set->candidateCapacity) return true;

    candidates = (RuntimeEmissiveLightCandidate3D*)realloc(
        set->candidates,
        sizeof(*set->candidates) * (size_t)capacity);
    if (!candidates) return false;

    set->candidates = candidates;
    set->candidateCapacity = capacity;
    return true;
}

static bool runtime_emissive_light_set_3d_append(
    RuntimeEmissiveLightSet3D* set,
    const RuntimeTriangle3D* triangle,
    int triangle_index,
    const RuntimeMaterialPayload3D* payload,
    double area) {
    RuntimeEmissiveLightCandidate3D* candidate = NULL;
    double luma = 0.0;
    double weight = 0.0;
    int next_capacity = 0;

    if (!set || !triangle || !payload || !(area > 1e-12) || !(payload->emissive > 0.0)) {
        return true;
    }

    if (set->candidateCount >= set->candidateCapacity) {
        next_capacity = set->candidateCapacity > 0 ? set->candidateCapacity * 2 : 8;
        if (!runtime_emissive_light_set_3d_reserve(set, next_capacity)) {
            return false;
        }
    }

    luma = runtime_emissive_light_set_3d_luma(payload->baseColorR,
                                              payload->baseColorG,
                                              payload->baseColorB);
    weight = payload->emissive *
             runtime_emissive_light_set_3d_clamp(luma, 0.05, 1.0) *
             runtime_emissive_light_set_3d_clamp(area, 0.25, 4.0);
    if (!(weight > 0.0) || !isfinite(weight)) {
        weight = payload->emissive;
    }

    candidate = &set->candidates[set->candidateCount++];
    memset(candidate, 0, sizeof(*candidate));
    candidate->triangleIndex = triangle_index;
    candidate->sceneObjectIndex = triangle->sceneObjectIndex;
    candidate->primitiveIndex = triangle->primitiveIndex;
    candidate->localTriangleIndex = triangle->localTriangleIndex;
    candidate->normal = triangle->normal;
    candidate->area = area;
    candidate->emissive = payload->emissive;
    candidate->baseColorR = payload->baseColorR;
    candidate->baseColorG = payload->baseColorG;
    candidate->baseColorB = payload->baseColorB;
    candidate->weight = weight;
    set->totalWeight += weight;
    candidate->cumulativeWeight = set->totalWeight;
    return true;
}

bool RuntimeEmissiveLightSet3D_BuildForScene(RuntimeEmissiveLightSet3D* set,
                                             const struct RuntimeScene3D* scene) {
    RuntimeEmissiveLightSet3D rebuilt = {0};

    if (!set || !scene) return false;
    RuntimeEmissiveLightSet3D_Init(&rebuilt);
    if (!scene->triangleMesh.triangles || scene->triangleMesh.triangleCount <= 0) {
        RuntimeEmissiveLightSet3D_Free(set);
        *set = rebuilt;
        set->valid = true;
        return true;
    }

    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        RuntimeMaterialPayload3D payload = {0};
        double area = 0.0;

        if (triangle->sceneObjectIndex < 0) {
            continue;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(triangle->sceneObjectIndex,
                                                                  &payload) ||
            !payload.valid ||
            !(payload.emissive > 1e-6)) {
            continue;
        }

        area = runtime_emissive_light_set_3d_triangle_area(triangle);
        if (!runtime_emissive_light_set_3d_append(&rebuilt,
                                                  triangle,
                                                  i,
                                                  &payload,
                                                  area)) {
            RuntimeEmissiveLightSet3D_Free(&rebuilt);
            RuntimeEmissiveLightSet3D_Free(set);
            return false;
        }
    }

    rebuilt.valid = true;
    RuntimeEmissiveLightSet3D_Free(set);
    *set = rebuilt;
    return true;
}
