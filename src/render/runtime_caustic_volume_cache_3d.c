#include "render/runtime_caustic_volume_cache_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static uint64_t runtime_caustic_volume_cache_cell_index(
    const RuntimeVolumeGrid3D* grid,
    uint32_t cell_x,
    uint32_t cell_y,
    uint32_t cell_z) {
    return (uint64_t)cell_x +
           ((uint64_t)grid->gridW * ((uint64_t)cell_y + ((uint64_t)grid->gridH * (uint64_t)cell_z)));
}

static bool runtime_caustic_volume_cache_position_to_local(
    const RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    double* out_x,
    double* out_y,
    double* out_z) {
    const RuntimeVolumeGrid3D* grid = NULL;
    if (!RuntimeCausticVolumeCache3D_IsAllocated(cache) || !out_x || !out_y || !out_z) {
        return false;
    }
    grid = &cache->grid;
    *out_x = (position.x - grid->origin.x) / grid->voxelSize;
    *out_y = (position.y - grid->origin.y) / grid->voxelSize;
    *out_z = (position.z - grid->origin.z) / grid->voxelSize;
    return *out_x >= 0.0 && *out_y >= 0.0 && *out_z >= 0.0 &&
           *out_x < (double)grid->gridW &&
           *out_y < (double)grid->gridH &&
           *out_z < (double)grid->gridD;
}

