#include "procedural/procedural_surface_graph_internal.h"

#include "app/ray_tracing_sha256.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const ProceduralSurfaceGraphRecipeInputSpec
    g_procedural_surface_graph_recipe_inputs[] = {
        {"recipe_id", PROCEDURAL_SURFACE_GRAPH_VALUE_STRING},
        {"seed", PROCEDURAL_SURFACE_GRAPH_VALUE_U64},
        {"coordinate_space", PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE},
        {"base_feature_size_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"micro_feature_size_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"octave_count", PROCEDURAL_SURFACE_GRAPH_VALUE_U32},
        {"lacunarity", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"persistence", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"ridge_valley_blend", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"macro_micro_mix", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"target_edge_length_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"displacement_amplitude_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"edge_lock_width_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"output_clamp", PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP},
        {"snow_elevation_threshold_units", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"snow_slope_threshold", PROCEDURAL_SURFACE_GRAPH_VALUE_F64},
        {"preview_max_triangles", PROCEDURAL_SURFACE_GRAPH_VALUE_U32},
        {"inspection_max_triangles", PROCEDURAL_SURFACE_GRAPH_VALUE_U32},
        {"final_max_triangles", PROCEDURAL_SURFACE_GRAPH_VALUE_U32},
        {"max_field_evaluations", PROCEDURAL_SURFACE_GRAPH_VALUE_U32},
};

const size_t g_procedural_surface_graph_recipe_input_count =
    sizeof(g_procedural_surface_graph_recipe_inputs) /
    sizeof(g_procedural_surface_graph_recipe_inputs[0]);

bool procedural_surface_graph_fail(
    ProceduralSurfaceGraphReport *report,
    ProceduralSurfaceGraphStatus status,
    const char *field,
    const char *message) {
    if (report) {
        memset(report, 0, sizeof(*report));
        report->status = status;
        snprintf(report->field, sizeof(report->field), "%s",
                 field ? field : "");
        snprintf(report->message, sizeof(report->message), "%s",
                 message ? message : "");
    }
    return false;
}

static void procedural_surface_graph_ok(ProceduralSurfaceGraphReport *report) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = PROCEDURAL_SURFACE_GRAPH_STATUS_OK;
    snprintf(report->message, sizeof(report->message), "ok");
}

bool procedural_surface_graph_id_valid(const char *value) {
    size_t length;
    if (!value || !value[0]) return false;
    length = strlen(value);
    if (length >= PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY) return false;
    for (size_t i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

void ProceduralSurfaceGraphV1_Init(ProceduralSurfaceGraphV1 *graph) {
    if (!graph) return;
    memset(graph, 0, sizeof(*graph));
    graph->schema_version = PROCEDURAL_SURFACE_GRAPH_SCHEMA_VERSION;
    graph->max_node_evaluations = PROCEDURAL_SURFACE_GRAPH_MAX_NODES;
}

int procedural_surface_graph_find_node(
    const ProceduralSurfaceGraphV1 *graph,
    const char *id) {
    if (!graph || !id) return -1;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].id, id) == 0) return (int)i;
    }
    return -1;
}

const ProceduralSurfaceGraphRecipeInputSpec *
procedural_surface_graph_recipe_input(const char *socket) {
    if (!socket) return NULL;
    for (size_t i = 0u;
         i < g_procedural_surface_graph_recipe_input_count; ++i) {
        if (strcmp(g_procedural_surface_graph_recipe_inputs[i].socket,
                   socket) == 0) {
            return &g_procedural_surface_graph_recipe_inputs[i];
        }
    }
    return NULL;
}

bool procedural_surface_graph_output_type(
    const ProceduralSurfaceGraphNode *node,
    const char *socket,
    ProceduralSurfaceGraphValueType *out_type) {
    if (!node || !socket || !out_type) return false;
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT &&
        strcmp(socket, "value") == 0) {
        *out_type = node->constant.type;
        return true;
    }
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD &&
        strcmp(socket, "value") == 0) {
        *out_type = PROCEDURAL_SURFACE_GRAPH_VALUE_F64;
        return true;
    }
    return false;
}

