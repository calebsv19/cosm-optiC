#include "render/runtime_caustic_photon_distributed_beam_cache_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_time.h"
#include "render/runtime_caustic_photon_volume_segment_normalization_3d.h"
#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"
#include "render/runtime_caustic_photon_sparse_brick_cache_3d.h"

enum {
    DISTRIBUTED_BEAM_SUBVOXEL_COUNT = 8,
    DISTRIBUTED_BEAM_FIELD_FLOAT_COUNT = 32
};

typedef struct {
    Vec3 closest;
    double t;
    double distance;
} DistributedBeamClosestPoint3D;

static double distributed_beam_elapsed_seconds(uint64_t started_at) {
    const uint64_t finished_at = core_time_now_ns();
    if (started_at == 0u || finished_at == 0u) return 0.0;
    return core_time_ns_to_seconds(core_time_diff_ns(finished_at, started_at));
}

static double distributed_beam_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static uint64_t distributed_beam_cell_index(const RuntimeVolumeGrid3D* grid,
                                            uint32_t x,
                                            uint32_t y,
                                            uint32_t z) {
    return (uint64_t)x + (uint64_t)grid->gridW *
        ((uint64_t)y + (uint64_t)grid->gridH * (uint64_t)z);
}

static Vec3 distributed_beam_cell_center(const RuntimeVolumeGrid3D* grid,
                                         uint32_t x,
                                         uint32_t y,
                                         uint32_t z) {
    return vec3(grid->origin.x + ((double)x + 0.5) * grid->voxelSize,
                grid->origin.y + ((double)y + 0.5) * grid->voxelSize,
                grid->origin.z + ((double)z + 0.5) * grid->voxelSize);
}

static bool distributed_beam_closest_point(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    Vec3 position,
    DistributedBeamClosestPoint3D* out_closest) {
    Vec3 axis;
    double axis_length_squared;
    double t;
    if (!segment || !out_closest) return false;
    axis = vec3_sub(segment->end, segment->start);
    axis_length_squared = vec3_dot(axis, axis);
    if (!(axis_length_squared > 1.0e-12)) return false;
    t = vec3_dot(vec3_sub(position, segment->start), axis) /
        axis_length_squared;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    out_closest->t = t;
    out_closest->closest = vec3_add(segment->start, vec3_scale(axis, t));
    out_closest->distance = vec3_length(vec3_sub(position, out_closest->closest));
    return true;
}
static bool distributed_beam_allocate_field(
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticDistributedBeamStorageBackend3D backend) {
    const uint64_t count = cache ? cache->grid.cellCount : 0u;
    if (!RuntimeCausticVolumeCache3D_IsAllocated(cache) || count == 0u ||
        count > (uint64_t)(SIZE_MAX /
            (DISTRIBUTED_BEAM_SUBVOXEL_COUNT * sizeof(float)))) {
        return false;
    }
    if (backend == RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS) {
        free(cache->beamDirectionX);
        free(cache->beamDirectionY);
        free(cache->beamDirectionZ);
        free(cache->beamDistanceWeighted);
        free(cache->beamDirectionWeight);
        free(cache->beamSubvoxelRadianceR);
        free(cache->beamSubvoxelRadianceG);
        free(cache->beamSubvoxelRadianceB);
        cache->beamDirectionX = NULL;
        cache->beamDirectionY = NULL;
        cache->beamDirectionZ = NULL;
        cache->beamDistanceWeighted = NULL;
        cache->beamDirectionWeight = NULL;
        cache->beamSubvoxelRadianceR = NULL;
        cache->beamSubvoxelRadianceG = NULL;
        cache->beamSubvoxelRadianceB = NULL;
        if (!cache->sparseBeamField) {
            cache->sparseBeamField =
                RuntimeCausticPhotonSparseBrickCache3D_Create(
                    &cache->grid, count * 3u * sizeof(float), 0u);
        }
        return cache->sparseBeamField != NULL;
    }
    RuntimeCausticPhotonSparseBrickCache3D_Destroy(cache->sparseBeamField);
    cache->sparseBeamField = NULL;
    if (!cache->beamDirectionX) {
        cache->beamDirectionX = (float*)calloc((size_t)count, sizeof(float));
    }
    if (!cache->beamDirectionY) {
        cache->beamDirectionY = (float*)calloc((size_t)count, sizeof(float));
    }
    if (!cache->beamDirectionZ) {
        cache->beamDirectionZ = (float*)calloc((size_t)count, sizeof(float));
    }
    if (!cache->beamDistanceWeighted) {
        cache->beamDistanceWeighted = (float*)calloc((size_t)count, sizeof(float));
    }
    if (!cache->beamDirectionWeight) {
        cache->beamDirectionWeight = (float*)calloc((size_t)count, sizeof(float));
    }
    if (!cache->beamSubvoxelRadianceR) {
        cache->beamSubvoxelRadianceR = (float*)calloc(
            (size_t)count * DISTRIBUTED_BEAM_SUBVOXEL_COUNT, sizeof(float));
    }
    if (!cache->beamSubvoxelRadianceG) {
        cache->beamSubvoxelRadianceG = (float*)calloc(
            (size_t)count * DISTRIBUTED_BEAM_SUBVOXEL_COUNT, sizeof(float));
    }
    if (!cache->beamSubvoxelRadianceB) {
        cache->beamSubvoxelRadianceB = (float*)calloc(
            (size_t)count * DISTRIBUTED_BEAM_SUBVOXEL_COUNT, sizeof(float));
    }
    return cache->beamDirectionX && cache->beamDirectionY &&
           cache->beamDirectionZ && cache->beamDistanceWeighted &&
           cache->beamDirectionWeight && cache->beamSubvoxelRadianceR &&
           cache->beamSubvoxelRadianceG && cache->beamSubvoxelRadianceB;
}