static double runtime_caustic_volume_cache_cell_luma(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

static Vec3 runtime_caustic_volume_cache_cell_radiance(const RuntimeCausticVolumeCache3D* cache,
                                                       uint32_t cell_x,
                                                       uint32_t cell_y,
                                                       uint32_t cell_z) {
    const uint64_t index =
        runtime_caustic_volume_cache_cell_index(&cache->grid, cell_x, cell_y, cell_z);
    if (index >= cache->grid.cellCount) {
        return vec3(0.0, 0.0, 0.0);
    }
    return vec3(cache->radianceR[index], cache->radianceG[index], cache->radianceB[index]);
}

static Vec3 runtime_caustic_volume_cache_lerp_vec3(Vec3 a, Vec3 b, double t) {
    return vec3(a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t);
}

static void* runtime_caustic_volume_cache_calloc_cells(uint64_t cell_count,
                                                       size_t element_size) {
    if (cell_count == 0u || element_size == 0u) return NULL;
    if (cell_count > (uint64_t)(SIZE_MAX / element_size)) return NULL;
    return calloc((size_t)cell_count, element_size);
}

void RuntimeCausticVolumeCache3D_Init(RuntimeCausticVolumeCache3D* cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
    RuntimeVolumeGrid3D_Reset(&cache->grid);
}

bool RuntimeCausticVolumeCache3D_IsAllocated(const RuntimeCausticVolumeCache3D* cache) {
    return cache && cache->ownsBuffers &&
           RuntimeVolumeGrid3D_IsConfigured(&cache->grid) &&
           cache->radianceR && cache->radianceG && cache->radianceB;
}

bool RuntimeCausticVolumeCache3D_Allocate(RuntimeCausticVolumeCache3D* cache,
                                          const RuntimeVolumeGrid3D* grid) {
    RuntimeCausticVolumeCache3D allocated;

    if (!cache || !RuntimeVolumeGrid3D_IsConfigured(grid)) {
        return false;
    }

    RuntimeCausticVolumeCache3D_Init(&allocated);
    allocated.grid = *grid;
    allocated.radianceR = (float*)runtime_caustic_volume_cache_calloc_cells(
        grid->cellCount, sizeof(float));
    allocated.radianceG = (float*)runtime_caustic_volume_cache_calloc_cells(
        grid->cellCount, sizeof(float));
    allocated.radianceB = (float*)runtime_caustic_volume_cache_calloc_cells(
        grid->cellCount, sizeof(float));
    if (!allocated.radianceR || !allocated.radianceG || !allocated.radianceB) {
        RuntimeCausticVolumeCache3D_Free(&allocated);
        return false;
    }
    allocated.ownsBuffers = true;

    RuntimeCausticVolumeCache3D_Free(cache);
    *cache = allocated;
    return true;
}

bool RuntimeCausticVolumeCache3D_AllocateFromVolume(
    RuntimeCausticVolumeCache3D* cache,
    const RuntimeVolumeAttachment3D* volume) {
    if (!volume || !volume->hasData || !RuntimeVolumeGrid3D_IsConfigured(&volume->grid)) {
        return false;
    }
    return RuntimeCausticVolumeCache3D_Allocate(cache, &volume->grid);
}

void RuntimeCausticVolumeCache3D_Clear(RuntimeCausticVolumeCache3D* cache) {
    if (!RuntimeCausticVolumeCache3D_IsAllocated(cache)) return;
    memset(cache->radianceR, 0, (size_t)cache->grid.cellCount * sizeof(float));
    memset(cache->radianceG, 0, (size_t)cache->grid.cellCount * sizeof(float));
    memset(cache->radianceB, 0, (size_t)cache->grid.cellCount * sizeof(float));
    cache->depositAttemptCount = 0u;
    cache->depositAcceptedCount = 0u;
    cache->depositRejectedCount = 0u;
    cache->footprintDepositCount = 0u;
    cache->footprintCellContributionCount = 0u;
    cache->sampleLookupCount = 0u;
    cache->sampleContributingCount = 0u;
    cache->footprintRadiusVoxelSum = 0.0;
    cache->footprintInputRadianceR = 0.0;
    cache->footprintInputRadianceG = 0.0;
    cache->footprintInputRadianceB = 0.0;
    cache->footprintDepositedRadianceR = 0.0;
    cache->footprintDepositedRadianceG = 0.0;
    cache->footprintDepositedRadianceB = 0.0;
}

void RuntimeCausticVolumeCache3D_Free(RuntimeCausticVolumeCache3D* cache) {
    if (!cache) return;
    free(cache->radianceR);
    free(cache->radianceG);
    free(cache->radianceB);
    RuntimeCausticVolumeCache3D_Init(cache);
}

bool RuntimeCausticVolumeCache3D_DepositAtPosition(RuntimeCausticVolumeCache3D* cache,
                                                   Vec3 position,
                                                   double radiance_r,
                                                   double radiance_g,
                                                   double radiance_b) {
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 0.0;
    uint32_t cell_x = 0u;
    uint32_t cell_y = 0u;
    uint32_t cell_z = 0u;
    uint64_t index = 0u;

    if (!cache) return false;
    cache->depositAttemptCount += 1u;
    if (!runtime_caustic_volume_cache_position_to_local(
            cache, position, &local_x, &local_y, &local_z)) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    if (radiance_r < 0.0) radiance_r = 0.0;
    if (radiance_g < 0.0) radiance_g = 0.0;
    if (radiance_b < 0.0) radiance_b = 0.0;
    cell_x = (uint32_t)local_x;
    cell_y = (uint32_t)local_y;
    cell_z = (uint32_t)local_z;
    index = runtime_caustic_volume_cache_cell_index(&cache->grid, cell_x, cell_y, cell_z);
    if (index >= cache->grid.cellCount) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    cache->radianceR[index] += (float)radiance_r;
    cache->radianceG[index] += (float)radiance_g;
    cache->radianceB[index] += (float)radiance_b;
    cache->depositAcceptedCount += 1u;
    return true;
}

bool RuntimeCausticVolumeCache3D_DepositFootprintAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    double radius_world,
    double radiance_r,
    double radiance_g,
    double radiance_b) {
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 0.0;
    double radius_voxels = 0.0;
    int min_x = 0;
    int min_y = 0;
    int min_z = 0;
    int max_x = 0;
    int max_y = 0;
    int max_z = 0;
    double weight_sum = 0.0;
    uint64_t cell_count = 0u;
    double deposited_r = 0.0;
    double deposited_g = 0.0;
    double deposited_b = 0.0;

    if (!cache) return false;
    cache->depositAttemptCount += 1u;
    if (!runtime_caustic_volume_cache_position_to_local(
            cache, position, &local_x, &local_y, &local_z)) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    if (radiance_r < 0.0) radiance_r = 0.0;
    if (radiance_g < 0.0) radiance_g = 0.0;
    if (radiance_b < 0.0) radiance_b = 0.0;
    if (!(radius_world > 0.0) || !isfinite(radius_world)) {
        radius_world = cache->grid.voxelSize * 0.5;
    }
    radius_voxels = radius_world / cache->grid.voxelSize;
    if (!(radius_voxels > 0.0) || !isfinite(radius_voxels)) {
        radius_voxels = 0.5;
    }
    if (radius_voxels < 0.5) radius_voxels = 0.5;

    min_x = (int)floor(local_x - radius_voxels - 0.5);
    min_y = (int)floor(local_y - radius_voxels - 0.5);
    min_z = (int)floor(local_z - radius_voxels - 0.5);
    max_x = (int)ceil(local_x + radius_voxels + 0.5);
    max_y = (int)ceil(local_y + radius_voxels + 0.5);
    max_z = (int)ceil(local_z + radius_voxels + 0.5);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (min_z < 0) min_z = 0;
    if (max_x >= (int)cache->grid.gridW) max_x = (int)cache->grid.gridW - 1;
    if (max_y >= (int)cache->grid.gridH) max_y = (int)cache->grid.gridH - 1;
    if (max_z >= (int)cache->grid.gridD) max_z = (int)cache->grid.gridD - 1;

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double center_x = (double)x + 0.5;
                const double center_y = (double)y + 0.5;
                const double center_z = (double)z + 0.5;
                const double dx = center_x - local_x;
                const double dy = center_y - local_y;
                const double dz = center_z - local_z;
                const double distance = sqrt((dx * dx) + (dy * dy) + (dz * dz));
                double weight = 0.0;
                if (distance <= radius_voxels) {
                    weight = 1.0 - (distance / fmax(radius_voxels, 1.0e-9));
                }
                if (weight <= 0.0 && distance <= 0.5) {
                    weight = 1.0;
                }
                if (weight > 0.0) {
                    weight_sum += weight;
                    cell_count += 1u;
                }
            }
        }
    }
    if (!(weight_sum > 0.0) || cell_count == 0u) {
        cache->depositRejectedCount += 1u;
        return false;
    }

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double center_x = (double)x + 0.5;
                const double center_y = (double)y + 0.5;
                const double center_z = (double)z + 0.5;
                const double dx = center_x - local_x;
                const double dy = center_y - local_y;
                const double dz = center_z - local_z;
                const double distance = sqrt((dx * dx) + (dy * dy) + (dz * dz));
                double weight = 0.0;
                if (distance <= radius_voxels) {
                    weight = 1.0 - (distance / fmax(radius_voxels, 1.0e-9));
                }
                if (weight <= 0.0 && distance <= 0.5) {
                    weight = 1.0;
                }
                if (weight > 0.0) {
                    const double normalized = weight / weight_sum;
                    const double add_r = radiance_r * normalized;
                    const double add_g = radiance_g * normalized;
                    const double add_b = radiance_b * normalized;
                    const uint64_t index = runtime_caustic_volume_cache_cell_index(
                        &cache->grid, (uint32_t)x, (uint32_t)y, (uint32_t)z);
                    if (index < cache->grid.cellCount) {
                        cache->radianceR[index] += (float)add_r;
                        cache->radianceG[index] += (float)add_g;
                        cache->radianceB[index] += (float)add_b;
                        deposited_r += add_r;
                        deposited_g += add_g;
                        deposited_b += add_b;
                    }
                }
            }
        }
    }

    cache->depositAcceptedCount += 1u;
    cache->footprintDepositCount += 1u;
    cache->footprintCellContributionCount += cell_count;
    cache->footprintRadiusVoxelSum += radius_voxels;
    cache->footprintInputRadianceR += radiance_r;
    cache->footprintInputRadianceG += radiance_g;
    cache->footprintInputRadianceB += radiance_b;
    cache->footprintDepositedRadianceR += deposited_r;
    cache->footprintDepositedRadianceG += deposited_g;
    cache->footprintDepositedRadianceB += deposited_b;
    return true;
}

