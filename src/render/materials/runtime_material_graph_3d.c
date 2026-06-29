#include "render/runtime_material_graph_3d.h"

#include <stdio.h>
#include <string.h>

static void runtime_material_graph_copy_text(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0u) return;
    (void)snprintf(dst, dst_size, "%s", src ? src : "");
}

static void runtime_material_graph_set_diagnostic(RuntimeMaterialGraphCompileResult* result,
                                                  const char* message) {
    if (!result) return;
    runtime_material_graph_copy_text(result->diagnostic,
                                     sizeof(result->diagnostic),
                                     message ? message : "material graph compile failed");
}

static bool runtime_material_graph_node_id_present(const RuntimeMaterialGraphDocument* document,
                                                   const char* node_id) {
    if (!document || !node_id || !node_id[0]) return false;
    for (int i = 0; i < document->nodeCount; ++i) {
        if (document->nodes[i].active && strcmp(document->nodes[i].nodeId, node_id) == 0) {
            return true;
        }
    }
    return false;
}

RuntimeMaterialGraphDocument RuntimeMaterialGraphDocumentMake(const char* graph_id) {
    RuntimeMaterialGraphDocument document;
    memset(&document, 0, sizeof(document));
    document.schemaVersion = RUNTIME_MATERIAL_GRAPH_SCHEMA_VERSION;
    runtime_material_graph_copy_text(document.graphId, sizeof(document.graphId), graph_id);
    return document;
}

RuntimeMaterialGraphDocument RuntimeMaterialGraphDocumentEmpty(void) {
    return RuntimeMaterialGraphDocumentMake("");
}

RuntimeMaterialGraphNode RuntimeMaterialGraphNodeMakeLayer(const char* node_id,
                                                           RuntimeMaterialTextureLayer layer) {
    RuntimeMaterialGraphNode node;
    memset(&node, 0, sizeof(node));
    node.active = true;
    node.kind = RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER;
    runtime_material_graph_copy_text(node.nodeId, sizeof(node.nodeId), node_id);
    node.layer = layer;
    return node;
}

RuntimeMaterialGraphNode RuntimeMaterialGraphNodeMakeChannelOutput(const char* node_id,
                                                                   const char* channel,
                                                                   const char* source,
                                                                   const char* file_name) {
    RuntimeMaterialGraphNode node;
    memset(&node, 0, sizeof(node));
    node.active = true;
    node.kind = RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT;
    runtime_material_graph_copy_text(node.nodeId, sizeof(node.nodeId), node_id);
    node.channelRef.active = true;
    runtime_material_graph_copy_text(node.channelRef.channel,
                                     sizeof(node.channelRef.channel),
                                     channel);
    runtime_material_graph_copy_text(node.channelRef.source,
                                     sizeof(node.channelRef.source),
                                     source);
    runtime_material_graph_copy_text(node.channelRef.fileName,
                                     sizeof(node.channelRef.fileName),
                                     file_name);
    return node;
}

bool RuntimeMaterialGraphDocumentAddNode(RuntimeMaterialGraphDocument* document,
                                         RuntimeMaterialGraphNode node) {
    if (!document || !node.active || !node.nodeId[0]) return false;
    if (document->nodeCount < 0 ||
        document->nodeCount >= RUNTIME_MATERIAL_GRAPH_MAX_NODES) {
        return false;
    }
    if (runtime_material_graph_node_id_present(document, node.nodeId)) {
        return false;
    }
    document->nodes[document->nodeCount] = node;
    document->nodeCount += 1;
    return true;
}

bool RuntimeMaterialGraphCompileToStack(const RuntimeMaterialGraphDocument* document,
                                        RuntimeMaterialGraphCompileResult* out_result) {
    RuntimeMaterialGraphCompileResult result;
    memset(&result, 0, sizeof(result));
    result.stack = RuntimeMaterialTextureStackEmpty();

    if (!document || !out_result) return false;
    if (document->schemaVersion != RUNTIME_MATERIAL_GRAPH_SCHEMA_VERSION) {
        runtime_material_graph_set_diagnostic(&result, "unsupported material graph schema");
        *out_result = result;
        return false;
    }
    if (!document->graphId[0]) {
        runtime_material_graph_set_diagnostic(&result, "material graph id is required");
        *out_result = result;
        return false;
    }
    if (document->nodeCount < 0 || document->nodeCount > RUNTIME_MATERIAL_GRAPH_MAX_NODES) {
        runtime_material_graph_set_diagnostic(&result, "material graph node count is invalid");
        *out_result = result;
        return false;
    }

    for (int i = 0; i < document->nodeCount; ++i) {
        const RuntimeMaterialGraphNode* node = &document->nodes[i];
        if (!node->active) continue;
        if (!node->nodeId[0]) {
            runtime_material_graph_set_diagnostic(&result, "material graph node id is required");
            *out_result = result;
            return false;
        }
        if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) {
            if (result.stack.layerCount >= RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
                runtime_material_graph_set_diagnostic(&result,
                                                     "material graph layer capacity exceeded");
                *out_result = result;
                return false;
            }
            result.stack.layers[result.stack.layerCount] =
                RuntimeMaterialTextureLayerNormalize(node->layer);
            result.stack.layerCount += 1;
            continue;
        }
        if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) {
            RuntimeMaterialAuthoredTextureChannelRef channel_ref = node->channelRef;
            if (!channel_ref.active || !channel_ref.channel[0]) {
                runtime_material_graph_set_diagnostic(&result,
                                                     "material graph channel output is empty");
                *out_result = result;
                return false;
            }
            if (!RuntimeMaterialAuthoredTextureChannelNameSupported(channel_ref.channel)) {
                runtime_material_graph_set_diagnostic(&result,
                                                     "material graph channel is unsupported");
                *out_result = result;
                return false;
            }
            if (result.channelRefCount >= RUNTIME_MATERIAL_GRAPH_MAX_CHANNEL_REFS) {
                runtime_material_graph_set_diagnostic(&result,
                                                     "material graph channel capacity exceeded");
                *out_result = result;
                return false;
            }
            result.channelRefs[result.channelRefCount] = channel_ref;
            result.channelRefCount += 1;
            continue;
        }
        runtime_material_graph_set_diagnostic(&result, "material graph node kind is unsupported");
        *out_result = result;
        return false;
    }

    result.stack = RuntimeMaterialTextureStackNormalize(result.stack);
    result.active = result.stack.layerCount > 0 || result.channelRefCount > 0;
    runtime_material_graph_set_diagnostic(&result,
                                         result.active ? "material graph compile ok"
                                                       : "material graph produced no output");
    *out_result = result;
    return result.active;
}
