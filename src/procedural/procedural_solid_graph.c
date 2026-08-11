#include "procedural_solid_graph_internal.h"

#include "app/ray_tracing_sha256.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void procedural_solid_graph_report_set(
    ProceduralSolidGraphReport *report,
    ProceduralSolidGraphStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSolidGraphStatus_Name(ProceduralSolidGraphStatus status) {
    switch (status) {
        case PROCEDURAL_SOLID_GRAPH_STATUS_OK: return "ok";
        case PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SOLID_GRAPH_STATUS_IO: return "io";
        case PROCEDURAL_SOLID_GRAPH_STATUS_JSON: return "json";
        case PROCEDURAL_SOLID_GRAPH_STATUS_SCHEMA: return "schema";
        case PROCEDURAL_SOLID_GRAPH_STATUS_IDENTITY: return "identity";
        case PROCEDURAL_SOLID_GRAPH_STATUS_CAPACITY: return "capacity";
        case PROCEDURAL_SOLID_GRAPH_STATUS_NODE: return "node";
        case PROCEDURAL_SOLID_GRAPH_STATUS_INPUT: return "input";
        case PROCEDURAL_SOLID_GRAPH_STATUS_CYCLE: return "cycle";
        case PROCEDURAL_SOLID_GRAPH_STATUS_DISCONNECTED:
            return "disconnected";
        case PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE: return "source";
        case PROCEDURAL_SOLID_GRAPH_STATUS_BUDGET: return "budget";
        case PROCEDURAL_SOLID_GRAPH_STATUS_EVALUATION: return "evaluation";
        case PROCEDURAL_SOLID_GRAPH_STATUS_CANONICALIZATION:
            return "canonicalization";
    }
    return "unknown";
}

const char *ProceduralSolidRegionKind_Name(ProceduralSolidRegionKind kind) {
    switch (kind) {
        case PROCEDURAL_SOLID_REGION_RETAINED: return "retained";
        case PROCEDURAL_SOLID_REGION_CUT: return "cut";
        case PROCEDURAL_SOLID_REGION_BLEND: return "blend";
    }
    return "unknown";
}

const char *ProceduralSolidNodeOp_Name(ProceduralSolidNodeOp op) {
    switch (op) {
        case PROCEDURAL_SOLID_NODE_SPHERE: return "sphere";
        case PROCEDURAL_SOLID_NODE_BOX: return "box";
        case PROCEDURAL_SOLID_NODE_CYLINDER_Z: return "cylinder_z";
        case PROCEDURAL_SOLID_NODE_SOURCE_MESH: return "source_mesh";
        case PROCEDURAL_SOLID_NODE_TRANSFORM: return "transform";
        case PROCEDURAL_SOLID_NODE_TWIST_Z: return "twist_z";
        case PROCEDURAL_SOLID_NODE_TAPER_Z: return "taper_z";
        case PROCEDURAL_SOLID_NODE_ROUND: return "round";
        case PROCEDURAL_SOLID_NODE_UNION: return "union";
        case PROCEDURAL_SOLID_NODE_INTERSECTION: return "intersection";
        case PROCEDURAL_SOLID_NODE_DIFFERENCE: return "difference";
        case PROCEDURAL_SOLID_NODE_SMOOTH_UNION: return "smooth_union";
        case PROCEDURAL_SOLID_NODE_INVALID: break;
    }
    return "invalid";
}