bool RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    Vec3 direction,
    double perpendicular_radius_world,
    double axial_radius_world,
    double radiance_r,
    double radiance_g,
    double radiance_b) {
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 0.0;
    double perpendicular_radius_voxels = 0.0;
    double axial_radius_voxels = 0.0;
    double bounds_radius_voxels = 0.0;
    Vec3 axis = vec3(0.0, 0.0, 0.0);
    int min_x = 0;
    int min_y = 0;
    int min_z = 0;
    int max_x = 0;
    int max_y = 0;
    int max_z = 0;
    double weight_sum = 0.0;
    uint64_t cell_count = 0u;
    double deposited_r = 0.0;
    double deposited_g = 0.0;
    double deposited_b = 0.0;

    if (!cache) return false;
    if (!(vec3_length(direction) > 1.0e-9) ||
        !(perpendicular_radius_world > 0.0) ||
        !(axial_radius_world > 0.0) ||
        !isfinite(perpendicular_radius_world) ||
        !isfinite(axial_radius_world)) {
        return RuntimeCausticVolumeCache3D_DepositFootprintAtPosition(cache,
                                                                      position,
                                                                      perpendicular_radius_world,
                                                                      radiance_r,
                                                                      radiance_g,
                                                                      radiance_b);
    }

    cache->depositAttemptCount += 1u;
    if (!runtime_caustic_volume_cache_position_to_local(
            cache, position, &local_x, &local_y, &local_z)) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    if (radiance_r < 0.0) radiance_r = 0.0;
    if (radiance_g < 0.0) radiance_g = 0.0;
    if (radiance_b < 0.0) radiance_b = 0.0;

    axis = vec3_normalize(direction);
    perpendicular_radius_voxels = perpendicular_radius_world / cache->grid.voxelSize;
    axial_radius_voxels = axial_radius_world / cache->grid.voxelSize;
    if (!(perpendicular_radius_voxels > 0.0) || !isfinite(perpendicular_radius_voxels)) {
        perpendicular_radius_voxels = 0.5;
    }
    if (!(axial_radius_voxels > 0.0) || !isfinite(axial_radius_voxels)) {
        axial_radius_voxels = perpendicular_radius_voxels;
    }
    if (perpendicular_radius_voxels < 0.5) perpendicular_radius_voxels = 0.5;
    if (axial_radius_voxels < 0.5) axial_radius_voxels = 0.5;
    bounds_radius_voxels = fmax(perpendicular_radius_voxels, axial_radius_voxels);

    min_x = (int)floor(local_x - bounds_radius_voxels - 0.5);
    min_y = (int)floor(local_y - bounds_radius_voxels - 0.5);
    min_z = (int)floor(local_z - bounds_radius_voxels - 0.5);
    max_x = (int)ceil(local_x + bounds_radius_voxels + 0.5);
    max_y = (int)ceil(local_y + bounds_radius_voxels + 0.5);
    max_z = (int)ceil(local_z + bounds_radius_voxels + 0.5);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (min_z < 0) min_z = 0;
    if (max_x >= (int)cache->grid.gridW) max_x = (int)cache->grid.gridW - 1;
    if (max_y >= (int)cache->grid.gridH) max_y = (int)cache->grid.gridH - 1;
    if (max_z >= (int)cache->grid.gridD) max_z = (int)cache->grid.gridD - 1;

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double center_x = (double)x + 0.5;
                const double center_y = (double)y + 0.5;
                const double center_z = (double)z + 0.5;
                const double dx = center_x - local_x;
                const double dy = center_y - local_y;
                const double dz = center_z - local_z;
                const double axial = (dx * axis.x) + (dy * axis.y) + (dz * axis.z);
                const double distance2 = (dx * dx) + (dy * dy) + (dz * dz);
                const double perp2 = fmax(0.0, distance2 - (axial * axial));
                const double shaped_distance = sqrt(
                    (perp2 / fmax(perpendicular_radius_voxels * perpendicular_radius_voxels, 1.0e-9)) +
                    ((axial * axial) / fmax(axial_radius_voxels * axial_radius_voxels, 1.0e-9)));
                const double raw_distance = sqrt(distance2);
                double weight = 0.0;
                if (shaped_distance <= 1.0) {
                    weight = 1.0 - shaped_distance;
                }
                if (weight <= 0.0 && raw_distance <= 0.5) {
                    weight = 1.0;
                }
                if (weight > 0.0) {
                    weight_sum += weight;
                    cell_count += 1u;
                }
            }
        }
    }
    if (!(weight_sum > 0.0) || cell_count == 0u) {
        cache->depositRejectedCount += 1u;
        return false;
    }

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double center_x = (double)x + 0.5;
                const double center_y = (double)y + 0.5;
                const double center_z = (double)z + 0.5;
                const double dx = center_x - local_x;
                const double dy = center_y - local_y;
                const double dz = center_z - local_z;
                const double axial = (dx * axis.x) + (dy * axis.y) + (dz * axis.z);
                const double distance2 = (dx * dx) + (dy * dy) + (dz * dz);
                const double perp2 = fmax(0.0, distance2 - (axial * axial));
                const double shaped_distance = sqrt(
                    (perp2 / fmax(perpendicular_radius_voxels * perpendicular_radius_voxels, 1.0e-9)) +
                    ((axial * axial) / fmax(axial_radius_voxels * axial_radius_voxels, 1.0e-9)));
                const double raw_distance = sqrt(distance2);
                double weight = 0.0;
                if (shaped_distance <= 1.0) {
                    weight = 1.0 - shaped_distance;
                }
                if (weight <= 0.0 && raw_distance <= 0.5) {
                    weight = 1.0;
                }
                if (weight > 0.0) {
                    const double normalized = weight / weight_sum;
                    const double add_r = radiance_r * normalized;
                    const double add_g = radiance_g * normalized;
                    const double add_b = radiance_b * normalized;
                    const uint64_t index = runtime_caustic_volume_cache_cell_index(
                        &cache->grid, (uint32_t)x, (uint32_t)y, (uint32_t)z);
                    if (index < cache->grid.cellCount) {
                        cache->radianceR[index] += (float)add_r;
                        cache->radianceG[index] += (float)add_g;
                        cache->radianceB[index] += (float)add_b;
                        deposited_r += add_r;
                        deposited_g += add_g;
                        deposited_b += add_b;
                    }
                }
            }
        }
    }

    cache->depositAcceptedCount += 1u;
    cache->footprintDepositCount += 1u;
    cache->footprintCellContributionCount += cell_count;
    cache->footprintRadiusVoxelSum +=
        ((2.0 * perpendicular_radius_voxels) + axial_radius_voxels) / 3.0;
    cache->footprintInputRadianceR += radiance_r;
    cache->footprintInputRadianceG += radiance_g;
    cache->footprintInputRadianceB += radiance_b;
    cache->footprintDepositedRadianceR += deposited_r;
    cache->footprintDepositedRadianceG += deposited_g;
    cache->footprintDepositedRadianceB += deposited_b;
    return true;
}

