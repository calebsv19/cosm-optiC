#include "render/runtime_caustic_beam_map_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_CAPACITY = 4096,
    RUNTIME_CAUSTIC_BEAM_MAP_MAX_CAPACITY = 262144,
    RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT = 4096,
    RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MIN_BUCKETS = 1024,
    RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MAX_BUCKETS = 65536
};

static const double RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_CELL_SIZE = 0.25;

static double beam_map_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double beam_map_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 beam_map_normalize_or_default(Vec3 value, Vec3 fallback) {
    if (!(vec3_length(value) > 1.0e-12)) return fallback;
    return vec3_normalize(value);
}

static bool beam_map_medium_matches(const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
                                    const RuntimeCausticBeamMapQuery3D* query) {
    if (!segment || !query || !query->requireMediumId) return true;
    return segment->mediumId == query->mediumId;
}

static double beam_map_kernel_area_normalization(double radius) {
    const double area = M_PI * radius * radius;
    if (!(area > 1.0e-12)) return 1.0;
    return 1.0 / area;
}

static double beam_map_positive_or_default(double value, double fallback) {
    return value > 0.0 ? value : fallback;
}

static uint64_t beam_map_accel_bucket_count(uint64_t capacity) {
    uint64_t bucket_count = capacity * 2u;
    if (bucket_count < RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MIN_BUCKETS) {
        bucket_count = RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MIN_BUCKETS;
    }
    if (bucket_count > RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MAX_BUCKETS) {
        bucket_count = RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_MAX_BUCKETS;
    }
    return bucket_count;
}

static int64_t beam_map_accel_cell_coord(double value, double cell_size) {
    return (int64_t)floor(value / cell_size);
}

