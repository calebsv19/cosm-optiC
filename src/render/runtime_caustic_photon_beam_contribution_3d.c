#include "render/runtime_caustic_photon_integration_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_volume_3d_sampling.h"

static bool beam_contribution_has_energy(Vec3 value) {
    return value.x > 0.0 || value.y > 0.0 || value.z > 0.0;
}

static double beam_contribution_positive_or_default(double value, double fallback) {
    return isfinite(value) && value > 0.0 ? value : fallback;
}

static bool beam_contribution_position_in_volume(const RuntimeVolumeAttachment3D* volume,
                                                 Vec3 position) {
    if (!volume) return false;
    return position.x >= volume->grid.boundsMin.x &&
           position.x <= volume->grid.boundsMax.x &&
           position.y >= volume->grid.boundsMin.y &&
           position.y <= volume->grid.boundsMax.y &&
           position.z >= volume->grid.boundsMin.z &&
           position.z <= volume->grid.boundsMax.z;
}

static bool beam_contribution_segment_volume_midpoint(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeVolumeAttachment3D* volume,
    Vec3* out_position) {
    Vec3 axis;
    double t_min = 0.0;
    double t_max = 1.0;
    double start_values[3];
    double axis_values[3];
    const double min_values[3] = {
        volume->grid.boundsMin.x, volume->grid.boundsMin.y, volume->grid.boundsMin.z};
    const double max_values[3] = {
        volume->grid.boundsMax.x, volume->grid.boundsMax.y, volume->grid.boundsMax.z};
    int axis_index;

    if (!segment || !volume || !out_position) return false;
    axis = vec3_sub(segment->end, segment->start);
    start_values[0] = segment->start.x;
    start_values[1] = segment->start.y;
    start_values[2] = segment->start.z;
    axis_values[0] = axis.x;
    axis_values[1] = axis.y;
    axis_values[2] = axis.z;
    for (axis_index = 0; axis_index < 3; ++axis_index) {
        const double start = start_values[axis_index];
        const double direction = axis_values[axis_index];
        double entry;
        double exit;

        if (fabs(direction) <= 1.0e-12) {
            if (start < min_values[axis_index] || start > max_values[axis_index]) {
                return false;
            }
            continue;
        }
        entry = (min_values[axis_index] - start) / direction;
        exit = (max_values[axis_index] - start) / direction;
        if (entry > exit) {
            const double swap = entry;
            entry = exit;
            exit = swap;
        }
        if (entry > t_min) t_min = entry;
        if (exit < t_max) t_max = exit;
        if (t_max < t_min) return false;
    }
    *out_position = vec3_add(segment->start, vec3_scale(axis, (t_min + t_max) * 0.5));
    return beam_contribution_position_in_volume(volume, *out_position);
}

bool RuntimeCausticPhotonIntegration3D_SelectVolumeBeamQueryForVolume(
    const RuntimeCausticBeamMap3D* beam_map,
    const RuntimeVolumeAttachment3D* volume,
    int medium_id,
    double radius,
    RuntimeCausticBeamMapQuery3D* out_query) {
    RuntimeCausticBeamMapQuery3D query;
    uint64_t i;

    if (out_query) memset(out_query, 0, sizeof(*out_query));
    if (!beam_map || !volume || !out_query || !beam_map->segments ||
        beam_map->segmentCount == 0u || !RuntimeVolumeGrid3D_IsConfigured(&volume->grid)) {
        return false;
    }

    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.mediumId = medium_id;
    query.requireMediumId = medium_id >= 0;
    query.segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    query.requireSegmentStage = true;
    query.radius = beam_contribution_positive_or_default(radius, query.radius);
    for (i = 0u; i < beam_map->segmentCount; ++i) {
        const RuntimeCausticPhotonVolumeBeamSegment3D* segment = &beam_map->segments[i];
        Vec3 midpoint;

        if (query.requireMediumId && segment->mediumId != medium_id) continue;
        if (segment->provenance.segmentStage != query.segmentStage) continue;
        if (!beam_contribution_segment_volume_midpoint(segment, volume, &midpoint)) continue;
        query.position = midpoint;
        query.direction = vec3_normalize(segment->direction);
        query.mediumId = segment->mediumId;
        query.requireMediumId = true;
        *out_query = query;
        return true;
    }
    return false;
}

