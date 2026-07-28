#include "procedural/procedural_surface_graph_internal.h"

#include <json-c/json.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool graph_json_exact_keys(
    json_object *object,
    const char *const *keys,
    size_t key_count) {
    if (!object || !json_object_is_type(object, json_type_object) ||
        (size_t)json_object_object_length(object) != key_count) {
        return false;
    }
    for (size_t i = 0u; i < key_count; ++i) {
        json_object *unused = NULL;
        if (!json_object_object_get_ex(object, keys[i], &unused)) return false;
    }
    return true;
}

static const char *graph_json_string(json_object *object, const char *key) {
    json_object *value = NULL;
    if (!object || !key ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static bool graph_json_u32(
    json_object *object,
    const char *key,
    uint32_t *out_value) {
    json_object *value = NULL;
    int64_t parsed;
    if (!object || !key || !out_value ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0 || (uint64_t)parsed > UINT32_MAX) return false;
    *out_value = (uint32_t)parsed;
    return true;
}

static ProceduralSurfaceGraphNodeKind graph_json_node_kind(
    const char *value) {
    if (!value) return PROCEDURAL_SURFACE_GRAPH_NODE_INVALID;
    if (strcmp(value, "constant") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT;
    }
    if (strcmp(value, "f64_add") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD;
    }
    if (strcmp(value, "recipe_output") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT;
    }
    return PROCEDURAL_SURFACE_GRAPH_NODE_INVALID;
}

static ProceduralSurfaceGraphValueType graph_json_value_type(
    const char *value) {
    if (!value) return PROCEDURAL_SURFACE_GRAPH_VALUE_INVALID;
    if (strcmp(value, "f64") == 0) return PROCEDURAL_SURFACE_GRAPH_VALUE_F64;
    if (strcmp(value, "u32") == 0) return PROCEDURAL_SURFACE_GRAPH_VALUE_U32;
    if (strcmp(value, "u64") == 0) return PROCEDURAL_SURFACE_GRAPH_VALUE_U64;
    if (strcmp(value, "string") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_VALUE_STRING;
    }
    if (strcmp(value, "coordinate_space") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE;
    }
    if (strcmp(value, "output_clamp") == 0) {
        return PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP;
    }
    return PROCEDURAL_SURFACE_GRAPH_VALUE_INVALID;
}

static bool graph_json_copy(
    char *destination,
    size_t capacity,
    const char *source) {
    if (!destination || capacity == 0u || !source ||
        snprintf(destination, capacity, "%s", source) >= (int)capacity) {
        return false;
    }
    return true;
}

static bool graph_json_parse_constant(
    json_object *object,
    ProceduralSurfaceGraphNode *node,
    ProceduralSurfaceGraphReport *report) {
    const char *const keys[] = {"id", "kind", "value_type", "value"};
    const char *type_name;
    json_object *value = NULL;
    if (!graph_json_exact_keys(object, keys, 4u)) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
            "constant node has missing or unknown fields");
    }
    type_name = graph_json_string(object, "value_type");
    node->constant.type = graph_json_value_type(type_name);
    if (!json_object_object_get_ex(object, "value", &value)) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
            "constant value is missing");
    }
    switch (node->constant.type) {
        case PROCEDURAL_SURFACE_GRAPH_VALUE_F64:
            if (!(json_object_is_type(value, json_type_double) ||
                  json_object_is_type(value, json_type_int))) {
                break;
            }
            node->constant.f64 = json_object_get_double(value);
            if (isfinite(node->constant.f64)) return true;
            break;
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U32: {
            int64_t parsed;
            if (!json_object_is_type(value, json_type_int)) break;
            parsed = json_object_get_int64(value);
            if (parsed < 0 || (uint64_t)parsed > UINT32_MAX) break;
            node->constant.u32 = (uint32_t)parsed;
            return true;
        }
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U64: {
            const char *encoded;
            if (!json_object_is_type(value, json_type_int)) break;
            encoded = json_object_get_string(value);
            if (!encoded || encoded[0] == '-') break;
            node->constant.u64 = json_object_get_uint64(value);
            return true;
        }
        case PROCEDURAL_SURFACE_GRAPH_VALUE_STRING:
        case PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE:
        case PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP:
            if (!json_object_is_type(value, json_type_string) ||
                !graph_json_copy(
                    node->constant.string_value,
                    sizeof(node->constant.string_value),
                    json_object_get_string(value))) {
                break;
            }
            return true;
        default:
            break;
    }
    return procedural_surface_graph_fail(
        report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
        "constant value does not match its declared type");
}