bool RuntimeCausticVolumeCache3D_SampleAtPosition(RuntimeCausticVolumeCache3D* cache,
                                                  Vec3 position,
                                                  Vec3* out_radiance) {
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 0.0;
    uint32_t x0 = 0u;
    uint32_t y0 = 0u;
    uint32_t z0 = 0u;
    uint32_t x1 = 0u;
    uint32_t y1 = 0u;
    uint32_t z1 = 0u;
    double fx = 0.0;
    double fy = 0.0;
    double fz = 0.0;
    Vec3 c000;
    Vec3 c100;
    Vec3 c010;
    Vec3 c110;
    Vec3 c001;
    Vec3 c101;
    Vec3 c011;
    Vec3 c111;
    Vec3 c00;
    Vec3 c10;
    Vec3 c01;
    Vec3 c11;
    Vec3 c0;
    Vec3 c1;
    Vec3 result;

    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (!cache || !out_radiance) return false;
    cache->sampleLookupCount += 1u;
    if (!runtime_caustic_volume_cache_position_to_local(
            cache, position, &local_x, &local_y, &local_z)) {
        return false;
    }

    x0 = (uint32_t)local_x;
    y0 = (uint32_t)local_y;
    z0 = (uint32_t)local_z;
    x1 = (x0 + 1u < cache->grid.gridW) ? (x0 + 1u) : x0;
    y1 = (y0 + 1u < cache->grid.gridH) ? (y0 + 1u) : y0;
    z1 = (z0 + 1u < cache->grid.gridD) ? (z0 + 1u) : z0;
    fx = local_x - floor(local_x);
    fy = local_y - floor(local_y);
    fz = local_z - floor(local_z);

    c000 = runtime_caustic_volume_cache_cell_radiance(cache, x0, y0, z0);
    c100 = runtime_caustic_volume_cache_cell_radiance(cache, x1, y0, z0);
    c010 = runtime_caustic_volume_cache_cell_radiance(cache, x0, y1, z0);
    c110 = runtime_caustic_volume_cache_cell_radiance(cache, x1, y1, z0);
    c001 = runtime_caustic_volume_cache_cell_radiance(cache, x0, y0, z1);
    c101 = runtime_caustic_volume_cache_cell_radiance(cache, x1, y0, z1);
    c011 = runtime_caustic_volume_cache_cell_radiance(cache, x0, y1, z1);
    c111 = runtime_caustic_volume_cache_cell_radiance(cache, x1, y1, z1);

    c00 = runtime_caustic_volume_cache_lerp_vec3(c000, c100, fx);
    c10 = runtime_caustic_volume_cache_lerp_vec3(c010, c110, fx);
    c01 = runtime_caustic_volume_cache_lerp_vec3(c001, c101, fx);
    c11 = runtime_caustic_volume_cache_lerp_vec3(c011, c111, fx);
    c0 = runtime_caustic_volume_cache_lerp_vec3(c00, c10, fy);
    c1 = runtime_caustic_volume_cache_lerp_vec3(c01, c11, fy);
    result = runtime_caustic_volume_cache_lerp_vec3(c0, c1, fz);
    *out_radiance = result;
    if (result.x > 0.0 || result.y > 0.0 || result.z > 0.0) {
        cache->sampleContributingCount += 1u;
    }
    return true;
}

