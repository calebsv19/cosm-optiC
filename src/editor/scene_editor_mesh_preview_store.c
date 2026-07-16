#include "editor/scene_editor_mesh_preview_store.h"

#include "editor/scene_editor_mesh_preview_contract.h"

#include <stdlib.h>
#include <string.h>

typedef struct SceneEditorMeshPreviewStore {
    int asset_count;
    int instance_count;
    int recovered_instance_count;
    bool valid[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreObjectVec3* vertex_normals[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    size_t vertex_normal_counts[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreMeshAssetRuntimeContract contracts[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    CoreMeshPreviewLodMesh lods[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
    RayTracingRuntimeMeshAssetInstance
        instances[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];
} SceneEditorMeshPreviewStore;

static SceneEditorMeshPreviewStore g_mesh_preview_store;

void SceneEditorMeshPreviewStoreReset(void) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        SceneEditorMeshPreviewFreeLod(&g_mesh_preview_store.lods[i]);
        free(g_mesh_preview_store.vertex_normals[i]);
    }
    memset(&g_mesh_preview_store, 0, sizeof(g_mesh_preview_store));
}

static bool scene_editor_mesh_preview_store_copy_vertex_normals(
    int asset_index,
    const CoreMeshAssetRuntimeDocument* document,
    const CoreMeshPreviewLodMesh* lod) {
    CoreObjectVec3* normals = NULL;
    if (asset_index < 0 || asset_index >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS ||
        !document || !lod || document->vertex_normal_count != document->vertex_count ||
        lod->vertex_count != document->vertex_count) {
        return false;
    }
    normals = (CoreObjectVec3*)malloc(document->vertex_count * sizeof(*normals));
    if (!normals) return false;
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        normals[i] = document->vertices[i].normal;
    }
    g_mesh_preview_store.vertex_normals[asset_index] = normals;
    g_mesh_preview_store.vertex_normal_counts[asset_index] = document->vertex_count;
    return true;
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
            (void)scene_editor_mesh_preview_store_copy_vertex_normals(
                i, &assets->assets[i].document, &g_mesh_preview_store.lods[i]);
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
                (void)scene_editor_mesh_preview_store_copy_vertex_normals(
                    asset_index, &document, &g_mesh_preview_store.lods[asset_index]);
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

bool SceneEditorMeshPreviewStoreGetVertexNormal(int asset_index,
                                                size_t vertex_index,
                                                CoreObjectVec3* out_normal) {
    if (out_normal) memset(out_normal, 0, sizeof(*out_normal));
    if (!out_normal || !SceneEditorMeshPreviewStoreIsValid(asset_index) ||
        vertex_index >= g_mesh_preview_store.vertex_normal_counts[asset_index] ||
        !g_mesh_preview_store.vertex_normals[asset_index]) {
        return false;
    }
    *out_normal = g_mesh_preview_store.vertex_normals[asset_index][vertex_index];
    return true;
}