static uint64_t beam_map_accel_hash_cell(int64_t x,
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

static void beam_map_accel_fill_int64(int64_t* values,
                                      uint64_t count,
                                      int64_t value) {
    if (!values) return;
    for (uint64_t i = 0u; i < count; ++i) {
        values[i] = value;
    }
}

static bool beam_map_accel_is_allocated(const RuntimeCausticBeamMap3D* map) {
    return map && map->accelerationBucketHeads && map->accelerationSegmentNext &&
           map->accelerationSegmentCellX && map->accelerationSegmentCellY &&
           map->accelerationSegmentCellZ && map->accelerationBucketCount > 0u &&
           map->accelerationCellSize > 1.0e-9;
}

static bool beam_map_accel_allocate(RuntimeCausticBeamMap3D* map,
                                    uint64_t capacity) {
    uint64_t bucket_count;

    if (!map || capacity == 0u) return false;
    bucket_count = beam_map_accel_bucket_count(capacity);
    map->accelerationBucketHeads =
        (int64_t*)malloc((size_t)bucket_count * sizeof(int64_t));
    map->accelerationSegmentNext =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationSegmentCellX =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationSegmentCellY =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationSegmentCellZ =
        (int64_t*)malloc((size_t)capacity * sizeof(int64_t));
    map->accelerationBucketCount = bucket_count;
    map->accelerationCellSize = RUNTIME_CAUSTIC_BEAM_MAP_ACCEL_CELL_SIZE;
    if (!beam_map_accel_is_allocated(map)) {
        free(map->accelerationBucketHeads);
        free(map->accelerationSegmentNext);
        free(map->accelerationSegmentCellX);
        free(map->accelerationSegmentCellY);
        free(map->accelerationSegmentCellZ);
        map->accelerationBucketHeads = NULL;
        map->accelerationSegmentNext = NULL;
        map->accelerationSegmentCellX = NULL;
        map->accelerationSegmentCellY = NULL;
        map->accelerationSegmentCellZ = NULL;
        map->accelerationBucketCount = 0u;
        map->accelerationCellSize = 0.0;
        return false;
    }
    beam_map_accel_fill_int64(map->accelerationBucketHeads, bucket_count, -1);
    beam_map_accel_fill_int64(map->accelerationSegmentNext, capacity, -1);
    beam_map_accel_fill_int64(map->accelerationSegmentCellX, capacity, 0);
    beam_map_accel_fill_int64(map->accelerationSegmentCellY, capacity, 0);
    beam_map_accel_fill_int64(map->accelerationSegmentCellZ, capacity, 0);
    return true;
}

static void beam_map_accel_clear(RuntimeCausticBeamMap3D* map) {
    if (!map) return;
    if (map->accelerationBucketHeads) {
        beam_map_accel_fill_int64(map->accelerationBucketHeads,
                                  map->accelerationBucketCount,
                                  -1);
    }
    if (map->accelerationSegmentNext) {
        beam_map_accel_fill_int64(map->accelerationSegmentNext,
                                  map->segmentCapacity,
                                  -1);
    }
    if (map->accelerationSegmentCellX) {
        beam_map_accel_fill_int64(map->accelerationSegmentCellX,
                                  map->segmentCapacity,
                                  0);
    }
    if (map->accelerationSegmentCellY) {
        beam_map_accel_fill_int64(map->accelerationSegmentCellY,
                                  map->segmentCapacity,
                                  0);
    }
    if (map->accelerationSegmentCellZ) {
        beam_map_accel_fill_int64(map->accelerationSegmentCellZ,
                                  map->segmentCapacity,
                                  0);
    }
    map->accelerationMaxSegmentRadius = 0.0;
    map->accelerationMaxSegmentHalfLength = 0.0;
    map->accelerationInsertedCount = 0u;
    map->accelerationFallbackLinearQueryCount = 0u;
    map->lastQueryAccelerationUsed = false;
    map->lastQueryGridCellVisitCount = 0u;
}

static void beam_map_accel_insert(
    RuntimeCausticBeamMap3D* map,
    uint64_t segment_index,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment) {
    Vec3 axis;
    Vec3 midpoint;
    double half_length;
    double radius;
    int64_t cell_x;
    int64_t cell_y;
    int64_t cell_z;
    uint64_t bucket;

    if (!beam_map_accel_is_allocated(map) || !segment ||
        segment_index >= map->segmentCapacity || segment_index > (uint64_t)INT64_MAX) {
        return;
    }
    axis = vec3_sub(segment->end, segment->start);
    half_length = 0.5 * vec3_length(axis);
    midpoint = vec3_scale(vec3_add(segment->start, segment->end), 0.5);
    radius = fmax(segment->radiusStart, segment->radiusEnd);
    cell_x = beam_map_accel_cell_coord(midpoint.x, map->accelerationCellSize);
    cell_y = beam_map_accel_cell_coord(midpoint.y, map->accelerationCellSize);
    cell_z = beam_map_accel_cell_coord(midpoint.z, map->accelerationCellSize);
    bucket = beam_map_accel_hash_cell(cell_x, cell_y, cell_z, map->accelerationBucketCount);
    map->accelerationSegmentCellX[segment_index] = cell_x;
    map->accelerationSegmentCellY[segment_index] = cell_y;
    map->accelerationSegmentCellZ[segment_index] = cell_z;
    map->accelerationSegmentNext[segment_index] = map->accelerationBucketHeads[bucket];
    map->accelerationBucketHeads[bucket] = (int64_t)segment_index;
    if (radius > map->accelerationMaxSegmentRadius) {
        map->accelerationMaxSegmentRadius = radius;
    }
    if (half_length > map->accelerationMaxSegmentHalfLength) {
        map->accelerationMaxSegmentHalfLength = half_length;
    }
    map->accelerationInsertedCount += 1u;
}

static bool beam_map_segment_closest_point(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    Vec3 position,
    Vec3* out_closest,
    double* out_t,
    double* out_distance) {
    Vec3 axis;
    Vec3 delta;
    double axis_len2;
    double t;
    Vec3 closest;
    Vec3 offset;

    if (!segment) return false;
    axis = vec3_sub(segment->end, segment->start);
    axis_len2 = vec3_dot(axis, axis);
    if (!(axis_len2 > 1.0e-12)) return false;
    delta = vec3_sub(position, segment->start);
    t = beam_map_clamp(vec3_dot(delta, axis) / axis_len2, 0.0, 1.0);
    closest = vec3_add(segment->start, vec3_scale(axis, t));
    offset = vec3_sub(position, closest);
    if (out_closest) *out_closest = closest;
    if (out_t) *out_t = t;
    if (out_distance) *out_distance = vec3_length(offset);
    return true;
}

static bool beam_map_query_limit_reached(RuntimeCausticBeamMapQueryResult3D* result) {
    if (!result) return true;
    if (result->candidateLimit > 0u && result->testedCount >= result->candidateLimit) {
        result->candidateLimitReached = true;
        return true;
    }
    return false;
}

static void beam_map_query_consider_segment(
    RuntimeCausticBeamMapQueryResult3D* result,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeCausticBeamMapQuery3D* active_query,
    Vec3 query_direction,
    double query_radius,
    double min_direction_dot) {
    Vec3 closest = vec3(0.0, 0.0, 0.0);
    double t = 0.0;
    double distance = 0.0;
    double radius = query_radius;
    double d2 = 0.0;
    double direction_dot;
    double weight;

    if (!result || !segment || !active_query) return;
    direction_dot = fabs(vec3_dot(query_direction, segment->direction));
    result->testedCount += 1u;
    if (!beam_map_segment_closest_point(segment,
                                        active_query->position,
                                        &closest,
                                        &t,
                                        &distance)) {
        return;
    }
    (void)closest;
    radius = fmax(query_radius,
                  segment->radiusStart +
                      (segment->radiusEnd - segment->radiusStart) * t);
    d2 = distance * distance;
    if (result->candidateCount == 0u || distance < result->nearestDistance) {
        result->nearestDistance = distance;
        result->nearestT = t;
        result->nearestDirectionDot = direction_dot;
    }
    result->candidateCount += 1u;
    if (d2 > radius * radius) {
        result->radiusRejectCount += 1u;
        return;
    }
    if (direction_dot < min_direction_dot) {
        result->directionRejectCount += 1u;
        return;
    }
    if (!beam_map_medium_matches(segment, active_query)) {
        result->mediumRejectCount += 1u;
        return;
    }
    weight = exp(-d2 / (2.0 * radius * radius)) *
             beam_map_clamp(direction_dot, 0.0, 1.0) *
             beam_map_kernel_area_normalization(radius) *
             segment->transmittance * segment->densityWeight;
    result->flux = vec3_add(result->flux, vec3_scale(segment->flux, weight));
    result->weightSum += weight;
    result->contributingCount += 1u;
}

void RuntimeCausticBeamMap3D_DefaultSettings(
    RuntimeCausticBeamMapSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->defaultCapacity = RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_CAPACITY;
    settings->defaultQueryRadius = 0.10;
    settings->minDirectionDot = 0.25;
    settings->defaultQueryCandidateLimit =
        RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT;
    settings->physicalEnergyScale = 1.0;
    settings->displayGain = 1.0;
}

void RuntimeCausticBeamMap3D_DefaultQuery(RuntimeCausticBeamMapQuery3D* query) {
    if (!query) return;
    memset(query, 0, sizeof(*query));
    query->direction = vec3(0.0, 0.0, 1.0);
    query->radius = 0.10;
    query->mediumId = -1;
    query->requireMediumId = true;
    query->minDirectionDot = 0.25;
    query->candidateLimit = RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_QUERY_CANDIDATE_LIMIT;
    query->physicalEnergyScale = 1.0;
    query->displayGain = 1.0;
}

void RuntimeCausticBeamMap3D_Init(RuntimeCausticBeamMap3D* map) {
    if (!map) return;
    memset(map, 0, sizeof(*map));
}

bool RuntimeCausticBeamMap3D_IsAllocated(const RuntimeCausticBeamMap3D* map) {
    return map && map->ownsSegments && map->segments && map->segmentCapacity > 0u;
}

bool RuntimeCausticBeamMap3D_Allocate(RuntimeCausticBeamMap3D* map,
                                      uint64_t segment_capacity) {
    RuntimeCausticBeamMap3D allocated;

    if (!map) return false;
    if (segment_capacity == 0u) {
        segment_capacity = RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_CAPACITY;
    }
    if (segment_capacity > RUNTIME_CAUSTIC_BEAM_MAP_MAX_CAPACITY) {
        segment_capacity = RUNTIME_CAUSTIC_BEAM_MAP_MAX_CAPACITY;
    }
    if (segment_capacity >
        (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticPhotonVolumeBeamSegment3D))) {
        return false;
    }

    RuntimeCausticBeamMap3D_Init(&allocated);
    allocated.segments = (RuntimeCausticPhotonVolumeBeamSegment3D*)calloc(
        (size_t)segment_capacity,
        sizeof(RuntimeCausticPhotonVolumeBeamSegment3D));
    if (!allocated.segments) return false;
    allocated.segmentCapacity = segment_capacity;
    allocated.ownsSegments = true;
    (void)beam_map_accel_allocate(&allocated, segment_capacity);

    RuntimeCausticBeamMap3D_Free(map);
    *map = allocated;
    return true;
}

