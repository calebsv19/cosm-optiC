#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_mesh_asset.h"

typedef struct RayTracingRuntimeMeshAssetPackSourceKey {
    char source_path[4096];
    int64_t source_mtime_sec;
    int64_t source_mtime_nsec;
    int64_t source_size_bytes;
    uint64_t source_checksum;
    uint32_t core_mesh_asset_schema_version;
    uint32_t ray_tracing_cache_schema_version;
    uint32_t pointer_size_bytes;
} RayTracingRuntimeMeshAssetPackSourceKey;

bool ray_tracing_runtime_mesh_asset_pack_write_file(
    const char* path,
    const CoreMeshAssetRuntimeDocument* document,
    char* out_diagnostics,
    size_t out_diagnostics_size);

bool ray_tracing_runtime_mesh_asset_pack_read_file(
    const char* path,
    CoreMeshAssetRuntimeDocument* out_document,
    char* out_diagnostics,
    size_t out_diagnostics_size);

bool ray_tracing_runtime_mesh_asset_pack_cache_path_for_source(
    const char* cache_root,
    const char* source_path,
    char* out_path,
    size_t out_path_size);

bool ray_tracing_runtime_mesh_asset_pack_checksum_file(const char* path,
                                                       uint64_t* out_checksum);

bool ray_tracing_runtime_mesh_asset_pack_write_cache_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* source_key,
    const CoreMeshAssetRuntimeDocument* document,
    char* out_diagnostics,
    size_t out_diagnostics_size);

bool ray_tracing_runtime_mesh_asset_pack_read_cache_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* expected_source_key,
    CoreMeshAssetRuntimeDocument* out_document,
    char* out_diagnostics,
    size_t out_diagnostics_size);
