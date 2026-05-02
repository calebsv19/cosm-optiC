#include "render/runtime_volume_3d_sampling.h"

#include <math.h>

static bool runtime_volume_3d_sampling_intersect_axis(double origin,
                                                      double direction,
                                                      double slab_min,
                                                      double slab_max,
                                                      double* io_t_enter,
                                                      double* io_t_exit) {
    double t0 = 0.0;
    double t1 = 0.0;

    if (!io_t_enter || !io_t_exit) return false;
    if (fabs(direction) <= 1e-9) {
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
                                     double t_min,
                                     double t_max,
                                     double* out_t_enter,
                                     double* out_t_exit) {
    double t_enter = t_min;
    double t_exit = t_max;

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
    uint32_t cell_x = 0u;
    uint32_t cell_y = 0u;
    uint32_t cell_z = 0u;
    uint64_t cell_index = 0u;

    if (!RuntimeVolume3D_HasSampleableDensity(attachment) || !attachment->channels.density) {
        return 0.0f;
    }

    grid = &attachment->grid;
    local_x = (position.x - grid->origin.x) / grid->voxelSize;
    local_y = (position.y - grid->origin.y) / grid->voxelSize;
    local_z = (position.z - grid->origin.z) / grid->voxelSize;
    if (local_x < 0.0 || local_y < 0.0 || local_z < 0.0 ||
        local_x >= (double)grid->gridW ||
        local_y >= (double)grid->gridH ||
        local_z >= (double)grid->gridD) {
        return 0.0f;
    }

    cell_x = (uint32_t)local_x;
    cell_y = (uint32_t)local_y;
    cell_z = (uint32_t)local_z;
    cell_index = runtime_volume_3d_sampling_cell_index(grid, cell_x, cell_y, cell_z);
    if (cell_index >= grid->cellCount) {
        return 0.0f;
    }
    if (attachment->channels.solidMask && attachment->channels.solidMask[cell_index] != 0u) {
        return 0.0f;
    }
    return attachment->channels.density[cell_index];
}