static bool distributed_beam_refine_grid_for_radius(
    RuntimeCausticVolumeCache3D* cache,
    double query_radius,
    uint64_t memory_ceiling_bytes,
    uint64_t* out_storage_bytes) {
    RuntimeVolumeGrid3D refined;
    const RuntimeVolumeGrid3D source = cache->grid;
    const double extent_x = source.boundsMax.x - source.boundsMin.x;
    const double extent_y = source.boundsMax.y - source.boundsMin.y;
    const double extent_z = source.boundsMax.z - source.boundsMin.z;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint64_t cell_count;
    uint64_t storage_bytes;
    if (out_storage_bytes) *out_storage_bytes = 0u;
    if (!(query_radius > 0.0) || source.voxelSize <= query_radius) return true;
    width = (uint32_t)ceil(extent_x / query_radius);
    height = (uint32_t)ceil(extent_y / query_radius);
    depth = (uint32_t)ceil(extent_z / query_radius);
    if (width == 0u || height == 0u || depth == 0u ||
        (uint64_t)height > UINT64_MAX / (uint64_t)width ||
        (uint64_t)depth > UINT64_MAX / ((uint64_t)width * (uint64_t)height)) {
        return false;
    }
    cell_count = (uint64_t)width * (uint64_t)height * (uint64_t)depth;
    if (cell_count > UINT64_MAX /
            ((uint64_t)DISTRIBUTED_BEAM_FIELD_FLOAT_COUNT * sizeof(float))) {
        return false;
    }
    storage_bytes = cell_count *
        (uint64_t)DISTRIBUTED_BEAM_FIELD_FLOAT_COUNT * sizeof(float);
    if (out_storage_bytes) *out_storage_bytes = storage_bytes;
    if (memory_ceiling_bytes > 0u && storage_bytes > memory_ceiling_bytes) {
        return false;
    }
    RuntimeVolumeGrid3D_Reset(&refined);
    if (!RuntimeVolumeGrid3D_Configure(
            &refined, source.formatVersion, width, height, depth,
            source.timeSeconds, source.frameIndex, source.dtSeconds,
            source.origin, query_radius, source.sceneUp,
            source.solidMaskCrc32)) {
        return false;
    }
    RuntimeCausticVolumeCache3D_Free(cache);
    return RuntimeCausticVolumeCache3D_Allocate(cache, &refined);
}

void RuntimeCausticDistributedBeamCache3D_DefaultSettings(
    RuntimeCausticDistributedBeamCacheSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->queryRadius = 0.10;
    settings->mediumId = 0;
    settings->requireMediumId = true;
    settings->segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    settings->requireSegmentStage = true;
    settings->buildMode = RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
    RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
        settings, RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW);
}

void RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
    RuntimeCausticDistributedBeamCacheSettings3D* settings,
    RuntimeCausticPhotonBudgetTier3D tier) {
    if (!settings) return;
    settings->qualityTier = tier;
    switch (tier) {
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL:
            settings->maxSegments = 4096u;
            settings->maxAxialSamples = 8388608u;
            settings->maxCellTests = 536870912u;
            settings->memoryCeilingBytes = UINT64_C(536870912);
            break;
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION:
            settings->maxSegments = 512u;
            settings->maxAxialSamples = 1048576u;
            settings->maxCellTests = 67108864u;
            settings->memoryCeilingBytes = UINT64_C(134217728);
            break;
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW:
        default:
            settings->qualityTier = RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW;
            settings->maxSegments = 64u;
            settings->maxAxialSamples = 131072u;
            settings->maxCellTests = 8388608u;
            settings->memoryCeilingBytes = UINT64_C(33554432);
            break;
    }
}

const char* RuntimeCausticDistributedBeamBuildMode3D_Label(
    RuntimeCausticDistributedBeamBuildMode3D mode) {
    return mode == RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_LINEAR_ORACLE
               ? "linear_oracle"
               : "raster_accelerated";
}

const char* RuntimeCausticDistributedBeamStorageBackend3D_Label(
    RuntimeCausticDistributedBeamStorageBackend3D backend) {
    return backend == RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS
        ? "sparse_bricks_4x4x4" : "dense";
}

RuntimeCausticDistributedBeamStorageBackend3D
RuntimeCausticDistributedBeamStorageBackend3D_FromLabel(const char* label) {
    if (label && strcmp(label, "sparse_bricks_4x4x4") == 0) {
        return RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS;
    }
    return RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_DENSE;
}

static bool distributed_beam_test_budget(
    RuntimeCausticDistributedBeamCacheReadback3D* readback) {
    readback->cellTestCount++;
    if (readback->maxCellTests > 0u &&
        readback->cellTestCount > readback->maxCellTests) {
        readback->cellTestLimitReached = true;
        readback->workBoundSatisfied = false;
        return false;
    }
    return true;
}

