#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"
#include "core_mesh_preview.h"

#define RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS 32
#define RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES 64
#define RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX 4096

typedef enum RayTracingRuntimeMeshRotationPivotPolicy {
    RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_AUTHORED_ORIGIN = 0,
    RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER = 1,
    RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM = 2
} RayTracingRuntimeMeshRotationPivotPolicy;

typedef enum RayTracingRuntimeMeshAssetPersistentCacheMode {
    RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE = 0,
    RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_DISABLED = 1,
    RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_ONLY = 2,
    RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_REFRESH = 3
} RayTracingRuntimeMeshAssetPersistentCacheMode;

typedef struct RayTracingRuntimeMeshPreviewInfo {
    bool preview_path_resolved;
    bool preview_file_exists;
    bool preview_file_readable;
    bool preview_schema_supported;
    bool preview_metadata_valid;
    char preview_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    CoreMeshPreviewRuntimeMetadata metadata;
} RayTracingRuntimeMeshPreviewInfo;

typedef struct RayTracingRuntimeMeshAsset {
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    bool file_stamp_valid;
    long long file_mtime_sec;
    long long file_mtime_nsec;
    long long file_size_bytes;
    CoreMeshAssetRuntimeDocument document;
    RayTracingRuntimeMeshPreviewInfo preview;
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
    RayTracingRuntimeMeshRotationPivotPolicy rotation_pivot_policy;
    double rotation_pivot_x;
    double rotation_pivot_y;
    double rotation_pivot_z;
} RayTracingRuntimeMeshAssetInstance;

typedef struct RayTracingRuntimeMeshAssetSkippedInstance {
    char object_id[64];
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    int scene_object_index;
    size_t file_size_bytes;
    size_t max_file_size_bytes;
    RayTracingRuntimeMeshPreviewInfo preview;
} RayTracingRuntimeMeshAssetSkippedInstance;

typedef struct RayTracingRuntimeMeshAssetSet {
    int asset_count;
    RayTracingRuntimeMeshAsset assets[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    int instance_count;
    RayTracingRuntimeMeshAssetInstance instances[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];
    int skipped_instance_count;
    RayTracingRuntimeMeshAssetSkippedInstance
        skipped_instances[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];
} RayTracingRuntimeMeshAssetSet;

typedef struct RayTracingRuntimeMeshAssetTimingStats {
    double total_ms;
    double scene_read_ms;
    double scene_parse_ms;
    double sidecar_path_resolution_ms;
    double asset_load_total_ms;
    double asset_runtime_document_load_ms;
    double asset_persistent_cache_read_ms;
    double asset_persistent_cache_write_ms;
    double asset_document_copy_ms;
    int asset_persistent_cache_mode;
    int asset_load_calls;
    int asset_cache_hits;
    int asset_cache_misses;
    int asset_persistent_cache_hits;
    int asset_persistent_cache_misses;
    int asset_persistent_cache_writes;
    int asset_persistent_cache_invalidations;
    int asset_persistent_cache_refreshes;
    int loaded_assets;
    int loaded_instances;
    unsigned long long loaded_asset_bytes;
    unsigned long long loaded_vertices;
    unsigned long long loaded_triangles;
} RayTracingRuntimeMeshAssetTimingStats;

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
bool ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
    const char* runtime_scene_path,
    size_t max_asset_file_bytes,
    RayTracingRuntimeMeshAssetSet* out_set,
    char* out_diagnostics,
    size_t out_diagnostics_size);

void ray_tracing_runtime_mesh_assets_reset_last(void);
void ray_tracing_runtime_mesh_assets_take_last(RayTracingRuntimeMeshAssetSet* loaded);
void ray_tracing_runtime_mesh_assets_take_last_for_scene(const char* runtime_scene_path,
                                                        RayTracingRuntimeMeshAssetSet* loaded);
bool ray_tracing_runtime_mesh_assets_last_matches_scene_file(const char* runtime_scene_path);
bool ray_tracing_runtime_mesh_assets_load_scene_file_to_last(const char* runtime_scene_path,
                                                            char* out_diagnostics,
                                                            size_t out_diagnostics_size);
const RayTracingRuntimeMeshAssetSet* ray_tracing_runtime_mesh_assets_last(void);

void ray_tracing_runtime_mesh_assets_reset_cache(void);
void ray_tracing_runtime_mesh_assets_cache_stats(unsigned long long* out_hits,
                                                unsigned long long* out_misses,
                                                unsigned long long* out_invalidations,
                                                int* out_cached_assets);
void ray_tracing_runtime_mesh_assets_timing_reset(void);
void ray_tracing_runtime_mesh_assets_timing_snapshot(
    RayTracingRuntimeMeshAssetTimingStats* out_stats);
