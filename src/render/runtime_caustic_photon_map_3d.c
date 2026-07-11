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
    RUNTIME_CAUSTIC_PHOTON_MAP_MAX_CAPACITY = 262144,
    RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT = 4096,
    RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MIN_BUCKETS = 1024,
    RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MAX_BUCKETS = 65536
};

static const double RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_CELL_SIZE = 0.25;

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

static double photon_map_positive_or_default(double value, double fallback) {
    return value > 0.0 ? value : fallback;
}

static uint64_t photon_map_accel_bucket_count(uint64_t capacity) {
    uint64_t bucket_count = capacity * 2u;
    if (bucket_count < RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MIN_BUCKETS) {
        bucket_count = RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MIN_BUCKETS;
    }
    if (bucket_count > RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MAX_BUCKETS) {
        bucket_count = RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_MAX_BUCKETS;
    }
    return bucket_count;
}

static int64_t photon_map_accel_cell_coord(double value, double cell_size) {
    return (int64_t)floor(value / cell_size);
}

static uint64_t photon_map_accel_hash_cell(int64_t x,
                                           int64_t y,
                                           int64_t z,
                                           uint64_t bucket_count) {
    uint64_t ux = (uint64_t)x;
    uint64_t uy = (uint64_t)y;
    uint64_t uz = (uint64_t)z;
    uint64_t h = ux * 73856093u;
    h ^= uy * 19349663u;
    h ^= uz * 83492791u;
    return bucket_count > 0u ? h % bucket_count : 0u;
}

static void photon_map_accel_fill_int64(int64_t* values,
                                        uint64_t count,
                                        int64_t value) {
    if (!values) return;
    for (uint64_t i = 0u; i < count; ++i) {
        values[i] = value;
    }
}

static bool photon_map_accel_is_allocated(const RuntimeCausticPhotonMap3D* map) {
    return map && map->accelerationBucketHeads && map->accelerationRecordNext &&
           map->accelerationRecordCellX && map->accelerationRecordCellY &&
           map->accelerationRecordCellZ && map->accelerationBucketCount > 0u &&
           map->accelerationCellSize > 1.0e-9;
}

static bool photon_map_accel_allocate(RuntimeCausticPhotonMap3D* map,
                                      uint64_t capacity) {
    uint64_t bucket_count;

    if (!map || capacity == 0u) return false;
    bucket_count = photon_map_accel_bucket_count(capacity);
    map->accelerationBucketHeads =
        (int64_t*)malloc((size_t)bucket_count * sizeof(int64_t));
    map->accelerationRecordNext =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationRecordCellX =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationRecordCellY =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationRecordCellZ =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationBucketCount = bucket_count;
    map->accelerationCellSize = RUNTIME_CAUSTIC_PHOTON_MAP_ACCEL_CELL_SIZE;
    if (!photon_map_accel_is_allocated(map)) {
        free(map->accelerationBucketHeads);
        free(map->accelerationRecordNext);
        free(map->accelerationRecordCellX);
        free(map->accelerationRecordCellY);
        free(map->accelerationRecordCellZ);
        map->accelerationBucketHeads = NULL;
        map->accelerationRecordNext = NULL;
        map->accelerationRecordCellX = NULL;
        map->accelerationRecordCellY = NULL;
        map->accelerationRecordCellZ = NULL;
        map->accelerationBucketCount = 0u;
        map->accelerationCellSize = 0.0;
        return false;
    }
    photon_map_accel_fill_int64(map->accelerationBucketHeads, bucket_count, -1);
    photon_map_accel_fill_int64(map->accelerationRecordNext, capacity, -1);
    photon_map_accel_fill_int64(map->accelerationRecordCellX, capacity, 0);
    photon_map_accel_fill_int64(map->accelerationRecordCellY, capacity, 0);
    photon_map_accel_fill_int64(map->accelerationRecordCellZ, capacity, 0);
    return true;
}