static bool distributed_beam_add_cell(
    RuntimeCausticVolumeCache3D* cache,
    uint64_t index,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    double kernel_weight,
    const double subvoxel_kernel[DISTRIBUTED_BEAM_SUBVOXEL_COUNT],
    double normalization,
    double beam_distance,
    RuntimeCausticDistributedBeamCacheReadback3D* readback) {
    const double weight = kernel_weight * segment->transmittance * normalization;
    const Vec3 flux = vec3_scale(segment->flux, weight);
    const double direction_weight = fmax(0.0, distributed_beam_luma(segment->flux)) *
        weight;
    const double voxel_volume = cache->grid.voxelSize * cache->grid.voxelSize *
        cache->grid.voxelSize;
    float* sparse = NULL;
    if (!(weight > 0.0) || index >= cache->grid.cellCount) return true;
    if (cache->sparseBeamField) {
        sparse = RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
            cache->sparseBeamField, index);
        if (!sparse) return false;
    }
    if (!(distributed_beam_luma(vec3(cache->radianceR[index],
                                     cache->radianceG[index],
                                     cache->radianceB[index])) > 0.0)) {
        readback->nonZeroCellCount++;
    }
    cache->radianceR[index] += (float)flux.x;
    cache->radianceG[index] += (float)flux.y;
    cache->radianceB[index] += (float)flux.z;
    if (sparse) {
        sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_X_3D] +=
            (float)(segment->direction.x * direction_weight);
        sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Y_3D] +=
            (float)(segment->direction.y * direction_weight);
        sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Z_3D] +=
            (float)(segment->direction.z * direction_weight);
        sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DISTANCE_WEIGHTED_3D] +=
            (float)(beam_distance * direction_weight);
        sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_WEIGHT_3D] +=
            (float)direction_weight;
    } else {
        cache->beamDirectionX[index] +=
            (float)(segment->direction.x * direction_weight);
        cache->beamDirectionY[index] +=
            (float)(segment->direction.y * direction_weight);
        cache->beamDirectionZ[index] +=
            (float)(segment->direction.z * direction_weight);
        cache->beamDistanceWeighted[index] +=
            (float)(beam_distance * direction_weight);
        cache->beamDirectionWeight[index] += (float)direction_weight;
    }
    if (subvoxel_kernel) {
        for (uint64_t subvoxel = 0u;
             subvoxel < DISTRIBUTED_BEAM_SUBVOXEL_COUNT;
             ++subvoxel) {
            const double subvoxel_weight = subvoxel_kernel[subvoxel] *
                segment->transmittance * normalization;
            const Vec3 subvoxel_flux = vec3_scale(
                segment->flux, subvoxel_weight);
            const uint64_t subvoxel_index =
                index * DISTRIBUTED_BEAM_SUBVOXEL_COUNT + subvoxel;
            if (!(subvoxel_weight > 0.0)) continue;
            if (!(distributed_beam_luma(vec3(
                    sparse
                        ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_R_3D +
                                 subvoxel]
                        : cache->beamSubvoxelRadianceR[subvoxel_index],
                    sparse
                        ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_G_3D +
                                 subvoxel]
                        : cache->beamSubvoxelRadianceG[subvoxel_index],
                    sparse
                        ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_B_3D +
                                 subvoxel]
                        : cache->beamSubvoxelRadianceB[subvoxel_index])) > 0.0)) {
                readback->nonZeroSubvoxelCount++;
            }
            if (sparse) {
                sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_R_3D + subvoxel] +=
                    (float)subvoxel_flux.x;
                sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_G_3D + subvoxel] +=
                    (float)subvoxel_flux.y;
                sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_B_3D + subvoxel] +=
                    (float)subvoxel_flux.z;
            } else {
                cache->beamSubvoxelRadianceR[subvoxel_index] +=
                    (float)subvoxel_flux.x;
                cache->beamSubvoxelRadianceG[subvoxel_index] +=
                    (float)subvoxel_flux.y;
                cache->beamSubvoxelRadianceB[subvoxel_index] +=
                    (float)subvoxel_flux.z;
            }
        }
    }
    readback->cachedIntegratedFlux = vec3_add(
        readback->cachedIntegratedFlux, vec3_scale(flux, voxel_volume));
    readback->cellContributionCount++;
    return true;
}

