#include "procedural_surface_field_graph_internal.h"

#include <json-c/json.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

bool procedural_surface_field_graph_exact_keys(
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

static ProceduralSurfaceFieldNodeOp parse_op(const char *name) {
    if (!name) return PROCEDURAL_SURFACE_FIELD_NODE_INVALID;
#define MATCH(value, enum_value) \
    if (strcmp(name, value) == 0) return enum_value
    MATCH("constant", PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT);
    MATCH("position_x", PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X);
    MATCH("position_y", PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y);
    MATCH("position_z", PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z);
    MATCH("add", PROCEDURAL_SURFACE_FIELD_NODE_ADD);
    MATCH("subtract", PROCEDURAL_SURFACE_FIELD_NODE_SUBTRACT);
    MATCH("multiply", PROCEDURAL_SURFACE_FIELD_NODE_MULTIPLY);
    MATCH("divide", PROCEDURAL_SURFACE_FIELD_NODE_DIVIDE);
    MATCH("minimum", PROCEDURAL_SURFACE_FIELD_NODE_MINIMUM);
    MATCH("maximum", PROCEDURAL_SURFACE_FIELD_NODE_MAXIMUM);
    MATCH("absolute", PROCEDURAL_SURFACE_FIELD_NODE_ABSOLUTE);
    MATCH("negate", PROCEDURAL_SURFACE_FIELD_NODE_NEGATE);
    MATCH("sine", PROCEDURAL_SURFACE_FIELD_NODE_SINE);
    MATCH("cosine", PROCEDURAL_SURFACE_FIELD_NODE_COSINE);
    MATCH("clamp01", PROCEDURAL_SURFACE_FIELD_NODE_CLAMP01);
    MATCH("smoothstep", PROCEDURAL_SURFACE_FIELD_NODE_SMOOTHSTEP);
    MATCH("mix", PROCEDURAL_SURFACE_FIELD_NODE_MIX);
    MATCH("power", PROCEDURAL_SURFACE_FIELD_NODE_POWER);
    MATCH("length2", PROCEDURAL_SURFACE_FIELD_NODE_LENGTH2);
    MATCH("value_noise_3d", PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D);
    MATCH("fbm_3d", PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D);
    MATCH("ridged_fbm_3d", PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D);
    MATCH("cellular_f1_3d", PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D);
#undef MATCH
    return PROCEDURAL_SURFACE_FIELD_NODE_INVALID;
}

static bool json_string(
    struct json_object *object,
    const char *key,
    char *out,
    size_t capacity) {
    struct json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_string) {
        return false;
    }
    text = json_object_get_string(value);
    if (!text || !text[0] || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool json_u32(
    struct json_object *object,
    const char *key,
    uint32_t *out) {
    struct json_object *value = NULL;
    int64_t parsed;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_int) {
        return false;
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0 || (uint64_t)parsed > UINT32_MAX) return false;
    *out = (uint32_t)parsed;
    return true;
}

static bool json_u64(
    struct json_object *object,
    const char *key,
    uint64_t *out) {
    struct json_object *value = NULL;
    int64_t parsed;
    if (!json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_int) {
        return false;
    }
    parsed = json_object_get_int64(value);
    if (parsed < 0) return false;
    *out = (uint64_t)parsed;
    return true;
}

static bool json_double(
    struct json_object *object,
    const char *key,
    double *out) {
    struct json_object *value = NULL;
    enum json_type type;
    if (!json_object_object_get_ex(object, key, &value)) return false;
    type = json_object_get_type(value);
    if (type != json_type_double && type != json_type_int) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static bool parse_inputs(
    struct json_object *object,
    ProceduralSurfaceFieldGraphNode *node) {
    struct json_object *array = NULL;
    if (!json_object_object_get_ex(object, "inputs", &array) ||
        json_object_get_type(array) != json_type_array) {
        return false;
    }
    node->input_count = json_object_array_length(array);
    if (node->input_count > PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_INPUTS) {
        return false;
    }
    for (size_t i = 0u; i < node->input_count; ++i) {
        struct json_object *value = json_object_array_get_idx(array, i);
        const char *text;
        if (!value || json_object_get_type(value) != json_type_string) {
            return false;
        }
        text = json_object_get_string(value);
        if (!text || !text[0] ||
            strlen(text) >= PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY) {
            return false;
        }
        snprintf(node->inputs[i], sizeof(node->inputs[i]), "%s", text);
    }
    return true;
}

static bool parse_node(
    struct json_object *object,
    ProceduralSurfaceFieldGraphNode *node) {
    static const char *const base_keys[] = {"id", "op"};
    static const char *const constant_keys[] = {"id", "op", "value"};
    static const char *const input_keys[] = {"id", "op", "inputs"};
    static const char *const noise_keys[] = {"id", "op", "inputs", "seed"};
    static const char *const fbm_keys[] = {
        "id", "op", "inputs", "seed", "octaves", "lacunarity", "persistence"};
    struct json_object *op_value = NULL;
    memset(node, 0, sizeof(*node));
    if (!json_string(object, "id", node->id, sizeof(node->id)) ||
        !json_object_object_get_ex(object, "op", &op_value) ||
        json_object_get_type(op_value) != json_type_string) {
        return false;
    }
    node->op = parse_op(json_object_get_string(op_value));
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT) {
        return procedural_surface_field_graph_exact_keys(
                   object, constant_keys,
                   sizeof(constant_keys) / sizeof(constant_keys[0])) &&
               json_double(object, "value", &node->value);
    }
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X ||
        node->op == PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y ||
        node->op == PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z) {
        return procedural_surface_field_graph_exact_keys(
            object, base_keys, sizeof(base_keys) / sizeof(base_keys[0]));
    }
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D ||
        node->op == PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D) {
        return procedural_surface_field_graph_exact_keys(
                   object, noise_keys,
                   sizeof(noise_keys) / sizeof(noise_keys[0])) &&
               parse_inputs(object, node) &&
               json_u64(object, "seed", &node->seed);
    }
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D ||
        node->op == PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D) {
        return procedural_surface_field_graph_exact_keys(
                   object, fbm_keys,
                   sizeof(fbm_keys) / sizeof(fbm_keys[0])) &&
               parse_inputs(object, node) &&
               json_u64(object, "seed", &node->seed) &&
               json_u32(object, "octaves", &node->octaves) &&
               json_double(object, "lacunarity", &node->lacunarity) &&
               json_double(object, "persistence", &node->persistence);
    }
    if (node->op != PROCEDURAL_SURFACE_FIELD_NODE_INVALID) {
        return procedural_surface_field_graph_exact_keys(
                   object, input_keys,
                   sizeof(input_keys) / sizeof(input_keys[0])) &&
               parse_inputs(object, node);
    }
    return false;
}

