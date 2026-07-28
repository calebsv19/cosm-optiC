#include "procedural/procedural_solid_authoring.h"

#include "core_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_report(ProceduralSolidAuthoringReport *report,
                       ProceduralSolidAuthoringStatus status,
                       const char *field,
                       const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSolidParameterTarget_Name(
    ProceduralSolidParameterTarget target) {
    switch (target) {
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_X: return "vector_a.x";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Y: return "vector_a.y";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Z: return "vector_a.z";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_X: return "vector_b.x";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Y: return "vector_b.y";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Z: return "vector_b.z";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_X: return "vector_c.x";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Y: return "vector_c.y";
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Z: return "vector_c.z";
        case PROCEDURAL_SOLID_PARAMETER_SCALAR_A: return "scalar_a";
        case PROCEDURAL_SOLID_PARAMETER_SCALAR_B: return "scalar_b";
        case PROCEDURAL_SOLID_PARAMETER_INVALID: break;
    }
    return "invalid";
}

const char *ProceduralSolidAuthoringStatus_Name(
    ProceduralSolidAuthoringStatus status) {
    switch (status) {
        case PROCEDURAL_SOLID_AUTHORING_STATUS_OK: return "ok";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_ARGUMENT: return "argument";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_GRAPH: return "graph";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_PARAMETER: return "parameter";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_RANGE: return "range";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_BASE_DIGEST:
            return "base_digest";
        case PROCEDURAL_SOLID_AUTHORING_STATUS_IO: return "io";
    }
    return "unknown";
}

static bool append_parameter(ProceduralSolidAuthoringView *view,
                             const ProceduralSolidGraphNode *node,
                             ProceduralSolidParameterTarget target,
                             const char *label,
                             const char *unit,
                             double value,
                             double minimum,
                             double maximum) {
    ProceduralSolidParameter *parameter;
    if (view->parameter_count >= PROCEDURAL_SOLID_AUTHORING_MAX_PARAMETERS) {
        return false;
    }
    parameter = &view->parameters[view->parameter_count++];
    memset(parameter, 0, sizeof(*parameter));
    snprintf(parameter->id, sizeof(parameter->id), "%s.%s", node->id,
             ProceduralSolidParameterTarget_Name(target));
    snprintf(parameter->node_id, sizeof(parameter->node_id), "%s", node->id);
    snprintf(parameter->label, sizeof(parameter->label), "%s", label);
    snprintf(parameter->unit, sizeof(parameter->unit), "%s", unit);
    parameter->target = target;
    parameter->value = value;
    parameter->minimum = minimum;
    parameter->maximum = maximum;
    return true;
}

#define ADD(TARGET, LABEL, UNIT, VALUE, MINIMUM, MAXIMUM) \
    append_parameter(view, node, (TARGET), (LABEL), (UNIT), (VALUE), \
                     (MINIMUM), (MAXIMUM))

static bool append_node_parameters(ProceduralSolidAuthoringView *view,
                                   const ProceduralSolidGraphNode *node) {
    switch (node->op) {
        case PROCEDURAL_SOLID_NODE_SPHERE:
            return ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A, "Radius", "units",
                       node->scalar_a, 0.000001, 1000000.0);
        case PROCEDURAL_SOLID_NODE_BOX:
            return ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_X,
                       "Half extent X", "units", node->vector_a.x,
                       0.000001, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Y,
                       "Half extent Y", "units", node->vector_a.y,
                       0.000001, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Z,
                       "Half extent Z", "units", node->vector_a.z,
                       0.000001, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A,
                       "Edge roundness", "units", node->scalar_a,
                       0.0, 1000000.0);
        case PROCEDURAL_SOLID_NODE_CYLINDER_Z:
            return ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A, "Radius", "units",
                       node->scalar_a, 0.000001, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_B,
                       "Half height", "units", node->scalar_b,
                       0.000001, 1000000.0);
        case PROCEDURAL_SOLID_NODE_TRANSFORM:
            return ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_X,
                       "Translation X", "units", node->vector_a.x,
                       -1000000.0, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Y,
                       "Translation Y", "units", node->vector_a.y,
                       -1000000.0, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Z,
                       "Translation Z", "units", node->vector_a.z,
                       -1000000.0, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_B_X,
                       "Rotation X", "radians", node->vector_b.x,
                       -1000.0, 1000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Y,
                       "Rotation Y", "radians", node->vector_b.y,
                       -1000.0, 1000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Z,
                       "Rotation Z", "radians", node->vector_b.z,
                       -1000.0, 1000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_C_X,
                       "Scale X", "ratio", node->vector_c.x,
                       -1000000.0, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Y,
                       "Scale Y", "ratio", node->vector_c.y,
                       -1000000.0, 1000000.0) &&
                   ADD(PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Z,
                       "Scale Z", "ratio", node->vector_c.z,
                       -1000000.0, 1000000.0);
        case PROCEDURAL_SOLID_NODE_TWIST_Z:
            return ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A,
                       "Twist per Z", "radians_per_unit", node->scalar_a,
                       -1000.0, 1000.0);
        case PROCEDURAL_SOLID_NODE_TAPER_Z:
            return ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A,
                       "Taper per Z", "ratio_per_unit", node->scalar_a,
                       -4.0, 4.0);
        case PROCEDURAL_SOLID_NODE_ROUND:
        case PROCEDURAL_SOLID_NODE_SMOOTH_UNION:
            return ADD(PROCEDURAL_SOLID_PARAMETER_SCALAR_A,
                       node->op == PROCEDURAL_SOLID_NODE_ROUND
                           ? "Round radius" : "Blend radius",
                       "units", node->scalar_a, 0.000001, 1000000.0);
        case PROCEDURAL_SOLID_NODE_SOURCE_MESH:
        case PROCEDURAL_SOLID_NODE_UNION:
        case PROCEDURAL_SOLID_NODE_INTERSECTION:
        case PROCEDURAL_SOLID_NODE_DIFFERENCE:
            return true;
        case PROCEDURAL_SOLID_NODE_INVALID:
            return false;
    }
    return false;
}
#undef ADD

