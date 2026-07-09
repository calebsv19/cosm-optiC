#include "render/runtime_material_graph_3d.h"

#include <stdio.h>
#include <string.h>

static bool runtime_material_graph_json_get_int(struct json_object* obj,
                                                const char* key,
                                                int* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_int) &&
        !json_object_is_type(value, json_type_double)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static bool runtime_material_graph_json_get_double(struct json_object* obj,
                                                   const char* key,
                                                   double* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static const char* runtime_material_graph_node_kind_stable_id(
    RuntimeMaterialGraphNodeKind kind) {
    if (kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) return "layer";
    if (kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) return "channel_output";
    return "none";
}

static RuntimeMaterialGraphNodeKind runtime_material_graph_node_kind_from_stable_id(
    const char* stable_id) {
    if (!stable_id) return RUNTIME_MATERIAL_GRAPH_NODE_KIND_NONE;
    if (strcmp(stable_id, "layer") == 0 ||
        strcmp(stable_id, "procedural_layer") == 0) {
        return RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER;
    }
    if (strcmp(stable_id, "channel_output") == 0) {
        return RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT;
    }
    return RUNTIME_MATERIAL_GRAPH_NODE_KIND_NONE;
}

static struct json_object* runtime_material_graph_params_to_json(
    RuntimeMaterialTexture3DParams params,
    bool snake_case) {
    struct json_object* parameters = json_object_new_object();
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    if (!parameters) return NULL;
    json_object_object_add(parameters,
                           snake_case ? "pattern_mode" : "patternMode",
                           json_object_new_int(params.patternMode));
    json_object_object_add(parameters, "coverage", json_object_new_double(params.coverage));
    json_object_object_add(parameters, "grain", json_object_new_double(params.grain));
    json_object_object_add(parameters,
                           snake_case ? "edge_softness" : "edgeSoftness",
                           json_object_new_double(params.edgeSoftness));
    json_object_object_add(parameters, "contrast", json_object_new_double(params.contrast));
    json_object_object_add(parameters, "flow", json_object_new_double(params.flow));
    json_object_object_add(parameters,
                           snake_case ? "color_depth" : "colorDepth",
                           json_object_new_double(params.colorDepth));
    json_object_object_add(parameters,
                           snake_case ? "surface_damage" : "surfaceDamage",
                           json_object_new_double(params.surfaceDamage));
    json_object_object_add(parameters, "seed", json_object_new_int(params.seed));
    return parameters;
}

static struct json_object* runtime_material_graph_placement_to_json(
    const RuntimeMaterialTexture3DPlacement* placement,
    bool snake_case) {
    struct json_object* placement_obj = json_object_new_object();
    if (!placement_obj || !placement) return placement_obj;
    json_object_object_add(placement_obj,
                           snake_case ? "offset_u" : "offsetU",
                           json_object_new_double(placement->offsetU));
    json_object_object_add(placement_obj,
                           snake_case ? "offset_v" : "offsetV",
                           json_object_new_double(placement->offsetV));
    json_object_object_add(placement_obj, "scale", json_object_new_double(placement->scale));
    json_object_object_add(placement_obj,
                           "strength",
                           json_object_new_double(placement->strength));
    json_object_object_add(placement_obj,
                           "rotation",
                           json_object_new_double(placement->rotation));
    return placement_obj;
}

static struct json_object* runtime_material_graph_layer_to_json(
    RuntimeMaterialTextureLayer layer,
    bool snake_case) {
    struct json_object* layer_obj = json_object_new_object();
    struct json_object* parameters = NULL;
    layer = RuntimeMaterialTextureLayerNormalize(layer);
    if (!layer_obj) return NULL;
    parameters = runtime_material_graph_params_to_json(layer.params, snake_case);
    if (!parameters) {
        json_object_put(layer_obj);
        return NULL;
    }
    json_object_object_add(layer_obj, "id", json_object_new_string(layer.layerId));
    json_object_object_add(layer_obj, "name", json_object_new_string(layer.displayName));
    json_object_object_add(layer_obj,
                           "role",
                           json_object_new_string(layer.role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE
                                                      ? "base"
                                                      : "overlay"));
    json_object_object_add(layer_obj,
                           "kind",
                           json_object_new_string(
                               RuntimeMaterialTextureLayerKindStableId(layer.kind)));
    json_object_object_add(layer_obj,
                           "blend",
                           json_object_new_string(
                               RuntimeMaterialTextureLayerBlendModeStableId(layer.blendMode)));
    json_object_object_add(layer_obj, "enabled", json_object_new_boolean(layer.enabled));
    json_object_object_add(layer_obj, "opacity", json_object_new_double(layer.opacity));
    json_object_object_add(layer_obj,
                           "placement",
                           runtime_material_graph_placement_to_json(&layer.placement,
                                                                    snake_case));
    json_object_object_add(layer_obj, "parameters", parameters);
    json_object_object_add(layer_obj,
                           snake_case ? "roughness_influence" : "roughnessInfluence",
                           json_object_new_double(layer.roughnessInfluence));
    json_object_object_add(layer_obj,
                           snake_case ? "reflectivity_influence" : "reflectivityInfluence",
                           json_object_new_double(layer.reflectivityInfluence));
    json_object_object_add(layer_obj,
                           snake_case ? "specular_influence" : "specularInfluence",
                           json_object_new_double(layer.specularInfluence));
    json_object_object_add(layer_obj,
                           snake_case ? "diffuse_influence" : "diffuseInfluence",
                           json_object_new_double(layer.diffuseInfluence));
    json_object_object_add(layer_obj,
                           snake_case ? "transparency_influence" : "transparencyInfluence",
                           json_object_new_double(layer.transparencyInfluence));
    return layer_obj;
}

static void runtime_material_graph_load_params(struct json_object* obj,
                                               RuntimeMaterialTexture3DParams* params) {
    struct json_object* parameters = NULL;
    if (!obj || !params) return;
    if (json_object_object_get_ex(obj, "parameters", &parameters) &&
        json_object_is_type(parameters, json_type_object)) {
        runtime_material_graph_json_get_int(parameters, "patternMode", &params->patternMode);
        runtime_material_graph_json_get_int(parameters, "pattern_mode", &params->patternMode);
        runtime_material_graph_json_get_double(parameters, "coverage", &params->coverage);
        runtime_material_graph_json_get_double(parameters, "grain", &params->grain);
        runtime_material_graph_json_get_double(parameters, "edgeSoftness", &params->edgeSoftness);
        runtime_material_graph_json_get_double(parameters, "edge_softness", &params->edgeSoftness);
        runtime_material_graph_json_get_double(parameters, "contrast", &params->contrast);
        runtime_material_graph_json_get_double(parameters, "flow", &params->flow);
        runtime_material_graph_json_get_double(parameters, "colorDepth", &params->colorDepth);
        runtime_material_graph_json_get_double(parameters, "color_depth", &params->colorDepth);
        runtime_material_graph_json_get_double(parameters,
                                               "surfaceDamage",
                                               &params->surfaceDamage);
        runtime_material_graph_json_get_double(parameters,
                                               "surface_damage",
                                               &params->surfaceDamage);
        runtime_material_graph_json_get_int(parameters, "seed", &params->seed);
    }
    *params = RuntimeMaterialTexture3DNormalizeParams(*params);
}

static void runtime_material_graph_load_placement(
    struct json_object* obj,
    RuntimeMaterialTexture3DPlacement* placement) {
    struct json_object* placement_obj = NULL;
    if (!obj || !placement) return;
    if (json_object_object_get_ex(obj, "placement", &placement_obj) &&
        json_object_is_type(placement_obj, json_type_object)) {
        runtime_material_graph_json_get_double(placement_obj, "offsetU", &placement->offsetU);
        runtime_material_graph_json_get_double(placement_obj, "offset_u", &placement->offsetU);
        runtime_material_graph_json_get_double(placement_obj, "offsetV", &placement->offsetV);
        runtime_material_graph_json_get_double(placement_obj, "offset_v", &placement->offsetV);
        runtime_material_graph_json_get_double(placement_obj, "scale", &placement->scale);
        runtime_material_graph_json_get_double(placement_obj, "strength", &placement->strength);
        runtime_material_graph_json_get_double(placement_obj, "rotation", &placement->rotation);
    }
}

static bool runtime_material_graph_load_layer(struct json_object* layer_obj,
                                              RuntimeMaterialTextureLayer* out_layer) {
    struct json_object* value = NULL;
    const char* kind_id = NULL;
    RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    RuntimeMaterialTextureLayer layer;
    if (!layer_obj || !json_object_is_type(layer_obj, json_type_object) || !out_layer) {
        return false;
    }
    if (json_object_object_get_ex(layer_obj, "kind", &value) &&
        json_object_is_type(value, json_type_string)) {
        kind_id = json_object_get_string(value);
    }
    kind = RuntimeMaterialTextureLayerKindFromStableId(kind_id);
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) return false;
    layer = RuntimeMaterialTextureLayerKindIsBase(kind)
                ? RuntimeMaterialTextureLayerMakeBase(kind)
                : RuntimeMaterialTextureLayerMakeOverlay(kind);
    if (json_object_object_get_ex(layer_obj, "id", &value) &&
        json_object_is_type(value, json_type_string)) {
        snprintf(layer.layerId, sizeof(layer.layerId), "%s", json_object_get_string(value));
    }
    if (json_object_object_get_ex(layer_obj, "name", &value) &&
        json_object_is_type(value, json_type_string)) {
        snprintf(layer.displayName,
                 sizeof(layer.displayName),
                 "%s",
                 json_object_get_string(value));
    }
    if (json_object_object_get_ex(layer_obj, "blend", &value) &&
        json_object_is_type(value, json_type_string)) {
        layer.blendMode =
            RuntimeMaterialTextureLayerBlendModeFromStableId(json_object_get_string(value));
    }
    if (json_object_object_get_ex(layer_obj, "enabled", &value) &&
        json_object_is_type(value, json_type_boolean)) {
        layer.enabled = json_object_get_boolean(value) != 0;
    }
    runtime_material_graph_json_get_double(layer_obj, "opacity", &layer.opacity);
    runtime_material_graph_load_placement(layer_obj, &layer.placement);
    runtime_material_graph_load_params(layer_obj, &layer.params);
    layer.placement.params = layer.params;
    runtime_material_graph_json_get_double(layer_obj,
                                           "roughnessInfluence",
                                           &layer.roughnessInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "roughness_influence",
                                           &layer.roughnessInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "reflectivityInfluence",
                                           &layer.reflectivityInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "reflectivity_influence",
                                           &layer.reflectivityInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "specularInfluence",
                                           &layer.specularInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "specular_influence",
                                           &layer.specularInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "diffuseInfluence",
                                           &layer.diffuseInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "diffuse_influence",
                                           &layer.diffuseInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "transparencyInfluence",
                                           &layer.transparencyInfluence);
    runtime_material_graph_json_get_double(layer_obj,
                                           "transparency_influence",
                                           &layer.transparencyInfluence);
    *out_layer = RuntimeMaterialTextureLayerNormalize(layer);
    return true;
}