uint32_t procedural_solid_graph_expected_inputs(ProceduralSolidNodeOp op) {
    switch (op) {
        case PROCEDURAL_SOLID_NODE_SPHERE:
        case PROCEDURAL_SOLID_NODE_BOX:
        case PROCEDURAL_SOLID_NODE_CYLINDER_Z:
        case PROCEDURAL_SOLID_NODE_SOURCE_MESH:
            return 0u;
        case PROCEDURAL_SOLID_NODE_TRANSFORM:
        case PROCEDURAL_SOLID_NODE_TWIST_Z:
        case PROCEDURAL_SOLID_NODE_TAPER_Z:
        case PROCEDURAL_SOLID_NODE_ROUND:
            return 1u;
        case PROCEDURAL_SOLID_NODE_UNION:
        case PROCEDURAL_SOLID_NODE_INTERSECTION:
        case PROCEDURAL_SOLID_NODE_DIFFERENCE:
        case PROCEDURAL_SOLID_NODE_SMOOTH_UNION:
            return 2u;
        case PROCEDURAL_SOLID_NODE_INVALID:
            return UINT32_MAX;
    }
    return UINT32_MAX;
}

void ProceduralSolidGraphV1_Init(ProceduralSolidGraphV1 *graph) {
    if (!graph) return;
    memset(graph, 0, sizeof(*graph));
    graph->schema_version = PROCEDURAL_SOLID_GRAPH_SCHEMA_VERSION;
}

static bool id_valid(const char *id) {
    size_t length;
    if (!id || !id[0]) return false;
    length = strlen(id);
    if (length >= PROCEDURAL_SOLID_GRAPH_ID_CAPACITY) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)id[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static bool finite_vec(CoreObjectVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

int procedural_solid_graph_find_node(
    const ProceduralSolidGraphV1 *graph,
    const char *id) {
    if (!graph || !id) return -1;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].id, id) == 0) return (int)i;
    }
    return -1;
}

static bool parameters_valid(const ProceduralSolidGraphNode *node) {
    if (!finite_vec(node->vector_a) || !finite_vec(node->vector_b) ||
        !finite_vec(node->vector_c) ||
        !isfinite(node->scalar_a) || !isfinite(node->scalar_b)) {
        return false;
    }
    switch (node->op) {
        case PROCEDURAL_SOLID_NODE_SPHERE:
            return node->scalar_a > 0.0;
        case PROCEDURAL_SOLID_NODE_BOX:
            return node->vector_a.x > 0.0 && node->vector_a.y > 0.0 &&
                   node->vector_a.z > 0.0 && node->scalar_a >= 0.0;
        case PROCEDURAL_SOLID_NODE_CYLINDER_Z:
            return node->scalar_a > 0.0 && node->scalar_b > 0.0;
        case PROCEDURAL_SOLID_NODE_SOURCE_MESH:
            return id_valid(node->source_id);
        case PROCEDURAL_SOLID_NODE_TRANSFORM:
            return fabs(node->vector_c.x) >= 1.0e-6 &&
                   fabs(node->vector_c.y) >= 1.0e-6 &&
                   fabs(node->vector_c.z) >= 1.0e-6;
        case PROCEDURAL_SOLID_NODE_TAPER_Z:
            return fabs(node->scalar_a) <= 4.0;
        case PROCEDURAL_SOLID_NODE_ROUND:
        case PROCEDURAL_SOLID_NODE_SMOOTH_UNION:
            return node->scalar_a > 0.0;
        case PROCEDURAL_SOLID_NODE_TWIST_Z:
        case PROCEDURAL_SOLID_NODE_UNION:
        case PROCEDURAL_SOLID_NODE_INTERSECTION:
        case PROCEDURAL_SOLID_NODE_DIFFERENCE:
            return true;
        case PROCEDURAL_SOLID_NODE_INVALID:
            return false;
    }
    return false;
}

static bool visit(const ProceduralSolidGraphV1 *graph,
                  size_t index,
                  uint8_t *states,
                  bool *reachable,
                  ProceduralSolidGraphReport *report) {
    const ProceduralSolidGraphNode *node = &graph->nodes[index];
    if (states[index] == 1u) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_CYCLE,
            node->id, "solid graph contains a cycle");
        return false;
    }
    if (states[index] == 2u) {
        reachable[index] = true;
        return true;
    }
    states[index] = 1u;
    reachable[index] = true;
    for (size_t i = 0u; i < node->input_count; ++i) {
        const int child = procedural_solid_graph_find_node(
            graph, node->inputs[i]);
        if (child < 0) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_INPUT,
                node->id, "solid node input does not resolve");
            return false;
        }
        if (!visit(graph, (size_t)child, states, reachable, report)) {
            return false;
        }
    }
    states[index] = 2u;
    return true;
}