bool ProceduralSolidAuthoring_Inspect(
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidAuthoringView *out_view,
    ProceduralSolidAuthoringReport *report) {
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidAuthoringView view;
    set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_OK, "", "ok");
    if (!graph || !out_view) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_ARGUMENT,
                   "arguments", "graph and authoring view are required");
        return false;
    }
    memset(&view, 0, sizeof(view));
    if (!ProceduralSolidGraphV1_Validate(graph, &graph_report) ||
        !ProceduralSolidGraphV1_Digest(
            graph, view.graph_digest_sha256, &graph_report)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_GRAPH,
                   graph_report.field, graph_report.message);
        return false;
    }
    view.node_count = graph->node_count;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        view.connection_count += graph->nodes[i].input_count;
        if (!append_node_parameters(&view, &graph->nodes[i])) {
            set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_PARAMETER,
                       graph->nodes[i].id,
                       "solid parameter manifest exceeds capacity");
            return false;
        }
    }
    *out_view = view;
    if (report) {
        snprintf(report->result_graph_digest_sha256,
                 sizeof(report->result_graph_digest_sha256), "%s",
                 view.graph_digest_sha256);
    }
    return true;
}

static double *parameter_value(ProceduralSolidGraphNode *node,
                               ProceduralSolidParameterTarget target) {
    switch (target) {
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_X: return &node->vector_a.x;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Y: return &node->vector_a.y;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Z: return &node->vector_a.z;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_X: return &node->vector_b.x;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Y: return &node->vector_b.y;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Z: return &node->vector_b.z;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_X: return &node->vector_c.x;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Y: return &node->vector_c.y;
        case PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Z: return &node->vector_c.z;
        case PROCEDURAL_SOLID_PARAMETER_SCALAR_A: return &node->scalar_a;
        case PROCEDURAL_SOLID_PARAMETER_SCALAR_B: return &node->scalar_b;
        case PROCEDURAL_SOLID_PARAMETER_INVALID: break;
    }
    return NULL;
}

bool ProceduralSolidAuthoring_ApplyParameter(
    const ProceduralSolidGraphV1 *base_graph,
    const char *expected_base_digest,
    const char *parameter_id,
    double value,
    ProceduralSolidGraphV1 *out_graph,
    ProceduralSolidAuthoringReport *report) {
    ProceduralSolidAuthoringView view;
    ProceduralSolidGraphV1 edited;
    ProceduralSolidGraphReport graph_report;
    const ProceduralSolidParameter *parameter = NULL;
    char result_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_OK, "", "ok");
    if (!base_graph || !expected_base_digest || !parameter_id ||
        !out_graph || !isfinite(value)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_ARGUMENT,
                   "arguments", "typed edit arguments are invalid");
        return false;
    }
    if (!ProceduralSolidAuthoring_Inspect(base_graph, &view, report)) {
        return false;
    }
    if (strcmp(view.graph_digest_sha256, expected_base_digest) != 0) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_BASE_DIGEST,
                   "expected_base_digest",
                   "solid graph changed since the edit was planned");
        return false;
    }
    for (size_t i = 0u; i < view.parameter_count; ++i) {
        if (strcmp(view.parameters[i].id, parameter_id) == 0) {
            parameter = &view.parameters[i];
            break;
        }
    }
    if (!parameter) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_PARAMETER,
                   parameter_id, "solid parameter id is not editable");
        return false;
    }
    if (value < parameter->minimum || value > parameter->maximum) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_RANGE,
                   parameter_id, "solid parameter value is outside range");
        return false;
    }
    edited = *base_graph;
    for (size_t i = 0u; i < edited.node_count; ++i) {
        if (strcmp(edited.nodes[i].id, parameter->node_id) == 0) {
            double *target =
                parameter_value(&edited.nodes[i], parameter->target);
            if (!target) break;
            *target = value;
            if (!ProceduralSolidGraphV1_Validate(&edited, &graph_report) ||
                !ProceduralSolidGraphV1_Digest(
                    &edited, result_digest, &graph_report)) {
                set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_GRAPH,
                           graph_report.field, graph_report.message);
                return false;
            }
            *out_graph = edited;
            if (report) {
                snprintf(report->base_graph_digest_sha256,
                         sizeof(report->base_graph_digest_sha256), "%s",
                         view.graph_digest_sha256);
                snprintf(report->result_graph_digest_sha256,
                         sizeof(report->result_graph_digest_sha256), "%s",
                         result_digest);
            }
            return true;
        }
    }
    set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_PARAMETER,
               parameter_id, "solid parameter node was not found");
    return false;
}

bool ProceduralSolidAuthoring_SaveGraphAtomic(
    const char *path,
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidAuthoringReport *report) {
    ProceduralSolidGraphReport graph_report;
    char *canonical = NULL;
    size_t length = 0u;
    CoreResult result;
    set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_OK, "", "ok");
    if (!path || !graph) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_ARGUMENT,
                   "path", "save path and solid graph are required");
        return false;
    }
    if (!ProceduralSolidGraphV1_CanonicalJson(
            graph, &canonical, &length, &graph_report)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_GRAPH,
                   graph_report.field, graph_report.message);
        return false;
    }
    result = core_io_write_all_atomic(path, canonical, length);
    free(canonical);
    if (result.code != CORE_OK) {
        set_report(report, PROCEDURAL_SOLID_AUTHORING_STATUS_IO,
                   "path", result.message);
        return false;
    }
    return true;
}