bool RuntimeCausticVolumeCache3D_SampleFilteredAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    double radius_world,
    Vec3* out_radiance) {
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 0.0;
    double radius_voxels = 0.0;
    int min_x = 0;
    int min_y = 0;
    int min_z = 0;
    int max_x = 0;
    int max_y = 0;
    int max_z = 0;
    double weight_sum = 0.0;
    Vec3 radiance_sum = vec3(0.0, 0.0, 0.0);

    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (!cache || !out_radiance) return false;
    cache->sampleLookupCount += 1u;
    if (!runtime_caustic_volume_cache_position_to_local(
            cache, position, &local_x, &local_y, &local_z)) {
        return false;
    }
    if (!(radius_world > 0.0) || !isfinite(radius_world)) {
        radius_world = cache->grid.voxelSize;
    }
    radius_voxels = radius_world / cache->grid.voxelSize;
    if (!(radius_voxels > 0.0) || !isfinite(radius_voxels)) {
        radius_voxels = 1.0;
    }
    if (radius_voxels < 0.5) radius_voxels = 0.5;
    if (radius_voxels > 3.25) radius_voxels = 3.25;

    min_x = (int)floor(local_x - radius_voxels - 0.5);
    min_y = (int)floor(local_y - radius_voxels - 0.5);
    min_z = (int)floor(local_z - radius_voxels - 0.5);
    max_x = (int)ceil(local_x + radius_voxels + 0.5);
    max_y = (int)ceil(local_y + radius_voxels + 0.5);
    max_z = (int)ceil(local_z + radius_voxels + 0.5);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (min_z < 0) min_z = 0;
    if (max_x >= (int)cache->grid.gridW) max_x = (int)cache->grid.gridW - 1;
    if (max_y >= (int)cache->grid.gridH) max_y = (int)cache->grid.gridH - 1;
    if (max_z >= (int)cache->grid.gridD) max_z = (int)cache->grid.gridD - 1;

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double center_x = (double)x + 0.5;
                const double center_y = (double)y + 0.5;
                const double center_z = (double)z + 0.5;
                const double dx = center_x - local_x;
                const double dy = center_y - local_y;
                const double dz = center_z - local_z;
                const double distance = sqrt((dx * dx) + (dy * dy) + (dz * dz));
                double weight = 0.0;
                if (distance <= radius_voxels) {
                    weight = 1.0 - (distance / fmax(radius_voxels, 1.0e-9));
                }
                if (weight <= 0.0 && distance <= 0.5) {
                    weight = 1.0;
                }
                if (weight > 0.0) {
                    const Vec3 cell = runtime_caustic_volume_cache_cell_radiance(
                        cache, (uint32_t)x, (uint32_t)y, (uint32_t)z);
                    if (cell.x > 0.0 || cell.y > 0.0 || cell.z > 0.0) {
                        radiance_sum = vec3_add(radiance_sum, vec3_scale(cell, weight));
                        weight_sum += weight;
                    }
                }
            }
        }
    }

    if (weight_sum > 0.0) {
        *out_radiance = vec3_scale(radiance_sum, 1.0 / weight_sum);
        cache->sampleContributingCount += 1u;
        return true;
    }

    return true;
}

