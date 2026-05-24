#include "render/runtime_volume_3d_sampling.h"

#include <math.h>

static double runtime_volume_3d_sampling_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_volume_3d_sampling_length_epsilon(void) {
    double epsilon [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1e-9;
    return epsilon;
}

static double runtime_volume_3d_sampling_world_to_local(
    double world_value [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_origin [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double voxel_size [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double local = (world_value - world_origin) / voxel_size;
    return local;
}

static bool runtime_volume_3d_sampling_intersect_axis(
    double origin [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double direction,
    double slab_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double slab_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                                      double* io_t_enter,
                                                      double* io_t_exit) {
    double t0 [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double t1 [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double epsilon = runtime_volume_3d_sampling_length_epsilon();

    if (!io_t_enter || !io_t_exit) return false;
    if (fabs(direction) <= epsilon) {
        return origin >= slab_min && origin <= slab_max;
    }

    t0 = (slab_min - origin) / direction;
    t1 = (slab_max - origin) / direction;
    if (t0 > t1) {
        const double swap = t0;
        t0 = t1;
        t1 = swap;
    }

    if (t0 > *io_t_enter) *io_t_enter = t0;
    if (t1 < *io_t_exit) *io_t_exit = t1;
    return *io_t_exit >= *io_t_enter;
}

static uint64_t runtime_volume_3d_sampling_cell_index(const RuntimeVolumeGrid3D* grid,
                                                      uint32_t cell_x,
                                                      uint32_t cell_y,
                                                      uint32_t cell_z) {
    return (uint64_t)cell_x +
           ((uint64_t)grid->gridW * ((uint64_t)cell_y + ((uint64_t)grid->gridH * (uint64_t)cell_z)));
}

static float runtime_volume_3d_sampling_density_cell(const RuntimeVolumeAttachment3D* attachment,
                                                     const RuntimeVolumeGrid3D* grid,
                                                     uint32_t cell_x,
                                                     uint32_t cell_y,
                                                     uint32_t cell_z) {
    const uint64_t cell_index = runtime_volume_3d_sampling_cell_index(grid, cell_x, cell_y, cell_z);
    if (cell_index >= grid->cellCount) {
        return 0.0f;
    }
    if (attachment->channels.solidMask && attachment->channels.solidMask[cell_index] != 0u) {
        return 0.0f;
    }
    return attachment->channels.density[cell_index];
}

bool RuntimeVolume3D_HasSampleableDensity(const RuntimeVolumeAttachment3D* attachment) {
    if (!attachment) return false;
    if (!attachment->enabled || !attachment->affectsLighting || !attachment->hasData) {
        return false;
    }
    if (!RuntimeVolumeGrid3D_IsConfigured(&attachment->grid)) {
        return false;
    }
    return RuntimeVolumeAttachment3D_HasChannel(attachment, RUNTIME_VOLUME_3D_CHANNEL_DENSITY) &&
           attachment->channels.density != NULL;
}

bool RuntimeVolume3D_ClipRayToBounds(const RuntimeVolumeAttachment3D* attachment,
                                     const Ray3D* ray,
                                     double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                     double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                     double* out_t_enter,
                                     double* out_t_exit) {
    double t_enter [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_min;
    double t_exit [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_max;

    if (!attachment || !ray || !out_t_enter || !out_t_exit) return false;
    if (!RuntimeVolume3D_HasSampleableDensity(attachment)) return false;
    if (!(t_max > t_min)) return false;

    if (!runtime_volume_3d_sampling_intersect_axis(ray->origin.x,
                                                   ray->direction.x,
                                                   attachment->grid.boundsMin.x,
                                                   attachment->grid.boundsMax.x,
                                                   &t_enter,
                                                   &t_exit) ||
        !runtime_volume_3d_sampling_intersect_axis(ray->origin.y,
                                                   ray->direction.y,
                                                   attachment->grid.boundsMin.y,
                                                   attachment->grid.boundsMax.y,
                                                   &t_enter,
                                                   &t_exit) ||
        !runtime_volume_3d_sampling_intersect_axis(ray->origin.z,
                                                   ray->direction.z,
                                                   attachment->grid.boundsMin.z,
                                                   attachment->grid.boundsMax.z,
                                                   &t_enter,
                                                   &t_exit)) {
        return false;
    }

    if (!(t_exit > t_enter)) return false;
    *out_t_enter = t_enter;
    *out_t_exit = t_exit;
    return true;
}

float RuntimeVolume3D_SampleDensityAtPosition(const RuntimeVolumeAttachment3D* attachment,
                                              Vec3 position) {
    const RuntimeVolumeGrid3D* grid = NULL;
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
    double c00 = 0.0;
    double c10 = 0.0;
    double c01 = 0.0;
    double c11 = 0.0;
    double c0 = 0.0;
    double c1 = 0.0;

    if (!RuntimeVolume3D_HasSampleableDensity(attachment) || !attachment->channels.density) {
        return 0.0f;
    }

    grid = &attachment->grid;
    local_x = runtime_volume_3d_sampling_world_to_local(position.x, grid->origin.x, grid->voxelSize);
    local_y = runtime_volume_3d_sampling_world_to_local(position.y, grid->origin.y, grid->voxelSize);
    local_z = runtime_volume_3d_sampling_world_to_local(position.z, grid->origin.z, grid->voxelSize);
    if (local_x < 0.0 || local_y < 0.0 || local_z < 0.0 ||
        local_x >= (double)grid->gridW ||
        local_y >= (double)grid->gridH ||
        local_z >= (double)grid->gridD) {
        return 0.0f;
    }

    x0 = (uint32_t)local_x;
    y0 = (uint32_t)local_y;
    z0 = (uint32_t)local_z;
    x1 = (x0 + 1u < grid->gridW) ? (x0 + 1u) : x0;
    y1 = (y0 + 1u < grid->gridH) ? (y0 + 1u) : y0;
    z1 = (z0 + 1u < grid->gridD) ? (z0 + 1u) : z0;
    fx = local_x - floor(local_x);
    fy = local_y - floor(local_y);
    fz = local_z - floor(local_z);

    c00 = (1.0 - fx) * runtime_volume_3d_sampling_density_cell(attachment, grid, x0, y0, z0) +
          fx * runtime_volume_3d_sampling_density_cell(attachment, grid, x1, y0, z0);
    c10 = (1.0 - fx) * runtime_volume_3d_sampling_density_cell(attachment, grid, x0, y1, z0) +
          fx * runtime_volume_3d_sampling_density_cell(attachment, grid, x1, y1, z0);
    c01 = (1.0 - fx) * runtime_volume_3d_sampling_density_cell(attachment, grid, x0, y0, z1) +
          fx * runtime_volume_3d_sampling_density_cell(attachment, grid, x1, y0, z1);
    c11 = (1.0 - fx) * runtime_volume_3d_sampling_density_cell(attachment, grid, x0, y1, z1) +
          fx * runtime_volume_3d_sampling_density_cell(attachment, grid, x1, y1, z1);
    c0 = (1.0 - fy) * c00 + fy * c10;
    c1 = (1.0 - fy) * c01 + fy * c11;
    return (float)((1.0 - fz) * c0 + fz * c1);
}
