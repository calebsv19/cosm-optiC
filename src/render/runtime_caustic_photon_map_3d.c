#include "render/runtime_caustic_photon_map_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_CAPACITY = 4096,
    RUNTIME_CAUSTIC_PHOTON_MAP_MAX_CAPACITY = 262144
};

static double photon_map_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_map_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 photon_map_normalize_or_default(Vec3 value, Vec3 fallback) {
    if (!(vec3_length(value) > 1.0e-12)) return fallback;
    return vec3_normalize(value);
}

static double photon_map_kernel_area_normalization(double radius) {
    const double area = M_PI * radius * radius;
    if (!(area > 1.0e-12)) return 1.0;
    return 1.0 / area;
}

static bool photon_map_receiver_identity_matches(
    const RuntimeCausticPhotonMapRecord3D* record,
    const RuntimeCausticPhotonMapQuery3D* query) {
    if (!record || !query || !query->requireReceiverIdentity) return true;
    if (record->sceneObjectIndex >= 0 && query->sceneObjectIndex >= 0 &&
        record->sceneObjectIndex != query->sceneObjectIndex) {
        return false;
    }
    if (record->primitiveIndex >= 0 && query->primitiveIndex >= 0 &&
        record->primitiveIndex != query->primitiveIndex) {
        return false;
    }
    if (record->triangleIndex >= 0 && query->triangleIndex >= 0 &&
        record->triangleIndex != query->triangleIndex) {
        return false;
    }
    return true;
}

void RuntimeCausticPhotonMap3D_DefaultSettings(
    RuntimeCausticPhotonMapSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->defaultCapacity = RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_CAPACITY;
    settings->defaultQueryRadius = 0.10;
    settings->minNormalDot = 0.25;
}

void RuntimeCausticPhotonMap3D_DefaultQuery(RuntimeCausticPhotonMapQuery3D* query) {
    if (!query) return;
    memset(query, 0, sizeof(*query));
    query->normal = vec3(0.0, 1.0, 0.0);
    query->radius = 0.10;
    query->sceneObjectIndex = -1;
    query->primitiveIndex = -1;
    query->triangleIndex = -1;
    query->requireReceiverIdentity = true;
    query->minNormalDot = 0.25;
}

void RuntimeCausticPhotonMap3D_Init(RuntimeCausticPhotonMap3D* map) {
    if (!map) return;
    memset(map, 0, sizeof(*map));
}

bool RuntimeCausticPhotonMap3D_IsAllocated(const RuntimeCausticPhotonMap3D* map) {
    return map && map->ownsRecords && map->records && map->recordCapacity > 0u;
}

bool RuntimeCausticPhotonMap3D_Allocate(RuntimeCausticPhotonMap3D* map,
                                        uint64_t record_capacity) {
    RuntimeCausticPhotonMap3D allocated;

    if (!map) return false;
    if (record_capacity == 0u) {
        record_capacity = RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_CAPACITY;
    }
    if (record_capacity > RUNTIME_CAUSTIC_PHOTON_MAP_MAX_CAPACITY) {
        record_capacity = RUNTIME_CAUSTIC_PHOTON_MAP_MAX_CAPACITY;
    }
    if (record_capacity > (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticPhotonMapRecord3D))) {
        return false;
    }

    RuntimeCausticPhotonMap3D_Init(&allocated);
    allocated.records = (RuntimeCausticPhotonMapRecord3D*)calloc(
        (size_t)record_capacity,
        sizeof(RuntimeCausticPhotonMapRecord3D));
    if (!allocated.records) return false;
    allocated.recordCapacity = record_capacity;
    allocated.ownsRecords = true;

    RuntimeCausticPhotonMap3D_Free(map);
    *map = allocated;
    return true;
}

void RuntimeCausticPhotonMap3D_Clear(RuntimeCausticPhotonMap3D* map) {
    if (!RuntimeCausticPhotonMap3D_IsAllocated(map)) return;
    memset(map->records,
           0,
           (size_t)map->recordCapacity * sizeof(RuntimeCausticPhotonMapRecord3D));
    map->recordCount = 0u;
    map->storeAttemptCount = 0u;
    map->storeAcceptedCount = 0u;
    map->storeRejectedCount = 0u;
    map->queryCount = 0u;
    map->queryHitCount = 0u;
    memset(&map->lastQuery, 0, sizeof(map->lastQuery));
}