bool procedural_surface_graph_input_type(
    const ProceduralSurfaceGraphNode *node,
    const char *socket,
    ProceduralSurfaceGraphValueType *out_type) {
    const ProceduralSurfaceGraphRecipeInputSpec *input;
    if (!node || !socket || !out_type) return false;
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD &&
        (strcmp(socket, "a") == 0 || strcmp(socket, "b") == 0)) {
        *out_type = PROCEDURAL_SURFACE_GRAPH_VALUE_F64;
        return true;
    }
    if (node->kind != PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT) {
        return false;
    }
    input = procedural_surface_graph_recipe_input(socket);
    if (!input) return false;
    *out_type = input->type;
    return true;
}

static bool graph_constant_valid(
    const ProceduralSurfaceGraphNode *node,
    ProceduralSurfaceGraphReport *report) {
    switch (node->constant.type) {
        case PROCEDURAL_SURFACE_GRAPH_VALUE_F64:
            if (!isfinite(node->constant.f64)) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, node->id,
                    "f64 constant must be finite");
            }
            return true;
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U32:
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U64:
            return true;
        case PROCEDURAL_SURFACE_GRAPH_VALUE_STRING:
            if (!procedural_surface_graph_id_valid(
                    node->constant.string_value)) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, node->id,
                    "string constant must use stable-id characters");
            }
            return true;
        case PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE:
            if (strcmp(node->constant.string_value, "object") != 0) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, node->id,
                    "coordinate-space constant must be object");
            }
            return true;
        case PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP:
            if (strcmp(node->constant.string_value, "signed_unit") != 0) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, node->id,
                    "output-clamp constant must be signed_unit");
            }
            return true;
        default:
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, node->id,
                "constant value type is unsupported");
    }
}

static bool graph_cycle_visit(
    const ProceduralSurfaceGraphV1 *graph,
    int node_index,
    unsigned char *state,
    ProceduralSurfaceGraphReport *report) {
    if (state[node_index] == 1u) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CYCLE,
            graph->nodes[node_index].id, "graph contains a dependency cycle");
    }
    if (state[node_index] == 2u) return true;
    state[node_index] = 1u;
    for (size_t i = 0u; i < graph->link_count; ++i) {
        const ProceduralSurfaceGraphLink *link = &graph->links[i];
        int source;
        if (strcmp(link->to_node, graph->nodes[node_index].id) != 0) continue;
        source = procedural_surface_graph_find_node(graph, link->from_node);
        if (source < 0 ||
            !graph_cycle_visit(graph, source, state, report)) {
            return false;
        }
    }
    state[node_index] = 2u;
    return true;
}

static void graph_mark_dependencies(
    const ProceduralSurfaceGraphV1 *graph,
    int node_index,
    bool *reachable) {
    if (reachable[node_index]) return;
    reachable[node_index] = true;
    for (size_t i = 0u; i < graph->link_count; ++i) {
        int source;
        if (strcmp(graph->links[i].to_node,
                   graph->nodes[node_index].id) != 0) {
            continue;
        }
        source = procedural_surface_graph_find_node(
            graph, graph->links[i].from_node);
        if (source >= 0) graph_mark_dependencies(graph, source, reachable);
    }
}

