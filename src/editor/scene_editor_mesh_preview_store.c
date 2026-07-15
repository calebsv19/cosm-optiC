#include "editor/scene_editor_mesh_preview_store.h"

#include "editor/scene_editor_mesh_preview_contract.h"

#include <string.h>

typedef struct SceneEditorMeshPreviewStore {
    bool valid[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreMeshPreviewLodMesh lods[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
} SceneEditorMeshPreviewStore;

static SceneEditorMeshPreviewStore g_mesh_preview_store;

void SceneEditorMeshPreviewStoreReset(void) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        SceneEditorMeshPreviewFreeLod(&g_mesh_preview_store.lods[i]);
    }
    memset(&g_mesh_preview_store, 0, sizeof(g_mesh_preview_store));
}

void SceneEditorMeshPreviewStorePrepare(const RayTracingRuntimeMeshAssetSet* assets) {
    SceneEditorMeshPreviewStoreReset();
    if (!assets) return;
    for (int i = 0; i < assets->asset_count; ++i) {
        core_mesh_preview_lod_mesh_init(&g_mesh_preview_store.lods[i]);
        g_mesh_preview_store.valid[i] = SceneEditorMeshPreviewBuildLod(
            &assets->assets[i].document,
            SCENE_EDITOR_MESH_PREVIEW_LOD_TRIANGLES,
            &g_mesh_preview_store.lods[i]);
    }
}

const CoreMeshPreviewLodMesh* SceneEditorMeshPreviewStoreGet(int asset_index) {
    if (!SceneEditorMeshPreviewStoreIsValid(asset_index)) return NULL;
    return &g_mesh_preview_store.lods[asset_index];
}

bool SceneEditorMeshPreviewStoreIsValid(int asset_index) {
    return asset_index >= 0 && asset_index < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS &&
           g_mesh_preview_store.valid[asset_index];
}
