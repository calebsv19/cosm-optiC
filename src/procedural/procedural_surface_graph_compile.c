#include "procedural/procedural_surface_graph_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct ProceduralSurfaceGraphEvaluation {
    unsigned char state[PROCEDURAL_SURFACE_GRAPH_MAX_NODES];
    ProceduralSurfaceGraphValue values[PROCEDURAL_SURFACE_GRAPH_MAX_NODES];
    uint32_t evaluated_count;
} ProceduralSurfaceGraphEvaluation;

static const ProceduralSurfaceGraphLink *graph_incoming_link(
    const ProceduralSurfaceGraphV1 *graph,
    const char *node_id,
    const char *socket) {
    if (!graph || !node_id || !socket) return NULL;
    for (size_t i = 0u; i < graph->link_count; ++i) {
        if (strcmp(graph->links[i].to_node, node_id) == 0 &&
            strcmp(graph->links[i].to_socket, socket) == 0) {
            return &graph->links[i];
        }
    }
    return NULL;
}

static bool graph_evaluate_node(
    const ProceduralSurfaceGraphV1 *graph,
    int node_index,
    ProceduralSurfaceGraphEvaluation *evaluation,
    ProceduralSurfaceGraphValue *out_value,
    ProceduralSurfaceGraphReport *report) {
    const ProceduralSurfaceGraphNode *node;
    if (!graph || node_index < 0 || !evaluation || !out_value) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
            "evaluation", "graph evaluation input is invalid");
    }
    if (evaluation->state[node_index] == 2u) {
        *out_value = evaluation->values[node_index];
        return true;
    }
    if (evaluation->state[node_index] == 1u) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CYCLE,
            graph->nodes[node_index].id,
            "graph evaluation encountered a cycle");
    }
    if (evaluation->evaluated_count >= graph->max_node_evaluations) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET,
            "max_node_evaluations",
            "graph evaluation exceeded its bounded budget");
    }
    node = &graph->nodes[node_index];
    evaluation->state[node_index] = 1u;
    evaluation->evaluated_count += 1u;
    if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT) {
        evaluation->values[node_index] = node->constant;
    } else if (node->kind == PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD) {
        const ProceduralSurfaceGraphLink *a_link =
            graph_incoming_link(graph, node->id, "a");
        const ProceduralSurfaceGraphLink *b_link =
            graph_incoming_link(graph, node->id, "b");
        ProceduralSurfaceGraphValue a;
        ProceduralSurfaceGraphValue b;
        int a_index = a_link ?
            procedural_surface_graph_find_node(graph, a_link->from_node) : -1;
        int b_index = b_link ?
            procedural_surface_graph_find_node(graph, b_link->from_node) : -1;
        if (a_index < 0 || b_index < 0 ||
            !graph_evaluate_node(
                graph, a_index, evaluation, &a, report) ||
            !graph_evaluate_node(
                graph, b_index, evaluation, &b, report)) {
            evaluation->state[node_index] = 0u;
            return false;
        }
        evaluation->values[node_index].type =
            PROCEDURAL_SURFACE_GRAPH_VALUE_F64;
        evaluation->values[node_index].f64 = a.f64 + b.f64;
    } else {
        evaluation->state[node_index] = 0u;
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NODE, node->id,
            "node does not evaluate to a scalar recipe value");
    }
    evaluation->state[node_index] = 2u;
    *out_value = evaluation->values[node_index];
    return true;
}

static bool graph_resolve_recipe_input(
    const ProceduralSurfaceGraphV1 *graph,
    const ProceduralSurfaceGraphNode *output_node,
    const ProceduralSurfaceGraphRecipeInputSpec *input,
    ProceduralSurfaceGraphEvaluation *evaluation,
    ProceduralSurfaceGraphValue *out_value,
    ProceduralSurfaceGraphReport *report) {
    const ProceduralSurfaceGraphLink *link =
        graph_incoming_link(graph, output_node->id, input->socket);
    int source = link ?
        procedural_surface_graph_find_node(graph, link->from_node) : -1;
    if (source < 0 ||
        !graph_evaluate_node(
            graph, source, evaluation, out_value, report)) {
        return false;
    }
    if (out_value->type != input->type) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE, input->socket,
            "evaluated value type differs from recipe input type");
    }
    return true;
}

