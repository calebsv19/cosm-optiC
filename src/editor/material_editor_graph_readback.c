#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"

bool MaterialEditorBuildFocusedGraphReadback(MaterialEditorGraphReadback* out_readback) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    bool has_graph = false;
    bool has_stack = false;

    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    out_readback->scene_object_index = focused_index;
    snprintf(out_readback->phase, sizeof(out_readback->phase), "M8-S6");
    snprintf(out_readback->authoring_state,
             sizeof(out_readback->authoring_state),
             "no_graph_document");
    snprintf(out_readback->evaluator_route,
             sizeof(out_readback->evaluator_route),
             "compiled_stack_fallback");
    snprintf(out_readback->integration_status,
             sizeof(out_readback->integration_status),
             "graph_mvp_create_layer_channel_clear");
    out_readback->visual_graph_mvp_available = true;

    if (focused_index < 0 || !material_editor_focused_object()) {
        snprintf(out_readback->integration_status,
                 sizeof(out_readback->integration_status),
                 "no_focused_object");
        return false;
    }

    has_graph = SceneEditorMaterialGraphGetObjectGraph(focused_index, &graph);
    has_stack = SceneEditorMaterialStackGetObjectStack(focused_index, &stack);
    out_readback->has_graph = has_graph;
    out_readback->has_compiled_stack_fallback = has_stack;
    out_readback->compiled_stack_layer_count = has_stack ? stack.layerCount : 0;

    if (!has_graph) {
        return true;
    }

    snprintf(out_readback->graph_id, sizeof(out_readback->graph_id), "%s", graph.graphId);
    snprintf(out_readback->authoring_state,
             sizeof(out_readback->authoring_state),
             "graph_document");
    out_readback->graph_node_count = graph.nodeCount;
    for (int i = 0; i < graph.nodeCount && i < RUNTIME_MATERIAL_GRAPH_MAX_NODES; ++i) {
        if (graph.nodes[i].active &&
            graph.nodes[i].kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) {
            out_readback->channel_ref_count += 1;
        }
    }
    snprintf(out_readback->integration_status,
             sizeof(out_readback->integration_status),
             "visual_graph_mvp_compiled_stack");
    return true;
}