void RuntimeCausticBeamMap3D_Clear(RuntimeCausticBeamMap3D* map) {
    if (!RuntimeCausticBeamMap3D_IsAllocated(map)) return;
    memset(map->segments,
           0,
           (size_t)map->segmentCapacity *
               sizeof(RuntimeCausticPhotonVolumeBeamSegment3D));
    map->segmentCount = 0u;
    map->storeAttemptCount = 0u;
    map->storeAcceptedCount = 0u;
    map->storeRejectedCount = 0u;
    map->queryCount = 0u;
    map->queryHitCount = 0u;
    map->totalQueriedPhysicalFlux = vec3(0.0, 0.0, 0.0);
    map->totalQueriedDisplayFlux = vec3(0.0, 0.0, 0.0);
    beam_map_accel_clear(map);
    memset(&map->lastQuery, 0, sizeof(map->lastQuery));
}

void RuntimeCausticBeamMap3D_Free(RuntimeCausticBeamMap3D* map) {
    if (!map) return;
    free(map->segments);
    free(map->accelerationBucketHeads);
    free(map->accelerationSegmentNext);
    free(map->accelerationSegmentCellX);
    free(map->accelerationSegmentCellY);
    free(map->accelerationSegmentCellZ);
    RuntimeCausticBeamMap3D_Init(map);
}

