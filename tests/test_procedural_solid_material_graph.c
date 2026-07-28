#include "procedural/procedural_solid_material_graph.h"
#include "procedural/procedural_solid_material_runtime_program.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_snow_composition(void) {
    ProceduralSolidMaterialGraphV1 graph;
    ProceduralSolidMaterialGraphReport report;
    ProceduralSolidAuthoredMaterialV1 materials[2];
    ProceduralSolidAuthoredMaterialReport material_report;
    ProceduralSolidMaterialGeometryInputs input = {0};
    ProceduralSolidAuthoredMaterialSurfaceV1 low, high, underside;
    const char *binding_digest =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    assert(ProceduralSolidMaterialGraphV1_FromTemplate(
        "snow_accumulation", "snow_graph", "authored_binding",
        binding_digest, &graph, &report));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate(
        "weathered_rock", "base_material", &materials[0], &material_report));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate(
        "snow", "snow_material", &materials[1], &material_report));
    input.height = 0.2;
    input.slope = 1.0;
    assert(ProceduralSolidMaterialGraphV1_Evaluate(
        &graph, &input, materials, 2u, &low, &report));
    input.height = 0.95;
    assert(ProceduralSolidMaterialGraphV1_Evaluate(
        &graph, &input, materials, 2u, &high, &report));
    input.slope = 0.0;
    assert(ProceduralSolidMaterialGraphV1_Evaluate(
        &graph, &input, materials, 2u, &underside, &report));
    assert(high.base_color_b > low.base_color_b + 0.4);
    assert(fabs(underside.base_color_b - low.base_color_b) < 1e-9);
}

static void test_typed_edit_connect_and_cycle_rejection(void) {
    ProceduralSolidMaterialGraphV1 graph, edited;
    ProceduralSolidMaterialGraphReport report;
    char digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    const char *binding_digest =
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    assert(ProceduralSolidMaterialGraphV1_FromTemplate(
        "concrete_pores", "pores", "binding", binding_digest,
        &graph, &report));
    assert(ProceduralSolidMaterialGraphV1_Digest(&graph, digest, &report));
    assert(ProceduralSolidMaterialGraphV1_SetParameter(
        &graph, digest, "pore_noise", "scale", "48",
        &edited, &report));
    assert(edited.nodes[1].scale == 48.0);
    assert(!ProceduralSolidMaterialGraphV1_SetParameter(
        &graph, "stale", "pore_noise", "scale", "20",
        &edited, &report));
    assert(report.status ==
           PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_STALE_BASE);
    assert(ProceduralSolidMaterialGraphV1_Digest(&graph, digest, &report));
    assert(!ProceduralSolidMaterialGraphV1_Connect(
        &graph, digest, "cavity_bias", "a", "cavity_bias",
        &edited, &report));
    assert(report.status == PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_CYCLE);
}

