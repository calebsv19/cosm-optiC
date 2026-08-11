#include "procedural/procedural_surface_feature_relief_shell.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIELD_PRESET_ROOT
#define PROCEDURAL_SURFACE_FIELD_PRESET_ROOT \
    "tests/fixtures/procedural_surface_field_presets"
#endif

#ifndef PROCEDURAL_SURFACE_FIXTURE_ROOT
#define PROCEDURAL_SURFACE_FIXTURE_ROOT \
    "tests/fixtures/procedural_surface_rock_prism_psg0"
#endif

typedef struct CompiledRelief {
    ProceduralSurfacePrismVertex *vertices;
    ProceduralSurfacePrismTriangle *triangles;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceFeatureReliefShellReceipt receipt;
} CompiledRelief;

static void binding_init(
    ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph) {
    ProceduralSurfaceBindingV1_Init(binding);
    snprintf(binding->binding_id, sizeof(binding->binding_id),
             "signed_relief_positive_y");
    snprintf(binding->graph_program_id, sizeof(binding->graph_program_id),
             "%s", graph->program_id);
    binding->selector = PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP;
    snprintf(binding->surface_group_id, sizeof(binding->surface_group_id),
             "positive_y");
    binding->projection = PROCEDURAL_SURFACE_PROJECTION_PLANAR_XZ;
    binding->displacement_direction =
        PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL;
}

static void root_init(
    ProceduralSurfaceFeatureRootV1 *root,
    uint32_t feature_id,
    double x,
    double z,
    double radius,
    double height_or_depth) {
    memset(root, 0, sizeof(*root));
    root->source_triangle = feature_id;
    root->barycentric[0] = 1.0 / 3.0;
    root->barycentric[1] = 1.0 / 3.0;
    root->barycentric[2] = 1.0 / 3.0;
    root->position = (ProceduralSurfaceFeatureVec3){x, 0.4, z};
    root->normal = (ProceduralSurfaceFeatureVec3){0.0, 1.0, 0.0};
    root->tangent = (ProceduralSurfaceFeatureVec3){1.0, 0.0, 0.0};
    root->bitangent = (ProceduralSurfaceFeatureVec3){0.0, 0.0, 1.0};
    root->radius = radius;
    root->aspect = 1.0;
    root->edge_softness = 0.14;
    root->rim_width = 0.22;
    root->height_or_depth = height_or_depth;
    root->population = height_or_depth < 0.0 ? 1u : 2u;
    root->feature_id = feature_id;
}

static CompiledRelief compile_relief(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFeatureFieldV1 *field,
    const char *expected_source_digest,
    const char *derived_id) {
    CompiledRelief compiled = {0};
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceFeatureReliefShellReport report;
    ProceduralSurfaceFieldBudget budget = {
        .max_evaluations = recipe->quality.max_field_evaluations};
    ProceduralSurfaceSelectedFaceShellRequest shell_request = {
        .source_asset_id = "semantic_wall_prism",
        .derived_asset_id = derived_id,
        .selected_face = PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Y,
        .cage = cage,
        .recipe = recipe,
        .graph = graph,
        .binding = binding,
        .quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL};
    ProceduralSurfaceFeatureReliefShellRequest request = {
        .selected_face_shell = shell_request,
        .feature_field = field,
        .expected_source_mesh_digest_sha256 = expected_source_digest,
        .relief_scale = 1.0};
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        cage, recipe, shell_request.quality, &compiled.requirements,
        &mesh_report));
    compiled.vertices = calloc(
        (size_t)compiled.requirements.vertex_count,
        sizeof(*compiled.vertices));
    compiled.triangles = calloc(
        (size_t)compiled.requirements.triangle_count,
        sizeof(*compiled.triangles));
    assert(compiled.vertices && compiled.triangles);
    compiled.buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = compiled.vertices,
        .vertex_capacity = (size_t)compiled.requirements.vertex_count,
        .triangles = compiled.triangles,
        .triangle_capacity = (size_t)compiled.requirements.triangle_count};
    assert(ProceduralSurfaceFeatureReliefShell_Compile(
        &request, &budget, &compiled.buffers, &compiled.requirements,
        &compiled.summary, &compiled.receipt, &report));
    return compiled;
}

static void compiled_free(CompiledRelief *compiled) {
    free(compiled->vertices);
    free(compiled->triangles);
    memset(compiled, 0, sizeof(*compiled));
}