void RuntimeCausticPhotonMap3D_Free(RuntimeCausticPhotonMap3D* map) {
    if (!map) return;
    free(map->records);
    RuntimeCausticPhotonMap3D_Init(map);
}

bool RuntimeCausticPhotonMap3D_StoreRecord(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonMapRecord3D* record) {
    RuntimeCausticPhotonMapRecord3D stored;

    if (!map) return false;
    map->storeAttemptCount += 1u;
    if (!RuntimeCausticPhotonMap3D_IsAllocated(map) || !record ||
        map->recordCount >= map->recordCapacity) {
        map->storeRejectedCount += 1u;
        return false;
    }
    if (!(record->pathPdf > 1.0e-12) || !(record->queryRadius > 1.0e-9) ||
        !(photon_map_luma(record->flux) > 1.0e-12)) {
        map->storeRejectedCount += 1u;
        return false;
    }
    stored = *record;
    stored.normal = photon_map_normalize_or_default(stored.normal, vec3(0.0, 1.0, 0.0));
    stored.incidentDirection =
        photon_map_normalize_or_default(stored.incidentDirection, vec3(0.0, -1.0, 0.0));
    stored.queryRadius = photon_map_clamp(stored.queryRadius, 0.001, 10.0);
    map->records[map->recordCount++] = stored;
    map->storeAcceptedCount += 1u;
    return true;
}

bool RuntimeCausticPhotonMap3D_StoreSurfaceHit(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonSurfaceHit3D* hit,
    double path_pdf,
    double query_radius) {
    RuntimeCausticPhotonMapRecord3D record;

    if (!hit) {
        if (map) {
            map->storeAttemptCount += 1u;
            map->storeRejectedCount += 1u;
        }
        return false;
    }
    memset(&record, 0, sizeof(record));
    record.photonId = hit->photonId;
    record.depth = hit->depth;
    record.position = hit->position;
    record.normal = hit->normal;
    record.incidentDirection = hit->incidentDirection;
    record.flux = hit->flux;
    record.pathPdf = path_pdf;
    record.queryRadius = query_radius > 0.0 ? query_radius : hit->footprintRadius;
    record.sceneObjectIndex = hit->sceneObjectIndex;
    record.primitiveIndex = hit->primitiveIndex;
    record.triangleIndex = hit->triangleIndex;
    return RuntimeCausticPhotonMap3D_StoreRecord(map, &record);
}

bool RuntimeCausticPhotonMap3D_StoreTraceReceiver(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonTrace3D* trace,
    Vec3 receiver_normal,
    double query_radius,
    int receiver_scene_object_index,
    int receiver_primitive_index,
    int receiver_triangle_index) {
    RuntimeCausticPhotonMapRecord3D record;

    if (!trace || !trace->valid || trace->receiverPlaneT <= 0.0) {
        if (map) {
            map->storeAttemptCount += 1u;
            map->storeRejectedCount += 1u;
        }
        return false;
    }
    memset(&record, 0, sizeof(record));
    record.photonId = trace->sample.photonId;
    record.depth = trace->finalState.depth;
    record.position = trace->receiverCrossing;
    record.normal = receiver_normal;
    record.incidentDirection = trace->postExitDirection;
    record.flux = trace->finalState.throughput;
    record.pathPdf = trace->finalState.pathPdf;
    record.queryRadius = query_radius;
    record.sceneObjectIndex = receiver_scene_object_index;
    record.primitiveIndex = receiver_primitive_index;
    record.triangleIndex = receiver_triangle_index;
    return RuntimeCausticPhotonMap3D_StoreRecord(map, &record);
}