struct json_object* RuntimeMaterialGraphDocumentToJsonObject(
    const RuntimeMaterialGraphDocument* document,
    bool snake_case) {
    struct json_object* root = NULL;
    struct json_object* nodes = NULL;
    if (!document || !document->graphId[0]) return NULL;
    root = json_object_new_object();
    nodes = json_object_new_array();
    if (!root || !nodes) {
        if (root) json_object_put(root);
        if (nodes) json_object_put(nodes);
        return NULL;
    }
    json_object_object_add(root,
                           snake_case ? "schema_version" : "schemaVersion",
                           json_object_new_int(document->schemaVersion));
    json_object_object_add(root,
                           snake_case ? "graph_id" : "graphId",
                           json_object_new_string(document->graphId));
    for (int i = 0; i < document->nodeCount && i < RUNTIME_MATERIAL_GRAPH_MAX_NODES; ++i) {
        const RuntimeMaterialGraphNode* node = &document->nodes[i];
        struct json_object* node_obj = NULL;
        if (!node->active || !node->nodeId[0]) continue;
        node_obj = json_object_new_object();
        if (!node_obj) {
            json_object_put(root);
            json_object_put(nodes);
            return NULL;
        }
        json_object_object_add(node_obj,
                               snake_case ? "node_id" : "nodeId",
                               json_object_new_string(node->nodeId));
        json_object_object_add(node_obj,
                               snake_case ? "node_kind" : "nodeKind",
                               json_object_new_string(
                                   runtime_material_graph_node_kind_stable_id(node->kind)));
        if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) {
            struct json_object* layer =
                runtime_material_graph_layer_to_json(node->layer, snake_case);
            if (!layer) {
                json_object_put(node_obj);
                json_object_put(root);
                json_object_put(nodes);
                return NULL;
            }
            json_object_object_add(node_obj, "layer", layer);
        } else if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) {
            struct json_object* channel_ref = json_object_new_object();
            if (!channel_ref) {
                json_object_put(node_obj);
                json_object_put(root);
                json_object_put(nodes);
                return NULL;
            }
            json_object_object_add(channel_ref,
                                   "channel",
                                   json_object_new_string(node->channelRef.channel));
            json_object_object_add(channel_ref,
                                   "source",
                                   json_object_new_string(node->channelRef.source));
            json_object_object_add(channel_ref,
                                   snake_case ? "file_name" : "fileName",
                                   json_object_new_string(node->channelRef.fileName));
            json_object_object_add(node_obj,
                                   snake_case ? "channel_ref" : "channelRef",
                                   channel_ref);
        }
        json_object_array_add(nodes, node_obj);
    }
    json_object_object_add(root, "nodes", nodes);
    return root;
}