bool RuntimeCausticBeamMap3D_StoreSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment) {
    RuntimeCausticPhotonVolumeBeamSegment3D stored;
    Vec3 axis;

    if (!map) return false;
    map->storeAttemptCount += 1u;
    if (!RuntimeCausticBeamMap3D_IsAllocated(map) || !segment ||
        map->segmentCount >= map->segmentCapacity) {
        map->storeRejectedCount += 1u;
        return false;
    }
    axis = vec3_sub(segment->end, segment->start);
    if (!(vec3_dot(axis, axis) > 1.0e-12) ||
        !(segment->transmittance > 1.0e-12) ||
        !(segment->densityWeight > 1.0e-12) ||
        !(beam_map_luma(segment->flux) > 1.0e-12)) {
        map->storeRejectedCount += 1u;
        return false;
    }
    stored = *segment;
    stored.direction = beam_map_normalize_or_default(stored.direction,
                                                     vec3_normalize(axis));
    stored.radiusStart = beam_map_clamp(stored.radiusStart, 0.001, 10.0);
    stored.radiusEnd = beam_map_clamp(stored.radiusEnd, 0.001, 10.0);
    stored.transmittance = beam_map_clamp(stored.transmittance, 0.0, 1.0);
    stored.densityWeight = beam_map_clamp(stored.densityWeight, 0.0, 1.0e6);
    map->segments[map->segmentCount] = stored;
    beam_map_accel_insert(map, map->segmentCount, &stored);
    map->segmentCount += 1u;
    map->storeAcceptedCount += 1u;
    return true;
}

