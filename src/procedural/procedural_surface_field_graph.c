#include "procedural_surface_field_graph_internal.h"

#include "app/ray_tracing_sha256.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void procedural_surface_field_graph_report_set(
    ProceduralSurfaceFieldGraphReport *report,
    ProceduralSurfaceFieldGraphStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSurfaceFieldGraphStatus_Name(
    ProceduralSurfaceFieldGraphStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IO: return "io";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_JSON: return "json";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IDENTITY: return "identity";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CAPACITY: return "capacity";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NODE: return "node";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_INPUT: return "input";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CYCLE: return "cycle";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_DISCONNECTED:
            return "disconnected";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET: return "budget";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_EVALUATION:
            return "evaluation";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OUTPUT: return "output";
        case PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CANONICALIZATION:
            return "canonicalization";
    }
    return "unknown";
}

const char *ProceduralSurfaceFieldNodeOp_Name(
    ProceduralSurfaceFieldNodeOp op) {
    switch (op) {
        case PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT: return "constant";
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X: return "position_x";
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y: return "position_y";
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z: return "position_z";
        case PROCEDURAL_SURFACE_FIELD_NODE_ADD: return "add";
        case PROCEDURAL_SURFACE_FIELD_NODE_SUBTRACT: return "subtract";
        case PROCEDURAL_SURFACE_FIELD_NODE_MULTIPLY: return "multiply";
        case PROCEDURAL_SURFACE_FIELD_NODE_DIVIDE: return "divide";
        case PROCEDURAL_SURFACE_FIELD_NODE_MINIMUM: return "minimum";
        case PROCEDURAL_SURFACE_FIELD_NODE_MAXIMUM: return "maximum";
        case PROCEDURAL_SURFACE_FIELD_NODE_ABSOLUTE: return "absolute";
        case PROCEDURAL_SURFACE_FIELD_NODE_NEGATE: return "negate";
        case PROCEDURAL_SURFACE_FIELD_NODE_SINE: return "sine";
        case PROCEDURAL_SURFACE_FIELD_NODE_COSINE: return "cosine";
        case PROCEDURAL_SURFACE_FIELD_NODE_CLAMP01: return "clamp01";
        case PROCEDURAL_SURFACE_FIELD_NODE_SMOOTHSTEP: return "smoothstep";
        case PROCEDURAL_SURFACE_FIELD_NODE_MIX: return "mix";
        case PROCEDURAL_SURFACE_FIELD_NODE_POWER: return "power";
        case PROCEDURAL_SURFACE_FIELD_NODE_LENGTH2: return "length2";
        case PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D:
            return "value_noise_3d";
        case PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D: return "fbm_3d";
        case PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D:
            return "ridged_fbm_3d";
        case PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D:
            return "cellular_f1_3d";
        case PROCEDURAL_SURFACE_FIELD_NODE_INVALID: break;
    }
    return "invalid";
}

uint32_t procedural_surface_field_graph_expected_inputs(
    ProceduralSurfaceFieldNodeOp op) {
    switch (op) {
        case PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT:
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X:
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y:
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z:
            return 0u;
        case PROCEDURAL_SURFACE_FIELD_NODE_ABSOLUTE:
        case PROCEDURAL_SURFACE_FIELD_NODE_NEGATE:
        case PROCEDURAL_SURFACE_FIELD_NODE_SINE:
        case PROCEDURAL_SURFACE_FIELD_NODE_COSINE:
        case PROCEDURAL_SURFACE_FIELD_NODE_CLAMP01:
            return 1u;
        case PROCEDURAL_SURFACE_FIELD_NODE_ADD:
        case PROCEDURAL_SURFACE_FIELD_NODE_SUBTRACT:
        case PROCEDURAL_SURFACE_FIELD_NODE_MULTIPLY:
        case PROCEDURAL_SURFACE_FIELD_NODE_DIVIDE:
        case PROCEDURAL_SURFACE_FIELD_NODE_MINIMUM:
        case PROCEDURAL_SURFACE_FIELD_NODE_MAXIMUM:
        case PROCEDURAL_SURFACE_FIELD_NODE_POWER:
        case PROCEDURAL_SURFACE_FIELD_NODE_LENGTH2:
            return 2u;
        case PROCEDURAL_SURFACE_FIELD_NODE_SMOOTHSTEP:
        case PROCEDURAL_SURFACE_FIELD_NODE_MIX:
            return 3u;
        case PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D:
        case PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D:
        case PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D:
        case PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D:
            return 4u;
        case PROCEDURAL_SURFACE_FIELD_NODE_INVALID:
            return UINT32_MAX;
    }
    return UINT32_MAX;
}

void ProceduralSurfaceFieldGraphV1_Init(
    ProceduralSurfaceFieldGraphV1 *graph) {
    if (!graph) return;
    memset(graph, 0, sizeof(*graph));
    graph->schema_version = PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA_VERSION;
}