bool ProceduralSurfaceGraphV1_Validate(
    const ProceduralSurfaceGraphV1 *graph,
    ProceduralSurfaceGraphReport *report) {
    int output_index = -1;
    unsigned char cycle_state[PROCEDURAL_SURFACE_GRAPH_MAX_NODES] = {0};
    bool reachable[PROCEDURAL_SURFACE_GRAPH_MAX_NODES] = {false};
    uint32_t required_domains =
        PROCEDURAL_SURFACE_GRAPH_DOMAIN_FIELD_IR |
        PROCEDURAL_SURFACE_GRAPH_DOMAIN_GEOMETRY |
        PROCEDURAL_SURFACE_GRAPH_DOMAIN_MATERIAL;
    if (!graph) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT, "graph",
            "graph is required");
    }
    if (graph->schema_version != PROCEDURAL_SURFACE_GRAPH_SCHEMA_VERSION) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_SCHEMA, "schema_version",
            "only procedural surface graph schema version 1 is supported");
    }
    if (!procedural_surface_graph_id_valid(graph->graph_id)) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_IDENTITY, "graph_id",
            "graph id must use stable-id characters");
    }
    if (graph->node_count == 0u ||
        graph->node_count > PROCEDURAL_SURFACE_GRAPH_MAX_NODES ||
        graph->link_count > PROCEDURAL_SURFACE_GRAPH_MAX_LINKS) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CAPACITY, "graph",
            "graph node or link count exceeds the bounded contract");
    }
    if (graph->max_node_evaluations == 0u ||
        graph->max_node_evaluations > PROCEDURAL_SURFACE_GRAPH_MAX_NODES ||
        graph->node_count > graph->max_node_evaluations) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET,
            "max_node_evaluations",
            "node evaluation budget is invalid or smaller than the graph");
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSurfaceGraphNode *node = &graph->nodes[i];
        if (!procedural_surface_graph_id_valid(node->id)) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_IDENTITY, "node.id",
                "node id must use stable-id characters");
        }
        for (size_t j = 0u; j < i; ++j) {
            if (strcmp(node->id, graph->nodes[j].id) == 0) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_IDENTITY, node->id,
                    "node ids must be unique");
            }
        }
        if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT) {
            if (!graph_constant_valid(node, report)) return false;
        } else if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT) {
            if (output_index >= 0 || node->output_domains != required_domains) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_NODE, node->id,
                    "exactly one output node with field, geometry, and material domains is required");
            }
            output_index = (int)i;
        } else if (node->kind != PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_NODE, node->id,
                "node kind is unsupported");
        }
    }
    if (output_index < 0) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NODE, "nodes",
            "recipe output node is missing");
    }
    for (size_t i = 0u; i < graph->link_count; ++i) {
        const ProceduralSurfaceGraphLink *link = &graph->links[i];
        int from = procedural_surface_graph_find_node(graph, link->from_node);
        int to = procedural_surface_graph_find_node(graph, link->to_node);
        ProceduralSurfaceGraphValueType from_type;
        ProceduralSurfaceGraphValueType to_type;
        if (from < 0 || to < 0) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_LINK, "links",
                "link endpoint does not exist");
        }
        if (!procedural_surface_graph_output_type(
                &graph->nodes[from], link->from_socket, &from_type) ||
            !procedural_surface_graph_input_type(
                &graph->nodes[to], link->to_socket, &to_type)) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_SOCKET, "links",
                "link references an unsupported socket");
        }
        if (from_type != to_type) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, link->to_socket,
                "link source and destination socket types differ");
        }
        for (size_t j = 0u; j < i; ++j) {
            const ProceduralSurfaceGraphLink *prior = &graph->links[j];
            if (strcmp(link->to_node, prior->to_node) == 0 &&
                strcmp(link->to_socket, prior->to_socket) == 0) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_LINK,
                    link->to_socket,
                    "input socket must have exactly one incoming link");
            }
        }
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSurfaceGraphNode *node = &graph->nodes[i];
        size_t expected_inputs =
            node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD ? 2u :
            node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT ?
                g_procedural_surface_graph_recipe_input_count : 0u;
        size_t actual_inputs = 0u;
        for (size_t j = 0u; j < graph->link_count; ++j) {
            if (strcmp(graph->links[j].to_node, node->id) == 0) {
                actual_inputs += 1u;
            }
        }
        if (actual_inputs != expected_inputs) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_LINK, node->id,
                "node does not have its exact required input set");
        }
        if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD) {
            bool has_a = false;
            bool has_b = false;
            for (size_t j = 0u; j < graph->link_count; ++j) {
                if (strcmp(graph->links[j].to_node, node->id) != 0) continue;
                has_a |= strcmp(graph->links[j].to_socket, "a") == 0;
                has_b |= strcmp(graph->links[j].to_socket, "b") == 0;
            }
            if (!has_a || !has_b) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_LINK, node->id,
                    "f64_add requires a and b inputs");
            }
        }
    }
    for (size_t i = 0u;
         i < g_procedural_surface_graph_recipe_input_count; ++i) {
        bool found = false;
        for (size_t j = 0u; j < graph->link_count; ++j) {
            found |= strcmp(graph->links[j].to_node,
                            graph->nodes[output_index].id) == 0 &&
                     strcmp(graph->links[j].to_socket,
                            g_procedural_surface_graph_recipe_inputs[i].socket) ==
                         0;
        }
        if (!found) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_LINK,
                g_procedural_surface_graph_recipe_inputs[i].socket,
                "recipe output input is unbound");
        }
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (!graph_cycle_visit(graph, (int)i, cycle_state, report)) return false;
    }
    graph_mark_dependencies(graph, output_index, reachable);
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (!reachable[i]) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_DISCONNECTED,
                graph->nodes[i].id,
                "all nodes must contribute to the recipe output");
        }
    }
    procedural_surface_graph_ok(report);
    return true;
}

