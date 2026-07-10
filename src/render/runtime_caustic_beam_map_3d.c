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
    RUNTIME_CAUSTIC_BEAM_MAP_MAX_CAPACITY = 262144
};

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

void RuntimeCausticBeamMap3D_DefaultSettings(
    RuntimeCausticBeamMapSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->defaultCapacity = RUNTIME_CAUSTIC_BEAM_MAP_DEFAULT_CAPACITY;
    settings->defaultQueryRadius = 0.10;
    settings->minDirectionDot = 0.25;
}

void RuntimeCausticBeamMap3D_DefaultQuery(RuntimeCausticBeamMapQuery3D* query) {
    if (!query) return;
    memset(query, 0, sizeof(*query));
    query->direction = vec3(0.0, 0.0, 1.0);
    query->radius = 0.10;
    query->mediumId = -1;
    query->requireMediumId = true;
    query->minDirectionDot = 0.25;
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
    memset(&map->lastQuery, 0, sizeof(map->lastQuery));
}

void RuntimeCausticBeamMap3D_Free(RuntimeCausticBeamMap3D* map) {
    if (!map) return;
    free(map->segments);
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
    map->segments[map->segmentCount++] = stored;
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

    for (uint64_t i = 0u; i < map->segmentCount; ++i) {
        const RuntimeCausticPhotonVolumeBeamSegment3D* segment = &map->segments[i];
        Vec3 closest = vec3(0.0, 0.0, 0.0);
        double t = 0.0;
        double distance = 0.0;
        double radius = query_radius;
        double d2 = 0.0;
        double direction_dot = fabs(vec3_dot(query_direction, segment->direction));
        double weight = 0.0;

        if (!beam_map_segment_closest_point(segment,
                                            active_query->position,
                                            &closest,
                                            &t,
                                            &distance)) {
            continue;
        }
        (void)closest;
        radius = fmax(query_radius,
                      segment->radiusStart +
                          (segment->radiusEnd - segment->radiusStart) * t);
        d2 = distance * distance;
        if (result.candidateCount == 0u || distance < result.nearestDistance) {
            result.nearestDistance = distance;
            result.nearestT = t;
            result.nearestDirectionDot = direction_dot;
        }
        result.candidateCount += 1u;
        if (d2 > radius * radius || direction_dot < min_direction_dot ||
            !beam_map_medium_matches(segment, active_query)) {
            continue;
        }
        weight = exp(-d2 / (2.0 * radius * radius)) *
                 beam_map_clamp(direction_dot, 0.0, 1.0) *
                 beam_map_kernel_area_normalization(radius) *
                 segment->transmittance * segment->densityWeight;
        result.flux = vec3_add(result.flux, vec3_scale(segment->flux, weight));
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
    diagnostics.lastQueryCandidateCount = map->lastQuery.candidateCount;
    diagnostics.lastQueryContributingCount = map->lastQuery.contributingCount;
    diagnostics.lastQueryNearestDistance = map->lastQuery.nearestDistance;
    diagnostics.lastQueryNearestT = map->lastQuery.nearestT;
    diagnostics.lastQueryNearestDirectionDot = map->lastQuery.nearestDirectionDot;
    diagnostics.lastQueryFlux = map->lastQuery.flux;
    for (uint64_t i = 0u; i < map->segmentCount; ++i) {
        diagnostics.totalStoredFlux =
            vec3_add(diagnostics.totalStoredFlux, map->segments[i].flux);
    }
    *out_diagnostics = diagnostics;
}
