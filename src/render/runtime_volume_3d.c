#include "render/runtime_volume_3d.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static double runtime_volume_3d_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_volume_3d_zero_time(void) {
    double zero [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0;
    return zero;
}

static double runtime_volume_3d_length_epsilon(void) {
    double epsilon [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1e-9;
    return epsilon;
}

static Vec3 runtime_volume_3d_default_scene_up(void) {
    return vec3(0.0, 0.0, 1.0);
}

static bool runtime_volume_3d_try_compute_cell_count(uint32_t grid_w,
                                                     uint32_t grid_h,
                                                     uint32_t grid_d,
                                                     uint64_t* out_count) {
    uint64_t count = 0;
    if (!out_count || grid_w == 0u || grid_h == 0u || grid_d == 0u) {
        return false;
    }

    count = (uint64_t)grid_w;
    if ((uint64_t)grid_h > UINT64_MAX / count) return false;
    count *= (uint64_t)grid_h;
    if ((uint64_t)grid_d > UINT64_MAX / count) return false;
    count *= (uint64_t)grid_d;
    *out_count = count;
    return true;
}

static void runtime_volume_3d_free_owned_channels(RuntimeVolumeChannels3D* channels) {
    if (!channels) return;
    free(channels->density);
    free(channels->velocityX);
    free(channels->velocityY);
    free(channels->velocityZ);
    free(channels->pressure);
    free(channels->solidMask);
    RuntimeVolumeChannels3D_Reset(channels);
}

static void* runtime_volume_3d_calloc_cells(uint64_t cell_count, size_t element_size) {
    if (cell_count == 0u || element_size == 0u) return NULL;
    if (cell_count > (uint64_t)(SIZE_MAX / element_size)) return NULL;
    return calloc((size_t)cell_count, element_size);
}

const char* RuntimeVolume3DSourceKindLabel(RuntimeVolume3DSourceKind kind) {
    switch (kind) {
        case RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:
            return "raw_vf3d";
        case RUNTIME_VOLUME_3D_SOURCE_PACK:
            return "pack";
        case RUNTIME_VOLUME_3D_SOURCE_MANIFEST:
            return "manifest";
        case RUNTIME_VOLUME_3D_SOURCE_NONE:
        default:
            return "none";
    }
}

bool RuntimeVolumeGrid3D_Configure(RuntimeVolumeGrid3D* grid,
                                   uint32_t format_version,
                                   uint32_t grid_w,
                                   uint32_t grid_h,
                                   uint32_t grid_d,
                                   double time_seconds [[fisics::dim(time)]] [[fisics::unit(second)]],
                                   uint64_t frame_index,
                                   double dt_seconds [[fisics::dim(time)]] [[fisics::unit(second)]],
                                   Vec3 origin,
                                   double voxel_size [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                   Vec3 scene_up,
                                   uint32_t solid_mask_crc32) {
    Vec3 scene_up_resolved = scene_up;
    Vec3 max_extent = {0};
    uint64_t cell_count = 0;
    double zero_length = runtime_volume_3d_zero_length();
    double zero_time = runtime_volume_3d_zero_time();
    double scene_up_length = 0.0;
    double epsilon = runtime_volume_3d_length_epsilon();
    double extent_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = zero_length;
    double extent_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = zero_length;
    double extent_z [[fisics::dim(length)]] [[fisics::unit(meter)]] = zero_length;

    if (!grid || !(voxel_size > zero_length)) {
        return false;
    }
    if (!(dt_seconds >= zero_time)) {
        return false;
    }
    if (!runtime_volume_3d_try_compute_cell_count(grid_w, grid_h, grid_d, &cell_count)) {
        return false;
    }
    scene_up_length = vec3_length(scene_up_resolved);
    if (scene_up_length <= epsilon) {
        scene_up_resolved = runtime_volume_3d_default_scene_up();
    }

    RuntimeVolumeGrid3D_Reset(grid);
    extent_x = (double)grid_w * voxel_size;
    extent_y = (double)grid_h * voxel_size;
    extent_z = (double)grid_d * voxel_size;
    max_extent = vec3(extent_x, extent_y, extent_z);
    grid->formatVersion = format_version;
    grid->gridW = grid_w;
    grid->gridH = grid_h;
    grid->gridD = grid_d;
    grid->cellCount = cell_count;
    grid->timeSeconds = time_seconds;
    grid->frameIndex = frame_index;
    grid->dtSeconds = dt_seconds;
    grid->origin = origin;
    grid->voxelSize = voxel_size;
    grid->sceneUp = scene_up_resolved;
    grid->boundsMin = origin;
    grid->boundsMax = vec3_add(origin, max_extent);
    grid->solidMaskCrc32 = solid_mask_crc32;
    grid->valid = true;
    return true;
}

bool RuntimeVolumeGrid3D_IsConfigured(const RuntimeVolumeGrid3D* grid) {
    double zero_length = runtime_volume_3d_zero_length();
    return grid && grid->valid && grid->cellCount > 0u && grid->voxelSize > zero_length;
}

void RuntimeVolumeGrid3D_Reset(RuntimeVolumeGrid3D* grid) {
    if (!grid) return;
    memset(grid, 0, sizeof(*grid));
    grid->sceneUp = runtime_volume_3d_default_scene_up();
}

bool RuntimeVolumeChannels3D_HasMask(const RuntimeVolumeChannels3D* channels,
                                     uint32_t channel_mask) {
    if (!channels || channel_mask == 0u) return false;
    return (channels->channelMask & channel_mask) == channel_mask;
}

void RuntimeVolumeChannels3D_Reset(RuntimeVolumeChannels3D* channels) {
    if (!channels) return;
    memset(channels, 0, sizeof(*channels));
}

void RuntimeVolumeAttachment3D_Init(RuntimeVolumeAttachment3D* attachment) {
    if (!attachment) return;
    memset(attachment, 0, sizeof(*attachment));
    attachment->sourceKind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    attachment->affectsLighting = true;
    RuntimeVolumeGrid3D_Reset(&attachment->grid);
    RuntimeVolumeChannels3D_Reset(&attachment->channels);
}

void RuntimeVolumeAttachment3D_ClearOwnedChannels(RuntimeVolumeAttachment3D* attachment) {
    if (!attachment) return;
    if (attachment->ownsChannelBuffers) {
        runtime_volume_3d_free_owned_channels(&attachment->channels);
    } else {
        RuntimeVolumeChannels3D_Reset(&attachment->channels);
    }
    attachment->ownsChannelBuffers = false;
    attachment->hasData = false;
}

bool RuntimeVolumeAttachment3D_AllocateOwnedChannels(RuntimeVolumeAttachment3D* attachment,
                                                     uint32_t channel_mask) {
    RuntimeVolumeChannels3D allocated = {0};

    if (!attachment) return false;
    if (channel_mask == 0u) {
        RuntimeVolumeAttachment3D_ClearOwnedChannels(attachment);
        return true;
    }
    if (!RuntimeVolumeGrid3D_IsConfigured(&attachment->grid)) {
        return false;
    }

    allocated.channelMask = channel_mask;
    if (channel_mask & RUNTIME_VOLUME_3D_CHANNEL_DENSITY) {
        allocated.density = (float*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(float));
        if (!allocated.density) goto fail;
    }
    if (channel_mask & RUNTIME_VOLUME_3D_CHANNEL_VELOCITY) {
        allocated.velocityX = (float*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(float));
        allocated.velocityY = (float*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(float));
        allocated.velocityZ = (float*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(float));
        if (!allocated.velocityX || !allocated.velocityY || !allocated.velocityZ) goto fail;
    }
    if (channel_mask & RUNTIME_VOLUME_3D_CHANNEL_PRESSURE) {
        allocated.pressure = (float*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(float));
        if (!allocated.pressure) goto fail;
    }
    if (channel_mask & RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK) {
        allocated.solidMask = (uint8_t*)runtime_volume_3d_calloc_cells(
            attachment->grid.cellCount, sizeof(uint8_t));
        if (!allocated.solidMask) goto fail;
    }

    RuntimeVolumeAttachment3D_ClearOwnedChannels(attachment);
    attachment->channels = allocated;
    attachment->ownsChannelBuffers = true;
    attachment->hasData = true;
    return true;

fail:
    runtime_volume_3d_free_owned_channels(&allocated);
    return false;
}

bool RuntimeVolumeAttachment3D_HasChannel(const RuntimeVolumeAttachment3D* attachment,
                                          uint32_t channel_mask) {
    if (!attachment) return false;
    return RuntimeVolumeChannels3D_HasMask(&attachment->channels, channel_mask);
}

void RuntimeVolumeAttachment3D_Reset(RuntimeVolumeAttachment3D* attachment) {
    if (!attachment) return;
    RuntimeVolumeAttachment3D_ClearOwnedChannels(attachment);
    RuntimeVolumeAttachment3D_Init(attachment);
}

void RuntimeVolumeAttachment3D_Free(RuntimeVolumeAttachment3D* attachment) {
    RuntimeVolumeAttachment3D_Reset(attachment);
}