static bool distributed_beam_segment_pass(
    RuntimeCausticVolumeCache3D* cache,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeCausticDistributedBeamCacheSettings3D* settings,
    bool deposit,
    double normalization,
    double* out_integral,
    RuntimeCausticDistributedBeamCacheReadback3D* readback) {
    const RuntimeVolumeGrid3D* grid = &cache->grid;
    const Vec3 axis = vec3_sub(segment->end, segment->start);
    const double length = vec3_length(axis);
    const double spacing_limit = grid->voxelSize * 0.5;
    const uint64_t sample_count = (uint64_t)fmax(1.0, ceil(length / spacing_limit));
    const double spacing = length / (double)sample_count;
    double integral = 0.0;

    if (!deposit) {
        if (readback->maxAxialSamples > 0u &&
            readback->axialSampleCount + sample_count > readback->maxAxialSamples) {
            readback->axialSampleLimitReached = true;
            readback->workBoundSatisfied = false;
            return false;
        }
        readback->axialSampleCount += sample_count;
        if (spacing > readback->maximumAxialSpacing) {
            readback->maximumAxialSpacing = spacing;
        }
    }

    for (uint64_t sample_index = 0u; sample_index < sample_count; ++sample_index) {
        uint32_t min_x = 0u;
        uint32_t min_y = 0u;
        uint32_t min_z = 0u;
        uint32_t max_x = grid->gridW - 1u;
        uint32_t max_y = grid->gridH - 1u;
        uint32_t max_z = grid->gridD - 1u;
        if (settings->buildMode ==
            RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED) {
            const double t0 = (double)sample_index / (double)sample_count;
            const double t1 = (double)(sample_index + 1u) / (double)sample_count;
            const Vec3 p0 = vec3_add(segment->start, vec3_scale(axis, t0));
            const Vec3 p1 = vec3_add(segment->start, vec3_scale(axis, t1));
            const double radius = settings->queryRadius + grid->voxelSize * 0.5;
            const double min_world_x = fmin(p0.x, p1.x) - radius;
            const double min_world_y = fmin(p0.y, p1.y) - radius;
            const double min_world_z = fmin(p0.z, p1.z) - radius;
            const double max_world_x = fmax(p0.x, p1.x) + radius;
            const double max_world_y = fmax(p0.y, p1.y) + radius;
            const double max_world_z = fmax(p0.z, p1.z) + radius;
            int ix0 = (int)floor((min_world_x - grid->origin.x) / grid->voxelSize);
            int iy0 = (int)floor((min_world_y - grid->origin.y) / grid->voxelSize);
            int iz0 = (int)floor((min_world_z - grid->origin.z) / grid->voxelSize);
            int ix1 = (int)floor((max_world_x - grid->origin.x) / grid->voxelSize);
            int iy1 = (int)floor((max_world_y - grid->origin.y) / grid->voxelSize);
            int iz1 = (int)floor((max_world_z - grid->origin.z) / grid->voxelSize);
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
                    DistributedBeamClosestPoint3D closest;
                    const Vec3 center = distributed_beam_cell_center(grid, x, y, z);
                    uint64_t owner_sample;
                    double kernel = 0.0;
                    double subvoxel_kernel[DISTRIBUTED_BEAM_SUBVOXEL_COUNT] = {0};
                    if (!distributed_beam_test_budget(readback)) return false;
                    if (!distributed_beam_closest_point(
                            segment, center, &closest)) {
                        continue;
                    }
                    owner_sample = (uint64_t)floor(closest.t * (double)sample_count);
                    if (owner_sample >= sample_count) owner_sample = sample_count - 1u;
                    if (owner_sample != sample_index) continue;
                    /*
                     * Store a deterministic cell average, not a midpoint impulse.
                     * Sixty-four fixed sub-cell samples materially reduce voxel-size
                     * energy drift while retaining a bounded, order-stable build.
                     */
                    for (int oz = 0; oz < 4; ++oz) {
                        for (int oy = 0; oy < 4; ++oy) {
                            for (int ox = 0; ox < 4; ++ox) {
                                DistributedBeamClosestPoint3D sub_closest;
                                const Vec3 sub_position = vec3_add(
                                    center,
                                    vec3_scale(vec3((double)ox - 1.5,
                                                    (double)oy - 1.5,
                                                    (double)oz - 1.5),
                                               grid->voxelSize * 0.25));
                                if (distributed_beam_closest_point(
                                        segment, sub_position, &sub_closest)) {
                                    const uint64_t subvoxel =
                                        (uint64_t)(ox >= 2) +
                                        2u * ((uint64_t)(oy >= 2) +
                                              2u * (uint64_t)(oz >= 2));
                                    subvoxel_kernel[subvoxel] +=
                                        RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                                            sub_closest.distance,
                                            settings->queryRadius) * (1.0 / 8.0);
                                }
                            }
                        }
                    }
                    for (uint64_t subvoxel = 0u;
                         subvoxel < DISTRIBUTED_BEAM_SUBVOXEL_COUNT;
                         ++subvoxel) {
                        kernel += subvoxel_kernel[subvoxel] *
                            (1.0 / DISTRIBUTED_BEAM_SUBVOXEL_COUNT);
                    }
                    if (!(kernel > 0.0)) continue;
                    if (deposit) {
                        if (!distributed_beam_add_cell(
                                cache,
                                distributed_beam_cell_index(grid, x, y, z),
                                segment,
                                kernel,
                                subvoxel_kernel,
                                normalization,
                                closest.t * length,
                                readback)) {
                            readback->memoryBoundSatisfied = false;
                            readback->workBoundSatisfied = false;
                            return false;
                        }
                    } else {
                        integral += kernel * grid->voxelSize * grid->voxelSize *
                            grid->voxelSize;
                    }
                }
            }
        }
    }
    if (out_integral) *out_integral = integral;
    return true;
}

