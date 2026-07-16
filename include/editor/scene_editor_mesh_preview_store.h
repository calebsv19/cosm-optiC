#pragma once

#include <stdbool.h>

#include "core_mesh_preview.h"
#include "import/runtime_mesh_asset_loader.h"

#define SCENE_EDITOR_MESH_PREVIEW_LOD_TRIANGLES 18000u

void SceneEditorMeshPreviewStoreReset(void);
void SceneEditorMeshPreviewStorePrepare(const RayTracingRuntimeMeshAssetSet* assets);
const CoreMeshPreviewLodMesh* SceneEditorMeshPreviewStoreGet(int asset_index);
const CoreMeshAssetRuntimeContract* SceneEditorMeshPreviewStoreGetContract(int asset_index);
int SceneEditorMeshPreviewStoreInstanceCount(void);
const RayTracingRuntimeMeshAssetInstance* SceneEditorMeshPreviewStoreGetInstance(
    int instance_index);
bool SceneEditorMeshPreviewStoreHasSceneObject(int scene_object_index);
int SceneEditorMeshPreviewStoreRecoveredInstanceCount(void);
bool SceneEditorMeshPreviewStoreIsValid(int asset_index);
bool SceneEditorMeshPreviewStoreGetVertexNormal(int asset_index,
                                                size_t vertex_index,
                                                CoreObjectVec3* out_normal);
