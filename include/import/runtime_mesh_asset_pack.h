#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"

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