bool RuntimeCausticDistributedBeamCache3D_Build(
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticDistributedBeamCacheSettings3D* settings,
    RuntimeCausticDistributedBeamCacheReadback3D* out_readback) {
    RuntimeCausticDistributedBeamCacheSettings3D defaults;
    const RuntimeCausticDistributedBeamCacheSettings3D* active = settings;
    RuntimeCausticDistributedBeamCacheReadback3D readback;
    double cache_voxel_size;
    const uint64_t build_started_at = core_time_now_ns();
    uint64_t phase_started_at;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active) {
        RuntimeCausticDistributedBeamCache3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    readback.attempted = true;
    readback.accelerated = active->buildMode ==
        RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
    readback.qualityTier = active->qualityTier;
    readback.buildMode = active->buildMode;
    readback.storageBackend = active->storageBackend;
    readback.queryRadius = active->queryRadius;
    readback.maxSegments = active->maxSegments;
    readback.maxAxialSamples = active->maxAxialSamples;
    readback.maxCellTests = active->maxCellTests;
    readback.memoryCeilingBytes = active->memoryCeilingBytes;
    readback.configurationAssertionEnabled =
        active->configurationAssertionEnabled;
    readback.configurationLifecycleAssertionEnabled =
        active->configurationLifecycleAssertionEnabled;
    readback.configurationLifecycleExpectedBuilt =
        active->configurationLifecycleExpectedBuilt;
    readback.configurationLifecycleExpectedActiveEmpty =
        active->configurationLifecycleExpectedActiveEmpty;
    readback.requestedStorageBackend = active->storageBackend;
    readback.requestedGridW = active->expectedGridW;
    readback.requestedGridH = active->expectedGridH;
    readback.requestedGridD = active->expectedGridD;
    readback.requestedVoxelSize = active->expectedVoxelSize;
    readback.requestedDiagnosticVoxelScale = active->diagnosticVoxelScale;
    readback.expectedMaximumPeakBytes = active->expectedMaximumPeakBytes;
    readback.workBoundSatisfied = true;
    if (!cache || !beam_map || !RuntimeCausticVolumeCache3D_IsAllocated(cache) ||
        !RuntimeCausticBeamMap3D_IsAllocated(beam_map) ||
        !(active->queryRadius > 0.0)) {
        *out_readback = readback;
        return false;
    }
    phase_started_at = core_time_now_ns();
    {
        uint64_t refined_storage_bytes = 0u;
        if (active->diagnosticVoxelScale > 0.0 &&
            active->diagnosticVoxelScale <= 1.0) {
            cache_voxel_size = active->queryRadius *
                active->diagnosticVoxelScale;
        } else switch (active->qualityTier) {
            case RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL:
                /*
                 * The compact beam kernel already bounds spatial detail by
                 * queryRadius. Refining the cache to half that support in all
                 * three axes multiplies storage and build work by eight while
                 * the camera consumer only reconstructs the same bounded
                 * field. Keep the final lattice at the physical support scale;
                 * direct/cache equivalence remains the accuracy gate.
                 */
                cache_voxel_size = active->queryRadius;
                break;
            case RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION:
                cache_voxel_size = active->queryRadius * 0.75;
                break;
            case RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW:
            default:
                cache_voxel_size = active->queryRadius * 1.25;
                break;
        }
        if (!distributed_beam_refine_grid_for_radius(
                cache, cache_voxel_size, active->memoryCeilingBytes,
                &refined_storage_bytes)) {
            readback.storageBytes = refined_storage_bytes;
            readback.memoryBoundSatisfied = active->memoryCeilingBytes == 0u ||
                refined_storage_bytes <= active->memoryCeilingBytes;
            readback.allocationClearSeconds =
                distributed_beam_elapsed_seconds(phase_started_at);
            readback.buildTotalSeconds =
                distributed_beam_elapsed_seconds(build_started_at);
            *out_readback = readback;
            return false;
        }
    }
    readback.gridCellCount = cache->grid.cellCount;
    readback.gridW = cache->grid.gridW;
    readback.gridH = cache->grid.gridH;
    readback.gridD = cache->grid.gridD;
    readback.voxelSize = cache->grid.voxelSize;
    readback.configurationAssertionMatched =
        !active->configurationAssertionEnabled ||
        (cache->grid.gridW == active->expectedGridW &&
         cache->grid.gridH == active->expectedGridH &&
         cache->grid.gridD == active->expectedGridD &&
         fabs(cache->grid.voxelSize - active->expectedVoxelSize) <= 1.0e-12);
    if (!readback.configurationAssertionMatched) {
        RuntimeCausticVolumeCache3D_Clear(cache);
        readback.buildTotalSeconds =
            distributed_beam_elapsed_seconds(build_started_at);
        *out_readback = readback;
        return false;
    }
    readback.conservativeSubvoxelField = true;
    readback.subvoxelCountPerCell = DISTRIBUTED_BEAM_SUBVOXEL_COUNT;
    readback.subvoxelSize = cache->grid.voxelSize * 0.5;
    readback.storageBytes = cache->grid.cellCount *
        (uint64_t)DISTRIBUTED_BEAM_FIELD_FLOAT_COUNT * sizeof(float);
    readback.memoryBoundSatisfied = active->memoryCeilingBytes == 0u ||
        readback.storageBytes <= active->memoryCeilingBytes;
    if (active->storageBackend ==
        RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS) {
        readback.memoryBoundSatisfied = true;
    }
    if (!readback.memoryBoundSatisfied || !distributed_beam_allocate_field(
            cache, active->storageBackend)) {
        readback.allocationClearSeconds =
            distributed_beam_elapsed_seconds(phase_started_at);
        readback.buildTotalSeconds =
            distributed_beam_elapsed_seconds(build_started_at);
        *out_readback = readback;
        return false;
    }
    RuntimeCausticVolumeCache3D_Clear(cache);
    readback.allocationClearSeconds =
        distributed_beam_elapsed_seconds(phase_started_at);

    if (active->configurationLifecycleAssertionEnabled &&
        active->configurationLifecycleExpectedActiveEmpty &&
        beam_map->segmentCount == 0u) {
        if (cache->sparseBeamField) {
            RuntimeCausticPhotonSparseBrickCacheStats3D sparse;
            RuntimeCausticPhotonSparseBrickCache3D_Snapshot(
                cache->sparseBeamField, &sparse);
            readback.sparseDirectoryBytes = sparse.directoryBytes;
            readback.sparseAllocatedBrickCount = sparse.allocatedBrickCount;
            readback.sparsePayloadBytes = sparse.payloadBytes;
            readback.sparseMetadataBytes = sparse.metadataBytes;
            readback.sparsePeakBytes = sparse.peakBytes;
            readback.sparseAllocationOrderHash = sparse.allocationOrderHash;
            readback.sparseAllocationFailureCount =
                sparse.allocationFailureCount;
            readback.storageBytes = sparse.peakBytes;
            readback.memoryBoundSatisfied =
                sparse.allocationFailureCount == 0u &&
                (active->memoryCeilingBytes == 0u ||
                 sparse.peakBytes <= active->memoryCeilingBytes) &&
                (!active->configurationAssertionEnabled ||
                 (active->expectedMaximumPeakBytes > 0u &&
                  sparse.peakBytes <= active->expectedMaximumPeakBytes));
        }
        readback.configurationLifecycleMatched =
            readback.configurationAssertionMatched &&
            readback.memoryBoundSatisfied && readback.workBoundSatisfied &&
            readback.sparseAllocatedBrickCount == 0u &&
            readback.sparsePayloadBytes == 0u;
        readback.buildTotalSeconds =
            distributed_beam_elapsed_seconds(build_started_at);
        *out_readback = readback;
        return false;
    }

    phase_started_at = core_time_now_ns();
    {
        RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D normalization;
        RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D normalization_readback;
        RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(
            &normalization);
        normalization.queryRadius = active->queryRadius;
        normalization.targetVoxelSize = cache->grid.voxelSize;
        normalization.mediumId = active->mediumId;
        normalization.requireMediumId = active->requireMediumId;
        normalization.segmentStage = active->segmentStage;
        normalization.requireSegmentStage = active->requireSegmentStage;
        normalization.accelerated = active->buildMode ==
            RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
        normalization.maxSegments = active->maxSegments;
        normalization.maxAxialSamples = active->maxAxialSamples;
        normalization.maxCellTests = active->maxCellTests;
        if (!RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
                beam_map, &cache->grid, &normalization,
                &normalization_readback)) {
            readback.workBoundSatisfied = normalization_readback.workBoundSatisfied;
            readback.segmentLimitReached = !normalization_readback.workBoundSatisfied &&
                normalization_readback.segmentPreparedCount >= active->maxSegments;
            readback.axialSampleLimitReached = !normalization_readback.workBoundSatisfied &&
                normalization_readback.axialSampleCount >= active->maxAxialSamples;
            readback.cellTestLimitReached = !normalization_readback.workBoundSatisfied &&
                normalization_readback.cellTestCount >= active->maxCellTests;
            readback.populationSeconds =
                distributed_beam_elapsed_seconds(phase_started_at);
            readback.buildTotalSeconds =
                distributed_beam_elapsed_seconds(build_started_at);
            *out_readback = readback;
            return false;
        }
        readback.segmentExaminedCount = normalization_readback.segmentExaminedCount;
        readback.segmentEligibleCount = normalization_readback.segmentEligibleCount;
        readback.segmentRejectedStageCount =
            normalization_readback.segmentRejectedStageCount;
        readback.segmentRejectedMediumCount =
            normalization_readback.segmentRejectedMediumCount;
        readback.segmentRejectedInvalidCount =
            normalization_readback.segmentRejectedInvalidCount;
        readback.segmentRejectedOutsideCount =
            normalization_readback.segmentRejectedOutsideCount;
        readback.axialSampleCount = normalization_readback.axialSampleCount;
        readback.cellTestCount = normalization_readback.cellTestCount;
        readback.maximumAxialSpacing = normalization_readback.maximumAxialSpacing;
        readback.segmentNormalizationCount =
            normalization_readback.segmentPreparedCount;
        readback.segmentNormalizationDenominatorSum =
            normalization_readback.denominatorSum;
        readback.segmentNormalizationScaleMinimum =
            normalization_readback.scaleMinimum;
        readback.segmentNormalizationScaleMaximum =
            normalization_readback.scaleMaximum;
        readback.segmentNormalizationScaleMean = normalization_readback.scaleMean;
    }
    for (uint64_t i = 0u; i < beam_map->segmentCount; ++i) {
        RuntimeCausticPhotonVolumeBeamSegment3D clipped;
        const RuntimeCausticPhotonVolumeBeamSegment3D* source = &beam_map->segments[i];
        const double normalization_scale =
            beam_map->segmentFiniteNormalization[i];
        double length;
        if (!(normalization_scale > 0.0) || !isfinite(normalization_scale)) continue;
        if (!RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
                source,
                cache->grid.boundsMin,
                cache->grid.boundsMax,
                &clipped)) {
            continue;
        }
        clipped.direction = vec3_normalize(vec3_sub(clipped.end, clipped.start));
        length = vec3_length(vec3_sub(clipped.end, clipped.start));
        if (!distributed_beam_segment_pass(
                cache, &clipped, active, true, normalization_scale, NULL,
                &readback)) {
            break;
        }
        readback.segmentRasterizedCount++;
        readback.expectedIntegratedFlux = vec3_add(
            readback.expectedIntegratedFlux,
            vec3_scale(clipped.flux, clipped.transmittance * length));
    }
    readback.populationSeconds = distributed_beam_elapsed_seconds(phase_started_at);

    if (!readback.workBoundSatisfied || readback.segmentLimitReached ||
        readback.axialSampleLimitReached || readback.cellTestLimitReached) {
        RuntimeCausticVolumeCache3D_Clear(cache);
        readback.buildTotalSeconds =
            distributed_beam_elapsed_seconds(build_started_at);
        *out_readback = readback;
        return false;
    }
    {
        const double expected = distributed_beam_luma(readback.expectedIntegratedFlux);
        const double cached = distributed_beam_luma(readback.cachedIntegratedFlux);
        readback.integratedFluxRelativeError = expected > 1.0e-12
            ? fabs(cached - expected) / expected
            : fabs(cached);
    }
    cache->physicalBeamField = readback.segmentRasterizedCount > 0u;
    cache->beamSubvoxelField = cache->physicalBeamField;
    readback.built = cache->physicalBeamField;
    readback.configurationLifecycleMatched =
        !active->configurationLifecycleAssertionEnabled ||
        (active->configurationLifecycleExpectedBuilt && readback.built);
    readback.buildTotalSeconds = distributed_beam_elapsed_seconds(build_started_at);
    if (cache->sparseBeamField) {
        RuntimeCausticPhotonSparseBrickCacheStats3D sparse;
        RuntimeCausticPhotonSparseBrickCache3D_Snapshot(
            cache->sparseBeamField, &sparse);
        readback.sparseDirectoryBytes = sparse.directoryBytes;
        readback.sparseAllocatedBrickCount = sparse.allocatedBrickCount;
        readback.sparsePayloadBytes = sparse.payloadBytes;
        readback.sparseMetadataBytes = sparse.metadataBytes;
        readback.sparsePeakBytes = sparse.peakBytes;
        readback.sparseAllocationOrderHash = sparse.allocationOrderHash;
        readback.sparseAllocationFailureCount = sparse.allocationFailureCount;
        readback.storageBytes = sparse.peakBytes;
        readback.memoryBoundSatisfied = sparse.allocationFailureCount == 0u &&
            (active->memoryCeilingBytes == 0u ||
             sparse.peakBytes <= active->memoryCeilingBytes);
        if (active->configurationAssertionEnabled &&
            (active->expectedMaximumPeakBytes == 0u ||
             sparse.peakBytes > active->expectedMaximumPeakBytes)) {
            readback.memoryBoundSatisfied = false;
            readback.configurationAssertionMatched = false;
            readback.built = false;
            readback.configurationLifecycleMatched = false;
            RuntimeCausticVolumeCache3D_Clear(cache);
        }
    }
    if (active->configurationLifecycleAssertionEnabled &&
        active->configurationLifecycleExpectedActiveEmpty) {
        const double expected =
            distributed_beam_luma(readback.expectedIntegratedFlux);
        const double cached =
            distributed_beam_luma(readback.cachedIntegratedFlux);
        readback.configurationLifecycleMatched =
            readback.attempted && !readback.built &&
            readback.configurationAssertionMatched &&
            readback.segmentRasterizedCount == 0u &&
            readback.cellContributionCount == 0u &&
            readback.nonZeroCellCount == 0u &&
            readback.nonZeroSubvoxelCount == 0u &&
            readback.sparseAllocatedBrickCount == 0u &&
            readback.sparsePayloadBytes == 0u &&
            fabs(expected) <= 1.0e-12 && fabs(cached) <= 1.0e-12 &&
            readback.integratedFluxRelativeError <= 1.0e-12 &&
            readback.memoryBoundSatisfied && readback.workBoundSatisfied;
    }
    *out_readback = readback;
    return readback.built;
}

