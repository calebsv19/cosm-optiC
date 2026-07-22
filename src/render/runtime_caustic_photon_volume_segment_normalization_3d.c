#include "render/runtime_caustic_photon_volume_segment_normalization_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"

static Vec3 normalization_cell_center(const RuntimeVolumeGrid3D* grid,
                                      uint32_t x,
                                      uint32_t y,
                                      uint32_t z) {
    return vec3(grid->origin.x + ((double)x + 0.5) * grid->voxelSize,
                grid->origin.y + ((double)y + 0.5) * grid->voxelSize,
                grid->origin.z + ((double)z + 0.5) * grid->voxelSize);
}

static bool normalization_closest_point(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    Vec3 position,
    double* out_t,
    double* out_distance) {
    const Vec3 axis = segment ? vec3_sub(segment->end, segment->start)
                              : vec3(0.0, 0.0, 0.0);
    const double length_squared = vec3_dot(axis, axis);
    double t;
    Vec3 closest;
    if (!segment || !(length_squared > 1.0e-12)) return false;
    t = vec3_dot(vec3_sub(position, segment->start), axis) / length_squared;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    closest = vec3_add(segment->start, vec3_scale(axis, t));
    if (out_t) *out_t = t;
    if (out_distance) *out_distance = vec3_length(vec3_sub(position, closest));
    return true;
}

void RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->queryRadius = 0.10;
    settings->targetVoxelSize = 0.125;
    settings->mediumId = 0;
    settings->requireMediumId = true;
    settings->segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    settings->requireSegmentStage = true;
    settings->accelerated = true;
}

double RuntimeCausticPhotonVolumeSegmentNormalization3D_TargetVoxelSize(
    double query_radius,
    RuntimeCausticPhotonBudgetTier3D tier) {
    if (!(query_radius > 0.0) || !isfinite(query_radius)) return 0.0;
    switch (tier) {
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL:
            return query_radius;
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION:
            return query_radius * 0.75;
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW:
        default:
            return query_radius * 1.25;
    }
}

bool RuntimeCausticPhotonVolumeSegmentNormalization3D_ResolveGrid(
    const RuntimeVolumeGrid3D* source,
    double target_voxel_size,
    RuntimeVolumeGrid3D* out_grid) {
    const double extent_x = source ? source->boundsMax.x - source->boundsMin.x : 0.0;
    const double extent_y = source ? source->boundsMax.y - source->boundsMin.y : 0.0;
    const double extent_z = source ? source->boundsMax.z - source->boundsMin.z : 0.0;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    if (out_grid) RuntimeVolumeGrid3D_Reset(out_grid);
    if (!out_grid || !RuntimeVolumeGrid3D_IsConfigured(source) ||
        !(target_voxel_size > 0.0) || !isfinite(target_voxel_size)) {
        return false;
    }
    if (source->voxelSize <= target_voxel_size) {
        *out_grid = *source;
        return true;
    }
    width = (uint32_t)ceil(extent_x / target_voxel_size);
    height = (uint32_t)ceil(extent_y / target_voxel_size);
    depth = (uint32_t)ceil(extent_z / target_voxel_size);
    return RuntimeVolumeGrid3D_Configure(
        out_grid,
        source->formatVersion,
        width,
        height,
        depth,
        source->timeSeconds,
        source->frameIndex,
        source->dtSeconds,
        source->origin,
        target_voxel_size,
        source->sceneUp,
        source->solidMaskCrc32);
}