static bool id_valid(const char *id) {
    size_t length;
    if (!id || !id[0]) return false;
    length = strlen(id);
    if (length >= PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)id[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

int procedural_surface_field_graph_find_node(
    const ProceduralSurfaceFieldGraphV1 *graph,
    const char *id) {
    if (!graph || !id) return -1;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].id, id) == 0) return (int)i;
    }
    return -1;
}

static bool node_parameters_valid(
    const ProceduralSurfaceFieldGraphNode *node) {
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT) {
        return isfinite(node->value);
    }
    if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D ||
        node->op == PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D) {
        return node->octaves > 0u && node->octaves <= 12u &&
               isfinite(node->lacunarity) &&
               node->lacunarity >= 1.0 && node->lacunarity <= 8.0 &&
               isfinite(node->persistence) &&
               node->persistence > 0.0 && node->persistence <= 1.0;
    }
    return true;
}

static bool visit_node(
    const ProceduralSurfaceFieldGraphV1 *graph,
    size_t index,
    uint8_t *states,
    bool *reachable,
    ProceduralSurfaceFieldGraphReport *report) {
    const ProceduralSurfaceFieldGraphNode *node = &graph->nodes[index];
    if (states[index] == 1u) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CYCLE,
            node->id, "field graph contains a cycle");
        return false;
    }
    if (states[index] == 2u) {
        reachable[index] = true;
        return true;
    }
    states[index] = 1u;
    reachable[index] = true;
    for (size_t i = 0u; i < node->input_count; ++i) {
        const int input_index =
            procedural_surface_field_graph_find_node(graph, node->inputs[i]);
        if (input_index < 0 ||
            !visit_node(graph, (size_t)input_index, states, reachable, report)) {
            return false;
        }
    }
    states[index] = 2u;
    return true;
}

bool ProceduralSurfaceFieldGraphV1_Validate(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldGraphReport *report) {
    uint8_t states[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES] = {0};
    bool reachable[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES] = {false};
    const char *outputs[9];
    const size_t output_count = 9u;
    procedural_surface_field_graph_report_set(
        report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK, "", "ok");
    if (!graph) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
            "graph", "field graph is required");
        return false;
    }
    if (graph->schema_version !=
            PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA_VERSION ||
        !id_valid(graph->program_id)) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_SCHEMA,
            "schema", "field graph schema or program id is invalid");
        return false;
    }
    if (graph->node_count == 0u ||
        graph->node_count > PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES ||
        graph->max_node_evaluations < graph->node_count ||
        graph->max_node_evaluations >
            PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CAPACITY,
            "node_count", "node count or evaluation budget is invalid");
        return false;
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSurfaceFieldGraphNode *node = &graph->nodes[i];
        const uint32_t expected =
            procedural_surface_field_graph_expected_inputs(node->op);
        if (!id_valid(node->id) || expected == UINT32_MAX ||
            node->input_count != expected || !node_parameters_valid(node)) {
            procedural_surface_field_graph_report_set(
                report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NODE,
                node->id, "field node kind, inputs, or parameters are invalid");
            return false;
        }
        for (size_t prior = 0u; prior < i; ++prior) {
            if (strcmp(node->id, graph->nodes[prior].id) == 0) {
                procedural_surface_field_graph_report_set(
                    report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IDENTITY,
                    node->id, "field node ids must be unique");
                return false;
            }
        }
        for (size_t input = 0u; input < node->input_count; ++input) {
            if (!id_valid(node->inputs[input]) ||
                procedural_surface_field_graph_find_node(
                    graph, node->inputs[input]) < 0) {
                procedural_surface_field_graph_report_set(
                    report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_INPUT,
                    node->id, "field node references an unknown input");
                return false;
            }
        }
    }
    outputs[0] = graph->outputs.height;
    outputs[1] = graph->outputs.macro;
    outputs[2] = graph->outputs.micro;
    outputs[3] = graph->outputs.cavity;
    outputs[4] = graph->outputs.mask;
    outputs[5] = graph->outputs.color_r;
    outputs[6] = graph->outputs.color_g;
    outputs[7] = graph->outputs.color_b;
    outputs[8] = graph->outputs.roughness;
    for (size_t i = 0u; i < output_count; ++i) {
        const int index =
            procedural_surface_field_graph_find_node(graph, outputs[i]);
        if (!id_valid(outputs[i]) || index < 0 ||
            !visit_node(graph, (size_t)index, states, reachable, report)) {
            if (report && report->status ==
                              PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK) {
                procedural_surface_field_graph_report_set(
                    report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OUTPUT,
                    "outputs", "field graph output references are invalid");
            }
            return false;
        }
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (!reachable[i]) {
            procedural_surface_field_graph_report_set(
                report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_DISCONNECTED,
                graph->nodes[i].id,
                "every field node must contribute to a published output");
            return false;
        }
    }
    return true;
}

