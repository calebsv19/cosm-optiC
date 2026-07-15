#pragma once

#include <stdbool.h>

#include "core_mesh_preview.h"
#include "import/runtime_mesh_asset_loader.h"

#define SCENE_EDITOR_MESH_PREVIEW_LOD_TRIANGLES 18000u

void SceneEditorMeshPreviewStoreReset(void);
void SceneEditorMeshPreviewStorePrepare(const RayTracingRuntimeMeshAssetSet* assets);
const CoreMeshPreviewLodMesh* SceneEditorMeshPreviewStoreGet(int asset_index);
bool SceneEditorMeshPreviewStoreIsValid(int asset_index);
