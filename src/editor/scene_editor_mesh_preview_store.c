#include "editor/scene_editor_mesh_preview_store.h"

#include "editor/scene_editor_mesh_preview_contract.h"

#include <string.h>

typedef struct SceneEditorMeshPreviewStore {
    int asset_count;
    int instance_count;
    int recovered_instance_count;
    bool valid[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreMeshAssetRuntimeContract contracts[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreMeshPreviewLodMesh lods[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    RayTracingRuntimeMeshAssetInstance
        instances[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];
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
        if (g_mesh_preview_store.valid[i]) {
            g_mesh_preview_store.contracts[i] = assets->assets[i].document.contract;
            g_mesh_preview_store.asset_count = i + 1;
        }
    }
    for (int i = 0; i < assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* source = &assets->instances[i];
        if (g_mesh_preview_store.instance_count >=
                RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES ||
            !SceneEditorMeshPreviewStoreIsValid(source->asset_index)) {
            continue;
        }
        g_mesh_preview_store.instances[g_mesh_preview_store.instance_count++] = *source;
    }
    for (int i = 0; i < assets->skipped_instance_count; ++i) {
        const RayTracingRuntimeMeshAssetSkippedInstance* skipped =
            &assets->skipped_instances[i];
        CoreMeshAssetRuntimeDocument document;
        RayTracingRuntimeMeshAssetInstance recovered = skipped->preview_instance;
        int asset_index = -1;
        if (g_mesh_preview_store.instance_count >=
                RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES ||
            g_mesh_preview_store.asset_count >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS) {
            break;
        }
        for (int existing = 0; existing < g_mesh_preview_store.asset_count; ++existing) {
            if (strcmp(g_mesh_preview_store.contracts[existing].asset_id,
                       skipped->asset_id) == 0) {
                asset_index = existing;
                break;
            }
        }
        if (asset_index < 0) {
            CoreResult load_result;
            asset_index = g_mesh_preview_store.asset_count;
            core_mesh_asset_runtime_document_init(&document);
            load_result = core_mesh_asset_runtime_document_load_file(skipped->path, &document);
            if (load_result.code != CORE_OK ||
                strcmp(document.contract.asset_id, skipped->asset_id) != 0) {
                core_mesh_asset_runtime_document_free(&document);
                continue;
            }
            core_mesh_preview_lod_mesh_init(&g_mesh_preview_store.lods[asset_index]);
            g_mesh_preview_store.valid[asset_index] = SceneEditorMeshPreviewBuildLod(
                &document,
                SCENE_EDITOR_MESH_PREVIEW_LOD_TRIANGLES,
                &g_mesh_preview_store.lods[asset_index]);
            if (g_mesh_preview_store.valid[asset_index]) {
                g_mesh_preview_store.contracts[asset_index] = document.contract;
                g_mesh_preview_store.asset_count += 1;
            }
            core_mesh_asset_runtime_document_free(&document);
            if (!g_mesh_preview_store.valid[asset_index]) continue;
        }
        recovered.asset_index = asset_index;
        g_mesh_preview_store.instances[g_mesh_preview_store.instance_count++] = recovered;
        g_mesh_preview_store.recovered_instance_count += 1;
    }
}

const CoreMeshPreviewLodMesh* SceneEditorMeshPreviewStoreGet(int asset_index) {
    if (!SceneEditorMeshPreviewStoreIsValid(asset_index)) return NULL;
    return &g_mesh_preview_store.lods[asset_index];
}

const CoreMeshAssetRuntimeContract* SceneEditorMeshPreviewStoreGetContract(int asset_index) {
    if (!SceneEditorMeshPreviewStoreIsValid(asset_index)) return NULL;
    return &g_mesh_preview_store.contracts[asset_index];
}

int SceneEditorMeshPreviewStoreInstanceCount(void) {
    return g_mesh_preview_store.instance_count;
}

const RayTracingRuntimeMeshAssetInstance* SceneEditorMeshPreviewStoreGetInstance(
    int instance_index) {
    if (instance_index < 0 || instance_index >= g_mesh_preview_store.instance_count) return NULL;
    return &g_mesh_preview_store.instances[instance_index];
}

bool SceneEditorMeshPreviewStoreHasSceneObject(int scene_object_index) {
    for (int i = 0; i < g_mesh_preview_store.instance_count; ++i) {
        if (g_mesh_preview_store.instances[i].scene_object_index == scene_object_index) return true;
    }
    return false;
}

int SceneEditorMeshPreviewStoreRecoveredInstanceCount(void) {
    return g_mesh_preview_store.recovered_instance_count;
}

bool SceneEditorMeshPreviewStoreIsValid(int asset_index) {
    return asset_index >= 0 && asset_index < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS &&
           g_mesh_preview_store.valid[asset_index];
}