bool RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeVolumeGrid3D* grid,
    double query_radius,
    bool accelerated,
    uint64_t max_axial_samples,
    uint64_t max_cell_tests,
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D* out_result) {
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D result;
    Vec3 axis;
    double length;
    uint64_t sample_count;
    double spacing;
    memset(&result, 0, sizeof(result));
    if (out_result) *out_result = result;
    if (!out_result) return false;
    result.attempted = true;
    if (!segment || !RuntimeVolumeGrid3D_IsConfigured(grid) ||
        !(query_radius > 0.0) || !isfinite(query_radius)) {
        *out_result = result;
        return false;
    }
    axis = vec3_sub(segment->end, segment->start);
    length = vec3_length(axis);
    result.segmentLength = length;
    if (!(length > 1.0e-6) || !isfinite(length)) {
        result.degenerate = true;
        *out_result = result;
        return false;
    }
    sample_count = (uint64_t)fmax(1.0, ceil(length / (grid->voxelSize * 0.5)));
    spacing = length / (double)sample_count;
    result.axialSampleCount = sample_count;
    result.maximumAxialSpacing = spacing;
    if (max_axial_samples > 0u && sample_count > max_axial_samples) {
        result.axialSampleLimitReached = true;
        *out_result = result;
        return false;
    }
    for (uint64_t sample_index = 0u; sample_index < sample_count; ++sample_index) {
        uint32_t min_x = 0u;
        uint32_t min_y = 0u;
        uint32_t min_z = 0u;
        uint32_t max_x = grid->gridW - 1u;
        uint32_t max_y = grid->gridH - 1u;
        uint32_t max_z = grid->gridD - 1u;
        if (accelerated) {
            const double t0 = (double)sample_index / (double)sample_count;
            const double t1 = (double)(sample_index + 1u) / (double)sample_count;
            const Vec3 p0 = vec3_add(segment->start, vec3_scale(axis, t0));
            const Vec3 p1 = vec3_add(segment->start, vec3_scale(axis, t1));
            const double radius = query_radius + grid->voxelSize * 0.5;
            int ix0 = (int)floor((fmin(p0.x, p1.x) - radius - grid->origin.x) /
                                 grid->voxelSize);
            int iy0 = (int)floor((fmin(p0.y, p1.y) - radius - grid->origin.y) /
                                 grid->voxelSize);
            int iz0 = (int)floor((fmin(p0.z, p1.z) - radius - grid->origin.z) /
                                 grid->voxelSize);
            int ix1 = (int)floor((fmax(p0.x, p1.x) + radius - grid->origin.x) /
                                 grid->voxelSize);
            int iy1 = (int)floor((fmax(p0.y, p1.y) + radius - grid->origin.y) /
                                 grid->voxelSize);
            int iz1 = (int)floor((fmax(p0.z, p1.z) + radius - grid->origin.z) /
                                 grid->voxelSize);
            if (ix1 < 0 || iy1 < 0 || iz1 < 0 ||
                ix0 >= (int)grid->gridW || iy0 >= (int)grid->gridH ||
                iz0 >= (int)grid->gridD) {
                continue;
            }
            if (ix0 < 0) ix0 = 0;
            if (iy0 < 0) iy0 = 0;
            if (iz0 < 0) iz0 = 0;
            if (ix1 >= (int)grid->gridW) ix1 = (int)grid->gridW - 1;
            if (iy1 >= (int)grid->gridH) iy1 = (int)grid->gridH - 1;
            if (iz1 >= (int)grid->gridD) iz1 = (int)grid->gridD - 1;
            min_x = (uint32_t)ix0;
            min_y = (uint32_t)iy0;
            min_z = (uint32_t)iz0;
            max_x = (uint32_t)ix1;
            max_y = (uint32_t)iy1;
            max_z = (uint32_t)iz1;
        }
        for (uint32_t z = min_z; z <= max_z; ++z) {
            for (uint32_t y = min_y; y <= max_y; ++y) {
                for (uint32_t x = min_x; x <= max_x; ++x) {
                    const Vec3 center = normalization_cell_center(grid, x, y, z);
                    double closest_t;
                    double kernel = 0.0;
                    uint64_t owner_sample;
                    result.cellTestCount++;
                    if (max_cell_tests > 0u &&
                        result.cellTestCount > max_cell_tests) {
                        result.cellTestLimitReached = true;
                        *out_result = result;
                        return false;
                    }
                    if (!normalization_closest_point(
                            segment, center, &closest_t, NULL)) {
                        continue;
                    }
                    owner_sample = (uint64_t)floor(closest_t * (double)sample_count);
                    if (owner_sample >= sample_count) owner_sample = sample_count - 1u;
                    if (owner_sample != sample_index) continue;
                    for (int oz = 0; oz < 4; ++oz) {
                        for (int oy = 0; oy < 4; ++oy) {
                            for (int ox = 0; ox < 4; ++ox) {
                                const Vec3 sub_position = vec3_add(
                                    center,
                                    vec3_scale(vec3((double)ox - 1.5,
                                                    (double)oy - 1.5,
                                                    (double)oz - 1.5),
                                               grid->voxelSize * 0.25));
                                double distance;
                                if (normalization_closest_point(
                                        segment, sub_position, NULL, &distance)) {
                                    kernel +=
                                        RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                                            distance, query_radius) * (1.0 / 64.0);
                                }
                            }
                        }
                    }
                    if (kernel > 0.0) {
                        result.discreteIntegral += kernel * grid->voxelSize *
                            grid->voxelSize * grid->voxelSize;
                    }
                }
            }
        }
    }
    if (!(result.discreteIntegral > 0.0) ||
        !isfinite(result.discreteIntegral)) {
        *out_result = result;
        return false;
    }
    result.scale = length / result.discreteIntegral;
    result.valid = result.scale >= 0.0 && isfinite(result.scale);
    *out_result = result;
    return result.valid;
}