static void test_all_geometry_inputs(void) {
    CoreMeshAssetRuntimeVertex vertices[4] = {
        {{-1.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{ 1.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{ 0.0,  1.0, 1.0}, {0.0, 0.0, 1.0}},
        {{ 0.0,  0.0,-1.0}, {0.0, 0.0,-1.0}},
    };
    CoreMeshAssetRuntimeTriangle triangles[2] = {
        {0u, 1u, 2u, "retained.shell"},
        {1u, 0u, 3u, "cut.shell"},
    };
    CoreMeshAssetRuntimeDocument mesh = {0};
    ProceduralSolidMaterialGeometryInputs inputs[2];
    ProceduralSolidMaterialGraphReport report;
    const char *kinds[2] = {"retained", "cut"};
    mesh.vertex_count = 4u;
    mesh.vertices = vertices;
    mesh.triangle_count = 2u;
    mesh.triangles = triangles;
    mesh.contract.local_bounds.min.x = -1.0;
    mesh.contract.local_bounds.min.y = -1.0;
    mesh.contract.local_bounds.min.z = -1.0;
    mesh.contract.local_bounds.max.x = 1.0;
    mesh.contract.local_bounds.max.y = 1.0;
    mesh.contract.local_bounds.max.z = 1.0;
    assert(ProceduralSolidMaterialGeometryInputs_Build(
        &mesh, kinds, inputs, 2u, &report));
    assert(inputs[0].height > inputs[1].height);
    assert(inputs[0].slope > 0.0);
    assert(inputs[1].slope == 0.0);
    assert(inputs[0].region_retained == 1.0);
    assert(inputs[1].region_cut == 1.0);
    assert(isfinite(inputs[0].curvature));
    assert(isfinite(inputs[0].cavity));
    assert(isfinite(inputs[0].boundary_distance));
}

static void test_continuous_hit_program(void) {
    CoreMeshAssetRuntimeVertex vertices[4] = {
        {{-1.0, -1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{ 1.0, -1.0, 0.4}, {0.0, 0.0, 1.0}},
        {{ 1.0,  1.0, 1.0}, {0.0, 0.0, 1.0}},
        {{-1.0,  1.0, 0.6}, {0.0, 0.0, 1.0}},
    };
    CoreMeshAssetRuntimeVertex original_vertices[4];
    CoreMeshAssetRuntimeTriangle triangles[2] = {
        {0u, 1u, 2u, "retained.shell"},
        {0u, 2u, 3u, "retained.shell"},
    };
    CoreMeshAssetRuntimeTriangle original_triangles[2];
    CoreMeshAssetRuntimeDocument mesh = {0};
    ProceduralSolidMaterialGraphV1 graph;
    ProceduralSolidMaterialGraphReport report = {0};
    ProceduralSolidAuthoredMaterialV1 materials[2];
    ProceduralSolidAuthoredMaterialReport material_report;
    ProceduralSolidMaterialRuntimeProgramV1 program;
    ProceduralSolidMaterialRuntimeSampleV1 left;
    ProceduralSolidMaterialRuntimeSampleV1 right;
    ProceduralSolidMaterialRuntimeSampleV1 low;
    ProceduralSolidMaterialRuntimeSampleV1 high;
    ProceduralSolidMaterialRuntimeSampleV1 weight_49;
    ProceduralSolidMaterialRuntimeSampleV1 weight_50;
    ProceduralSolidMaterialRuntimeSampleV1 weight_51;
    const char *kinds[2] = {"retained", "retained"};
    const char *binding_digest =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    memcpy(original_vertices, vertices, sizeof(vertices));
    memcpy(original_triangles, triangles, sizeof(triangles));
    mesh.vertex_count = 4u;
    mesh.vertex_normal_count = 4u;
    mesh.vertices = vertices;
    mesh.triangle_count = 2u;
    mesh.triangles = triangles;
    mesh.contract.local_bounds.min.x = -1.0;
    mesh.contract.local_bounds.min.y = -1.0;
    mesh.contract.local_bounds.min.z = 0.0;
    mesh.contract.local_bounds.max.x = 1.0;
    mesh.contract.local_bounds.max.y = 1.0;
    mesh.contract.local_bounds.max.z = 1.0;
    assert(ProceduralSolidMaterialGraphV1_FromTemplate(
        "snow_accumulation", "continuous_snow", "authored_binding",
        binding_digest, &graph, &report));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate(
        "weathered_rock", "base_material", &materials[0],
        &material_report));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate(
        "snow", "snow_material", &materials[1], &material_report));
    ProceduralSolidMaterialRuntimeProgramV1_Init(&program);
    assert(ProceduralSolidMaterialRuntimeProgramV1_Build(
        &graph, materials, 2u, &mesh, kinds, &program, &report));
    assert(memcmp(vertices, original_vertices, sizeof(vertices)) == 0);
    assert(memcmp(triangles, original_triangles, sizeof(triangles)) == 0);

    /* The same point on the shared 0->2 edge must agree from both faces. */
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.6, 0.0, 0.4, &left, &report));
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 1u, 0.6, 0.4, 0.0, &right, &report));
    assert(fabs(left.geometry.height - right.geometry.height) < 1e-12);
    assert(fabs(left.geometry.slope - right.geometry.slope) < 1e-12);
    assert(fabs(left.primary_layer_weight -
                right.primary_layer_weight) < 1e-12);
    assert(fabs(left.surface.base_color_r -
                right.surface.base_color_r) < 1e-12);

    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.8, 0.0, 0.2, &low, &report));
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.2, 0.0, 0.8, &high, &report));
    assert(high.geometry.height > low.geometry.height);
    assert(high.primary_layer_weight > low.primary_layer_weight);
    assert(high.surface.base_color_b > low.surface.base_color_b);

    snprintf(
        program.graph.layers[1].weight_node_id,
        sizeof(program.graph.layers[1].weight_node_id), "height");
    program.graph.nodes[1].kind = PROCEDURAL_SOLID_MATERIAL_NODE_CONSTANT;
    program.graph.nodes[1].value = 0.49;
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.5, 0.0, 0.5, &weight_49, &report));
    program.graph.nodes[1].value = 0.50;
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.5, 0.0, 0.5, &weight_50, &report));
    program.graph.nodes[1].value = 0.51;
    assert(ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
        &program, 0u, 0.5, 0.0, 0.5, &weight_51, &report));
    assert(weight_49.texture_count == 2u);
    assert(weight_50.texture_count == 2u);
    assert(weight_51.texture_count == 2u);
    assert(strcmp(weight_49.textures[0].texture.kind, "stone") == 0);
    assert(strcmp(weight_49.textures[1].texture.kind, "fog") == 0);
    assert(fabs(weight_49.textures[1].weight - 0.49) < 1e-12);
    assert(fabs(weight_50.textures[1].weight - 0.50) < 1e-12);
    assert(fabs(weight_51.textures[1].weight - 0.51) < 1e-12);
    assert(!weight_49.surface.texture.enabled);
    assert(!weight_50.surface.texture.enabled);
    assert(!weight_51.surface.texture.enabled);
    assert(fabs(
        (weight_50.surface.base_color_b - weight_49.surface.base_color_b) -
        (weight_51.surface.base_color_b - weight_50.surface.base_color_b)) <
        1e-12);
    ProceduralSolidMaterialRuntimeProgramV1_Free(&program);
}

int main(void) {
    test_snow_composition();
    test_typed_edit_connect_and_cycle_rejection();
    test_all_geometry_inputs();
    test_continuous_hit_program();
    puts("procedural solid material graph tests passed");
    return 0;
}
