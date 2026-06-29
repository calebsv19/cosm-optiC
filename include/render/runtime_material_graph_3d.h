#ifndef RENDER_RUNTIME_MATERIAL_GRAPH_3D_H
#define RENDER_RUNTIME_MATERIAL_GRAPH_3D_H

#include <stdbool.h>
#include <stddef.h>

#include <json-c/json.h>

#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

#define RUNTIME_MATERIAL_GRAPH_SCHEMA_VERSION 1
#define RUNTIME_MATERIAL_GRAPH_ID_CAPACITY 64
#define RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY 64
#define RUNTIME_MATERIAL_GRAPH_MAX_NODES 16
#define RUNTIME_MATERIAL_GRAPH_MAX_CHANNEL_REFS \
    RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_CHANNEL_REFS
#define RUNTIME_MATERIAL_GRAPH_DIAGNOSTIC_CAPACITY 160

typedef enum {
    RUNTIME_MATERIAL_GRAPH_NODE_KIND_NONE = 0,
    RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER = 1,
    RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT = 2
} RuntimeMaterialGraphNodeKind;

typedef struct {
    bool active;
    char nodeId[RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY];
    RuntimeMaterialGraphNodeKind kind;
    RuntimeMaterialTextureLayer layer;
    RuntimeMaterialAuthoredTextureChannelRef channelRef;
} RuntimeMaterialGraphNode;

typedef struct {
    int schemaVersion;
    char graphId[RUNTIME_MATERIAL_GRAPH_ID_CAPACITY];
    int nodeCount;
    RuntimeMaterialGraphNode nodes[RUNTIME_MATERIAL_GRAPH_MAX_NODES];
} RuntimeMaterialGraphDocument;

typedef struct {
    bool active;
    RuntimeMaterialTextureStack stack;
    int channelRefCount;
    RuntimeMaterialAuthoredTextureChannelRef
        channelRefs[RUNTIME_MATERIAL_GRAPH_MAX_CHANNEL_REFS];
    char diagnostic[RUNTIME_MATERIAL_GRAPH_DIAGNOSTIC_CAPACITY];
} RuntimeMaterialGraphCompileResult;

RuntimeMaterialGraphDocument RuntimeMaterialGraphDocumentMake(const char* graph_id);
RuntimeMaterialGraphDocument RuntimeMaterialGraphDocumentEmpty(void);

RuntimeMaterialGraphNode RuntimeMaterialGraphNodeMakeLayer(
    const char* node_id,
    RuntimeMaterialTextureLayer layer);
RuntimeMaterialGraphNode RuntimeMaterialGraphNodeMakeChannelOutput(const char* node_id,
                                                                   const char* channel,
                                                                   const char* source,
                                                                   const char* file_name);

bool RuntimeMaterialGraphDocumentAddNode(RuntimeMaterialGraphDocument* document,
                                         RuntimeMaterialGraphNode node);
bool RuntimeMaterialGraphCompileToStack(const RuntimeMaterialGraphDocument* document,
                                        RuntimeMaterialGraphCompileResult* out_result);
struct json_object* RuntimeMaterialGraphDocumentToJsonObject(
    const RuntimeMaterialGraphDocument* document,
    bool snake_case);
bool RuntimeMaterialGraphDocumentFromJsonObject(struct json_object* obj,
                                                RuntimeMaterialGraphDocument* out_document);

#endif