void RuntimeCausticVolumeCache3D_SnapshotDiagnostics(
    const RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticVolumeCacheDiagnostics3D* out_diagnostics) {
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics = {0};
    uint64_t i = 0u;
    double centroid_weight = 0.0;
    Vec3 centroid_sum = vec3(0.0, 0.0, 0.0);

    if (!out_diagnostics) return;
    if (!RuntimeCausticVolumeCache3D_IsAllocated(cache)) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_NONE;
        *out_diagnostics = diagnostics;
        return;
    }

    diagnostics.allocated = true;
    diagnostics.gridW = cache->grid.gridW;
    diagnostics.gridH = cache->grid.gridH;
    diagnostics.gridD = cache->grid.gridD;
    diagnostics.cellCount = cache->grid.cellCount;
    diagnostics.origin = cache->grid.origin;
    diagnostics.boundsMin = cache->grid.boundsMin;
    diagnostics.boundsMax = cache->grid.boundsMax;
    diagnostics.voxelSize = cache->grid.voxelSize;
    diagnostics.allocatedCellCount = cache->grid.cellCount;
    diagnostics.depositAttemptCount = cache->depositAttemptCount;
    diagnostics.depositAcceptedCount = cache->depositAcceptedCount;
    diagnostics.depositRejectedCount = cache->depositRejectedCount;
    diagnostics.footprintDepositCount = cache->footprintDepositCount;
    diagnostics.footprintCellContributionCount = cache->footprintCellContributionCount;
    diagnostics.sampleLookupCount = cache->sampleLookupCount;
    diagnostics.sampleContributingCount = cache->sampleContributingCount;
    diagnostics.footprintInputRadianceR = cache->footprintInputRadianceR;
    diagnostics.footprintInputRadianceG = cache->footprintInputRadianceG;
    diagnostics.footprintInputRadianceB = cache->footprintInputRadianceB;
    diagnostics.footprintDepositedRadianceR = cache->footprintDepositedRadianceR;
    diagnostics.footprintDepositedRadianceG = cache->footprintDepositedRadianceG;
    diagnostics.footprintDepositedRadianceB = cache->footprintDepositedRadianceB;
    diagnostics.averageFootprintRadiusVoxels =
        cache->footprintDepositCount > 0u
            ? cache->footprintRadiusVoxelSum / (double)cache->footprintDepositCount
            : 0.0;

    for (i = 0u; i < cache->grid.cellCount; ++i) {
        const double r = cache->radianceR[i];
        const double g = cache->radianceG[i];
        const double b = cache->radianceB[i];
        const double luma = runtime_caustic_volume_cache_cell_luma(r, g, b);
        diagnostics.totalRadianceR += r;
        diagnostics.totalRadianceG += g;
        diagnostics.totalRadianceB += b;
        if (r > 0.0 || g > 0.0 || b > 0.0) {
            const uint64_t xy_count =
                (uint64_t)cache->grid.gridW * (uint64_t)cache->grid.gridH;
            const uint32_t z = xy_count > 0u ? (uint32_t)(i / xy_count) : 0u;
            const uint64_t xy_index = xy_count > 0u ? (i % xy_count) : 0u;
            const uint32_t y = cache->grid.gridW > 0u
                                   ? (uint32_t)(xy_index / (uint64_t)cache->grid.gridW)
                                   : 0u;
            const uint32_t x = cache->grid.gridW > 0u
                                   ? (uint32_t)(xy_index % (uint64_t)cache->grid.gridW)
                                   : 0u;
            const Vec3 center =
                vec3(cache->grid.origin.x + (((double)x + 0.5) * cache->grid.voxelSize),
                     cache->grid.origin.y + (((double)y + 0.5) * cache->grid.voxelSize),
                     cache->grid.origin.z + (((double)z + 0.5) * cache->grid.voxelSize));
            diagnostics.nonZeroCellCount += 1u;
            if (!diagnostics.hasNonZeroBounds) {
                diagnostics.nonZeroBoundsMin = center;
                diagnostics.nonZeroBoundsMax = center;
                diagnostics.hasNonZeroBounds = true;
            } else {
                if (center.x < diagnostics.nonZeroBoundsMin.x) diagnostics.nonZeroBoundsMin.x = center.x;
                if (center.y < diagnostics.nonZeroBoundsMin.y) diagnostics.nonZeroBoundsMin.y = center.y;
                if (center.z < diagnostics.nonZeroBoundsMin.z) diagnostics.nonZeroBoundsMin.z = center.z;
                if (center.x > diagnostics.nonZeroBoundsMax.x) diagnostics.nonZeroBoundsMax.x = center.x;
                if (center.y > diagnostics.nonZeroBoundsMax.y) diagnostics.nonZeroBoundsMax.y = center.y;
                if (center.z > diagnostics.nonZeroBoundsMax.z) diagnostics.nonZeroBoundsMax.z = center.z;
            }
            if (luma > 0.0) {
                centroid_sum = vec3_add(centroid_sum, vec3_scale(center, luma));
                centroid_weight += luma;
            }
        }
        if (luma > diagnostics.maxCellRadiance) {
            diagnostics.maxCellRadiance = luma;
        }
    }
    if (centroid_weight > 0.0) {
        diagnostics.radianceCentroid = vec3_scale(centroid_sum, 1.0 / centroid_weight);
    } else if (diagnostics.hasNonZeroBounds) {
        diagnostics.radianceCentroid =
            vec3_scale(vec3_add(diagnostics.nonZeroBoundsMin, diagnostics.nonZeroBoundsMax),
                       0.5);
    }
    if (diagnostics.sampleContributingCount > 0u) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED;
    } else if (diagnostics.nonZeroCellCount > 0u) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_POPULATED;
    } else {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY;
    }
    *out_diagnostics = diagnostics;
}
