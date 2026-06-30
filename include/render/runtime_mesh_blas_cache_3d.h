#ifndef RENDER_RUNTIME_MESH_BLAS_CACHE_3D_H
#define RENDER_RUNTIME_MESH_BLAS_CACHE_3D_H

#include <stdbool.h>

#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_scene_accel_3d.h"

bool RuntimeMeshBLASCache3D_PrepareAssetSet(
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
void RuntimeMeshBLASCache3D_SnapshotDiagnostics(
    RuntimeSceneAcceleration3DDiagnostics* out_diagnostics);
void RuntimeMeshBLASCache3D_ResetForTests(void);
const char* RuntimeMeshBLASCache3D_LastDiagnostics(void);

#endif