static void photon_map_accel_clear(RuntimeCausticPhotonMap3D* map) {
    if (!map) return;
    if (map->accelerationBucketHeads) {
        photon_map_accel_fill_int64(map->accelerationBucketHeads,
                                    map->accelerationBucketCount,
                                    -1);
    }
    if (map->accelerationRecordNext) {
        photon_map_accel_fill_int64(map->accelerationRecordNext,
                                    map->recordCapacity,
                                    -1);
    }
    if (map->accelerationRecordCellX) {
        photon_map_accel_fill_int64(map->accelerationRecordCellX,
                                    map->recordCapacity,
                                    0);
    }
    if (map->accelerationRecordCellY) {
        photon_map_accel_fill_int64(map->accelerationRecordCellY,
                                    map->recordCapacity,
                                    0);
    }
    if (map->accelerationRecordCellZ) {
        photon_map_accel_fill_int64(map->accelerationRecordCellZ,
                                    map->recordCapacity,
                                    0);
    }
    map->accelerationMaxRecordQueryRadius = 0.0;
    map->accelerationInsertedCount = 0u;
    map->accelerationFallbackLinearQueryCount = 0u;
    map->lastQueryAccelerationUsed = false;
    map->lastQueryGridCellVisitCount = 0u;
}

static void photon_map_accel_insert(RuntimeCausticPhotonMap3D* map,
                                    uint64_t record_index,
                                    const RuntimeCausticPhotonMapRecord3D* record) {
    int64_t cell_x;
    int64_t cell_y;
    int64_t cell_z;
    uint64_t bucket;

    if (!photon_map_accel_is_allocated(map) || !record ||
        record_index >= map->recordCapacity || record_index > (uint64_t)INT64_MAX) {
        return;
    }
    cell_x = photon_map_accel_cell_coord(record->position.x, map->accelerationCellSize);
    cell_y = photon_map_accel_cell_coord(record->position.y, map->accelerationCellSize);
    cell_z = photon_map_accel_cell_coord(record->position.z, map->accelerationCellSize);
    bucket = photon_map_accel_hash_cell(cell_x, cell_y, cell_z, map->accelerationBucketCount);
    map->accelerationRecordCellX[record_index] = cell_x;
    map->accelerationRecordCellY[record_index] = cell_y;
    map->accelerationRecordCellZ[record_index] = cell_z;
    map->accelerationRecordNext[record_index] = map->accelerationBucketHeads[bucket];
    map->accelerationBucketHeads[bucket] = (int64_t)record_index;
    if (record->queryRadius > map->accelerationMaxRecordQueryRadius) {
        map->accelerationMaxRecordQueryRadius = record->queryRadius;
    }
    map->accelerationInsertedCount += 1u;
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

static bool photon_map_query_limit_reached(
    RuntimeCausticPhotonMapQueryResult3D* result) {
    if (!result) return true;
    if (result->candidateLimit > 0u && result->testedCount >= result->candidateLimit) {
        result->candidateLimitReached = true;
        return true;
    }
    return false;
}

static void photon_map_query_consider_record(
    RuntimeCausticPhotonMapQueryResult3D* result,
    const RuntimeCausticPhotonMapRecord3D* record,
    const RuntimeCausticPhotonMapQuery3D* active_query,
    Vec3 query_normal,
    double query_radius,
    double min_normal_dot) {
    Vec3 delta;
    double radius;
    double d2;
    double distance;
    double normal_dot;
    double weight;
    Vec3 normalized_flux;

    if (!result || !record || !active_query) return;
    delta = vec3_sub(active_query->position, record->position);
    radius = fmax(query_radius, record->queryRadius);
    d2 = vec3_dot(delta, delta);
    distance = sqrt(fmax(d2, 0.0));
    normal_dot = fabs(vec3_dot(query_normal, record->normal));
    result->testedCount += 1u;
    if (result->candidateCount == 0u || distance < result->nearestDistance) {
        result->nearestDistance = distance;
        result->nearestNormalDot = normal_dot;
    }
    result->candidateCount += 1u;
    if (d2 > radius * radius || normal_dot < min_normal_dot ||
        !photon_map_receiver_identity_matches(record, active_query)) {
        return;
    }
    weight = exp(-d2 / (2.0 * radius * radius)) *
             photon_map_clamp(normal_dot, 0.0, 1.0) *
             photon_map_kernel_area_normalization(radius);
    normalized_flux = vec3_scale(record->flux, 1.0 / record->pathPdf);
    result->flux = vec3_add(result->flux, vec3_scale(normalized_flux, weight));
    result->weightSum += weight;
    result->contributingCount += 1u;
}

void RuntimeCausticPhotonMap3D_DefaultSettings(
    RuntimeCausticPhotonMapSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->defaultCapacity = RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_CAPACITY;
    settings->defaultQueryRadius = 0.10;
    settings->minNormalDot = 0.25;
    settings->defaultQueryCandidateLimit =
        RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT;
    settings->physicalEnergyScale = 1.0;
    settings->displayGain = 1.0;
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
    query->candidateLimit = RUNTIME_CAUSTIC_PHOTON_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT;
    query->physicalEnergyScale = 1.0;
    query->displayGain = 1.0;
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
    (void)photon_map_accel_allocate(&allocated, record_capacity);

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
    map->totalQueriedPhysicalFlux = vec3(0.0, 0.0, 0.0);
    map->totalQueriedDisplayFlux = vec3(0.0, 0.0, 0.0);
    photon_map_accel_clear(map);
    memset(&map->lastQuery, 0, sizeof(map->lastQuery));
}

void RuntimeCausticPhotonMap3D_Free(RuntimeCausticPhotonMap3D* map) {
    if (!map) return;
    free(map->records);
    free(map->accelerationBucketHeads);
    free(map->accelerationRecordNext);
    free(map->accelerationRecordCellX);
    free(map->accelerationRecordCellY);
    free(map->accelerationRecordCellZ);
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
    map->records[map->recordCount] = stored;
    photon_map_accel_insert(map, map->recordCount, &stored);
    map->recordCount += 1u;
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
    double physical_energy_scale = 1.0;
    double display_gain = 1.0;
    bool acceleration_used = false;
    uint64_t grid_cell_visit_count = 0u;

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
    result.candidateLimit = active_query->candidateLimit;
    physical_energy_scale =
        photon_map_positive_or_default(active_query->physicalEnergyScale, 1.0);
    display_gain = photon_map_positive_or_default(active_query->displayGain, 1.0);
    result.physicalEnergyScale = physical_energy_scale;
    result.displayGain = display_gain;

    if (photon_map_accel_is_allocated(map) && map->accelerationInsertedCount == map->recordCount) {
        const double search_radius =
            query_radius + fmax(map->accelerationMaxRecordQueryRadius, 0.0);
        const int64_t query_cell_x =
            photon_map_accel_cell_coord(active_query->position.x, map->accelerationCellSize);
        const int64_t query_cell_y =
            photon_map_accel_cell_coord(active_query->position.y, map->accelerationCellSize);
        const int64_t query_cell_z =
            photon_map_accel_cell_coord(active_query->position.z, map->accelerationCellSize);
        const int64_t cell_radius =
            (int64_t)ceil(search_radius / map->accelerationCellSize);
        bool stop = false;

        acceleration_used = true;
        for (int64_t z = query_cell_z - cell_radius; z <= query_cell_z + cell_radius && !stop; ++z) {
            for (int64_t y = query_cell_y - cell_radius; y <= query_cell_y + cell_radius && !stop; ++y) {
                for (int64_t x = query_cell_x - cell_radius; x <= query_cell_x + cell_radius; ++x) {
                    const uint64_t bucket =
                        photon_map_accel_hash_cell(x, y, z, map->accelerationBucketCount);
                    int64_t record_index = map->accelerationBucketHeads[bucket];
                    grid_cell_visit_count += 1u;
                    while (record_index >= 0) {
                        const uint64_t index = (uint64_t)record_index;
                        const int64_t next = map->accelerationRecordNext[index];
                        if (map->accelerationRecordCellX[index] == x &&
                            map->accelerationRecordCellY[index] == y &&
                            map->accelerationRecordCellZ[index] == z) {
                            if (photon_map_query_limit_reached(&result)) {
                                stop = true;
                                break;
                            }
                            photon_map_query_consider_record(&result,
                                                             &map->records[index],
                                                             active_query,
                                                             query_normal,
                                                             query_radius,
                                                             min_normal_dot);
                        }
                        record_index = next;
                    }
                    if (stop) break;
                }
            }
        }
    } else {
        map->accelerationFallbackLinearQueryCount += 1u;
        for (uint64_t i = 0u; i < map->recordCount; ++i) {
            if (photon_map_query_limit_reached(&result)) break;
            photon_map_query_consider_record(&result,
                                             &map->records[i],
                                             active_query,
                                             query_normal,
                                             query_radius,
                                             min_normal_dot);
        }
    }

    if (physical_energy_scale != 1.0) {
        result.flux = vec3_scale(result.flux, physical_energy_scale);
    }
    result.physicalFlux = result.flux;
    result.displayFlux = vec3_scale(result.physicalFlux, display_gain);
    result.hit = result.contributingCount > 0u &&
                 (result.flux.x > 0.0 || result.flux.y > 0.0 || result.flux.z > 0.0);
    if (result.hit) map->queryHitCount += 1u;
    map->totalQueriedPhysicalFlux =
        vec3_add(map->totalQueriedPhysicalFlux, result.physicalFlux);
    map->totalQueriedDisplayFlux =
        vec3_add(map->totalQueriedDisplayFlux, result.displayFlux);
    map->lastQueryAccelerationUsed = acceleration_used;
    map->lastQueryGridCellVisitCount = grid_cell_visit_count;
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
    diagnostics.lastQueryTestedCount = map->lastQuery.testedCount;
    diagnostics.lastQueryCandidateCount = map->lastQuery.candidateCount;
    diagnostics.lastQueryContributingCount = map->lastQuery.contributingCount;
    diagnostics.lastQueryCandidateLimit = map->lastQuery.candidateLimit;
    diagnostics.lastQueryCandidateLimitReached = map->lastQuery.candidateLimitReached;
    diagnostics.lastQueryAccelerationUsed = map->lastQueryAccelerationUsed;
    diagnostics.lastQueryGridCellVisitCount = map->lastQueryGridCellVisitCount;
    diagnostics.lastQueryNearestDistance = map->lastQuery.nearestDistance;
    diagnostics.lastQueryNearestNormalDot = map->lastQuery.nearestNormalDot;
    diagnostics.lastQueryFlux = map->lastQuery.flux;
    diagnostics.lastQueryPhysicalFlux = map->lastQuery.physicalFlux;
    diagnostics.lastQueryDisplayFlux = map->lastQuery.displayFlux;
    diagnostics.lastQueryPhysicalEnergyScale = map->lastQuery.physicalEnergyScale;
    diagnostics.lastQueryDisplayGain = map->lastQuery.displayGain;
    diagnostics.totalQueriedPhysicalFlux = map->totalQueriedPhysicalFlux;
    diagnostics.totalQueriedDisplayFlux = map->totalQueriedDisplayFlux;
    diagnostics.accelerationAllocated = photon_map_accel_is_allocated(map);
    diagnostics.accelerationCellSize = map->accelerationCellSize;
    diagnostics.accelerationBucketCount = map->accelerationBucketCount;
    diagnostics.accelerationInsertedCount = map->accelerationInsertedCount;
    diagnostics.accelerationFallbackLinearQueryCount =
        map->accelerationFallbackLinearQueryCount;
    for (uint64_t i = 0u; i < map->recordCount; ++i) {
        diagnostics.totalStoredFlux =
            vec3_add(diagnostics.totalStoredFlux, map->records[i].flux);
    }
    *out_diagnostics = diagnostics;
}