bool RuntimeCausticPhotonIntegration3D_DepositVolumeContributionFromBeamMap(
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticVolumeCache3D* volume_cache,
    const RuntimeVolumeAttachment3D* volume,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticBeamMapQuery3D* query,
    RuntimeCausticPhotonBeamContributionReadback3D* out_readback) {
    RuntimeCausticPhotonBeamContributionReadback3D readback;
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticBeamMapQuery3D default_query;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    const RuntimeCausticBeamMapQuery3D* active_query = query;
    RuntimeCausticBeamMapQueryResult3D result;
    double attenuation_distance = 0.0;
    double volume_weight = 0.0;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (!active_query) {
        RuntimeCausticBeamMap3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }

    readback.attempted = true;
    readback.suppressed = !active_settings->renderContributionEnabled;
    readback.volumeSampleable = RuntimeVolume3D_HasSampleableDensity(volume);
    readback.beamMapAllocated = RuntimeCausticBeamMap3D_IsAllocated(beam_map);
    readback.mediumId = active_query->mediumId;
    readback.position = active_query->position;
    readback.direction = vec3_normalize(active_query->direction);
    readback.radius = beam_contribution_positive_or_default(
        active_query->radius,
        active_settings->volumeQueryRadius);
    if (!(readback.radius > 0.0)) readback.radius = 0.10;

    if (readback.suppressed ||
        RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY ||
        !active_settings->volumeQueryEnabled || !readback.volumeSampleable ||
        !readback.beamMapAllocated || !RuntimeCausticVolumeCache3D_IsAllocated(volume_cache)) {
        *out_readback = readback;
        return false;
    }

    memset(&result, 0, sizeof(result));
    readback.queryAttemptCount = 1u;
    readback.queryHit = RuntimeCausticBeamMap3D_Query(beam_map, active_query, &result);
    readback.queryHitCount = readback.queryHit ? 1u : 0u;
    readback.candidateCount = result.candidateCount;
    readback.contributingCount = result.contributingCount;
    readback.radiusRejectCount = result.radiusRejectCount;
    readback.directionRejectCount = result.directionRejectCount;
    readback.mediumRejectCount = result.mediumRejectCount;
    readback.physicalFlux = result.physicalFlux;
    readback.displayFlux = result.displayFlux;
    readback.density = fmax(0.0,
                             (double)RuntimeVolume3D_SampleDensityAtPosition(
                                 volume,
                                 active_query->position));
    attenuation_distance = fmax(readback.radius * 2.0, volume->grid.voxelSize);
    readback.transmittance = exp(-readback.density * attenuation_distance);
    volume_weight = readback.density * readback.transmittance;
    readback.radiance = vec3_scale(readback.displayFlux, volume_weight);
    readback.contributionEligible = readback.queryHit &&
                                   readback.density > 0.0 &&
                                   beam_contribution_has_energy(readback.physicalFlux) &&
                                   beam_contribution_has_energy(readback.radiance);
    if (readback.contributionEligible) {
        readback.volumeDepositAttemptCount = 1u;
        readback.volumeDeposited =
            RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
                volume_cache,
                active_query->position,
                active_query->direction,
                readback.radius,
                readback.radius,
                readback.radiance.x,
                readback.radiance.y,
                readback.radiance.z);
        readback.volumeDepositAcceptedCount = readback.volumeDeposited ? 1u : 0u;
    }

    *out_readback = readback;
    return readback.volumeDeposited;
}