static bool distributed_beam_position_to_cell(
    const RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    uint32_t cell[3]) {
    const double lx = (position.x - cache->grid.origin.x) / cache->grid.voxelSize;
    const double ly = (position.y - cache->grid.origin.y) / cache->grid.voxelSize;
    const double lz = (position.z - cache->grid.origin.z) / cache->grid.voxelSize;
    if (lx < 0.0 || ly < 0.0 || lz < 0.0 ||
        lx >= (double)cache->grid.gridW ||
        ly >= (double)cache->grid.gridH ||
        lz >= (double)cache->grid.gridD) {
        return false;
    }
    cell[0] = (uint32_t)floor(lx);
    cell[1] = (uint32_t)floor(ly);
    cell[2] = (uint32_t)floor(lz);
    return true;
}

static Vec3 distributed_beam_cell_flux(
    const RuntimeCausticVolumeCache3D* cache,
    uint64_t index) {
    if (!cache || index >= cache->grid.cellCount) {
        return vec3(0.0, 0.0, 0.0);
    }
    return vec3(cache->radianceR[index],
                cache->radianceG[index],
                cache->radianceB[index]);
}

static Vec3 distributed_beam_subvoxel_flux(
    const RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    const uint32_t cell[3]) {
    const double local_x =
        (position.x - cache->grid.origin.x) / cache->grid.voxelSize;
    const double local_y =
        (position.y - cache->grid.origin.y) / cache->grid.voxelSize;
    const double local_z =
        (position.z - cache->grid.origin.z) / cache->grid.voxelSize;
    const uint64_t subvoxel =
        (uint64_t)(local_x - floor(local_x) >= 0.5) +
        2u * ((uint64_t)(local_y - floor(local_y) >= 0.5) +
              2u * (uint64_t)(local_z - floor(local_z) >= 0.5));
    const uint64_t index = distributed_beam_cell_index(
        &cache->grid, cell[0], cell[1], cell[2]);
    const uint64_t subvoxel_index =
        index * DISTRIBUTED_BEAM_SUBVOXEL_COUNT + subvoxel;
    if (cache->sparseBeamField) {
        const float* sparse = RuntimeCausticPhotonSparseBrickCache3D_FindCell(
            cache->sparseBeamField, index);
        if (!sparse) return vec3(0.0, 0.0, 0.0);
        return vec3(
            sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_R_3D + subvoxel],
            sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_G_3D + subvoxel],
            sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_B_3D + subvoxel]);
    }
    if (!cache->beamSubvoxelField || !cache->beamSubvoxelRadianceR ||
        !cache->beamSubvoxelRadianceG || !cache->beamSubvoxelRadianceB) {
        return distributed_beam_cell_flux(cache, index);
    }
    return vec3(cache->beamSubvoxelRadianceR[subvoxel_index],
                cache->beamSubvoxelRadianceG[subvoxel_index],
                cache->beamSubvoxelRadianceB[subvoxel_index]);
}

