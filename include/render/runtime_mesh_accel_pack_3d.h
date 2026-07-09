#ifndef RENDER_RUNTIME_MESH_ACCEL_PACK_3D_H
#define RENDER_RUNTIME_MESH_ACCEL_PACK_3D_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/runtime_mesh_asset_pack.h"
#include "render/runtime_scene_3d.h"

typedef struct RuntimeMeshAccelPack3DKey {
    RayTracingRuntimeMeshAssetPackSourceKey source_key;
    uint32_t accel_cache_schema_version;
    uint32_t blas_builder_version;
    uint32_t triangle_layout_version;
    uint32_t bvh_layout_version;
    uint32_t bvh_builder_policy_version;
    uint32_t acceleration_policy;
    uint32_t pointer_size_bytes;
    uint32_t source_triangle_count;
} RuntimeMeshAccelPack3DKey;

bool RuntimeMeshAccelPack3D_CachePathForSource(const char* cache_root,
                                               const char* source_path,
                                               char* out_path,
                                               size_t out_path_size);

bool RuntimeMeshAccelPack3D_WriteCacheFile(const char* path,
                                           const RuntimeMeshAccelPack3DKey* key,
                                           const RuntimeTriangleMesh3D* mesh,
                                           char* out_diagnostics,
                                           size_t out_diagnostics_size);

bool RuntimeMeshAccelPack3D_ReadCacheFile(const char* path,
                                          const RuntimeMeshAccelPack3DKey* expected_key,
                                          RuntimeTriangleMesh3D* out_mesh,
                                          char* out_diagnostics,
                                          size_t out_diagnostics_size);

#endif