bool RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
    RuntimeCausticBeamMap3D* map,
    const RuntimeVolumeGrid3D* source_grid,
    const RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D* settings,
    RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D* out_readback) {
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D defaults;
    const RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D* active = settings;
    RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D readback;
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D eligibility_settings;
    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback || !map || !map->segmentFiniteNormalization) return false;
    if (!active) {
        RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    readback.attempted = true;
    readback.workBoundSatisfied = true;
    map->finiteSegmentNormalizationPrepared = false;
    map->finiteSegmentNormalizationCount = 0u;
    map->finiteSegmentNormalizationScaleMinimum = 0.0;
    map->finiteSegmentNormalizationScaleMaximum = 0.0;
    map->finiteSegmentNormalizationScaleMean = 0.0;
    memset(map->segmentFiniteNormalization, 0,
           (size_t)map->segmentCapacity * sizeof(double));
    if (!RuntimeCausticPhotonVolumeSegmentNormalization3D_ResolveGrid(
            source_grid, active->targetVoxelSize, &readback.grid)) {
        *out_readback = readback;
        return false;
    }
    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(&eligibility_settings);
    eligibility_settings.queryRadius = active->queryRadius;
    eligibility_settings.mediumId = active->mediumId;
    eligibility_settings.requireMediumId = active->requireMediumId;
    eligibility_settings.segmentStage = active->segmentStage;
    eligibility_settings.requireSegmentStage = active->requireSegmentStage;
    for (uint64_t i = 0u; i < map->segmentCount; ++i) {
        RuntimeCausticPhotonVolumeBeamSegment3D clipped;
        RuntimeCausticPhotonVolumeSegmentNormalizationResult3D result;
        RuntimeCausticPhotonVolumeBeamEligibility3D eligibility;
        readback.segmentExaminedCount++;
        eligibility = RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
            &map->segments[i], &eligibility_settings);
        if (eligibility != RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE) {
            if (eligibility == RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE)
                readback.segmentRejectedStageCount++;
            else if (eligibility == RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM)
                readback.segmentRejectedMediumCount++;
            else
                readback.segmentRejectedInvalidCount++;
            continue;
        }
        readback.segmentEligibleCount++;
        if (active->maxSegments > 0u &&
            readback.segmentPreparedCount >= active->maxSegments) {
            readback.workBoundSatisfied = false;
            break;
        }
        if (!RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
                &map->segments[i], readback.grid.boundsMin,
                readback.grid.boundsMax, &clipped)) {
            readback.segmentRejectedOutsideCount++;
            continue;
        }
        if (!RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
                &clipped,
                &readback.grid,
                active->queryRadius,
                active->accelerated,
                active->maxAxialSamples > readback.axialSampleCount
                    ? active->maxAxialSamples - readback.axialSampleCount
                    : 0u,
                active->maxCellTests > readback.cellTestCount
                    ? active->maxCellTests - readback.cellTestCount
                    : 0u,
                &result)) {
            readback.axialSampleCount += result.axialSampleCount;
            readback.cellTestCount += result.cellTestCount;
            if (result.axialSampleLimitReached || result.cellTestLimitReached) {
                readback.workBoundSatisfied = false;
                break;
            }
            readback.segmentRejectedOutsideCount++;
            continue;
        }
        readback.axialSampleCount += result.axialSampleCount;
        readback.cellTestCount += result.cellTestCount;
        if (result.maximumAxialSpacing > readback.maximumAxialSpacing) {
            readback.maximumAxialSpacing = result.maximumAxialSpacing;
        }
        map->segmentFiniteNormalization[i] = result.scale;
        readback.segmentPreparedCount++;
        readback.denominatorSum += result.discreteIntegral;
        if (readback.segmentPreparedCount == 1u ||
            result.scale < readback.scaleMinimum) {
            readback.scaleMinimum = result.scale;
        }
        if (result.scale > readback.scaleMaximum) {
            readback.scaleMaximum = result.scale;
        }
        readback.scaleMean += (result.scale - readback.scaleMean) /
            (double)readback.segmentPreparedCount;
    }
    readback.prepared = readback.workBoundSatisfied &&
        readback.segmentPreparedCount > 0u;
    if (readback.prepared) {
        map->finiteSegmentNormalizationPrepared = true;
        map->finiteSegmentNormalizationCount = readback.segmentPreparedCount;
        map->finiteSegmentNormalizationScaleMinimum = readback.scaleMinimum;
        map->finiteSegmentNormalizationScaleMaximum = readback.scaleMaximum;
        map->finiteSegmentNormalizationScaleMean = readback.scaleMean;
    }
    *out_readback = readback;
    return readback.prepared;
}