bool RuntimeCausticDistributedBeamCache3D_Sample(
    const RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    RuntimeCausticBeamMapQueryResult3D* out_result) {
    RuntimeCausticBeamMapQueryResult3D result;
    uint32_t cell[3];
    uint64_t index;
    double direction_weight;
    const float* sparse = NULL;
    memset(&result, 0, sizeof(result));
    if (out_result) *out_result = result;
    if (!out_result || !cache || !cache->physicalBeamField ||
        (!cache->sparseBeamField &&
         (!cache->beamDirectionX || !cache->beamDirectionY ||
          !cache->beamDirectionZ || !cache->beamDistanceWeighted ||
          !cache->beamDirectionWeight)) ||
        !distributed_beam_position_to_cell(cache, position, cell)) {
        return false;
    }
    index = distributed_beam_cell_index(
        &cache->grid, cell[0], cell[1], cell[2]);
    if (cache->sparseBeamField) {
        sparse = RuntimeCausticPhotonSparseBrickCache3D_FindCell(
            cache->sparseBeamField, index);
        if (!sparse) return false;
    }
    result.physicalFlux = distributed_beam_subvoxel_flux(cache, position, cell);
    result.flux = result.physicalFlux;
    result.displayFlux = result.physicalFlux;
    direction_weight = sparse
        ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_WEIGHT_3D]
        : cache->beamDirectionWeight[index];
    if (!(distributed_beam_luma(result.physicalFlux) > 0.0) ||
        !(direction_weight > 0.0)) {
        return false;
    }
    result.meanBeamDirection = vec3_normalize(vec3(
        sparse ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_X_3D]
               : cache->beamDirectionX[index],
        sparse ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Y_3D]
               : cache->beamDirectionY[index],
        sparse ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Z_3D]
               : cache->beamDirectionZ[index]));
    result.meanBeamDistance =
        (sparse ? sparse[RUNTIME_CAUSTIC_SPARSE_FIELD_DISTANCE_WEIGHTED_3D]
                : cache->beamDistanceWeighted[index]) / direction_weight;
    result.beamDirectionWeightSum = direction_weight;
    result.contributingCount = 1u;
    result.effectiveSampleCount = 1u;
    result.hit = true;
    *out_result = result;
    return true;
}

