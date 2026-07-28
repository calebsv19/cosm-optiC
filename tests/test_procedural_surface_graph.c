#include "procedural/procedural_surface_derived_asset.h"
#include "procedural/procedural_surface_field_3d.h"
#include "procedural/procedural_surface_graph.h"
#include "procedural/procedural_surface_material.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIXTURE_ROOT
#define PROCEDURAL_SURFACE_FIXTURE_ROOT \
    "tests/fixtures/procedural_surface_rock_prism_psg0"
#endif

#define SAMPLE_COUNT 11u
#define EXPECTED_GRAPH_DIGEST \
    "05ab6a7f6fcb5d2bbc039b6dbf280e8a26f5ec0813cebdad18d207bf9f317ae5"

static const char *const sample_ids[SAMPLE_COUNT] = {
    "origin", "plane_interior_a", "plane_interior_b", "plane_edge_lock",
    "prism_pos_x", "prism_neg_x", "prism_pos_y", "prism_neg_y",
    "prism_pos_z", "prism_neg_z", "shared_corner"};

static const ProceduralSurfaceFieldPoint3D sample_points[SAMPLE_COUNT] = {
    {0.0, 0.0, 0.0},
    {-1.25, -0.75, 0.0},
    {0.75, 1.0, 0.0},
    {1.875, 0.0, 0.0},
    {2.0, 0.0, 0.0},
    {-2.0, 0.0, 0.0},
    {0.0, 1.5, 0.0},
    {0.0, -1.5, 0.0},
    {0.0, 0.0, 1.0},
    {0.0, 0.0, -1.0},
    {2.0, 1.5, 1.0},
};

typedef struct DownstreamIdentity {
    char field_digest[PROCEDURAL_SURFACE_FIELD_DIGEST_CAPACITY];
    char shell_digest[PROCEDURAL_SURFACE_PRISM_MESH_DIGEST_CAPACITY];
    char material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY];
    char cache_identity[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
} DownstreamIdentity;

static int find_node(ProceduralSurfaceGraphV1 *graph, const char *id) {
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].id, id) == 0) return (int)i;
    }
    return -1;
}

static int find_link_to(
    ProceduralSurfaceGraphV1 *graph,
    const char *node,
    const char *socket) {
    for (size_t i = 0u; i < graph->link_count; ++i) {
        if (strcmp(graph->links[i].to_node, node) == 0 &&
            strcmp(graph->links[i].to_socket, socket) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void append_link(
    ProceduralSurfaceGraphV1 *graph,
    const char *from,
    const char *to,
    const char *socket) {
    ProceduralSurfaceGraphLink *link = &graph->links[graph->link_count++];
    snprintf(link->from_node, sizeof(link->from_node), "%s", from);
    snprintf(link->from_socket, sizeof(link->from_socket), "value");
    snprintf(link->to_node, sizeof(link->to_node), "%s", to);
    snprintf(link->to_socket, sizeof(link->to_socket), "%s", socket);
}

static DownstreamIdentity downstream_identity(
    const ProceduralSurfaceRecipeV1 *recipe) {
    DownstreamIdentity identity = {0};
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceFieldReport field_report;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceMaterialReport material_report;
    ProceduralSurfaceDerivedAssetReport derived_report;
    ProceduralSurfaceFieldOutput outputs[SAMPLE_COUNT];
    ProceduralSurfaceFieldBudget sample_budget;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceFieldBudget mesh_budget;
    ProceduralSurfacePrismVertex *vertices = NULL;
    ProceduralSurfacePrismTriangle *triangles = NULL;
    ProceduralSurfaceMaterialSample *materials = NULL;
    char (*material_id_storage)[32] = NULL;
    const char **material_ids = NULL;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char cage_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    ProceduralSurfaceCageContract cage = {
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 4.0,
        .height_units = 3.0,
        .depth_units = 2.0,
        .target_edge_length_units = recipe->target_edge_length_units};

    assert(ProceduralSurfaceRecipeV1_Digest(
        recipe, recipe_digest, &recipe_report));
    ProceduralSurfaceFieldBudget_Init(recipe, &sample_budget);
    for (size_t i = 0u; i < SAMPLE_COUNT; ++i) {
        assert(ProceduralSurfaceField3D_Evaluate(
            recipe, sample_points[i], &sample_budget, &outputs[i],
            &field_report));
    }
    assert(ProceduralSurfaceField3D_SummaryDigest(
        recipe_digest, sample_ids, outputs, SAMPLE_COUNT,
        identity.field_digest, &field_report));
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        &cage, recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &requirements, &mesh_report));
    vertices = calloc((size_t)requirements.vertex_count, sizeof(*vertices));
    triangles = calloc((size_t)requirements.triangle_count, sizeof(*triangles));
    materials = calloc((size_t)requirements.vertex_count, sizeof(*materials));
    material_id_storage =
        calloc((size_t)requirements.vertex_count, sizeof(*material_id_storage));
    material_ids =
        calloc((size_t)requirements.vertex_count, sizeof(*material_ids));
    assert(vertices && triangles && materials &&
           material_id_storage && material_ids);
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    mesh_budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = recipe->quality.max_field_evaluations};
    assert(ProceduralSurfacePrismMesh_Generate(
        &cage, recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &mesh_budget, &buffers, &summary, &mesh_report));
    snprintf(identity.shell_digest, sizeof(identity.shell_digest), "%s",
             summary.mesh_digest_sha256);
    for (size_t i = 0u; i < buffers.vertex_count; ++i) {
        snprintf(material_id_storage[i], sizeof(material_id_storage[i]),
                 "vertex_%04zu", i);
        material_ids[i] = material_id_storage[i];
        assert(ProceduralSurfaceMaterial_Evaluate(
            recipe, &vertices[i].field, vertices[i].position,
            vertices[i].normal, &materials[i], &material_report));
    }
    assert(ProceduralSurfaceMaterial_SummaryDigest(
        recipe_digest, identity.shell_digest, material_ids, materials,
        buffers.vertex_count, identity.material_digest, &material_report));
    assert(ProceduralSurfaceDerivedAsset_CageDigest(
        cage.kind, cage.width_units, cage.height_units, cage.depth_units,
        cage_digest, &derived_report));
    assert(ProceduralSurfaceDerivedAsset_CacheIdentity(
        recipe_digest, cage_digest, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        identity.shell_digest, identity.material_digest,
        identity.cache_identity, &derived_report));
    free(material_ids);
    free(material_id_storage);
    free(materials);
    free(triangles);
    free(vertices);
    return identity;
}

