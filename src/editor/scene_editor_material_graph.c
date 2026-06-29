#include "editor/scene_editor_material_graph.h"

#include <string.h>

#include "editor/scene_editor_material_stack.h"

static bool s_scene_editor_material_graph_has_object_graph[MAX_OBJECTS];
static RuntimeMaterialGraphDocument s_scene_editor_material_object_graphs[MAX_OBJECTS];

static bool scene_editor_material_graph_valid_index(int scene_object_index) {
    return scene_object_index >= 0 && scene_object_index < MAX_OBJECTS;
}

void SceneEditorMaterialGraphResetAll(void) {
    memset(s_scene_editor_material_graph_has_object_graph,
           0,
           sizeof(s_scene_editor_material_graph_has_object_graph));
    memset(s_scene_editor_material_object_graphs,
           0,
           sizeof(s_scene_editor_material_object_graphs));
}

bool SceneEditorMaterialGraphSetObjectGraph(
    int scene_object_index,
    const RuntimeMaterialGraphDocument* document,
    RuntimeMaterialGraphCompileResult* out_compile_result) {
    RuntimeMaterialGraphCompileResult compile_result;
    if (!scene_editor_material_graph_valid_index(scene_object_index) || !document) {
        return false;
    }
    memset(&compile_result, 0, sizeof(compile_result));
    if (!RuntimeMaterialGraphCompileToStack(document, &compile_result)) {
        if (out_compile_result) *out_compile_result = compile_result;
        return false;
    }
    if (!SceneEditorMaterialStackSetObjectStack(scene_object_index, &compile_result.stack)) {
        if (out_compile_result) *out_compile_result = compile_result;
        return false;
    }
    s_scene_editor_material_object_graphs[scene_object_index] = *document;
    s_scene_editor_material_graph_has_object_graph[scene_object_index] = true;
    if (out_compile_result) *out_compile_result = compile_result;
    return true;
}

bool SceneEditorMaterialGraphClearObjectGraph(int scene_object_index) {
    if (!scene_editor_material_graph_valid_index(scene_object_index)) return false;
    s_scene_editor_material_graph_has_object_graph[scene_object_index] = false;
    memset(&s_scene_editor_material_object_graphs[scene_object_index],
           0,
           sizeof(s_scene_editor_material_object_graphs[scene_object_index]));
    return true;
}

bool SceneEditorMaterialGraphHasObjectGraph(int scene_object_index) {
    if (!scene_editor_material_graph_valid_index(scene_object_index)) return false;
    return s_scene_editor_material_graph_has_object_graph[scene_object_index];
}

bool SceneEditorMaterialGraphGetObjectGraph(int scene_object_index,
                                            RuntimeMaterialGraphDocument* out_document) {
    if (!scene_editor_material_graph_valid_index(scene_object_index) ||
        !out_document ||
        !s_scene_editor_material_graph_has_object_graph[scene_object_index]) {
        return false;
    }
    *out_document = s_scene_editor_material_object_graphs[scene_object_index];
    return true;
}
