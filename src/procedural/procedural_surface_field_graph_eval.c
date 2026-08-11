#include "procedural_surface_field_graph_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct FieldEvaluation {
    const ProceduralSurfaceFieldGraphV1 *graph;
    ProceduralSurfaceFieldPoint3D point;
    double values[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES];
    uint8_t states[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES];
    uint32_t evaluations;
    ProceduralSurfaceFieldGraphReport *report;
} FieldEvaluation;

static double clamp01(double value) {
    return fmin(1.0, fmax(0.0, value));
}

static bool evaluate_index(
    FieldEvaluation *evaluation,
    size_t index,
    double *out_value) {
    const ProceduralSurfaceFieldGraphNode *node;
    double inputs[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_INPUTS] = {0};
    double value = 0.0;
    if (index >= evaluation->graph->node_count) return false;
    if (evaluation->states[index] == 2u) {
        *out_value = evaluation->values[index];
        return true;
    }
    node = &evaluation->graph->nodes[index];
    if (evaluation->states[index] == 1u) {
        procedural_surface_field_graph_report_set(
            evaluation->report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CYCLE,
            node->id, "cycle encountered during field evaluation");
        return false;
    }
    if (evaluation->evaluations >=
        evaluation->graph->max_node_evaluations) {
        procedural_surface_field_graph_report_set(
            evaluation->report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET,
            node->id, "per-sample node evaluation budget exhausted");
        return false;
    }
    evaluation->states[index] = 1u;
    ++evaluation->evaluations;
    for (size_t i = 0u; i < node->input_count; ++i) {
        const int input_index = procedural_surface_field_graph_find_node(
            evaluation->graph, node->inputs[i]);
        if (input_index < 0 ||
            !evaluate_index(evaluation, (size_t)input_index, &inputs[i])) {
            return false;
        }
    }
    switch (node->op) {
        case PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT:
            value = node->value;
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X:
            value = evaluation->point.x;
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y:
            value = evaluation->point.y;
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z:
            value = evaluation->point.z;
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_ADD:
            value = inputs[0] + inputs[1];
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_SUBTRACT:
            value = inputs[0] - inputs[1];
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_MULTIPLY:
            value = inputs[0] * inputs[1];
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_DIVIDE:
            if (fabs(inputs[1]) <= 1.0e-15) goto evaluation_failure;
            value = inputs[0] / inputs[1];
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_MINIMUM:
            value = fmin(inputs[0], inputs[1]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_MAXIMUM:
            value = fmax(inputs[0], inputs[1]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_ABSOLUTE:
            value = fabs(inputs[0]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_NEGATE:
            value = -inputs[0];
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_SINE:
            value = sin(inputs[0]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_COSINE:
            value = cos(inputs[0]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_CLAMP01:
            value = clamp01(inputs[0]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_SMOOTHSTEP: {
            double t;
            if (!(inputs[1] > inputs[0])) goto evaluation_failure;
            t = clamp01((inputs[2] - inputs[0]) /
                        (inputs[1] - inputs[0]));
            value = t * t * (3.0 - (2.0 * t));
            break;
        }
        case PROCEDURAL_SURFACE_FIELD_NODE_MIX:
            value = inputs[0] + ((inputs[1] - inputs[0]) * inputs[2]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_POWER:
            if (inputs[0] < 0.0) goto evaluation_failure;
            value = pow(inputs[0], inputs[1]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_LENGTH2:
            value = hypot(inputs[0], inputs[1]);
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D:
            if (!(inputs[3] > 0.0) ||
                !procedural_surface_field_graph_noise_value(
                    node->seed, inputs[0] / inputs[3],
                    inputs[1] / inputs[3], inputs[2] / inputs[3],
                    &value)) {
                goto evaluation_failure;
            }
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D:
        case PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D:
            if (!procedural_surface_field_graph_noise_fbm(
                    node->seed, inputs[0], inputs[1], inputs[2], inputs[3],
                    node->octaves, node->lacunarity, node->persistence,
                    node->op ==
                        PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D,
                    &value)) {
                goto evaluation_failure;
            }
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D:
            if (!procedural_surface_field_graph_noise_cellular_f1(
                    node->seed, inputs[0], inputs[1], inputs[2], inputs[3],
                    &value)) {
                goto evaluation_failure;
            }
            break;
        case PROCEDURAL_SURFACE_FIELD_NODE_INVALID:
            goto evaluation_failure;
    }
    if (!isfinite(value)) goto evaluation_failure;
    evaluation->states[index] = 2u;
    evaluation->values[index] = value;
    *out_value = value;
    return true;

evaluation_failure:
    procedural_surface_field_graph_report_set(
        evaluation->report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_EVALUATION,
        node->id, "field node produced an invalid numeric result");
    return false;
}

static bool evaluate_output(
    FieldEvaluation *evaluation,
    const char *node_id,
    double *out_value) {
    const int index =
        procedural_surface_field_graph_find_node(evaluation->graph, node_id);
    if (index < 0) {
        procedural_surface_field_graph_report_set(
            evaluation->report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OUTPUT,
            node_id, "field output node does not exist");
        return false;
    }
    return evaluate_index(evaluation, (size_t)index, out_value);
}

bool ProceduralSurfaceFieldGraphV1_Evaluate(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceFieldGraphSample *out_sample,
    ProceduralSurfaceFieldGraphReport *report) {
    FieldEvaluation evaluation;
    ProceduralSurfaceFieldGraphSample sample;
    ProceduralSurfaceFieldBudget budget;
    procedural_surface_field_graph_report_set(
        report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK, "", "ok");
    if (!graph || !sample_budget || !out_sample) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
            "arguments", "graph, sample budget, and output are required");
        return false;
    }
    if (graph->schema_version !=
            PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA_VERSION ||
        graph->node_count == 0u ||
        graph->node_count > PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES ||
        graph->max_node_evaluations < graph->node_count ||
        !isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_SCHEMA,
            "graph", "graph or evaluation point is invalid");
        return false;
    }
    if (sample_budget->evaluations >= sample_budget->max_evaluations) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET,
            "sample_budget", "field sample budget exhausted");
        return false;
    }
    memset(&evaluation, 0, sizeof(evaluation));
    memset(&sample, 0, sizeof(sample));
    evaluation.graph = graph;
    evaluation.point = point;
    evaluation.report = report;
    budget = *sample_budget;
    if (!evaluate_output(&evaluation, graph->outputs.height, &sample.height) ||
        !evaluate_output(
            &evaluation, graph->outputs.macro, &sample.macro_variation) ||
        !evaluate_output(
            &evaluation, graph->outputs.micro, &sample.micro_variation) ||
        !evaluate_output(
            &evaluation, graph->outputs.cavity, &sample.cavity) ||
        !evaluate_output(&evaluation, graph->outputs.mask, &sample.mask) ||
        !evaluate_output(
            &evaluation, graph->outputs.color_r, &sample.color_r) ||
        !evaluate_output(
            &evaluation, graph->outputs.color_g, &sample.color_g) ||
        !evaluate_output(
            &evaluation, graph->outputs.color_b, &sample.color_b) ||
        !evaluate_output(
            &evaluation, graph->outputs.roughness, &sample.roughness)) {
        return false;
    }
    const double tolerance = 1.0e-9;
    if (sample.height < -1.0 - tolerance ||
        sample.height > 1.0 + tolerance ||
        sample.macro_variation < -1.0 - tolerance ||
        sample.macro_variation > 1.0 + tolerance ||
        sample.micro_variation < -1.0 - tolerance ||
        sample.micro_variation > 1.0 + tolerance ||
        sample.cavity < -tolerance || sample.cavity > 1.0 + tolerance ||
        sample.mask < -tolerance || sample.mask > 1.0 + tolerance ||
        sample.color_r < -tolerance || sample.color_r > 1.0 + tolerance ||
        sample.color_g < -tolerance || sample.color_g > 1.0 + tolerance ||
        sample.color_b < -tolerance || sample.color_b > 1.0 + tolerance ||
        sample.roughness < -tolerance ||
        sample.roughness > 1.0 + tolerance) {
        procedural_surface_field_graph_report_set(
            report, PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OUTPUT,
            "outputs", "field graph output is outside its declared range");
        return false;
    }
    sample.height = fmin(1.0, fmax(-1.0, sample.height));
    sample.macro_variation =
        fmin(1.0, fmax(-1.0, sample.macro_variation));
    sample.micro_variation =
        fmin(1.0, fmax(-1.0, sample.micro_variation));
    sample.cavity = clamp01(sample.cavity);
    sample.mask = clamp01(sample.mask);
    sample.color_r = clamp01(sample.color_r);
    sample.color_g = clamp01(sample.color_g);
    sample.color_b = clamp01(sample.color_b);
    sample.roughness = clamp01(sample.roughness);
    ++budget.evaluations;
    *sample_budget = budget;
    *out_sample = sample;
    return true;
}

bool ProceduralSurfaceFieldGraphV1_EvaluateLegacy(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    ProceduralSurfaceFieldGraphSample sample;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceFieldOutput field;
    if (report) {
        memset(report, 0, sizeof(*report));
        report->status = PROCEDURAL_SURFACE_FIELD_STATUS_OK;
        snprintf(report->message, sizeof(report->message), "ok");
    }
    if (!out_field) {
        if (report) {
            report->status = PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT;
            snprintf(report->field, sizeof(report->field), "out_field");
            snprintf(report->message, sizeof(report->message),
                     "legacy field output is required");
        }
        return false;
    }
    if (!ProceduralSurfaceFieldGraphV1_Evaluate(
            graph, point, sample_budget, &sample, &graph_report)) {
        if (report) {
            report->status =
                graph_report.status ==
                        PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET
                    ? PROCEDURAL_SURFACE_FIELD_STATUS_BUDGET
                    : PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT;
            snprintf(report->field, sizeof(report->field), "%s",
                     graph_report.field);
            snprintf(report->message, sizeof(report->message), "%s",
                     graph_report.message);
        }
        return false;
    }
    memset(&field, 0, sizeof(field));
    field.height = sample.height;
    field.macro_variation = sample.macro_variation;
    field.micro_variation = sample.micro_variation;
    field.rock_mask = sample.mask;
    field.roughness = sample.roughness;
    field.snow_precursor = sample.mask;
    *out_field = field;
    return true;
}
