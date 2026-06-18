#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RUNTIME_WATER_SURFACE_PATH_MAX 4096

typedef struct RuntimeWaterSurfaceMaterial {
    bool valid;
    double ior;
    double absorption_distance_m;
    double absorption_rgb[3];
} RuntimeWaterSurfaceMaterial;

typedef struct RuntimeWaterSurfaceFrame {
    bool valid;
    char source_manifest_path[RUNTIME_WATER_SURFACE_PATH_MAX];
    char frame_path[RUNTIME_WATER_SURFACE_PATH_MAX];
    char schema[64];
    char frame_contract[64];
    char surface_axis[8];
    char layout[32];
    uint64_t frame_index;
    double time_seconds;
    double dt_seconds;
    uint32_t grid_w;
    uint32_t grid_d;
    uint64_t sample_count;
    uint32_t volume_grid_w;
    uint32_t volume_grid_h;
    uint32_t volume_grid_d;
    double origin_x;
    double origin_y;
    double origin_z;
    double sample_origin_x;
    double sample_origin_z;
    double sample_spacing_x;
    double sample_spacing_z;
    double density_threshold;
    uint32_t wet_columns;
    uint32_t dry_columns;
    uint32_t solid_columns;
    uint32_t water_cells;
    double surface_min_y;
    double surface_max_y;
    double surface_avg_y;
    double max_slope;
    bool finite_normals;
    float* heights_y;
    float* normals_xyz;
    RuntimeWaterSurfaceMaterial material;
} RuntimeWaterSurfaceFrame;

void RuntimeWaterSurfaceFrame_Init(RuntimeWaterSurfaceFrame* frame);
void RuntimeWaterSurfaceFrame_Reset(RuntimeWaterSurfaceFrame* frame);
void RuntimeWaterSurfaceFrame_Free(RuntimeWaterSurfaceFrame* frame);

bool RuntimeWaterSurfaceImport_LoadSourceAtFrame(const char* source_path,
                                                 int requested_frame_index,
                                                 RuntimeWaterSurfaceFrame* out_frame,
                                                 bool* out_found,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size);
