#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t version;            // 1 or 2
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;         // v2+, 0 if unknown
    float    origin_x;           // v2+, defaults 0
    float    origin_y;           // v2+, defaults 0
    float    cell_size;          // v2+, defaults 1
    uint32_t obstacle_mask_crc32; // v2+, 0 otherwise
} FluidFrameMeta;

typedef struct {
    int   w;
    int   h;
    float *density;
    float *velX;
    float *velY;
    FluidFrameMeta meta;
} FluidFrame;

typedef struct {
    char  *path;       // asset name/path
    float  pos_x_norm; // 0..1 normalized
    float  pos_y_norm; // 0..1 normalized
    float  rotation_deg;
    float  scale;
    int    is_static;
} FluidImportShape;

typedef struct {
    char              **paths;  // array of frame paths (owned)
    FluidFrameMeta     *meta;   // same length as paths
    size_t              count;
    uint32_t            grid_w;
    uint32_t            grid_h;
    float               cell_size;
    float               origin_x;
    float               origin_y;
    uint32_t            obstacle_mask_crc32;
    FluidImportShape   *imports;
    size_t              import_count;
} FluidManifest;

bool fluid_frame_load(const char *path, FluidFrame *out);
void fluid_frame_free(FluidFrame *frame);

bool fluid_manifest_load(const char *manifest_path, FluidManifest *out);
void fluid_manifest_free(FluidManifest *manifest);

// Convenience: load a single frame without a manifest. Uses header metadata
// (origin defaults 0/0, cell_size 1 for v1) and leaves manifest fields empty.
bool fluid_frame_load_single(const char *path, FluidFrame *out);
