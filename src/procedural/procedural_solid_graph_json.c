#include "procedural_solid_graph_internal.h"

#include <json-c/json.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

bool procedural_solid_graph_exact_keys(
    struct json_object *object,
    const char *const *keys,
    size_t key_count) {
    size_t found = 0u;
    if (!object || json_object_get_type(object) != json_type_object) {
        return false;
    }
    json_object_object_foreach(object, key, value) {
        bool known = false;
        (void)value;
        for (size_t i = 0u; i < key_count; ++i) {
            if (strcmp(key, keys[i]) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
        ++found;
    }
    if (found != key_count) return false;
    for (size_t i = 0u; i < key_count; ++i) {
        struct json_object *value = NULL;
        if (!json_object_object_get_ex(object, keys[i], &value)) return false;
    }
    return true;
}

static ProceduralSolidNodeOp parse_op(const char *name) {
    if (!name) return PROCEDURAL_SOLID_NODE_INVALID;
#define MATCH(text, value) if (strcmp(name, text) == 0) return value
    MATCH("sphere", PROCEDURAL_SOLID_NODE_SPHERE);
    MATCH("box", PROCEDURAL_SOLID_NODE_BOX);
    MATCH("cylinder_z", PROCEDURAL_SOLID_NODE_CYLINDER_Z);
    MATCH("source_mesh", PROCEDURAL_SOLID_NODE_SOURCE_MESH);
    MATCH("transform", PROCEDURAL_SOLID_NODE_TRANSFORM);
    MATCH("twist_z", PROCEDURAL_SOLID_NODE_TWIST_Z);
    MATCH("taper_z", PROCEDURAL_SOLID_NODE_TAPER_Z);
    MATCH("round", PROCEDURAL_SOLID_NODE_ROUND);
    MATCH("union", PROCEDURAL_SOLID_NODE_UNION);
    MATCH("intersection", PROCEDURAL_SOLID_NODE_INTERSECTION);
    MATCH("difference", PROCEDURAL_SOLID_NODE_DIFFERENCE);
    MATCH("smooth_union", PROCEDURAL_SOLID_NODE_SMOOTH_UNION);
#undef MATCH
    return PROCEDURAL_SOLID_NODE_INVALID;
}

static bool json_string(struct json_object *object,
                        const char *key,
                        char *out,
                        size_t capacity,
                        bool allow_empty) {
    struct json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_string) {
        return false;
    }
    text = json_object_get_string(value);
    if (!text || (!allow_empty && !text[0]) || strlen(text) >= capacity) {
        return false;
    }
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool json_u32(struct json_object *object,
                     const char *key,
                     uint32_t *out) {
    struct json_object *value = NULL;
    const int64_t parsed = json_object_object_get_ex(object, key, &value)
        ? json_object_get_int64(value) : -1;
    if (!value || json_object_get_type(value) != json_type_int ||
        parsed < 0 || (uint64_t)parsed > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)parsed;
    return true;
}

static bool json_double(struct json_object *object,
                        const char *key,
                        double *out) {
    struct json_object *value = NULL;
    enum json_type type;
    if (!json_object_object_get_ex(object, key, &value)) return false;
    type = json_object_get_type(value);
    if (type != json_type_int && type != json_type_double) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static bool json_vec3(struct json_object *object,
                      const char *key,
                      CoreObjectVec3 *out) {
    struct json_object *array = NULL;
    double values[3];
    if (!json_object_object_get_ex(object, key, &array) ||
        json_object_get_type(array) != json_type_array ||
        json_object_array_length(array) != 3u) {
        return false;
    }
    for (size_t i = 0u; i < 3u; ++i) {
        struct json_object *value = json_object_array_get_idx(array, i);
        enum json_type type;
        if (!value) return false;
        type = json_object_get_type(value);
        if (type != json_type_int && type != json_type_double) return false;
        values[i] = json_object_get_double(value);
        if (!isfinite(values[i])) return false;
    }
    *out = (CoreObjectVec3){values[0], values[1], values[2]};
    return true;
}

static bool parse_inputs(struct json_object *object,
                         ProceduralSolidGraphNode *node) {
    struct json_object *array = NULL;
    if (!json_object_object_get_ex(object, "inputs", &array) ||
        json_object_get_type(array) != json_type_array) {
        return false;
    }
    node->input_count = json_object_array_length(array);
    if (node->input_count > PROCEDURAL_SOLID_GRAPH_MAX_INPUTS) return false;
    for (size_t i = 0u; i < node->input_count; ++i) {
        struct json_object *value = json_object_array_get_idx(array, i);
        const char *text;
        if (!value || json_object_get_type(value) != json_type_string) {
            return false;
        }
        text = json_object_get_string(value);
        if (!text || !text[0] ||
            strlen(text) >= PROCEDURAL_SOLID_GRAPH_ID_CAPACITY) {
            return false;
        }
        snprintf(node->inputs[i], sizeof(node->inputs[i]), "%s", text);
    }
    return true;
}

static bool parse_node(struct json_object *object,
                       ProceduralSolidGraphNode *node) {
    static const char *const keys[] = {
        "id", "op", "inputs", "source_id", "vector_a", "vector_b",
        "vector_c", "scalar_a", "scalar_b"};
    struct json_object *op = NULL;
    memset(node, 0, sizeof(*node));
    if (!procedural_solid_graph_exact_keys(
            object, keys, sizeof(keys) / sizeof(keys[0])) ||
        !json_string(object, "id", node->id, sizeof(node->id), false) ||
        !json_object_object_get_ex(object, "op", &op) ||
        json_object_get_type(op) != json_type_string ||
        !parse_inputs(object, node) ||
        !json_string(object, "source_id", node->source_id,
                     sizeof(node->source_id), true) ||
        !json_vec3(object, "vector_a", &node->vector_a) ||
        !json_vec3(object, "vector_b", &node->vector_b) ||
        !json_vec3(object, "vector_c", &node->vector_c) ||
        !json_double(object, "scalar_a", &node->scalar_a) ||
        !json_double(object, "scalar_b", &node->scalar_b)) {
        return false;
    }
    node->op = parse_op(json_object_get_string(op));
    return node->op != PROCEDURAL_SOLID_NODE_INVALID;
}

bool ProceduralSolidGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSolidGraphV1 *out_graph,
    ProceduralSolidGraphReport *report) {
    static const char *const root_keys[] = {
        "schema", "schema_version", "graph_id", "semantic_source_id",
        "max_node_evaluations", "nodes", "output"};
    ProceduralSolidGraphV1 parsed;
    struct json_object *root = NULL;
    struct json_object *schema = NULL;
    struct json_object *nodes = NULL;
    procedural_solid_graph_report_set(
        report, PROCEDURAL_SOLID_GRAPH_STATUS_OK, "", "ok");
    if (!path || !out_graph) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT,
            "path", "solid graph path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_IO,
            "path", "failed to read solid graph JSON");
        return false;
    }
    ProceduralSolidGraphV1_Init(&parsed);
    if (!procedural_solid_graph_exact_keys(
            root, root_keys, sizeof(root_keys) / sizeof(root_keys[0])) ||
        !json_object_object_get_ex(root, "schema", &schema) ||
        json_object_get_type(schema) != json_type_string ||
        strcmp(json_object_get_string(schema),
               PROCEDURAL_SOLID_GRAPH_SCHEMA) != 0 ||
        !json_u32(root, "schema_version", &parsed.schema_version) ||
        !json_string(root, "graph_id", parsed.graph_id,
                     sizeof(parsed.graph_id), false) ||
        !json_string(root, "semantic_source_id", parsed.semantic_source_id,
                     sizeof(parsed.semantic_source_id), false) ||
        !json_u32(root, "max_node_evaluations",
                  &parsed.max_node_evaluations) ||
        !json_string(root, "output", parsed.output,
                     sizeof(parsed.output), false) ||
        !json_object_object_get_ex(root, "nodes", &nodes) ||
        json_object_get_type(nodes) != json_type_array) {
        json_object_put(root);
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_JSON,
            "root", "solid graph JSON root is invalid");
        return false;
    }
    parsed.node_count = json_object_array_length(nodes);
    if (parsed.node_count > PROCEDURAL_SOLID_GRAPH_MAX_NODES) {
        json_object_put(root);
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_CAPACITY,
            "nodes", "solid graph node capacity exceeded");
        return false;
    }
    for (size_t i = 0u; i < parsed.node_count; ++i) {
        if (!parse_node(json_object_array_get_idx(nodes, i),
                        &parsed.nodes[i])) {
            json_object_put(root);
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_JSON,
                "nodes", "solid graph node JSON is invalid");
            return false;
        }
    }
    json_object_put(root);
    if (!ProceduralSolidGraphV1_Validate(&parsed, report)) return false;
    *out_graph = parsed;
    return true;
}
