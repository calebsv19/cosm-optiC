#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"

#define RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS 32
#define RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES 64
#define RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX 4096

typedef struct RayTracingRuntimeMeshAsset {
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    CoreMeshAssetRuntimeDocument document;
} RayTracingRuntimeMeshAsset;

typedef struct RayTracingRuntimeMeshAssetInstance {
    char object_id[64];
    char asset_id[64];
    int asset_index;
    int scene_object_index;
    double position_x;
    double position_y;
    double position_z;
    double rotation_x;
    double rotation_y;
    double rotation_z;
    double scale_x;
    double scale_y;
    double scale_z;
} RayTracingRuntimeMeshAssetInstance;

typedef struct RayTracingRuntimeMeshAssetSet {
    int asset_count;
    RayTracingRuntimeMeshAsset assets[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    int instance_count;
    RayTracingRuntimeMeshAssetInstance instances[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];
} RayTracingRuntimeMeshAssetSet;

void ray_tracing_runtime_mesh_asset_set_init(RayTracingRuntimeMeshAssetSet* set);
void ray_tracing_runtime_mesh_asset_set_free(RayTracingRuntimeMeshAssetSet* set);

bool ray_tracing_runtime_mesh_asset_resolve_path(const char* runtime_scene_path,
                                                 const char* asset_id,
                                                 char* out_path,
                                                 size_t out_path_size,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size);

bool ray_tracing_runtime_mesh_assets_load_scene_file(const char* runtime_scene_path,
                                                     RayTracingRuntimeMeshAssetSet* out_set,
                                                     char* out_diagnostics,
                                                     size_t out_diagnostics_size);

void ray_tracing_runtime_mesh_assets_reset_last(void);
void ray_tracing_runtime_mesh_assets_take_last(RayTracingRuntimeMeshAssetSet* loaded);
bool ray_tracing_runtime_mesh_assets_load_scene_file_to_last(const char* runtime_scene_path,
                                                            char* out_diagnostics,
                                                            size_t out_diagnostics_size);
const RayTracingRuntimeMeshAssetSet* ray_tracing_runtime_mesh_assets_last(void);

void ray_tracing_runtime_mesh_assets_reset_cache(void);
void ray_tracing_runtime_mesh_assets_cache_stats(unsigned long long* out_hits,
                                                unsigned long long* out_misses,
                                                unsigned long long* out_invalidations,
                                                int* out_cached_assets);