bool ProceduralSolidGraphV1_Validate(
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidGraphReport *report) {
    uint8_t states[PROCEDURAL_SOLID_GRAPH_MAX_NODES] = {0};
    bool reachable[PROCEDURAL_SOLID_GRAPH_MAX_NODES] = {false};
    procedural_solid_graph_report_set(
        report, PROCEDURAL_SOLID_GRAPH_STATUS_OK, "", "ok");
    if (!graph) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT,
            "graph", "solid graph is required");
        return false;
    }
    if (graph->schema_version != PROCEDURAL_SOLID_GRAPH_SCHEMA_VERSION ||
        !id_valid(graph->graph_id) || !id_valid(graph->semantic_source_id) ||
        !id_valid(graph->output)) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_SCHEMA,
            "schema", "solid graph schema or identities are invalid");
        return false;
    }
    if (graph->node_count == 0u ||
        graph->node_count > PROCEDURAL_SOLID_GRAPH_MAX_NODES ||
        graph->max_node_evaluations < graph->node_count ||
        graph->max_node_evaluations > 4096u) {
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_CAPACITY,
            "node_count", "solid graph node or evaluation budget is invalid");
        return false;
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSolidGraphNode *node = &graph->nodes[i];
        const uint32_t expected =
            procedural_solid_graph_expected_inputs(node->op);
        if (!id_valid(node->id) || expected == UINT32_MAX ||
            node->input_count != expected || !parameters_valid(node)) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_NODE,
                node->id, "solid node definition is invalid");
            return false;
        }
        for (size_t j = i + 1u; j < graph->node_count; ++j) {
            if (strcmp(node->id, graph->nodes[j].id) == 0) {
                procedural_solid_graph_report_set(
                    report, PROCEDURAL_SOLID_GRAPH_STATUS_IDENTITY,
                    node->id, "solid node ids must be unique");
                return false;
            }
        }
    }
    {
        const int output = procedural_solid_graph_find_node(
            graph, graph->output);
        if (output < 0 ||
            !visit(graph, (size_t)output, states, reachable, report)) {
            if (output < 0) {
                procedural_solid_graph_report_set(
                    report, PROCEDURAL_SOLID_GRAPH_STATUS_INPUT,
                    "output", "solid graph output does not resolve");
            }
            return false;
        }
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (!reachable[i]) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_DISCONNECTED,
                graph->nodes[i].id,
                "solid graph contains a disconnected node");
            return false;
        }
    }
    return true;
}

static int node_ptr_compare(const void *lhs, const void *rhs) {
    const ProceduralSolidGraphNode *const *a = lhs;
    const ProceduralSolidGraphNode *const *b = rhs;
    return strcmp((*a)->id, (*b)->id);
}

static bool append_text(char **buffer,
                        size_t *length,
                        size_t *capacity,
                        const char *text) {
    const size_t add = strlen(text);
    char *next;
    if (*length > SIZE_MAX - add - 1u) return false;
    if (*length + add + 1u > *capacity) {
        size_t target = *capacity ? *capacity : 1024u;
        while (target < *length + add + 1u) {
            if (target > SIZE_MAX / 2u) return false;
            target *= 2u;
        }
        next = realloc(*buffer, target);
        if (!next) return false;
        *buffer = next;
        *capacity = target;
    }
    memcpy(*buffer + *length, text, add);
    *length += add;
    (*buffer)[*length] = '\0';
    return true;
}

