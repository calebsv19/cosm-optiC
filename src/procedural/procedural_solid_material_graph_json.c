#include "procedural/procedural_solid_material_graph.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include <json-c/json.h>

#include <stdio.h>
#include <string.h>

static void report_set(
    ProceduralSolidMaterialGraphReport *report,
    ProceduralSolidMaterialGraphStatus status,
    const char *field, const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static json_object *graph_json(
    const ProceduralSolidMaterialGraphV1 *graph) {
    json_object *root = json_object_new_object();
    json_object *binding = json_object_new_object();
    json_object *nodes = json_object_new_array();
    json_object *layers = json_object_new_array();
    if (!root || !binding || !nodes || !layers) {
        if (root) json_object_put(root);
        return NULL;
    }
    json_object_object_add(root, "schema",
        json_object_new_string(PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA));
    json_object_object_add(root, "schema_version",
        json_object_new_int(PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA_VERSION));
    json_object_object_add(root, "graph_id",
        json_object_new_string(graph->graph_id));
    json_object_object_add(binding, "binding_id",
        json_object_new_string(graph->authored_binding_id));
    json_object_object_add(binding, "binding_digest_sha256",
        json_object_new_string(graph->authored_binding_digest_sha256));
    json_object_object_add(root, "authored_binding", binding);
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSolidMaterialNodeV1 *node = &graph->nodes[i];
        json_object *item = json_object_new_object();
        json_object *inputs = json_object_new_object();
        json_object *params = json_object_new_object();
        json_object_object_add(item, "node_id",
            json_object_new_string(node->node_id));
        json_object_object_add(item, "kind",
            json_object_new_string(
                ProceduralSolidMaterialNodeKind_Name(node->kind)));
        if (node->input_a[0])
            json_object_object_add(inputs, "a",
                json_object_new_string(node->input_a));
        if (node->input_b[0])
            json_object_object_add(inputs, "b",
                json_object_new_string(node->input_b));
        json_object_object_add(item, "inputs", inputs);
        json_object_object_add(params, "value",
            json_object_new_double(node->value));
        json_object_object_add(params, "minimum",
            json_object_new_double(node->minimum));
        json_object_object_add(params, "maximum",
            json_object_new_double(node->maximum));
        json_object_object_add(params, "scale",
            json_object_new_double(node->scale));
        json_object_object_add(params, "offset",
            json_object_new_double(node->offset));
        json_object_object_add(params, "seed",
            json_object_new_int(node->seed));
        json_object_object_add(params, "region_kind",
            json_object_new_string(node->region_kind));
        json_object_object_add(item, "parameters", params);
        json_object_array_add(nodes, item);
    }
    json_object_object_add(root, "nodes", nodes);
    for (size_t i = 0u; i < graph->layer_count; ++i) {
        const ProceduralSolidMaterialLayerV1 *layer = &graph->layers[i];
        json_object *item = json_object_new_object();
        json_object_object_add(item, "material_id",
            json_object_new_string(layer->material_id));
        json_object_object_add(item, "material_path",
            json_object_new_string(layer->material_path));
        json_object_object_add(item, "material_digest_sha256",
            json_object_new_string(layer->material_digest_sha256));
        json_object_object_add(item, "weight_node_id",
            json_object_new_string(layer->weight_node_id));
        json_object_array_add(layers, item);
    }
    json_object_object_add(root, "layers", layers);
    return root;
}

bool ProceduralSolidMaterialGraphV1_Digest(
    const ProceduralSolidMaterialGraphV1 *graph,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY],
    ProceduralSolidMaterialGraphReport *report) {
    json_object *root;
    const char *text;
    bool ok;
    if (!out_digest ||
        !ProceduralSolidMaterialGraphV1_Validate(graph, report)) return false;
    root = graph_json(graph);
    if (!root) return false;
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ok = ray_tracing_sha256_bytes(text, strlen(text), out_digest);
    json_object_put(root);
    if (!ok) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_IO,
                   "digest", "unable to hash material graph");
        return false;
    }
    if (report) snprintf(report->graph_digest_sha256,
                         sizeof(report->graph_digest_sha256), "%s",
                         out_digest);
    return true;
}

bool ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(
    const char *path, const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidMaterialGraphReport *report) {
    char digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    json_object *root;
    const char *text;
    CoreResult result;
    if (!path || !ProceduralSolidMaterialGraphV1_Digest(
                     graph, digest, report)) return false;
    root = graph_json(graph);
    if (!root) return false;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    if (result.code != CORE_OK) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_IO,
                   "path", result.message);
        return false;
    }
    if (report) snprintf(report->graph_digest_sha256,
                         sizeof(report->graph_digest_sha256), "%s", digest);
    return true;
}