static bool append_text(
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

static int compare_node_indices(const void *a, const void *b, void *context) {
    const ProceduralSurfaceFieldGraphV1 *graph = context;
    const size_t ia = *(const size_t *)a;
    const size_t ib = *(const size_t *)b;
    return strcmp(graph->nodes[ia].id, graph->nodes[ib].id);
}

static void sort_indices(
    const ProceduralSurfaceFieldGraphV1 *graph,
    size_t *indices) {
    for (size_t i = 0u; i < graph->node_count; ++i) indices[i] = i;
    for (size_t i = 1u; i < graph->node_count; ++i) {
        const size_t value = indices[i];
        size_t j = i;
        while (j > 0u &&
               compare_node_indices(&value, &indices[j - 1u],
                                    (void *)graph) < 0) {
            indices[j] = indices[j - 1u];
            --j;
        }
        indices[j] = value;
    }
}

bool ProceduralSurfaceFieldGraphV1_CanonicalJson(
    const ProceduralSurfaceFieldGraphV1 *graph,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceFieldGraphReport *report) {
    size_t indices[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES];
    size_t length = 0u;
    if (!out_json || out_capacity == 0u) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
            "out_json", "canonical JSON output is required");
        return false;
    }
    out_json[0] = '\0';
    if (!ProceduralSurfaceFieldGraphV1_Validate(graph, report)) return false;
    sort_indices(graph, indices);
    if (!append_text(
            out_json, out_capacity, &length,
            "{\"schema\":\"%s\",\"schema_version\":1,"
            "\"program_id\":\"%s\",\"max_node_evaluations\":%u,\"nodes\":[",
            PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA, graph->program_id,
            graph->max_node_evaluations)) {
        goto capacity_failure;
    }
    for (size_t ordered = 0u; ordered < graph->node_count; ++ordered) {
        const ProceduralSurfaceFieldGraphNode *node =
            &graph->nodes[indices[ordered]];
        if (!append_text(out_json, out_capacity, &length,
                         "%s{\"id\":\"%s\",\"op\":\"%s\"",
                         ordered ? "," : "", node->id,
                         ProceduralSurfaceFieldNodeOp_Name(node->op))) {
            goto capacity_failure;
        }
        if (node->input_count > 0u) {
            if (!append_text(out_json, out_capacity, &length, ",\"inputs\":[")) {
                goto capacity_failure;
            }
            for (size_t input = 0u; input < node->input_count; ++input) {
                if (!append_text(out_json, out_capacity, &length,
                                 "%s\"%s\"", input ? "," : "",
                                 node->inputs[input])) {
                    goto capacity_failure;
                }
            }
            if (!append_text(out_json, out_capacity, &length, "]")) {
                goto capacity_failure;
            }
        }
        if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT &&
            !append_text(out_json, out_capacity, &length,
                         ",\"value\":%.17g", node->value)) {
            goto capacity_failure;
        }
        if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D ||
            node->op == PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D) {
            if (!append_text(out_json, out_capacity, &length,
                             ",\"seed\":%llu",
                             (unsigned long long)node->seed)) {
                goto capacity_failure;
            }
        }
        if (node->op == PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D ||
            node->op == PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D) {
            if (!append_text(
                    out_json, out_capacity, &length,
                    ",\"seed\":%llu,\"octaves\":%u,"
                    "\"lacunarity\":%.17g,\"persistence\":%.17g",
                    (unsigned long long)node->seed, node->octaves,
                    node->lacunarity, node->persistence)) {
                goto capacity_failure;
            }
        }
        if (!append_text(out_json, out_capacity, &length, "}")) {
            goto capacity_failure;
        }
    }
    if (!append_text(
            out_json, out_capacity, &length,
            "],\"outputs\":{\"height\":\"%s\",\"macro\":\"%s\","
            "\"micro\":\"%s\",\"cavity\":\"%s\",\"mask\":\"%s\","
            "\"color_r\":\"%s\",\"color_g\":\"%s\",\"color_b\":\"%s\","
            "\"roughness\":\"%s\"}}",
            graph->outputs.height, graph->outputs.macro,
            graph->outputs.micro, graph->outputs.cavity,
            graph->outputs.mask, graph->outputs.color_r,
            graph->outputs.color_g, graph->outputs.color_b,
            graph->outputs.roughness)) {
        goto capacity_failure;
    }
    return true;

capacity_failure:
    out_json[0] = '\0';
    procedural_surface_field_graph_report_set(
        report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CANONICALIZATION,
        "out_json", "canonical field graph JSON capacity is insufficient");
    return false;
}

bool ProceduralSurfaceFieldGraphV1_Digest(
    const ProceduralSurfaceFieldGraphV1 *graph,
    char out_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceFieldGraphReport *report) {
    char *canonical;
    bool ok;
    if (!out_digest) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
            "out_digest", "field graph digest output is required");
        return false;
    }
    out_digest[0] = '\0';
    canonical = calloc(PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY, 1u);
    if (!canonical) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CAPACITY,
            "canonical", "unable to allocate canonical field graph storage");
        return false;
    }
    ok = ProceduralSurfaceFieldGraphV1_CanonicalJson(
             graph, canonical,
             PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY, report) &&
         ray_tracing_sha256_bytes(
             canonical, strlen(canonical), out_digest);
    free(canonical);
    if (!ok && report &&
        report->status == PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CANONICALIZATION,
            "digest", "unable to hash canonical field graph");
    }
    return ok;
}