int main(void) {
    static const char source_digest[] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFeatureFieldV1 field = {0};
    ProceduralSurfaceCageContract cage = {
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 4.0,
        .height_units = 0.8,
        .depth_units = 4.0,
        .target_edge_length_units = 0.1};
    CompiledRelief first;
    CompiledRelief repeat;

    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIELD_PRESET_ROOT "/pitted_concrete.json",
        &graph, &graph_report));
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &recipe, &recipe_report));
    snprintf(recipe.recipe_id, sizeof(recipe.recipe_id),
             "signed_spot_relief");
    recipe.target_edge_length_units = cage.target_edge_length_units;
    recipe.displacement_amplitude_units = 0.08;
    recipe.edge_lock_width_units = 0.18;
    assert(ProceduralSurfaceRecipeV1_Validate(&recipe, &recipe_report));
    binding_init(&binding, &graph);

    snprintf(field.source_mesh_digest_sha256,
             sizeof(field.source_mesh_digest_sha256), "%s", source_digest);
    snprintf(field.authoring_digest_sha256,
             sizeof(field.authoring_digest_sha256),
             "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    field.seed = 24018u;
    field.normal_compatibility_cosine = 0.8;
    field.feature_count = 3u;
    root_init(&field.features[0], 1001u, -0.75, 0.0, 0.55, -0.045);
    root_init(&field.features[1], 1002u, 0.75, 0.0, 0.50, 0.038);
    root_init(&field.features[2], 1003u, 0.0, 0.9, 0.25, 0.0);
    assert(ProceduralSurfaceFeatureFieldV1_Validate(&field));

    first = compile_relief(
        &cage, &recipe, &graph, &binding, &field, source_digest,
        "signed_relief_shell");
    repeat = compile_relief(
        &cage, &recipe, &graph, &binding, &field, source_digest,
        "signed_relief_shell");

    assert(first.receipt.schema_version == 1u);
    assert(first.receipt.feature_count == 3u);
    assert(first.receipt.negative_depth_feature_count == 1u);
    assert(first.receipt.positive_height_feature_count == 1u);
    assert(first.receipt.zero_height_feature_count == 1u);
    assert(first.receipt.negatively_displaced_vertex_count > 0u);
    assert(first.receipt.positively_displaced_vertex_count > 0u);
    assert(first.receipt.minimum_emitted_displacement_units < -0.03);
    assert(first.receipt.maximum_emitted_displacement_units > 0.025);
    assert(first.receipt.maximum_candidates_considered_per_vertex == 3u);
    assert(first.receipt.feature_source_identity_bound);
    assert(first.receipt.one_coherent_derived_shell);
    assert(first.receipt.selected_face_shell
               .maximum_unselected_face_absolute_displacement_units == 0.0);
    assert(first.summary.boundary_edge_count == 0u);
    assert(first.summary.connected_component_count == 1u);
    assert(first.summary.euler_characteristic == 2);
    assert(first.summary.maximum_edge_absolute_displacement_units == 0.0);
    assert(first.summary.signed_volume_units3 > 0.0);
    assert(strcmp(first.summary.mesh_digest_sha256,
                  repeat.summary.mesh_digest_sha256) == 0);
    assert(memcmp(&first.receipt, &repeat.receipt,
                  sizeof(first.receipt)) == 0);

    {
        ProceduralSurfaceFeatureReliefShellReceipt sentinel;
        ProceduralSurfaceFeatureReliefShellReport report;
        ProceduralSurfaceFieldBudget budget = {
            .max_evaluations = recipe.quality.max_field_evaluations};
        ProceduralSurfaceFeatureReliefShellRequest request = {
            .selected_face_shell = {
                .source_asset_id = "semantic_wall_prism",
                .derived_asset_id = "stale_relief",
                .selected_face = PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Y,
                .cage = &cage,
                .recipe = &recipe,
                .graph = &graph,
                .binding = &binding,
                .quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL},
            .feature_field = &field,
            .expected_source_mesh_digest_sha256 =
                "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            .relief_scale = 1.0};
        memset(&sentinel, 0x5a, sizeof(sentinel));
        assert(!ProceduralSurfaceFeatureReliefShell_Compile(
            &request, &budget, &first.buffers, &first.requirements,
            &first.summary, &sentinel, &report));
        assert(report.status ==
               PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_SOURCE_IDENTITY);
        for (size_t i = 0u; i < sizeof(sentinel); ++i) {
            assert(((const unsigned char *)&sentinel)[i] == 0x5a);
        }
    }

    printf(
        "signed feature relief passed: negative_vertices=%llu "
        "positive_vertices=%llu min=%.9f max=%.9f triangles=%llu digest=%s\n",
        (unsigned long long)
            first.receipt.negatively_displaced_vertex_count,
        (unsigned long long)
            first.receipt.positively_displaced_vertex_count,
        first.receipt.minimum_emitted_displacement_units,
        first.receipt.maximum_emitted_displacement_units,
        (unsigned long long)first.summary.triangle_count,
        first.summary.mesh_digest_sha256);
    compiled_free(&repeat);
    compiled_free(&first);
    return 0;
}
