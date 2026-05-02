#include "import/fluid_volume_import_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct VolumeFrameHeaderVf3dV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    uint32_t solid_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderVf3dV1;

static const uint32_t VOLUME_VF3D_MAGIC = ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');
static const uint32_t VOLUME_VF3D_VERSION_V1 = 1u;

static bool fluid_volume_import_3d_has_extension(const char* path, const char* ext) {
    size_t path_len = 0;
    size_t ext_len = 0;
    if (!path || !ext) return false;
    path_len = strlen(path);
    ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + (path_len - ext_len), ext) == 0;
}

bool fluid_volume_import_3d_classify_path(const char* path,
                                          RuntimeVolume3DSourceKind* out_kind) {
    RuntimeVolume3DSourceKind kind = RUNTIME_VOLUME_3D_SOURCE_NONE;

    if (!path || !path[0]) return false;
    if (fluid_volume_import_3d_has_extension(path, ".vf3d")) {
        kind = RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
    } else if (fluid_volume_import_3d_has_extension(path, ".pack")) {
        kind = RUNTIME_VOLUME_3D_SOURCE_PACK;
    } else if (fluid_volume_import_3d_has_extension(path, ".json")) {
        kind = RUNTIME_VOLUME_3D_SOURCE_MANIFEST;
    } else {
        return false;
    }

    if (out_kind) {
        *out_kind = kind;
    }
    return true;
}

bool fluid_volume_import_3d_path_is_supported(const char* path) {
    return fluid_volume_import_3d_classify_path(path, NULL);
}

static void fluid_volume_import_3d_diag(char* out_diagnostics,
                                        size_t out_diagnostics_size,
                                        const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool fluid_volume_import_3d_read_exact(FILE* file, void* dst, size_t bytes) {
    if (!file || !dst) return false;
    if (bytes == 0u) return true;
    return fread(dst, 1u, bytes, file) == bytes;
}

bool fluid_volume_import_3d_load_raw(const char* path,
                                     RuntimeVolumeAttachment3D* out_attachment,
                                     char* out_diagnostics,
                                     size_t out_diagnostics_size) {
    VolumeFrameHeaderVf3dV1 header = {0};
    FILE* file = NULL;
    const uint32_t channel_mask =
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY |
        RUNTIME_VOLUME_3D_CHANNEL_VELOCITY |
        RUNTIME_VOLUME_3D_CHANNEL_PRESSURE |
        RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    Vec3 origin = {0};
    Vec3 scene_up = {0};
    size_t float_bytes = 0u;
    size_t solid_bytes = 0u;

    fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!path || !path[0] || !out_attachment) {
        return false;
    }

    RuntimeVolumeAttachment3D_Reset(out_attachment);
    file = fopen(path, "rb");
    if (!file) {
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "failed to open raw vf3d");
        return false;
    }
    if (!fluid_volume_import_3d_read_exact(file, &header, sizeof(header))) {
        fclose(file);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "failed to read raw vf3d header");
        return false;
    }
    if (header.magic != VOLUME_VF3D_MAGIC) {
        fclose(file);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "invalid raw vf3d magic");
        return false;
    }
    if (header.version != VOLUME_VF3D_VERSION_V1) {
        fclose(file);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "unsupported raw vf3d version");
        return false;
    }
    scene_up = vec3((double)header.scene_up_x,
                    (double)header.scene_up_y,
                    (double)header.scene_up_z);
    if (vec3_length(scene_up) <= 1e-9) {
        fclose(file);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "raw vf3d scene_up must be non-zero");
        return false;
    }

    origin = vec3((double)header.origin_x,
                  (double)header.origin_y,
                  (double)header.origin_z);
    if (!RuntimeVolumeGrid3D_Configure(&out_attachment->grid,
                                       header.version,
                                       header.grid_w,
                                       header.grid_h,
                                       header.grid_d,
                                       header.time_seconds,
                                       header.frame_index,
                                       header.dt_seconds,
                                       origin,
                                       (double)header.voxel_size,
                                       scene_up,
                                       header.solid_mask_crc32)) {
        fclose(file);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "raw vf3d header failed validation");
        return false;
    }
    if (!RuntimeVolumeAttachment3D_AllocateOwnedChannels(out_attachment, channel_mask)) {
        fclose(file);
        RuntimeVolumeAttachment3D_Reset(out_attachment);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "raw vf3d channel allocation failed");
        return false;
    }

    float_bytes = (size_t)out_attachment->grid.cellCount * sizeof(float);
    solid_bytes = (size_t)out_attachment->grid.cellCount * sizeof(uint8_t);
    if (!fluid_volume_import_3d_read_exact(file, out_attachment->channels.density, float_bytes) ||
        !fluid_volume_import_3d_read_exact(file, out_attachment->channels.velocityX, float_bytes) ||
        !fluid_volume_import_3d_read_exact(file, out_attachment->channels.velocityY, float_bytes) ||
        !fluid_volume_import_3d_read_exact(file, out_attachment->channels.velocityZ, float_bytes) ||
        !fluid_volume_import_3d_read_exact(file, out_attachment->channels.pressure, float_bytes) ||
        !fluid_volume_import_3d_read_exact(file, out_attachment->channels.solidMask, solid_bytes)) {
        fclose(file);
        RuntimeVolumeAttachment3D_Reset(out_attachment);
        fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                    "failed to read raw vf3d payload");
        return false;
    }

    fclose(file);
    out_attachment->sourceKind = RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
    out_attachment->enabled = true;
    out_attachment->hasData = true;
    fluid_volume_import_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
