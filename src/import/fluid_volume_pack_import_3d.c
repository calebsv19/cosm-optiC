#include "import/fluid_volume_import_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core_pack.h"

typedef struct Vf3dHeaderCanonical {
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
} Vf3dHeaderCanonical;

static void fluid_volume_pack_import_3d_diag(char* out_diagnostics,
                                             size_t out_diagnostics_size,
                                             const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool fluid_volume_pack_import_3d_find_chunk(CorePackReader* reader,
                                                   const char type[4],
                                                   CorePackChunkInfo* out_chunk) {
    CoreResult result = core_pack_reader_find_chunk(reader, type, 0u, out_chunk);
    return result.code == CORE_OK;
}

static bool fluid_volume_pack_import_3d_read_exact_chunk(CorePackReader* reader,
                                                         const char type[4],
                                                         void* dst,
                                                         uint64_t expected_size,
                                                         char* out_diagnostics,
                                                         size_t out_diagnostics_size) {
    CorePackChunkInfo chunk = {0};
    CoreResult result = core_result_ok();
    if (!fluid_volume_pack_import_3d_find_chunk(reader, type, &chunk)) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "required vf3d pack chunk missing");
        return false;
    }
    if (chunk.size != expected_size) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "vf3d pack chunk size mismatch");
        return false;
    }
    result = core_pack_reader_read_chunk_data(reader, &chunk, dst, expected_size);
    if (result.code != CORE_OK) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "failed to read vf3d pack chunk");
        return false;
    }
    return true;
}

bool fluid_volume_import_3d_load_pack(const char* path,
                                      RuntimeVolumeAttachment3D* out_attachment,
                                      char* out_diagnostics,
                                      size_t out_diagnostics_size) {
    CorePackReader reader = {0};
    CoreResult open_result = core_result_ok();
    CoreResult close_result = core_result_ok();
    Vf3dHeaderCanonical header = {0};
    CorePackChunkInfo chunk = {0};
    bool has_velx = false;
    bool has_vely = false;
    bool has_velz = false;
    bool has_pressure = false;
    bool has_solid = false;
    uint32_t channel_mask = RUNTIME_VOLUME_3D_CHANNEL_DENSITY;
    size_t float_bytes = 0u;
    size_t solid_bytes = 0u;
    Vec3 origin = {0};
    Vec3 scene_up = {0};

    fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!path || !path[0] || !out_attachment) {
        return false;
    }

    RuntimeVolumeAttachment3D_Reset(out_attachment);
    open_result = core_pack_reader_open(path, &reader);
    if (open_result.code != CORE_OK) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "failed to open vf3d pack");
        return false;
    }

    if (!fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                      "VF3H",
                                                      &header,
                                                      (uint64_t)sizeof(header),
                                                      out_diagnostics,
                                                      out_diagnostics_size)) {
        goto fail;
    }
    scene_up = vec3((double)header.scene_up_x,
                    (double)header.scene_up_y,
                    (double)header.scene_up_z);
    if (vec3_length(scene_up) <= 1e-9) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "vf3d pack scene_up must be non-zero");
        goto fail;
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
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "vf3d pack header failed validation");
        goto fail;
    }

    has_velx = fluid_volume_pack_import_3d_find_chunk(&reader, "VELX", &chunk);
    has_vely = fluid_volume_pack_import_3d_find_chunk(&reader, "VELY", &chunk);
    has_velz = fluid_volume_pack_import_3d_find_chunk(&reader, "VELZ", &chunk);
    if (has_velx || has_vely || has_velz) {
        if (!(has_velx && has_vely && has_velz)) {
            fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                             "vf3d pack velocity chunks must be all present or all absent");
            goto fail;
        }
        channel_mask |= RUNTIME_VOLUME_3D_CHANNEL_VELOCITY;
    }
    has_pressure = fluid_volume_pack_import_3d_find_chunk(&reader, "PRES", &chunk);
    has_solid = fluid_volume_pack_import_3d_find_chunk(&reader, "SOLI", &chunk);
    if (has_pressure) {
        channel_mask |= RUNTIME_VOLUME_3D_CHANNEL_PRESSURE;
    }
    if (has_solid) {
        channel_mask |= RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK;
    }

    if (!RuntimeVolumeAttachment3D_AllocateOwnedChannels(out_attachment, channel_mask)) {
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "vf3d pack channel allocation failed");
        goto fail;
    }

    float_bytes = (size_t)out_attachment->grid.cellCount * sizeof(float);
    solid_bytes = (size_t)out_attachment->grid.cellCount * sizeof(uint8_t);
    if (!fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                      "DENS",
                                                      out_attachment->channels.density,
                                                      (uint64_t)float_bytes,
                                                      out_diagnostics,
                                                      out_diagnostics_size)) {
        goto fail;
    }
    if (has_velx) {
        if (!fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                          "VELX",
                                                          out_attachment->channels.velocityX,
                                                          (uint64_t)float_bytes,
                                                          out_diagnostics,
                                                          out_diagnostics_size) ||
            !fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                          "VELY",
                                                          out_attachment->channels.velocityY,
                                                          (uint64_t)float_bytes,
                                                          out_diagnostics,
                                                          out_diagnostics_size) ||
            !fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                          "VELZ",
                                                          out_attachment->channels.velocityZ,
                                                          (uint64_t)float_bytes,
                                                          out_diagnostics,
                                                          out_diagnostics_size)) {
            goto fail;
        }
    }
    if (has_pressure &&
        !fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                      "PRES",
                                                      out_attachment->channels.pressure,
                                                      (uint64_t)float_bytes,
                                                      out_diagnostics,
                                                      out_diagnostics_size)) {
        goto fail;
    }
    if (has_solid &&
        !fluid_volume_pack_import_3d_read_exact_chunk(&reader,
                                                      "SOLI",
                                                      out_attachment->channels.solidMask,
                                                      (uint64_t)solid_bytes,
                                                      out_diagnostics,
                                                      out_diagnostics_size)) {
        goto fail;
    }

    close_result = core_pack_reader_close(&reader);
    if (close_result.code != CORE_OK) {
        RuntimeVolumeAttachment3D_Reset(out_attachment);
        fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size,
                                         "failed to close vf3d pack");
        return false;
    }

    out_attachment->sourceKind = RUNTIME_VOLUME_3D_SOURCE_PACK;
    out_attachment->enabled = true;
    out_attachment->hasData = true;
    fluid_volume_pack_import_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;

fail:
    close_result = core_pack_reader_close(&reader);
    (void)close_result;
    RuntimeVolumeAttachment3D_Reset(out_attachment);
    return false;
}
