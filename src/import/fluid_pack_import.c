#include "import/fluid_pack_import.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core_pack.h"

typedef struct Vf2dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
} Vf2dHeaderCanonical;

static bool has_extension(const char *path, const char *ext) {
    if (!path || !ext) return false;
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + (path_len - ext_len), ext) == 0;
}

bool fluid_pack_path_is_pack(const char *path) {
    return has_extension(path, ".pack");
}

bool fluid_pack_derive_legacy_vf2d_path(const char *pack_path, char *out_path, size_t out_path_size) {
    if (!pack_path || !out_path || out_path_size == 0) return false;
    if (!fluid_pack_path_is_pack(pack_path)) return false;

    size_t path_len = strlen(pack_path);
    if (path_len + 1 > out_path_size) return false;
    memcpy(out_path, pack_path, path_len + 1);
    memcpy(out_path + path_len - 5u, ".vf2d", 6u);
    return true;
}

static bool read_required_chunk(CorePackReader *reader,
                                const char type[4],
                                void *dst,
                                uint64_t dst_size) {
    CorePackChunkInfo chunk;
    CoreResult find_r = core_pack_reader_find_chunk(reader, type, 0, &chunk);
    if (find_r.code != CORE_OK) return false;
    if (chunk.size != dst_size) return false;
    CoreResult read_r = core_pack_reader_read_chunk_data(reader, &chunk, dst, dst_size);
    return read_r.code == CORE_OK;
}

bool fluid_pack_frame_load(const char *path, FluidFrame *out) {
    if (!path || !out) return false;

    CorePackReader reader;
    CoreResult open_r = core_pack_reader_open(path, &reader);
    if (open_r.code != CORE_OK) return false;

    bool ok = false;
    FluidFrame frame;
    memset(&frame, 0, sizeof(frame));

    do {
        Vf2dHeaderCanonical header;
        if (!read_required_chunk(&reader, "VFHD", &header, (uint64_t)sizeof(header))) break;
        if (header.grid_w == 0 || header.grid_h == 0) break;

        size_t cell_count = (size_t)header.grid_w * (size_t)header.grid_h;
        size_t data_bytes = cell_count * sizeof(float);
        if (data_bytes / sizeof(float) != cell_count) break;
        if (cell_count > SIZE_MAX / sizeof(float)) break;

        frame.w = (int)header.grid_w;
        frame.h = (int)header.grid_h;
        frame.meta.version = header.version;
        frame.meta.grid_w = header.grid_w;
        frame.meta.grid_h = header.grid_h;
        frame.meta.time_seconds = header.time_seconds;
        frame.meta.frame_index = header.frame_index;
        frame.meta.dt_seconds = header.dt_seconds;
        frame.meta.origin_x = header.origin_x;
        frame.meta.origin_y = header.origin_y;
        frame.meta.cell_size = (header.cell_size > 0.0f) ? header.cell_size : 1.0f;
        frame.meta.obstacle_mask_crc32 = header.obstacle_mask_crc32;

        frame.density = (float *)malloc(data_bytes);
        frame.velX = (float *)malloc(data_bytes);
        frame.velY = (float *)malloc(data_bytes);
        if (!frame.density || !frame.velX || !frame.velY) break;

        if (!read_required_chunk(&reader, "DENS", frame.density, (uint64_t)data_bytes)) break;
        if (!read_required_chunk(&reader, "VELX", frame.velX, (uint64_t)data_bytes)) break;
        if (!read_required_chunk(&reader, "VELY", frame.velY, (uint64_t)data_bytes)) break;

        ok = true;
    } while (0);

    CoreResult close_r = core_pack_reader_close(&reader);
    if (close_r.code != CORE_OK) ok = false;

    if (!ok) {
        fluid_frame_free(&frame);
        return false;
    }

    *out = frame;
    return true;
}
