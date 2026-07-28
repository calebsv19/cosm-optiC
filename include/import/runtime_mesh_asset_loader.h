#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"
#include "core_mesh_preview.h"
#include "procedural/procedural_surface_derived_asset.h"
#include "procedural/procedural_solid_authored_material_binding.h"
#include "procedural/procedural_solid_material_graph.h"
#include "procedural/procedural_solid_material_runtime_program.h"
#include "procedural/procedural_solid_material_binding.h"

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

typedef struct RayTracingRuntimeMeshAssetFileDependency {
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    bool stamp_valid;
    long long mtime_sec;
    long long mtime_nsec;
    long long size_bytes;
} RayTracingRuntimeMeshAssetFileDependency;

typedef struct RayTracingRuntimeMeshAsset {
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    bool file_stamp_valid;
    long long file_mtime_sec;
    long long file_mtime_nsec;
    long long file_size_bytes;
    CoreMeshAssetRuntimeDocument document;
    RayTracingRuntimeMeshPreviewInfo preview;
    bool procedural_surface_valid;
    char procedural_manifest_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    RayTracingRuntimeMeshAssetFileDependency procedural_manifest_dependency;
    RayTracingRuntimeMeshAssetFileDependency procedural_recipe_dependency;
    RayTracingRuntimeMeshAssetFileDependency procedural_field_graph_dependency;
    RayTracingRuntimeMeshAssetFileDependency procedural_binding_dependency;
    RayTracingRuntimeMeshAssetFileDependency procedural_material_dependency;
    ProceduralSurfaceDerivedAssetManifest procedural_manifest;
    ProceduralSurfaceDerivedAssetMaterial procedural_material;
    bool procedural_solid_material_reference_observed;
    bool procedural_solid_material_reference_absent;
    bool procedural_solid_material_valid;
    char procedural_solid_material_binding_path[
        RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    RayTracingRuntimeMeshAssetFileDependency
        procedural_solid_material_binding_dependency;
    ProceduralSolidMaterialBindingV1 procedural_solid_material_binding;
    bool procedural_solid_authored_reference_observed;
    bool procedural_solid_authored_reference_absent;
    bool procedural_solid_authored_material_valid;
    char procedural_solid_authored_binding_path[
        RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    RayTracingRuntimeMeshAssetFileDependency
        procedural_solid_authored_binding_dependency;
    ProceduralSolidAuthoredMaterialBindingV1
        procedural_solid_authored_binding;
    size_t procedural_solid_authored_material_count;
    ProceduralSolidAuthoredMaterialV1
        procedural_solid_authored_materials[PROCEDURAL_SOLID_REGION_MAX];
    RayTracingRuntimeMeshAssetFileDependency
        procedural_solid_authored_material_dependencies[
            PROCEDURAL_SOLID_REGION_MAX];
    bool procedural_solid_material_graph_observed;
    bool procedural_solid_material_graph_absent;
    bool procedural_solid_material_graph_valid;
    char procedural_solid_material_graph_path[
        RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    RayTracingRuntimeMeshAssetFileDependency
        procedural_solid_material_graph_dependency;
    ProceduralSolidMaterialGraphV1 procedural_solid_material_graph;
    size_t procedural_solid_material_graph_material_count;
    ProceduralSolidAuthoredMaterialV1 procedural_solid_material_graph_materials[
        PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    RayTracingRuntimeMeshAssetFileDependency
        procedural_solid_material_graph_material_dependencies[
            PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    ProceduralSolidAuthoredMaterialSurfaceV1
        *procedural_solid_composed_triangle_materials;
    size_t procedural_solid_composed_triangle_material_count;
    ProceduralSolidMaterialRuntimeProgramV1
        procedural_solid_material_runtime_program;
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
    RayTracingRuntimeMeshAssetInstance preview_instance;
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
    int procedural_surface_assets;
    unsigned long long procedural_surface_vertices;
    char procedural_surface_cache_identity_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char procedural_surface_cage_digest_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char procedural_surface_shell_digest_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char procedural_surface_material_digest_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char procedural_surface_collision_owner[32];
    int procedural_solid_material_assets;
    unsigned long long procedural_solid_material_regions;
    char procedural_solid_material_binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    char procedural_solid_material_mesh_digest_sha256[
        PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    char procedural_solid_material_region_digest_sha256[
        PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    int procedural_solid_authored_material_assets;
    unsigned long long procedural_solid_authored_material_regions;
    char procedural_solid_authored_binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
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
