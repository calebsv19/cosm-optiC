#ifndef RENDER_RUNTIME_MESH_BLAS_CACHE_3D_H
#define RENDER_RUNTIME_MESH_BLAS_CACHE_3D_H

#include <stdbool.h>

#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_scene_accel_3d.h"

typedef struct RuntimeMeshBLASCache3DView {
    bool ready;
    char assetId[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    long long mtimeSec;
    long long mtimeNsec;
    long long fileSize;
    size_t vertexCount;
    size_t sourceTriangleCount;
    const RuntimeTriangleMesh3D* localMesh;
} RuntimeMeshBLASCache3DView;

bool RuntimeMeshBLASCache3D_PrepareAssetSet(
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
bool RuntimeMeshBLASCache3D_FindAsset(const RayTracingRuntimeMeshAsset* asset,
                                      RuntimeMeshBLASCache3DView* out_view);
void RuntimeMeshBLASCache3D_SnapshotDiagnostics(
    RuntimeSceneAcceleration3DDiagnostics* out_diagnostics);
void RuntimeMeshBLASCache3D_ResetForTests(void);
const char* RuntimeMeshBLASCache3D_LastDiagnostics(void);

#endif