bool RuntimeCausticPhotonMap3D_Query(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonMapQuery3D* query,
    RuntimeCausticPhotonMapQueryResult3D* out_result) {
    RuntimeCausticPhotonMapQuery3D default_query;
    const RuntimeCausticPhotonMapQuery3D* active_query = query;
    RuntimeCausticPhotonMapQueryResult3D result;
    Vec3 query_normal = vec3(0.0, 1.0, 0.0);
    double query_radius = 0.10;
    double min_normal_dot = 0.25;

    memset(&result, 0, sizeof(result));
    result.nearestDistance = 0.0;
    result.nearestNormalDot = 0.0;
    if (out_result) *out_result = result;
    if (!map || !out_result) return false;
    map->queryCount += 1u;
    if (!RuntimeCausticPhotonMap3D_IsAllocated(map)) {
        map->lastQuery = result;
        return false;
    }
    if (!active_query) {
        RuntimeCausticPhotonMap3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }
    query_normal =
        photon_map_normalize_or_default(active_query->normal, vec3(0.0, 1.0, 0.0));
    query_radius = photon_map_clamp(active_query->radius, 0.001, 10.0);
    min_normal_dot = photon_map_clamp(active_query->minNormalDot, 0.0, 1.0);

    for (uint64_t i = 0u; i < map->recordCount; ++i) {
        const RuntimeCausticPhotonMapRecord3D* record = &map->records[i];
        Vec3 delta = vec3_sub(active_query->position, record->position);
        double radius = fmax(query_radius, record->queryRadius);
        double d2 = vec3_dot(delta, delta);
        double distance = sqrt(fmax(d2, 0.0));
        double normal_dot = fabs(vec3_dot(query_normal, record->normal));
        double weight = 0.0;
        Vec3 normalized_flux = vec3(0.0, 0.0, 0.0);

        if (result.candidateCount == 0u || distance < result.nearestDistance) {
            result.nearestDistance = distance;
            result.nearestNormalDot = normal_dot;
        }
        result.candidateCount += 1u;
        if (d2 > radius * radius || normal_dot < min_normal_dot ||
            !photon_map_receiver_identity_matches(record, active_query)) {
            continue;
        }
        weight = exp(-d2 / (2.0 * radius * radius)) *
                 photon_map_clamp(normal_dot, 0.0, 1.0) *
                 photon_map_kernel_area_normalization(radius);
        normalized_flux = vec3_scale(record->flux, 1.0 / record->pathPdf);
        result.flux = vec3_add(result.flux, vec3_scale(normalized_flux, weight));
        result.weightSum += weight;
        result.contributingCount += 1u;
    }

    result.hit = result.contributingCount > 0u &&
                 (result.flux.x > 0.0 || result.flux.y > 0.0 || result.flux.z > 0.0);
    if (result.hit) map->queryHitCount += 1u;
    map->lastQuery = result;
    *out_result = result;
    return result.hit;
}

void RuntimeCausticPhotonMap3D_SnapshotDiagnostics(
    const RuntimeCausticPhotonMap3D* map,
    RuntimeCausticPhotonMapDiagnostics3D* out_diagnostics) {
    RuntimeCausticPhotonMapDiagnostics3D diagnostics;

    if (!out_diagnostics) return;
    memset(&diagnostics, 0, sizeof(diagnostics));
    if (!RuntimeCausticPhotonMap3D_IsAllocated(map)) {
        *out_diagnostics = diagnostics;
        return;
    }
    diagnostics.allocated = true;
    diagnostics.recordCapacity = map->recordCapacity;
    diagnostics.recordCount = map->recordCount;
    diagnostics.storeAttemptCount = map->storeAttemptCount;
    diagnostics.storeAcceptedCount = map->storeAcceptedCount;
    diagnostics.storeRejectedCount = map->storeRejectedCount;
    diagnostics.queryCount = map->queryCount;
    diagnostics.queryHitCount = map->queryHitCount;
    diagnostics.lastQueryCandidateCount = map->lastQuery.candidateCount;
    diagnostics.lastQueryContributingCount = map->lastQuery.contributingCount;
    diagnostics.lastQueryNearestDistance = map->lastQuery.nearestDistance;
    diagnostics.lastQueryNearestNormalDot = map->lastQuery.nearestNormalDot;
    diagnostics.lastQueryFlux = map->lastQuery.flux;
    for (uint64_t i = 0u; i < map->recordCount; ++i) {
        diagnostics.totalStoredFlux =
            vec3_add(diagnostics.totalStoredFlux, map->records[i].flux);
    }
    *out_diagnostics = diagnostics;
}