static int graph_node_index_compare(const void *a, const void *b, void *context) {
    const ProceduralSurfaceGraphV1 *graph = context;
    size_t ia = *(const size_t *)a;
    size_t ib = *(const size_t *)b;
    return strcmp(graph->nodes[ia].id, graph->nodes[ib].id);
}

static int graph_link_index_compare(const void *a, const void *b, void *context) {
    const ProceduralSurfaceGraphV1 *graph = context;
    const ProceduralSurfaceGraphLink *la =
        &graph->links[*(const size_t *)a];
    const ProceduralSurfaceGraphLink *lb =
        &graph->links[*(const size_t *)b];
    int result = strcmp(la->from_node, lb->from_node);
    if (result == 0) result = strcmp(la->from_socket, lb->from_socket);
    if (result == 0) result = strcmp(la->to_node, lb->to_node);
    if (result == 0) result = strcmp(la->to_socket, lb->to_socket);
    return result;
}

static void graph_sort_indices(
    size_t *indices,
    size_t count,
    int (*compare)(const void *, const void *, void *),
    void *context) {
    for (size_t i = 1u; i < count; ++i) {
        size_t key = indices[i];
        size_t j = i;
        while (j > 0u && compare(&key, &indices[j - 1u], context) < 0) {
            indices[j] = indices[j - 1u];
            j -= 1u;
        }
        indices[j] = key;
    }
}

static bool graph_append(
    char *buffer,
    size_t capacity,
    size_t *length,
    const char *format,
    ...) {
    va_list args;
    int written;
    if (!buffer || !length || *length >= capacity) return false;
    va_start(args, format);
    written = vsnprintf(buffer + *length, capacity - *length, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

bool ProceduralSurfaceGraphV1_CanonicalJson(
    const ProceduralSurfaceGraphV1 *graph,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceGraphReport *report) {
    size_t node_order[PROCEDURAL_SURFACE_GRAPH_MAX_NODES];
    size_t link_order[PROCEDURAL_SURFACE_GRAPH_MAX_LINKS];
    size_t length = 0u;
    if (!out_json || out_capacity == 0u) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
            "out_json", "canonical output buffer is required");
    }
    out_json[0] = '\0';
    if (!ProceduralSurfaceGraphV1_Validate(graph, report)) return false;
    for (size_t i = 0u; i < graph->node_count; ++i) node_order[i] = i;
    for (size_t i = 0u; i < graph->link_count; ++i) link_order[i] = i;
    graph_sort_indices(node_order, graph->node_count,
                       graph_node_index_compare, (void *)graph);
    graph_sort_indices(link_order, graph->link_count,
                       graph_link_index_compare, (void *)graph);
    if (!graph_append(
            out_json, out_capacity, &length,
            "{\"schema\":\"%s\",\"schema_version\":1,\"graph_id\":\"%s\","
            "\"max_node_evaluations\":%u,\"nodes\":[",
            PROCEDURAL_SURFACE_GRAPH_SCHEMA, graph->graph_id,
            graph->max_node_evaluations)) {
        goto capacity_fail;
    }
    for (size_t order = 0u; order < graph->node_count; ++order) {
        const ProceduralSurfaceGraphNode *node =
            &graph->nodes[node_order[order]];
        if (order > 0u &&
            !graph_append(out_json, out_capacity, &length, ",")) {
            goto capacity_fail;
        }
        if (!graph_append(out_json, out_capacity, &length,
                          "{\"id\":\"%s\",\"kind\":\"%s\"",
                          node->id,
                          ProceduralSurfaceGraphNodeKind_Name(node->kind))) {
            goto capacity_fail;
        }
        if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT) {
            if (!graph_append(out_json, out_capacity, &length,
                              ",\"value_type\":\"%s\",\"value\":",
                              ProceduralSurfaceGraphValueType_Name(
                                  node->constant.type))) {
                goto capacity_fail;
            }
            if (node->constant.type == PROCEDURAL_SURFACE_GRAPH_VALUE_F64) {
                if (!graph_append(out_json, out_capacity, &length, "%.17g",
                                  node->constant.f64)) {
                    goto capacity_fail;
                }
            } else if (node->constant.type ==
                       PROCEDURAL_SURFACE_GRAPH_VALUE_U32) {
                if (!graph_append(out_json, out_capacity, &length, "%u",
                                  node->constant.u32)) {
                    goto capacity_fail;
                }
            } else if (node->constant.type ==
                       PROCEDURAL_SURFACE_GRAPH_VALUE_U64) {
                if (!graph_append(out_json, out_capacity, &length, "%" PRIu64,
                                  node->constant.u64)) {
                    goto capacity_fail;
                }
            } else if (!graph_append(
                           out_json, out_capacity, &length, "\"%s\"",
                           node->constant.string_value)) {
                goto capacity_fail;
            }
        } else if (node->kind ==
                   PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT) {
            if (!graph_append(
                    out_json, out_capacity, &length,
                    ",\"output_domains\":[\"field_ir\",\"geometry\",\"material\"]")) {
                goto capacity_fail;
            }
        }
        if (!graph_append(out_json, out_capacity, &length, "}")) {
            goto capacity_fail;
        }
    }
    if (!graph_append(out_json, out_capacity, &length, "],\"links\":[")) {
        goto capacity_fail;
    }
    for (size_t order = 0u; order < graph->link_count; ++order) {
        const ProceduralSurfaceGraphLink *link =
            &graph->links[link_order[order]];
        if (order > 0u &&
            !graph_append(out_json, out_capacity, &length, ",")) {
            goto capacity_fail;
        }
        if (!graph_append(
                out_json, out_capacity, &length,
                "{\"from_node\":\"%s\",\"from_socket\":\"%s\","
                "\"to_node\":\"%s\",\"to_socket\":\"%s\"}",
                link->from_node, link->from_socket,
                link->to_node, link->to_socket)) {
            goto capacity_fail;
        }
    }
    if (!graph_append(out_json, out_capacity, &length, "]}")) {
        goto capacity_fail;
    }
    procedural_surface_graph_ok(report);
    return true;

