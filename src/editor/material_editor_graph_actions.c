#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_face_preview.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"

static void material_editor_graph_node_id(char* out,
                                          size_t out_size,
                                          const char* prefix,
                                          int index) {
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "%s_%02d", prefix ? prefix : "node", index);
}

static RuntimeMaterialTextureLayerKind material_editor_graph_next_overlay_kind(
    const RuntimeMaterialGraphDocument* graph) {
    bool has_rust = false;
    bool has_grime = false;
    bool has_oil = false;
    bool has_fog = false;
    if (graph) {
        for (int i = 0; i < graph->nodeCount && i < RUNTIME_MATERIAL_GRAPH_MAX_NODES; ++i) {
            const RuntimeMaterialGraphNode* node = &graph->nodes[i];
            if (!node->active || node->kind != RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) continue;
            if (node->layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) has_rust = true;
            if (node->layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) has_grime = true;
            if (node->layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) has_oil = true;
            if (node->layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) has_fog = true;
        }
    }
    if (!has_rust) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    if (!has_grime) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME;
    if (!has_oil) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL;
    if (!has_fog) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
}

static bool material_editor_graph_seed_from_stack(int focused_index,
                                                  const SceneObject* obj,
                                                  RuntimeMaterialGraphDocument* out_graph) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    char graph_id[RUNTIME_MATERIAL_GRAPH_ID_CAPACITY];
    if (!out_graph || !obj || focused_index < 0) return false;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(obj, focused_index, &stack)) {
        return false;
    }
    snprintf(graph_id, sizeof(graph_id), "material_obj_%d_graph", focused_index);
    *out_graph = RuntimeMaterialGraphDocumentMake(graph_id);
    for (int i = 0; i < stack.layerCount && i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
        char node_id[RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY];
        material_editor_graph_node_id(node_id, sizeof(node_id), "layer", i);
        if (!RuntimeMaterialGraphDocumentAddNode(
                out_graph,
                RuntimeMaterialGraphNodeMakeLayer(node_id, stack.layers[i]))) {
            return false;
        }
    }
    return true;
}

static bool material_editor_graph_get_or_seed(RuntimeMaterialGraphDocument* out_graph,
                                              int* out_focused_index,
                                              SceneObject** out_obj) {
    SceneObject* obj = material_editor_focused_object();
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    if (!out_graph || !obj || focused_index < 0) return false;
    if (!SceneEditorMaterialGraphGetObjectGraph(focused_index, out_graph) &&
        !material_editor_graph_seed_from_stack(focused_index, obj, out_graph)) {
        return false;
    }
    if (out_focused_index) *out_focused_index = focused_index;
    if (out_obj) *out_obj = obj;
    return true;
}

bool MaterialEditorEnsureGraphForFocused(void) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialGraphCompileResult compile_result = {0};
    SceneObject* obj = NULL;
    int focused_index = -1;
    if (!material_editor_graph_get_or_seed(&graph, &focused_index, &obj)) return false;
    if (!SceneEditorMaterialGraphSetObjectGraph(focused_index, &graph, &compile_result)) {
        return false;
    }
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorAddGraphLayerNodeForFocused(void) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialGraphCompileResult compile_result = {0};
    RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    RuntimeMaterialTextureLayer layer;
    SceneObject* obj = NULL;
    char node_id[RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY];
    int focused_index = -1;
    if (!material_editor_graph_get_or_seed(&graph, &focused_index, &obj)) return false;
    if (graph.nodeCount >= RUNTIME_MATERIAL_GRAPH_MAX_NODES) return false;
    kind = material_editor_graph_next_overlay_kind(&graph);
    layer = RuntimeMaterialTextureLayerMakeOverlay(kind);
    snprintf(layer.layerId,
             sizeof(layer.layerId),
             "%s_%d",
             RuntimeMaterialTextureLayerKindStableId(kind),
             graph.nodeCount);
    material_editor_graph_node_id(node_id, sizeof(node_id), "layer", graph.nodeCount);
    if (!RuntimeMaterialGraphDocumentAddNode(&graph,
                                             RuntimeMaterialGraphNodeMakeLayer(node_id, layer))) {
        return false;
    }
    if (!SceneEditorMaterialGraphSetObjectGraph(focused_index, &graph, &compile_result)) {
        return false;
    }
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorAddGraphChannelNodeForFocused(void) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialGraphCompileResult compile_result = {0};
    SceneObject* obj = NULL;
    char node_id[RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY];
    int focused_index = -1;
    if (!material_editor_graph_get_or_seed(&graph, &focused_index, &obj)) return false;
    if (graph.nodeCount >= RUNTIME_MATERIAL_GRAPH_MAX_NODES) return false;
    material_editor_graph_node_id(node_id, sizeof(node_id), "roughness", graph.nodeCount);
    if (!RuntimeMaterialGraphDocumentAddNode(
            &graph,
            RuntimeMaterialGraphNodeMakeChannelOutput(node_id,
                                                      "roughness.scalar",
                                                      "luminance",
                                                      "graph_roughness.png"))) {
        return false;
    }
    if (!SceneEditorMaterialGraphSetObjectGraph(focused_index, &graph, &compile_result)) {
        return false;
    }
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorClearGraphForFocused(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_index < 0) return false;
    if (!SceneEditorMaterialGraphClearObjectGraph(focused_index)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}