static bool graph_json_parse_domains(
    json_object *object,
    ProceduralSurfaceGraphNode *node,
    ProceduralSurfaceGraphReport *report) {
    const char *const keys[] = {"id", "kind", "output_domains"};
    json_object *domains = NULL;
    if (!graph_json_exact_keys(object, keys, 3u) ||
        !json_object_object_get_ex(object, "output_domains", &domains) ||
        !json_object_is_type(domains, json_type_array) ||
        json_object_array_length(domains) != 3u) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
            "recipe output must declare three output domains");
    }
    for (size_t i = 0u; i < 3u; ++i) {
        json_object *value = json_object_array_get_idx(domains, i);
        const char *name;
        uint32_t flag = 0u;
        if (!value || !json_object_is_type(value, json_type_string)) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
                "output domain must be a string");
        }
        name = json_object_get_string(value);
        if (strcmp(name, "field_ir") == 0) {
            flag = PROCEDURAL_SURFACE_GRAPH_DOMAIN_FIELD_IR;
        } else if (strcmp(name, "geometry") == 0) {
            flag = PROCEDURAL_SURFACE_GRAPH_DOMAIN_GEOMETRY;
        } else if (strcmp(name, "material") == 0) {
            flag = PROCEDURAL_SURFACE_GRAPH_DOMAIN_MATERIAL;
        } else {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
                "output domain is unsupported");
        }
        if ((node->output_domains & flag) != 0u) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
                "output domains must be unique");
        }
        node->output_domains |= flag;
    }
    return true;
}

static bool graph_json_parse_node(
    json_object *object,
    ProceduralSurfaceGraphNode *node,
    ProceduralSurfaceGraphReport *report) {
    const char *id;
    const char *kind_name;
    const char *const simple_keys[] = {"id", "kind"};
    if (!object || !node) return false;
    memset(node, 0, sizeof(*node));
    id = graph_json_string(object, "id");
    kind_name = graph_json_string(object, "kind");
    if (!graph_json_copy(node->id, sizeof(node->id), id)) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, "node.id",
            "node id is missing or too long");
    }
    node->kind = graph_json_node_kind(kind_name);
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT) {
        return graph_json_parse_constant(object, node, report);
    }
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD) {
        if (!graph_json_exact_keys(object, simple_keys, 2u)) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
                "f64_add node has missing or unknown fields");
        }
        return true;
    }
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT) {
        return graph_json_parse_domains(object, node, report);
    }
    return procedural_surface_graph_fail(
        report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, node->id,
        "node kind is unsupported");
}

static bool graph_json_parse_link(
    json_object *object,
    ProceduralSurfaceGraphLink *link,
    ProceduralSurfaceGraphReport *report) {
    const char *const keys[] = {
        "from_node", "from_socket", "to_node", "to_socket"};
    if (!graph_json_exact_keys(object, keys, 4u) ||
        !graph_json_copy(link->from_node, sizeof(link->from_node),
                         graph_json_string(object, "from_node")) ||
        !graph_json_copy(link->from_socket, sizeof(link->from_socket),
                         graph_json_string(object, "from_socket")) ||
        !graph_json_copy(link->to_node, sizeof(link->to_node),
                         graph_json_string(object, "to_node")) ||
        !graph_json_copy(link->to_socket, sizeof(link->to_socket),
                         graph_json_string(object, "to_socket"))) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, "links",
            "link has missing, unknown, or oversized fields");
    }
    return true;
}

bool ProceduralSurfaceGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceGraphV1 *out_graph,
    ProceduralSurfaceGraphReport *report) {
    const char *const root_keys[] = {
        "schema", "schema_version", "graph_id", "max_node_evaluations",
        "nodes", "links"};
    ProceduralSurfaceGraphV1 graph;
    json_object *root = NULL;
    json_object *nodes = NULL;
    json_object *links = NULL;
    const char *schema;
    if (!path || !out_graph) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT, "path",
            "graph path and output are required");
    }
    root = json_object_from_file(path);
    if (!root) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_IO, "path",
            "unable to read procedural graph file");
    }
    ProceduralSurfaceGraphV1_Init(&graph);
    schema = graph_json_string(root, "schema");
    if (!graph_json_exact_keys(root, root_keys, 6u) ||
        !schema || strcmp(schema, PROCEDURAL_SURFACE_GRAPH_SCHEMA) != 0 ||
        !graph_json_u32(root, "schema_version", &graph.schema_version) ||
        !graph_json_copy(graph.graph_id, sizeof(graph.graph_id),
                         graph_json_string(root, "graph_id")) ||
        !graph_json_u32(root, "max_node_evaluations",
                        &graph.max_node_evaluations) ||
        !json_object_object_get_ex(root, "nodes", &nodes) ||
        !json_object_is_type(nodes, json_type_array) ||
        !json_object_object_get_ex(root, "links", &links) ||
        !json_object_is_type(links, json_type_array)) {
        json_object_put(root);
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_JSON, "graph",
            "graph root has missing, unknown, or invalid fields");
    }
    graph.node_count = json_object_array_length(nodes);
    graph.link_count = json_object_array_length(links);
    if (graph.node_count > PROCEDURAL_SURFACE_GRAPH_MAX_NODES ||
        graph.link_count > PROCEDURAL_SURFACE_GRAPH_MAX_LINKS) {
        json_object_put(root);
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CAPACITY, "graph",
            "graph exceeds bounded node or link capacity");
    }
    for (size_t i = 0u; i < graph.node_count; ++i) {
        if (!graph_json_parse_node(
                json_object_array_get_idx(nodes, i), &graph.nodes[i], report)) {
            json_object_put(root);
            return false;
        }
    }
    for (size_t i = 0u; i < graph.link_count; ++i) {
        if (!graph_json_parse_link(
                json_object_array_get_idx(links, i), &graph.links[i], report)) {
            json_object_put(root);
            return false;
        }
    }
    json_object_put(root);
    if (!ProceduralSurfaceGraphV1_Validate(&graph, report)) return false;
    *out_graph = graph;
    return true;
}