static bool graph_assign_recipe_value(
    ProceduralSurfaceRecipeV1 *recipe,
    const char *socket,
    const ProceduralSurfaceGraphValue *value) {
    if (strcmp(socket, "recipe_id") == 0) {
        return snprintf(recipe->recipe_id, sizeof(recipe->recipe_id), "%s",
                        value->string_value) < (int)sizeof(recipe->recipe_id);
    }
    if (strcmp(socket, "seed") == 0) recipe->seed = value->u64;
    else if (strcmp(socket, "coordinate_space") == 0) {
        recipe->coordinate_space = PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT;
    } else if (strcmp(socket, "base_feature_size_units") == 0) {
        recipe->base_feature_size_units = value->f64;
    } else if (strcmp(socket, "micro_feature_size_units") == 0) {
        recipe->micro_feature_size_units = value->f64;
    } else if (strcmp(socket, "octave_count") == 0) {
        recipe->octave_count = value->u32;
    } else if (strcmp(socket, "lacunarity") == 0) {
        recipe->lacunarity = value->f64;
    } else if (strcmp(socket, "persistence") == 0) {
        recipe->persistence = value->f64;
    } else if (strcmp(socket, "ridge_valley_blend") == 0) {
        recipe->ridge_valley_blend = value->f64;
    } else if (strcmp(socket, "macro_micro_mix") == 0) {
        recipe->macro_micro_mix = value->f64;
    } else if (strcmp(socket, "target_edge_length_units") == 0) {
        recipe->target_edge_length_units = value->f64;
    } else if (strcmp(socket, "displacement_amplitude_units") == 0) {
        recipe->displacement_amplitude_units = value->f64;
    } else if (strcmp(socket, "edge_lock_width_units") == 0) {
        recipe->edge_lock_width_units = value->f64;
    } else if (strcmp(socket, "output_clamp") == 0) {
        recipe->output_clamp = PROCEDURAL_SURFACE_OUTPUT_CLAMP_SIGNED_UNIT;
    } else if (strcmp(socket, "snow_elevation_threshold_units") == 0) {
        recipe->snow_elevation_threshold_units = value->f64;
    } else if (strcmp(socket, "snow_slope_threshold") == 0) {
        recipe->snow_slope_threshold = value->f64;
    } else if (strcmp(socket, "preview_max_triangles") == 0) {
        recipe->quality.preview_max_triangles = value->u32;
    } else if (strcmp(socket, "inspection_max_triangles") == 0) {
        recipe->quality.inspection_max_triangles = value->u32;
    } else if (strcmp(socket, "final_max_triangles") == 0) {
        recipe->quality.final_max_triangles = value->u32;
    } else if (strcmp(socket, "max_field_evaluations") == 0) {
        recipe->quality.max_field_evaluations = value->u32;
    } else {
        return false;
    }
    return true;
}