static void test_golden_compile_and_identity(void) {
    ProceduralSurfaceGraphV1 graph;
    ProceduralSurfaceRecipeV1 compiled;
    ProceduralSurfaceRecipeV1 fixture_recipe;
    ProceduralSurfaceGraphCompilePlan plan;
    ProceduralSurfaceGraphReport report;
    ProceduralSurfaceRecipeReport recipe_report;
    DownstreamIdentity identity;
    char compiled_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char fixture_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char plan_json[2048];

    assert(ProceduralSurfaceGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/graph.json", &graph, &report));
    assert(ProceduralSurfaceGraphV1_CompileRecipe(
        &graph, &compiled, &plan, &report));
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &fixture_recipe, &recipe_report));
    assert(ProceduralSurfaceRecipeV1_Digest(
        &compiled, compiled_digest, &recipe_report));
    assert(ProceduralSurfaceRecipeV1_Digest(
        &fixture_recipe, fixture_digest, &recipe_report));
    assert(strcmp(compiled_digest, fixture_digest) == 0);
    assert(strcmp(compiled_digest,
                  "563d838258da20c7c9a106323470fedc642558d90154ce62f25f2a006fe99525") ==
           0);
    assert(plan.node_count == 21u && plan.link_count == 20u);
    assert(plan.evaluated_node_count == 21u);
    assert(plan.field_ir_output && plan.geometry_output &&
           plan.material_output);
    assert(ProceduralSurfaceGraphCompilePlan_CanonicalJson(
        &plan, plan_json, sizeof(plan_json), &report));
    assert(strstr(plan_json, "\"output_domains\":[\"field_ir\",\"geometry\",\"material\"]"));
    identity = downstream_identity(&compiled);
    assert(strcmp(identity.field_digest,
                  "ed2d2bf6d1e8939ab4a99ca9d3872316c1d3081f173572da68fcdd352653065d") ==
           0);
    assert(strcmp(identity.shell_digest,
                  "f6fd32de40f0e0ceccfde8d70678cdd076acaba23d5b9510b69a23702e9f7a1f") ==
           0);
    assert(strcmp(identity.material_digest,
                  "694ca67f570cb52c7b5009b24922914c415ea50830c624884f79ba94b96583bd") ==
           0);
    assert(strcmp(identity.cache_identity,
                  "60e315e7db7616692572d6f664500893c97c9c8e1ec963844125c28cb238f470") ==
           0);
    printf("PSG-6 graph digest observed: %s\n", plan.graph_digest_sha256);
    assert(strcmp(plan.graph_digest_sha256, EXPECTED_GRAPH_DIGEST) == 0);
}

