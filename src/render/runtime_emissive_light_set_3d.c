#include "render/runtime_emissive_light_set_3d.h"

#include <math.h>
#include <stdio.h>
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

static Vec3 runtime_emissive_light_set_3d_triangle_centroid(
    const RuntimeTriangle3D* triangle) {
    if (!triangle) return vec3(0.0, 0.0, 0.0);
    return vec3((triangle->p0.x + triangle->p1.x + triangle->p2.x) / 3.0,
                (triangle->p0.y + triangle->p1.y + triangle->p2.y) / 3.0,
                (triangle->p0.z + triangle->p1.z + triangle->p2.z) / 3.0);
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
    const RuntimePrimitive3D* primitive,
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
    if (primitive) {
        candidate->primitiveKind = primitive->kind;
        if (primitive->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            candidate->primitiveOrigin = primitive->shape.plane.origin;
            candidate->primitiveAxisU = primitive->shape.plane.axisU;
            candidate->primitiveAxisV = primitive->shape.plane.axisV;
            candidate->primitiveWidth = primitive->shape.plane.width;
            candidate->primitiveHeight = primitive->shape.plane.height;
        }
    }
    candidate->centroid = runtime_emissive_light_set_3d_triangle_centroid(triangle);
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

bool RuntimeEmissiveLightSet3D_AppendRegistryDiagnostics(const RuntimeEmissiveLightSet3D* set,
                                                         RuntimeLightSet3D* registry) {
    return RuntimeEmissiveLightSet3D_AppendRegistryEntries(
        set,
        registry,
        RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_DIAGNOSTICS_ONLY);
}

static int runtime_emissive_light_set_3d_count_object_candidates(
    const RuntimeEmissiveLightSet3D* set,
    int scene_object_index) {
    int count = 0;
    if (!set || scene_object_index < 0) return 0;
    for (int i = 0; i < set->candidateCount; ++i) {
        if (set->candidates[i].sceneObjectIndex == scene_object_index) {
            ++count;
        }
    }
    return count;
}

static bool runtime_emissive_light_set_3d_is_rect_proxy_candidate(
    const RuntimeEmissiveLightSet3D* set,
    const RuntimeEmissiveLightCandidate3D* candidate,
    int object_candidate_count) {
    int primitive_index = -1;
    if (!set || !candidate || object_candidate_count <= 1) return false;
    if (candidate->primitiveKind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) return false;
    if (!(candidate->primitiveWidth > 0.0) || !(candidate->primitiveHeight > 0.0)) {
        return false;
    }
    primitive_index = candidate->primitiveIndex;
    for (int i = 0; i < set->candidateCount; ++i) {
        const RuntimeEmissiveLightCandidate3D* other = &set->candidates[i];
        if (other->sceneObjectIndex != candidate->sceneObjectIndex) continue;
        if (other->primitiveIndex != primitive_index ||
            other->primitiveKind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            return false;
        }
    }
    return true;
}

typedef struct {
    int candidateCount;
    double totalArea;
    double totalWeight;
    Vec3 centroid;
    Vec3 averageNormal;
    double proxyRadius;
} RuntimeEmissiveLightObjectSummary3D;

static RuntimeEmissiveLightObjectSummary3D runtime_emissive_light_set_3d_object_summary(
    const RuntimeEmissiveLightSet3D* set,
    int scene_object_index) {
    RuntimeEmissiveLightObjectSummary3D summary = {0};
    Vec3 weighted_centroid = vec3(0.0, 0.0, 0.0);
    Vec3 weighted_normal = vec3(0.0, 0.0, 0.0);

    if (!set || scene_object_index < 0) return summary;
    for (int i = 0; i < set->candidateCount; ++i) {
        const RuntimeEmissiveLightCandidate3D* candidate = &set->candidates[i];
        double area = 0.0;
        if (candidate->sceneObjectIndex != scene_object_index) continue;
        area = candidate->area > 0.0 && isfinite(candidate->area) ? candidate->area : 0.0;
        summary.candidateCount += 1;
        summary.totalArea += area;
        summary.totalWeight += candidate->weight > 0.0 && isfinite(candidate->weight)
                                   ? candidate->weight
                                   : 0.0;
        weighted_centroid = vec3_add(weighted_centroid,
                                     vec3_scale(candidate->centroid, area));
        weighted_normal = vec3_add(weighted_normal,
                                   vec3_scale(candidate->normal, area));
    }

    if (summary.totalArea > 1e-12) {
        summary.centroid = vec3_scale(weighted_centroid, 1.0 / summary.totalArea);
        summary.averageNormal = vec3_normalize(weighted_normal);
        if (vec3_length(summary.averageNormal) <= 1e-9) {
            summary.averageNormal = vec3(0.0, 1.0, 0.0);
        }
        for (int i = 0; i < set->candidateCount; ++i) {
            const RuntimeEmissiveLightCandidate3D* candidate = &set->candidates[i];
            double equivalent_radius = 0.0;
            double candidate_radius = 0.0;
            if (candidate->sceneObjectIndex != scene_object_index) continue;
            equivalent_radius = candidate->area > 0.0
                                    ? sqrt(candidate->area / 3.14159265358979323846)
                                    : 0.0;
            candidate_radius = vec3_length(vec3_sub(candidate->centroid, summary.centroid)) +
                               equivalent_radius;
            if (candidate_radius > summary.proxyRadius && isfinite(candidate_radius)) {
                summary.proxyRadius = candidate_radius;
            }
        }
    }

    return summary;
}

static Vec3 runtime_emissive_light_set_3d_rect_normal(
    const RuntimeEmissiveLightCandidate3D* candidate) {
    Vec3 normal = vec3(0.0, 0.0, 0.0);
    if (!candidate) return normal;
    normal = vec3_cross(candidate->primitiveAxisU, candidate->primitiveAxisV);
    if (vec3_length(normal) > 1e-9) {
        return vec3_normalize(normal);
    }
    return candidate->normal;
}

bool RuntimeEmissiveLightSet3D_AppendRegistryEntries(
    const RuntimeEmissiveLightSet3D* set,
    RuntimeLightSet3D* registry,
    RuntimeEmissiveLightRegistryMode3D mode) {
    bool seen[MAX_OBJECTS] = {0};

    if (!set || !registry) return false;
    if (!set->valid || set->candidateCount <= 0) return true;

    for (int i = 0; i < set->candidateCount; ++i) {
        const RuntimeEmissiveLightCandidate3D* candidate = &set->candidates[i];
        RuntimeLightSource3D source;
        RuntimeEmissiveLightObjectSummary3D summary = {0};
        int object_index = candidate->sceneObjectIndex;
        int object_candidate_count = 0;
        bool enable_simple_proxy = false;
        bool enable_rect_proxy = false;
        double radius = 0.0;

        if (object_index < 0 || object_index >= MAX_OBJECTS || seen[object_index]) {
            continue;
        }
        seen[object_index] = true;
        object_candidate_count =
            runtime_emissive_light_set_3d_count_object_candidates(set, object_index);
        summary = runtime_emissive_light_set_3d_object_summary(set, object_index);
        enable_simple_proxy =
            mode == RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_ENABLE_SIMPLE_PROXIES &&
            object_candidate_count == 1;
        enable_rect_proxy =
            mode == RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_ENABLE_SIMPLE_PROXIES &&
            runtime_emissive_light_set_3d_is_rect_proxy_candidate(set,
                                                                  candidate,
                                                                  object_candidate_count);

        RuntimeLightSource3D_Init(&source);
        snprintf(source.id,
                 sizeof(source.id),
                 (enable_simple_proxy || enable_rect_proxy) ? "material_emitter_proxy_%d"
                                                            : "material_emitter_object_%d",
                 object_index);
        source.kind = enable_rect_proxy ? RUNTIME_LIGHT_SOURCE_3D_KIND_RECT
                      : enable_simple_proxy ? RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE
                                            : RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE;
        source.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER;
        source.enabled = enable_simple_proxy || enable_rect_proxy;
        source.position = enable_rect_proxy ? candidate->primitiveOrigin : summary.centroid;
        source.normal = summary.averageNormal;
        if (enable_rect_proxy) {
            source.axisU = candidate->primitiveAxisU;
            source.axisV = candidate->primitiveAxisV;
            source.normal = runtime_emissive_light_set_3d_rect_normal(candidate);
            source.width = candidate->primitiveWidth;
            source.height = candidate->primitiveHeight;
        }
        radius = sqrt(candidate->area / 3.14159265358979323846);
        source.radius = isfinite(radius) && radius > 0.0 ? radius : 0.0;
        source.color = vec3(candidate->baseColorR,
                            candidate->baseColorG,
                            candidate->baseColorB);
        source.intensity = candidate->emissive;
        source.emissiveCandidateCount = summary.candidateCount;
        source.emissiveArea = summary.totalArea;
        source.emissiveWeight = summary.totalWeight;
        source.emissiveCentroid = summary.centroid;
        source.emissiveAverageNormal = summary.averageNormal;
        source.emissiveProxyRadius = summary.proxyRadius;
        source.meshAreaSamplerOnly =
            source.kind == RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE &&
            !source.enabled;
        if (enable_rect_proxy) {
            source.radius = 0.5 * sqrt(source.width * source.width +
                                       source.height * source.height);
            source.emissiveProxyRadius = source.radius;
        } else if (!enable_simple_proxy) {
            source.radius = source.emissiveProxyRadius;
        }
        if (!source.enabled) {
            source.falloffDistance = 0.0;
        }
        if (!RuntimeLightSet3D_Append(registry, &source, NULL)) {
            return false;
        }
    }
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
        const RuntimePrimitive3D* primitive = NULL;
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

        if (triangle->primitiveIndex >= 0 &&
            triangle->primitiveIndex < scene->primitiveCount) {
            primitive = &scene->primitives[triangle->primitiveIndex];
        }
        area = runtime_emissive_light_set_3d_triangle_area(triangle);
        if (!runtime_emissive_light_set_3d_append(&rebuilt,
                                                  triangle,
                                                  primitive,
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