bool ProceduralSurfaceGraphV1_CompileRecipe(
    const ProceduralSurfaceGraphV1 *graph,
    ProceduralSurfaceRecipeV1 *out_recipe,
    ProceduralSurfaceGraphCompilePlan *out_plan,
    ProceduralSurfaceGraphReport *report) {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceGraphCompilePlan plan;
    ProceduralSurfaceGraphEvaluation evaluation;
    ProceduralSurfaceRecipeReport recipe_report;
    const ProceduralSurfaceGraphNode *output_node = NULL;
    if (!graph || !out_recipe || !out_plan) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
            "compile", "graph, recipe output, and plan output are required");
    }
    if (!ProceduralSurfaceGraphV1_Validate(graph, report)) return false;
    memset(&recipe, 0, sizeof(recipe));
    memset(&plan, 0, sizeof(plan));
    memset(&evaluation, 0, sizeof(evaluation));
    recipe.schema_version = PROCEDURAL_SURFACE_RECIPE_SCHEMA_VERSION;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (graph->nodes[i].kind ==
            PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT) {
            output_node = &graph->nodes[i];
            break;
        }
    }
    if (!output_node) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NODE, "nodes",
            "recipe output node is missing");
    }
    for (size_t i = 0u;
         i < g_procedural_surface_graph_recipe_input_count; ++i) {
        ProceduralSurfaceGraphValue value;
        if (!graph_resolve_recipe_input(
                graph, output_node,
                &g_procedural_surface_graph_recipe_inputs[i],
                &evaluation, &value, report) ||
            !graph_assign_recipe_value(
                &recipe,
                g_procedural_surface_graph_recipe_inputs[i].socket,
                &value)) {
            if (report && report->status ==
                              PROCEDURAL_SURFACE_GRAPH_STATUS_OK) {
                return procedural_surface_graph_fail(
                    report, PROCEDURAL_SURFACE_GRAPH_STATUS_RECIPE,
                    g_procedural_surface_graph_recipe_inputs[i].socket,
                    "unable to assign compiled recipe input");
            }
            return false;
        }
    }
    evaluation.evaluated_count += 1u;
    if (evaluation.evaluated_count > graph->max_node_evaluations) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET,
            "max_node_evaluations",
            "compiled graph exceeded its evaluation budget");
    }
    if (!ProceduralSurfaceRecipeV1_Validate(&recipe, &recipe_report)) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_RECIPE,
            recipe_report.field, recipe_report.message);
    }
    plan.valid = true;
    snprintf(plan.graph_id, sizeof(plan.graph_id), "%s", graph->graph_id);
    snprintf(plan.output_node_id, sizeof(plan.output_node_id), "%s",
             output_node->id);
    plan.node_count = (uint32_t)graph->node_count;
    plan.link_count = (uint32_t)graph->link_count;
    plan.evaluated_node_count = evaluation.evaluated_count;
    plan.max_node_evaluations = graph->max_node_evaluations;
    plan.field_ir_output = true;
    plan.geometry_output = true;
    plan.material_output = true;
    if (!ProceduralSurfaceGraphV1_Digest(
            graph, plan.graph_digest_sha256, report) ||
        !ProceduralSurfaceRecipeV1_Digest(
            &recipe, plan.recipe_digest_sha256, &recipe_report)) {
        if (report && report->status ==
                          PROCEDURAL_SURFACE_GRAPH_STATUS_OK) {
            return procedural_surface_graph_fail(
                report, PROCEDURAL_SURFACE_GRAPH_STATUS_RECIPE,
                recipe_report.field, recipe_report.message);
        }
        return false;
    }
    *out_recipe = recipe;
    *out_plan = plan;
    if (report) {
        memset(report, 0, sizeof(*report));
        report->status = PROCEDURAL_SURFACE_GRAPH_STATUS_OK;
        snprintf(report->message, sizeof(report->message), "ok");
    }
    return true;
}

bool ProceduralSurfaceGraphCompilePlan_CanonicalJson(
    const ProceduralSurfaceGraphCompilePlan *plan,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceGraphReport *report) {
    int written;
    if (!plan || !out_json || out_capacity == 0u) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
            "compile_plan", "compile plan and output buffer are required");
    }
    out_json[0] = '\0';
    if (!plan->valid || !procedural_surface_graph_id_valid(plan->graph_id) ||
        !procedural_surface_graph_id_valid(plan->output_node_id) ||
        plan->node_count == 0u ||
        plan->evaluated_node_count > plan->max_node_evaluations ||
        !plan->field_ir_output || !plan->geometry_output ||
        !plan->material_output) {
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION,
            "compile_plan", "compile plan is incomplete");
    }
    written = snprintf(
        out_json, out_capacity,
        "{\"schema\":\"ray_tracing.procedural_surface_graph_compile_plan\","
        "\"schema_version\":1,\"graph_id\":\"%s\","
        "\"output_node_id\":\"%s\",\"graph_digest_sha256\":\"%s\","
        "\"recipe_digest_sha256\":\"%s\",\"node_count\":%u,"
        "\"link_count\":%u,\"evaluated_node_count\":%u,"
        "\"max_node_evaluations\":%u,"
        "\"output_domains\":[\"field_ir\",\"geometry\",\"material\"]}",
        plan->graph_id, plan->output_node_id, plan->graph_digest_sha256,
        plan->recipe_digest_sha256, plan->node_count, plan->link_count,
        plan->evaluated_node_count, plan->max_node_evaluations);
    if (written < 0 || (size_t)written >= out_capacity) {
        out_json[0] = '\0';
        return procedural_surface_graph_fail(
            report, PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION,
            "compile_plan", "compile plan exceeds output capacity");
    }
    if (report) {
        memset(report, 0, sizeof(*report));
        report->status = PROCEDURAL_SURFACE_GRAPH_STATUS_OK;
        snprintf(report->message, sizeof(report->message), "ok");
    }
    return true;
}