bool RuntimeMaterialGraphDocumentFromJsonObject(struct json_object* obj,
                                                RuntimeMaterialGraphDocument* out_document) {
    struct json_object* value = NULL;
    struct json_object* nodes = NULL;
    const char* graph_id = NULL;
    RuntimeMaterialGraphDocument document;
    int schema_version = RUNTIME_MATERIAL_GRAPH_SCHEMA_VERSION;
    if (!obj || !json_object_is_type(obj, json_type_object) || !out_document) return false;
    runtime_material_graph_json_get_int(obj, "schemaVersion", &schema_version);
    runtime_material_graph_json_get_int(obj, "schema_version", &schema_version);
    if (json_object_object_get_ex(obj, "graphId", &value) &&
        json_object_is_type(value, json_type_string)) {
        graph_id = json_object_get_string(value);
    } else if (json_object_object_get_ex(obj, "graph_id", &value) &&
               json_object_is_type(value, json_type_string)) {
        graph_id = json_object_get_string(value);
    }
    if (!graph_id || !graph_id[0]) return false;
    document = RuntimeMaterialGraphDocumentMake(graph_id);
    document.schemaVersion = schema_version;
    if (!json_object_object_get_ex(obj, "nodes", &nodes) ||
        !json_object_is_type(nodes, json_type_array)) {
        return false;
    }
    for (size_t i = 0u;
         i < json_object_array_length(nodes) &&
         document.nodeCount < RUNTIME_MATERIAL_GRAPH_MAX_NODES;
         ++i) {
        struct json_object* node_obj = json_object_array_get_idx(nodes, i);
        struct json_object* node_id_obj = NULL;
        struct json_object* node_kind_obj = NULL;
        const char* node_id = NULL;
        RuntimeMaterialGraphNodeKind node_kind = RUNTIME_MATERIAL_GRAPH_NODE_KIND_NONE;
        if (!node_obj || !json_object_is_type(node_obj, json_type_object)) continue;
        if ((json_object_object_get_ex(node_obj, "nodeId", &node_id_obj) ||
             json_object_object_get_ex(node_obj, "node_id", &node_id_obj)) &&
            json_object_is_type(node_id_obj, json_type_string)) {
            node_id = json_object_get_string(node_id_obj);
        }
        if ((json_object_object_get_ex(node_obj, "nodeKind", &node_kind_obj) ||
             json_object_object_get_ex(node_obj, "node_kind", &node_kind_obj)) &&
            json_object_is_type(node_kind_obj, json_type_string)) {
            node_kind =
                runtime_material_graph_node_kind_from_stable_id(
                    json_object_get_string(node_kind_obj));
        }
        if (!node_id || !node_id[0]) continue;
        if (node_kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) {
            struct json_object* layer_obj = NULL;
            RuntimeMaterialTextureLayer layer;
            if (!json_object_object_get_ex(node_obj, "layer", &layer_obj) ||
                !runtime_material_graph_load_layer(layer_obj, &layer)) {
                continue;
            }
            (void)RuntimeMaterialGraphDocumentAddNode(
                &document,
                RuntimeMaterialGraphNodeMakeLayer(node_id, layer));
        } else if (node_kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) {
            struct json_object* channel_ref = NULL;
            struct json_object* channel_obj = NULL;
            struct json_object* source_obj = NULL;
            struct json_object* file_obj = NULL;
            const char* channel = "";
            const char* source = "";
            const char* file_name = "";
            if (!(json_object_object_get_ex(node_obj, "channelRef", &channel_ref) ||
                  json_object_object_get_ex(node_obj, "channel_ref", &channel_ref)) ||
                !json_object_is_type(channel_ref, json_type_object)) {
                continue;
            }
            if (json_object_object_get_ex(channel_ref, "channel", &channel_obj) &&
                json_object_is_type(channel_obj, json_type_string)) {
                channel = json_object_get_string(channel_obj);
            }
            if (json_object_object_get_ex(channel_ref, "source", &source_obj) &&
                json_object_is_type(source_obj, json_type_string)) {
                source = json_object_get_string(source_obj);
            }
            if ((json_object_object_get_ex(channel_ref, "fileName", &file_obj) ||
                 json_object_object_get_ex(channel_ref, "file_name", &file_obj)) &&
                json_object_is_type(file_obj, json_type_string)) {
                file_name = json_object_get_string(file_obj);
            }
            (void)RuntimeMaterialGraphDocumentAddNode(
                &document,
                RuntimeMaterialGraphNodeMakeChannelOutput(node_id, channel, source, file_name));
        }
    }
    *out_document = document;
    return document.nodeCount > 0;
}