static bool parse_outputs(
    struct json_object *object,
    ProceduralSurfaceFieldGraphOutputs *outputs) {
    static const char *const keys[] = {
        "height", "macro", "micro", "cavity", "mask",
        "color_r", "color_g", "color_b", "roughness"};
    return procedural_surface_field_graph_exact_keys(
               object, keys, sizeof(keys) / sizeof(keys[0])) &&
           json_string(object, "height", outputs->height,
                       sizeof(outputs->height)) &&
           json_string(object, "macro", outputs->macro,
                       sizeof(outputs->macro)) &&
           json_string(object, "micro", outputs->micro,
                       sizeof(outputs->micro)) &&
           json_string(object, "cavity", outputs->cavity,
                       sizeof(outputs->cavity)) &&
           json_string(object, "mask", outputs->mask,
                       sizeof(outputs->mask)) &&
           json_string(object, "color_r", outputs->color_r,
                       sizeof(outputs->color_r)) &&
           json_string(object, "color_g", outputs->color_g,
                       sizeof(outputs->color_g)) &&
           json_string(object, "color_b", outputs->color_b,
                       sizeof(outputs->color_b)) &&
           json_string(object, "roughness", outputs->roughness,
                       sizeof(outputs->roughness));
}

bool ProceduralSurfaceFieldGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceFieldGraphV1 *out_graph,
    ProceduralSurfaceFieldGraphReport *report) {
    static const char *const root_keys[] = {
        "schema", "schema_version", "program_id",
        "max_node_evaluations", "nodes", "outputs"};
    struct json_object *root = NULL;
    struct json_object *schema = NULL;
    struct json_object *nodes = NULL;
    struct json_object *outputs = NULL;
    ProceduralSurfaceFieldGraphV1 graph;
    bool result = false;
    procedural_surface_field_graph_report_set(
        report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK, "", "ok");
    if (!path || !out_graph) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
            "path", "field graph path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IO,
            "path", "unable to parse field graph JSON file");
        return false;
    }
    ProceduralSurfaceFieldGraphV1_Init(&graph);
    if (!procedural_surface_field_graph_exact_keys(
            root, root_keys, sizeof(root_keys) / sizeof(root_keys[0])) ||
        !json_object_object_get_ex(root, "schema", &schema) ||
        json_object_get_type(schema) != json_type_string ||
        strcmp(json_object_get_string(schema),
               PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA) != 0 ||
        !json_u32(root, "schema_version", &graph.schema_version) ||
        !json_string(root, "program_id", graph.program_id,
                     sizeof(graph.program_id)) ||
        !json_u32(root, "max_node_evaluations",
                  &graph.max_node_evaluations) ||
        !json_object_object_get_ex(root, "nodes", &nodes) ||
        json_object_get_type(nodes) != json_type_array ||
        !json_object_object_get_ex(root, "outputs", &outputs) ||
        json_object_get_type(outputs) != json_type_object) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_SCHEMA,
            "root", "field graph root schema is invalid");
        goto cleanup;
    }
    graph.node_count = json_object_array_length(nodes);
    if (graph.node_count == 0u ||
        graph.node_count > PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CAPACITY,
            "nodes", "field graph node array is outside capacity");
        goto cleanup;
    }
    for (size_t i = 0u; i < graph.node_count; ++i) {
        struct json_object *node = json_object_array_get_idx(nodes, i);
        if (!parse_node(node, &graph.nodes[i])) {
            procedural_surface_field_graph_report_set(
                report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NODE,
                "nodes", "field graph node JSON is invalid");
            goto cleanup;
        }
    }
    if (!parse_outputs(outputs, &graph.outputs) ||
        !ProceduralSurfaceFieldGraphV1_Validate(&graph, report)) {
        goto cleanup;
    }
    *out_graph = graph;
    result = true;

cleanup:
    json_object_put(root);
    return result;
}