bool RuntimeCausticBeamMap3D_StoreTraceSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonTrace3D* trace,
    double radius_start,
    double radius_end,
    double transmittance,
    double density_weight,
    int medium_id) {
    RuntimeCausticPhotonVolumeBeamSegment3D segment;

    if (!trace || !trace->valid || trace->receiverPlaneT <= 0.0) {
        if (map) {
            map->storeAttemptCount += 1u;
            map->storeRejectedCount += 1u;
        }
        return false;
    }
    memset(&segment, 0, sizeof(segment));
    segment.photonId = trace->sample.photonId;
    segment.depth = trace->finalState.depth;
    segment.start = trace->postExitOrigin;
    segment.end = trace->receiverCrossing;
    segment.direction = trace->postExitDirection;
    segment.flux = trace->finalState.throughput;
    segment.radiusStart = radius_start;
    segment.radiusEnd = radius_end;
    segment.transmittance = transmittance;
    segment.densityWeight = density_weight;
    segment.mediumId = medium_id;
    return RuntimeCausticBeamMap3D_StoreSegment(map, &segment);
}

bool RuntimeCausticBeamMap3D_Query(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticBeamMapQuery3D* query,
    RuntimeCausticBeamMapQueryResult3D* out_result) {
    RuntimeCausticBeamMapQuery3D default_query;
    const RuntimeCausticBeamMapQuery3D* active_query = query;
    RuntimeCausticBeamMapQueryResult3D result;
    Vec3 query_direction = vec3(0.0, 0.0, 1.0);
    double query_radius = 0.10;
    double min_direction_dot = 0.25;
    double physical_energy_scale = 1.0;
    double display_gain = 1.0;
    bool acceleration_used = false;
    uint64_t grid_cell_visit_count = 0u;

    memset(&result, 0, sizeof(result));
    if (out_result) *out_result = result;
    if (!map || !out_result) return false;
    map->queryCount += 1u;
    if (!RuntimeCausticBeamMap3D_IsAllocated(map)) {
        map->lastQuery = result;
        return false;
    }
    if (!active_query) {
        RuntimeCausticBeamMap3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }
    query_direction =
        beam_map_normalize_or_default(active_query->direction, vec3(0.0, 0.0, 1.0));
    query_radius = beam_map_clamp(active_query->radius, 0.001, 10.0);
    min_direction_dot = beam_map_clamp(active_query->minDirectionDot, 0.0, 1.0);
    result.candidateLimit = active_query->candidateLimit;
    physical_energy_scale =
        beam_map_positive_or_default(active_query->physicalEnergyScale, 1.0);
    display_gain = beam_map_positive_or_default(active_query->displayGain, 1.0);
    result.physicalEnergyScale = physical_energy_scale;
    result.displayGain = display_gain;

    if (beam_map_accel_is_allocated(map) &&
        map->accelerationInsertedCount == map->segmentCount) {
        const double search_radius =
            query_radius + fmax(map->accelerationMaxSegmentRadius, 0.0) +
            fmax(map->accelerationMaxSegmentHalfLength, 0.0);
        const int64_t query_cell_x =
            beam_map_accel_cell_coord(active_query->position.x, map->accelerationCellSize);
        const int64_t query_cell_y =
            beam_map_accel_cell_coord(active_query->position.y, map->accelerationCellSize);
        const int64_t query_cell_z =
            beam_map_accel_cell_coord(active_query->position.z, map->accelerationCellSize);
        const int64_t cell_radius =
            (int64_t)ceil(search_radius / map->accelerationCellSize);
        bool stop = false;

        acceleration_used = true;
        for (int64_t z = query_cell_z - cell_radius; z <= query_cell_z + cell_radius && !stop; ++z) {
            for (int64_t y = query_cell_y - cell_radius; y <= query_cell_y + cell_radius && !stop; ++y) {
                for (int64_t x = query_cell_x - cell_radius; x <= query_cell_x + cell_radius; ++x) {
                    const uint64_t bucket =
                        beam_map_accel_hash_cell(x, y, z, map->accelerationBucketCount);
                    int64_t segment_index = map->accelerationBucketHeads[bucket];
                    grid_cell_visit_count += 1u;
                    while (segment_index >= 0) {
                        const uint64_t index = (uint64_t)segment_index;
                        const int64_t next = map->accelerationSegmentNext[index];
                        if (map->accelerationSegmentCellX[index] == x &&
                            map->accelerationSegmentCellY[index] == y &&
                            map->accelerationSegmentCellZ[index] == z) {
                            if (beam_map_query_limit_reached(&result)) {
                                stop = true;
                                break;
                            }
                            beam_map_query_consider_segment(&result,
                                                            &map->segments[index],
                                                            active_query,
                                                            query_direction,
                                                            query_radius,
                                                            min_direction_dot);
                        }
                        segment_index = next;
                    }
                    if (stop) break;
                }
            }
        }
    } else {
        map->accelerationFallbackLinearQueryCount += 1u;
        for (uint64_t i = 0u; i < map->segmentCount; ++i) {
            if (beam_map_query_limit_reached(&result)) break;
            beam_map_query_consider_segment(&result,
                                            &map->segments[i],
                                            active_query,
                                            query_direction,
                                            query_radius,
                                            min_direction_dot);
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

void RuntimeCausticBeamMap3D_SnapshotDiagnostics(
    const RuntimeCausticBeamMap3D* map,
    RuntimeCausticBeamMapDiagnostics3D* out_diagnostics) {
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    if (!out_diagnostics) return;
    memset(&diagnostics, 0, sizeof(diagnostics));
    if (!RuntimeCausticBeamMap3D_IsAllocated(map)) {
        *out_diagnostics = diagnostics;
        return;
    }
    diagnostics.allocated = true;
    diagnostics.segmentCapacity = map->segmentCapacity;
    diagnostics.segmentCount = map->segmentCount;
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
    diagnostics.lastQueryNearestT = map->lastQuery.nearestT;
    diagnostics.lastQueryNearestDirectionDot = map->lastQuery.nearestDirectionDot;
    diagnostics.lastQueryFlux = map->lastQuery.flux;
    diagnostics.lastQueryPhysicalFlux = map->lastQuery.physicalFlux;
    diagnostics.lastQueryDisplayFlux = map->lastQuery.displayFlux;
    diagnostics.lastQueryPhysicalEnergyScale = map->lastQuery.physicalEnergyScale;
    diagnostics.lastQueryDisplayGain = map->lastQuery.displayGain;
    diagnostics.totalQueriedPhysicalFlux = map->totalQueriedPhysicalFlux;
    diagnostics.totalQueriedDisplayFlux = map->totalQueriedDisplayFlux;
    diagnostics.accelerationAllocated = beam_map_accel_is_allocated(map);
    diagnostics.accelerationCellSize = map->accelerationCellSize;
    diagnostics.accelerationBucketCount = map->accelerationBucketCount;
    diagnostics.accelerationInsertedCount = map->accelerationInsertedCount;
    diagnostics.accelerationFallbackLinearQueryCount =
        map->accelerationFallbackLinearQueryCount;
    for (uint64_t i = 0u; i < map->segmentCount; ++i) {
        diagnostics.totalStoredFlux =
            vec3_add(diagnostics.totalStoredFlux, map->segments[i].flux);
    }
    *out_diagnostics = diagnostics;
}