bool RuntimeCausticDistributedBeamCache3D_ReadCellFields(
    const RuntimeCausticVolumeCache3D* cache,
    uint64_t linearCellIndex,
    float outFields[RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D]) {
    const float* sparse = NULL;
    if (!cache || !outFields || linearCellIndex >= cache->grid.cellCount ||
        !cache->radianceR || !cache->radianceG || !cache->radianceB) return false;
    memset(outFields, 0,
           RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D *
               sizeof(*outFields));
    outFields[0] = cache->radianceR[linearCellIndex];
    outFields[1] = cache->radianceG[linearCellIndex];
    outFields[2] = cache->radianceB[linearCellIndex];
    if (cache->sparseBeamField) {
        sparse = RuntimeCausticPhotonSparseBrickCache3D_FindCell(
            cache->sparseBeamField, linearCellIndex);
        if (sparse) {
            memcpy(&outFields[3], sparse,
                   RUNTIME_CAUSTIC_SPARSE_BRICK_FIELD_FLOAT_COUNT_3D *
                       sizeof(*sparse));
        }
        return true;
    }
    if (!cache->beamDirectionX || !cache->beamDirectionY ||
        !cache->beamDirectionZ || !cache->beamDistanceWeighted ||
        !cache->beamDirectionWeight || !cache->beamSubvoxelRadianceR ||
        !cache->beamSubvoxelRadianceG || !cache->beamSubvoxelRadianceB) {
        return true;
    }
    outFields[3] = cache->beamDirectionX[linearCellIndex];
    outFields[4] = cache->beamDirectionY[linearCellIndex];
    outFields[5] = cache->beamDirectionZ[linearCellIndex];
    outFields[6] = cache->beamDistanceWeighted[linearCellIndex];
    outFields[7] = cache->beamDirectionWeight[linearCellIndex];
    for (uint64_t subvoxel = 0u; subvoxel < 8u; ++subvoxel) {
        const uint64_t subvoxelIndex = linearCellIndex * 8u + subvoxel;
        outFields[8u + subvoxel] = cache->beamSubvoxelRadianceR[subvoxelIndex];
        outFields[16u + subvoxel] = cache->beamSubvoxelRadianceG[subvoxelIndex];
        outFields[24u + subvoxel] = cache->beamSubvoxelRadianceB[subvoxelIndex];
    }
    return true;
}
