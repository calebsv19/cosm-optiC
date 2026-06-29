#ifndef EDITOR_SCENE_EDITOR_MATERIAL_GRAPH_H
#define EDITOR_SCENE_EDITOR_MATERIAL_GRAPH_H

#include <stdbool.h>

#include "render/runtime_material_graph_3d.h"

void SceneEditorMaterialGraphResetAll(void);
bool SceneEditorMaterialGraphSetObjectGraph(
    int scene_object_index,
    const RuntimeMaterialGraphDocument* document,
    RuntimeMaterialGraphCompileResult* out_compile_result);
bool SceneEditorMaterialGraphClearObjectGraph(int scene_object_index);
bool SceneEditorMaterialGraphHasObjectGraph(int scene_object_index);
bool SceneEditorMaterialGraphGetObjectGraph(int scene_object_index,
                                            RuntimeMaterialGraphDocument* out_document);

#endif
