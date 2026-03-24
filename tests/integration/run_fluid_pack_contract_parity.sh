#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CHECKER_C="$TMP_DIR/check_fluid_pack_contract_parity.c"
CHECKER_BIN="$TMP_DIR/check_fluid_pack_contract_parity"
PACK_PATH="$ROOT_DIR/../shared/core/core_pack/tests/fixtures/physics_v1_sample.pack"

cat > "$CHECKER_C" <<'C_EOF'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_pack.h"
#include "import/fluid_import.h"
#include "import/fluid_pack_import.h"

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

void fluid_frame_free(FluidFrame *frame) {
    if (!frame) return;
    free(frame->density);
    free(frame->velX);
    free(frame->velY);
    frame->density = NULL;
    frame->velX = NULL;
    frame->velY = NULL;
    frame->w = 0;
    frame->h = 0;
    memset(&frame->meta, 0, sizeof(frame->meta));
}

static int fail(const char *msg) {
    fprintf(stderr, "fluid pack parity test failed: %s\n", msg);
    return 1;
}

static int read_chunk_required(CorePackReader *r,
                               const char type[4],
                               void *dst,
                               uint64_t expected_size) {
    CorePackChunkInfo info;
    CoreResult cr = core_pack_reader_find_chunk(r, type, 0, &info);
    if (cr.code != CORE_OK) return 0;
    if (info.size != expected_size) return 0;
    cr = core_pack_reader_read_chunk_data(r, &info, dst, expected_size);
    return cr.code == CORE_OK;
}

int main(int argc, char **argv) {
    CorePackReader reader;
    CoreResult r;
    Vf2dHeaderCanonical header;
    size_t cell_count;
    size_t bytes;
    float *dens = NULL;
    float *velx = NULL;
    float *vely = NULL;
    FluidFrame frame;
    int ok = 0;

    if (argc != 2) return 2;

    memset(&reader, 0, sizeof(reader));
    r = core_pack_reader_open(argv[1], &reader);
    if (r.code != CORE_OK) return fail("core_pack_reader_open");

    memset(&header, 0, sizeof(header));
    if (!read_chunk_required(&reader, "VFHD", &header, (uint64_t)sizeof(header))) {
        core_pack_reader_close(&reader);
        return fail("missing/invalid VFHD chunk");
    }
    if (header.grid_w == 0u || header.grid_h == 0u) {
        core_pack_reader_close(&reader);
        return fail("invalid grid dims");
    }

    cell_count = (size_t)header.grid_w * (size_t)header.grid_h;
    if (header.grid_w != 0u && cell_count / (size_t)header.grid_w != (size_t)header.grid_h) {
        core_pack_reader_close(&reader);
        return fail("grid overflow");
    }
    if (cell_count > SIZE_MAX / sizeof(float)) {
        core_pack_reader_close(&reader);
        return fail("float bytes overflow");
    }
    bytes = cell_count * sizeof(float);

    dens = (float *)malloc(bytes);
    velx = (float *)malloc(bytes);
    vely = (float *)malloc(bytes);
    if (!dens || !velx || !vely) {
        core_pack_reader_close(&reader);
        free(dens);
        free(velx);
        free(vely);
        return fail("allocation failed");
    }

    if (!read_chunk_required(&reader, "DENS", dens, (uint64_t)bytes) ||
        !read_chunk_required(&reader, "VELX", velx, (uint64_t)bytes) ||
        !read_chunk_required(&reader, "VELY", vely, (uint64_t)bytes)) {
        core_pack_reader_close(&reader);
        free(dens);
        free(velx);
        free(vely);
        return fail("missing/invalid field chunk(s)");
    }

    r = core_pack_reader_close(&reader);
    if (r.code != CORE_OK) {
        free(dens);
        free(velx);
        free(vely);
        return fail("core_pack_reader_close");
    }

    memset(&frame, 0, sizeof(frame));
    if (!fluid_pack_frame_load(argv[1], &frame)) {
        free(dens);
        free(velx);
        free(vely);
        return fail("fluid_pack_frame_load");
    }

    if ((uint32_t)frame.w != header.grid_w || (uint32_t)frame.h != header.grid_h) goto mismatch;
    if (frame.meta.version != header.version) goto mismatch;
    if (frame.meta.grid_w != header.grid_w || frame.meta.grid_h != header.grid_h) goto mismatch;
    if (frame.meta.frame_index != header.frame_index) goto mismatch;
    if (frame.meta.time_seconds != header.time_seconds) goto mismatch;
    if (frame.meta.dt_seconds != header.dt_seconds) goto mismatch;
    if (frame.meta.origin_x != header.origin_x || frame.meta.origin_y != header.origin_y) goto mismatch;
    if (frame.meta.cell_size != header.cell_size) goto mismatch;
    if (frame.meta.obstacle_mask_crc32 != header.obstacle_mask_crc32) goto mismatch;

    if (memcmp(frame.density, dens, bytes) != 0) goto mismatch;
    if (memcmp(frame.velX, velx, bytes) != 0) goto mismatch;
    if (memcmp(frame.velY, vely, bytes) != 0) goto mismatch;

    ok = 1;

mismatch:
    fluid_frame_free(&frame);
    free(dens);
    free(velx);
    free(vely);

    if (!ok) return fail("loader/chunk parity mismatch");
    puts("fluid pack contract parity test passed.");
    return 0;
}
C_EOF

cc -std=c11 -Wall -Wextra -Wpedantic -g \
   -I"$ROOT_DIR/include" \
   -I"$ROOT_DIR/src" \
   -I"$ROOT_DIR/../shared/core/core_pack/include" \
   -I"$ROOT_DIR/../shared/core/core_io/include" \
   -I"$ROOT_DIR/../shared/core/core_base/include" \
   "$CHECKER_C" \
   "$ROOT_DIR/src/import/fluid_pack_import.c" \
   "$ROOT_DIR/../shared/core/core_pack/src/core_pack.c" \
   "$ROOT_DIR/../shared/core/core_io/src/core_io.c" \
   "$ROOT_DIR/../shared/core/core_base/src/core_base.c" \
   -o "$CHECKER_BIN" -lm

"$CHECKER_BIN" "$PACK_PATH"