static void test_canonical_order_independence(void) {
    ProceduralSurfaceGraphV1 graph;
    ProceduralSurfaceGraphV1 reversed;
    ProceduralSurfaceGraphReport report;
    char first[PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY];
    char second[PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY];
    assert(ProceduralSurfaceGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/graph.json", &graph, &report));
    reversed = graph;
    for (size_t i = 0u; i < graph.node_count; ++i) {
        reversed.nodes[i] = graph.nodes[graph.node_count - i - 1u];
    }
    for (size_t i = 0u; i < graph.link_count; ++i) {
        reversed.links[i] = graph.links[graph.link_count - i - 1u];
    }
    assert(ProceduralSurfaceGraphV1_Digest(&graph, first, &report));
    assert(ProceduralSurfaceGraphV1_Digest(&reversed, second, &report));
    assert(strcmp(first, second) == 0);
}

static void test_editable_math_node_equivalence(void) {
    ProceduralSurfaceGraphV1 graph;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceGraphCompilePlan plan;
    ProceduralSurfaceGraphReport report;
    ProceduralSurfaceRecipeReport recipe_report;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    int output_link;
    ProceduralSurfaceGraphNode *zero;
    ProceduralSurfaceGraphNode *sum;

    assert(ProceduralSurfaceGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/graph.json", &graph, &report));
    output_link = find_link_to(
        &graph, "surface_output", "displacement_amplitude_units");
    assert(output_link >= 0);
    zero = &graph.nodes[graph.node_count++];
    snprintf(zero->id, sizeof(zero->id), "zero_adjustment");
    zero->kind = PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT;
    zero->constant.type = PROCEDURAL_SURFACE_GRAPH_VALUE_F64;
    zero->constant.f64 = 0.0;
    sum = &graph.nodes[graph.node_count++];
    snprintf(sum->id, sizeof(sum->id), "displacement_sum");
    sum->kind = PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD;
    snprintf(graph.links[output_link].from_node,
             sizeof(graph.links[output_link].from_node),
             "displacement_sum");
    append_link(
        &graph, "displacement_amplitude", "displacement_sum", "a");
    append_link(&graph, "zero_adjustment", "displacement_sum", "b");
    assert(ProceduralSurfaceGraphV1_CompileRecipe(
        &graph, &recipe, &plan, &report));
    assert(plan.node_count == 23u && plan.link_count == 22u);
    assert(plan.evaluated_node_count == 23u);
    assert(ProceduralSurfaceRecipeV1_Digest(
        &recipe, recipe_digest, &recipe_report));
    assert(strcmp(recipe_digest,
                  "563d838258da20c7c9a106323470fedc642558d90154ce62f25f2a006fe99525") ==
           0);
}

static void test_cycle_type_budget_and_transactionality(void) {
    ProceduralSurfaceGraphV1 graph;
    ProceduralSurfaceGraphV1 invalid;
    ProceduralSurfaceGraphReport report;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeV1 recipe_sentinel;
    ProceduralSurfaceGraphCompilePlan plan;
    ProceduralSurfaceGraphCompilePlan plan_sentinel;
    ProceduralSurfaceGraphV1 graph_sentinel;
    int base;
    int micro;
    int base_link;

    assert(ProceduralSurfaceGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/graph.json", &graph, &report));
    memset(&recipe, 0x5a, sizeof(recipe));
    memset(&plan, 0x6b, sizeof(plan));
    recipe_sentinel = recipe;
    plan_sentinel = plan;

    invalid = graph;
    invalid.max_node_evaluations = (uint32_t)invalid.node_count - 1u;
    assert(!ProceduralSurfaceGraphV1_CompileRecipe(
        &invalid, &recipe, &plan, &report));
    assert(report.status == PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET);
    assert(memcmp(&recipe, &recipe_sentinel, sizeof(recipe)) == 0);
    assert(memcmp(&plan, &plan_sentinel, sizeof(plan)) == 0);

    invalid = graph;
    base_link = find_link_to(
        &invalid, "surface_output", "base_feature_size_units");
    assert(base_link >= 0);
    snprintf(invalid.links[base_link].from_node,
             sizeof(invalid.links[base_link].from_node), "octave_count");
    assert(!ProceduralSurfaceGraphV1_Validate(&invalid, &report));
    assert(report.status == PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE);

    invalid = graph;
    base = find_node(&invalid, "base_feature_size");
    micro = find_node(&invalid, "micro_feature_size");
    assert(base >= 0 && micro >= 0);
    invalid.nodes[base].kind = PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD;
    invalid.nodes[micro].kind = PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD;
    memset(&invalid.nodes[base].constant, 0,
           sizeof(invalid.nodes[base].constant));
    memset(&invalid.nodes[micro].constant, 0,
           sizeof(invalid.nodes[micro].constant));
    append_link(&invalid, "micro_feature_size", "base_feature_size", "a");
    append_link(&invalid, "lacunarity", "base_feature_size", "b");
    append_link(&invalid, "base_feature_size", "micro_feature_size", "a");
    append_link(&invalid, "persistence", "micro_feature_size", "b");
    assert(!ProceduralSurfaceGraphV1_Validate(&invalid, &report));
    assert(report.status == PROCEDURAL_SURFACE_GRAPH_STATUS_CYCLE);

    graph_sentinel = invalid;
    assert(!ProceduralSurfaceGraphV1_LoadJsonFile(
        "/tmp/procedural-surface-graph-does-not-exist.json",
        &invalid, &report));
    assert(memcmp(&invalid, &graph_sentinel, sizeof(invalid)) == 0);
}

int main(void) {
    test_golden_compile_and_identity();
    test_canonical_order_independence();
    test_editable_math_node_equivalence();
    test_cycle_type_budget_and_transactionality();
    printf("procedural surface PSG-6 graph/compiler contract passed\n");
    return 0;
}