capacity_fail:
    out_json[0] = '\0';
    return procedural_surface_graph_fail(
        report, PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION,
        "canonical_json", "canonical graph exceeds output capacity");
}

bool ProceduralSurfaceGraphV1_Digest(
    const ProceduralSurfaceGraphV1 *graph,
    char out_digest[PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceGraphReport *report) {
    char canonical[PROCEDURAL_SURFACE_GRAPH_CANONICAL_CAPACITY];
    if (!out_digest) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
            "out_digest", "graph digest output is required");
    }
    out_digest[0] = '\0';
    if (!ProceduralSurfaceGraphV1_CanonicalJson(
            graph, canonical, sizeof(canonical), report) ||
        !ray_tracing_sha256_bytes(canonical, strlen(canonical), out_digest)) {
        if (report && report->status ==
                          PROCEDURAL_SURFACE_GRAPH_STATUS_OK) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION,
                "graph_digest", "unable to hash canonical graph");
        }
        return false;
    }
    procedural_surface_graph_ok(report);
    return true;
}

const char *ProceduralSurfaceGraphValueType_Name(
    ProceduralSurfaceGraphValueType type) {
    switch (type) {
        case PROCEDURAL_SURFACE_GRAPH_VALUE_F64: return "f64";
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U32: return "u32";
        case PROCEDURAL_SURFACE_GRAPH_VALUE_U64: return "u64";
        case PROCEDURAL_SURFACE_GRAPH_VALUE_STRING: return "string";
        case PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE:
            return "coordinate_space";
        case PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP:
            return "output_clamp";
        default: return "invalid";
    }
}

const char *ProceduralSurfaceGraphNodeKind_Name(
    ProceduralSurfaceGraphNodeKind kind) {
    switch (kind) {
        case PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT: return "constant";
        case PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD: return "f64_add";
        case PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT:
            return "recipe_output";
        default: return "invalid";
    }
}

const char *ProceduralSurfaceGraphStatus_Name(
    ProceduralSurfaceGraphStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_GRAPH_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_IO: return "io";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_JSON: return "json";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_IDENTITY: return "identity";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_CAPACITY: return "capacity";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_NODE: return "node";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_SOCKET: return "socket";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE: return "type";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_LINK: return "link";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_CYCLE: return "cycle";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_DISCONNECTED:
            return "disconnected";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET: return "budget";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_RECIPE: return "recipe";
        case PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION:
            return "canonicalization";
        default: return "unknown";
    }
}