static bool append_format(char **buffer,
                          size_t *length,
                          size_t *capacity,
                          const char *format,
                          ...) {
    char text[1024];
    va_list args;
    int count;
    va_start(args, format);
    count = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return count >= 0 && (size_t)count < sizeof(text) &&
           append_text(buffer, length, capacity, text);
}

bool ProceduralSolidGraphV1_CanonicalJson(
    const ProceduralSolidGraphV1 *graph,
    char **out_json,
    size_t *out_length,
    ProceduralSolidGraphReport *report) {
    const ProceduralSolidGraphNode *ordered[PROCEDURAL_SOLID_GRAPH_MAX_NODES];
    char *json = NULL;
    size_t length = 0u;
    size_t capacity = 0u;
    if (!out_json || !out_length || !ProceduralSolidGraphV1_Validate(
            graph, report)) {
        if (!out_json || !out_length) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT,
                "canonical", "canonical output pointers are required");
        }
        return false;
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        ordered[i] = &graph->nodes[i];
    }
    qsort(ordered, graph->node_count, sizeof(*ordered), node_ptr_compare);
    if (!append_format(
            &json, &length, &capacity,
            "{\"schema\":\"%s\",\"schema_version\":%u,"
            "\"graph_id\":\"%s\",\"semantic_source_id\":\"%s\","
            "\"max_node_evaluations\":%u,\"nodes\":[",
            PROCEDURAL_SOLID_GRAPH_SCHEMA, graph->schema_version,
            graph->graph_id, graph->semantic_source_id,
            graph->max_node_evaluations)) {
        goto fail;
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSolidGraphNode *node = ordered[i];
        if (!append_format(
                &json, &length, &capacity,
                "%s{\"id\":\"%s\",\"op\":\"%s\",\"inputs\":[",
                i ? "," : "", node->id,
                ProceduralSolidNodeOp_Name(node->op))) {
            goto fail;
        }
        for (size_t j = 0u; j < node->input_count; ++j) {
            if (!append_format(&json, &length, &capacity, "%s\"%s\"",
                               j ? "," : "", node->inputs[j])) {
                goto fail;
            }
        }
        if (!append_format(
                &json, &length, &capacity,
                "],\"source_id\":\"%s\",\"vector_a\":[%.17g,%.17g,%.17g],"
                "\"vector_b\":[%.17g,%.17g,%.17g],"
                "\"vector_c\":[%.17g,%.17g,%.17g],"
                "\"scalar_a\":%.17g,\"scalar_b\":%.17g}",
                node->source_id,
                node->vector_a.x, node->vector_a.y, node->vector_a.z,
                node->vector_b.x, node->vector_b.y, node->vector_b.z,
                node->vector_c.x, node->vector_c.y, node->vector_c.z,
                node->scalar_a, node->scalar_b)) {
            goto fail;
        }
    }
    if (!append_format(&json, &length, &capacity, "],\"output\":\"%s\"}",
                       graph->output)) {
        goto fail;
    }
    *out_json = json;
    *out_length = length;
    return true;
fail:
    free(json);
    procedural_solid_graph_report_set(
        report, PROCEDURAL_SOLID_GRAPH_STATUS_CANONICALIZATION,
        "canonical", "solid graph canonical JSON allocation failed");
    return false;
}

bool ProceduralSolidGraphV1_Digest(
    const ProceduralSolidGraphV1 *graph,
    char out_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY],
    ProceduralSolidGraphReport *report) {
    char *canonical = NULL;
    size_t length = 0u;
    if (!out_digest || !ProceduralSolidGraphV1_CanonicalJson(
            graph, &canonical, &length, report)) {
        return false;
    }
    if (!ray_tracing_sha256_bytes(canonical, length, out_digest)) {
        free(canonical);
        procedural_solid_graph_report_set(
            report, PROCEDURAL_SOLID_GRAPH_STATUS_CANONICALIZATION,
            "digest", "solid graph SHA-256 failed");
        return false;
    }
    free(canonical);
    return true;
}