static bool object_get(
    json_object *object, const char *key, json_type type,
    json_object **out) {
    return object && json_object_object_get_ex(object, key, out) &&
           json_object_get_type(*out) == type;
}

static bool text_get(
    json_object *object, const char *key, char *out, size_t capacity,
    bool required) {
    json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value)) {
        if (!required) {
            out[0] = '\0';
            return true;
        }
        return false;
    }
    if (json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool number_get(
    json_object *object, const char *key, double *out) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        (json_object_get_type(value) != json_type_double &&
         json_object_get_type(value) != json_type_int)) return false;
    *out = json_object_get_double(value);
    return true;
}

bool ProceduralSolidMaterialGraphV1_LoadJsonFile(
    const char *path, ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialGraphV1 graph;
    json_object *root = NULL, *value = NULL, *binding = NULL;
    json_object *nodes = NULL, *layers = NULL;
    if (!path || !out_graph) return false;
    root = json_object_from_file(path);
    ProceduralSolidMaterialGraphV1_Init(&graph);
    if (!root || json_object_get_type(root) != json_type_object ||
        !object_get(root, "schema", json_type_string, &value) ||
        strcmp(json_object_get_string(value),
               PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA) != 0 ||
        !object_get(root, "schema_version", json_type_int, &value) ||
        json_object_get_int(value) !=
            (int)PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA_VERSION ||
        !text_get(root, "graph_id", graph.graph_id,
                  sizeof(graph.graph_id), true) ||
        !object_get(root, "authored_binding", json_type_object, &binding) ||
        !text_get(binding, "binding_id", graph.authored_binding_id,
                  sizeof(graph.authored_binding_id), true) ||
        !text_get(binding, "binding_digest_sha256",
                  graph.authored_binding_digest_sha256,
                  sizeof(graph.authored_binding_digest_sha256), true) ||
        !object_get(root, "nodes", json_type_array, &nodes) ||
        !object_get(root, "layers", json_type_array, &layers)) goto invalid;
    graph.node_count = json_object_array_length(nodes);
    graph.layer_count = json_object_array_length(layers);
    if (graph.node_count > PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES ||
        graph.layer_count > PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS)
        goto invalid;
    for (size_t i = 0u; i < graph.node_count; ++i) {
        ProceduralSolidMaterialNodeV1 *node = &graph.nodes[i];
        json_object *item = json_object_array_get_idx(nodes, i);
        json_object *inputs = NULL, *params = NULL;
        char kind[32] = {0};
        json_object *seed = NULL;
        if (!item || !text_get(item, "node_id", node->node_id,
                               sizeof(node->node_id), true) ||
            !text_get(item, "kind", kind, sizeof(kind), true) ||
            !ProceduralSolidMaterialNodeKind_FromName(kind, &node->kind) ||
            !object_get(item, "inputs", json_type_object, &inputs) ||
            !text_get(inputs, "a", node->input_a,
                      sizeof(node->input_a), false) ||
            !text_get(inputs, "b", node->input_b,
                      sizeof(node->input_b), false) ||
            !object_get(item, "parameters", json_type_object, &params) ||
            !number_get(params, "value", &node->value) ||
            !number_get(params, "minimum", &node->minimum) ||
            !number_get(params, "maximum", &node->maximum) ||
            !number_get(params, "scale", &node->scale) ||
            !number_get(params, "offset", &node->offset) ||
            !object_get(params, "seed", json_type_int, &seed) ||
            !text_get(params, "region_kind", node->region_kind,
                      sizeof(node->region_kind), false)) goto invalid;
        node->seed = json_object_get_int(seed);
    }
    for (size_t i = 0u; i < graph.layer_count; ++i) {
        ProceduralSolidMaterialLayerV1 *layer = &graph.layers[i];
        json_object *item = json_object_array_get_idx(layers, i);
        if (!item ||
            !text_get(item, "material_id", layer->material_id,
                      sizeof(layer->material_id), true) ||
            !text_get(item, "material_path", layer->material_path,
                      sizeof(layer->material_path), false) ||
            !text_get(item, "material_digest_sha256",
                      layer->material_digest_sha256,
                      sizeof(layer->material_digest_sha256), false) ||
            !text_get(item, "weight_node_id", layer->weight_node_id,
                      sizeof(layer->weight_node_id), true)) goto invalid;
    }
    json_object_put(root);
    if (!ProceduralSolidMaterialGraphV1_Validate(&graph, report)) return false;
    *out_graph = graph;
    return true;
invalid:
    if (root) json_object_put(root);
    report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_JSON,
               "graph", "material graph JSON is incomplete or invalid");
    return false;
}
