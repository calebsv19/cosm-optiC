#include "procedural/procedural_surface_binding.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIELD_PRESET_ROOT
#define PROCEDURAL_SURFACE_FIELD_PRESET_ROOT \
    "tests/fixtures/procedural_surface_field_presets"
#endif

static void fixture_path(
    char *out,
    size_t capacity,
    const char *name) {
    snprintf(out, capacity, "%s/%s",
             PROCEDURAL_SURFACE_FIELD_PRESET_ROOT, name);
}

static void test_upward_binding(void) {
    char graph_path[1024];
    char binding_path[1024];
    char digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport report;
    ProceduralSurfaceFieldBudget budget = {.max_evaluations = 8u};
    ProceduralSurfaceBoundSample top;
    ProceduralSurfaceBoundSample side;
    const ProceduralSurfaceFieldPoint3D point = {0.17, -0.21, 0.6};
    fixture_path(graph_path, sizeof(graph_path), "wind_shaped_sand.json");
    fixture_path(binding_path, sizeof(binding_path),
                 "wind_shaped_sand.top.binding.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    assert(ProceduralSurfaceBindingV1_LoadJsonFile(
        binding_path, &binding, &report));
    assert(ProceduralSurfaceBindingV1_Validate(
        &binding, &graph, &report));
    assert(ProceduralSurfaceBindingV1_Digest(
        &binding, digest, &report));
    assert(ProceduralSurfaceBinding_Evaluate(
        &binding, &graph, point,
        (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0},
        "positive_z", &budget, &top, &report));
    assert(ProceduralSurfaceBinding_Evaluate(
        &binding, &graph, point,
        (ProceduralSurfaceFieldPoint3D){1.0, 0.0, 0.0},
        "positive_x", &budget, &side, &report));
    assert(top.application_weight == 1.0);
    assert(side.application_weight == 0.0);
    assert(side.graph_sample.height == 0.0);
    assert(side.graph_sample.color_r == binding.fallback_color_r);
    assert(side.graph_sample.color_g == binding.fallback_color_g);
    assert(side.graph_sample.color_b == binding.fallback_color_b);
    assert(side.graph_sample.roughness == binding.fallback_roughness);
    assert(top.evaluation_point.z == 0.0);
    assert(budget.evaluations == 2u);
    printf("surface binding digest=%s top_height=%.9f side_height=%.9f\n",
           digest, top.graph_sample.height, side.graph_sample.height);
}

static void test_all_surface_binding(void) {
    char graph_path[1024];
    char binding_path[1024];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport report;
    ProceduralSurfaceFieldBudget budget = {.max_evaluations = 2u};
    ProceduralSurfaceBoundSample sample;
    fixture_path(graph_path, sizeof(graph_path), "pitted_concrete.json");
    fixture_path(binding_path, sizeof(binding_path),
                 "pitted_concrete.all.binding.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    assert(ProceduralSurfaceBindingV1_LoadJsonFile(
        binding_path, &binding, &report));
    assert(ProceduralSurfaceBinding_Evaluate(
        &binding, &graph,
        (ProceduralSurfaceFieldPoint3D){0.1, 0.2, 0.3},
        (ProceduralSurfaceFieldPoint3D){1.0, 0.0, 0.0},
        "positive_x", &budget, &sample, &report));
    assert(sample.application_weight == 1.0);
}

static void test_rejections_are_transactional(void) {
    char graph_path[1024];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport report;
    ProceduralSurfaceBoundSample output;
    ProceduralSurfaceFieldBudget budget = {.max_evaluations = 1u};
    fixture_path(graph_path, sizeof(graph_path), "wind_shaped_sand.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    ProceduralSurfaceBindingV1_Init(&binding);
    snprintf(binding.binding_id, sizeof(binding.binding_id), "bad_binding");
    snprintf(binding.graph_program_id, sizeof(binding.graph_program_id), "%s",
             graph.program_id);
    binding.projection_scale = 0.0;
    memset(&output, 0x5a, sizeof(output));
    {
        ProceduralSurfaceBoundSample before = output;
        assert(!ProceduralSurfaceBinding_Evaluate(
            &binding, &graph,
            (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 0.0},
            (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0},
            "", &budget, &output, &report));
        assert(report.status ==
               PROCEDURAL_SURFACE_BINDING_STATUS_PROJECTION);
        assert(memcmp(&output, &before, sizeof(output)) == 0);
        assert(budget.evaluations == 0u);
    }
}

int main(void) {
    test_upward_binding();
    test_all_surface_binding();
    test_rejections_are_transactional();
    return 0;
}
