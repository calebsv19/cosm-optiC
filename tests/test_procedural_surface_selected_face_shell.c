#include "procedural/procedural_surface_selected_face_shell.h"

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

typedef struct CompiledShell {
    ProceduralSurfacePrismVertex *vertices;
    ProceduralSurfacePrismTriangle *triangles;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceSelectedFaceShellReceipt receipt;
} CompiledShell;

static void binding_init(
    ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph) {
    ProceduralSurfaceBindingV1_Init(binding);
    snprintf(binding->binding_id, sizeof(binding->binding_id),
             "psg18_positive_z");
    snprintf(binding->graph_program_id, sizeof(binding->graph_program_id),
             "%s", graph->program_id);
    binding->selector = PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP;
    snprintf(binding->surface_group_id, sizeof(binding->surface_group_id),
             "positive_z");
    binding->projection = PROCEDURAL_SURFACE_PROJECTION_PLANAR_XY;
    binding->displacement_direction =
        PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP;
}

static CompiledShell compile_shell(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const char *derived_id) {
    CompiledShell shell = {0};
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceSelectedFaceShellReport report;
    ProceduralSurfaceFieldBudget budget = {
        .max_evaluations = recipe->quality.max_field_evaluations};
    const ProceduralSurfaceSelectedFaceShellRequest request = {
        .source_asset_id = "semantic_prism",
        .derived_asset_id = derived_id,
        .selected_face = PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z,
        .cage = cage,
        .recipe = recipe,
        .graph = graph,
        .binding = binding,
        .quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL};
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        cage, recipe, request.quality, &shell.requirements, &mesh_report));
    shell.vertices = calloc(
        (size_t)shell.requirements.vertex_count, sizeof(*shell.vertices));
    shell.triangles = calloc(
        (size_t)shell.requirements.triangle_count, sizeof(*shell.triangles));
    assert(shell.vertices && shell.triangles);
    shell.buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = shell.vertices,
        .vertex_capacity = (size_t)shell.requirements.vertex_count,
        .triangles = shell.triangles,
        .triangle_capacity = (size_t)shell.requirements.triangle_count};
    assert(ProceduralSurfaceSelectedFaceShell_Compile(
        &request, &budget, &shell.buffers, &shell.requirements, &shell.summary,
        &shell.receipt, &report));
    return shell;
}

static void shell_free(CompiledShell *shell) {
    free(shell->vertices);
    free(shell->triangles);
    memset(shell, 0, sizeof(*shell));
}

int main(void) {
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceBindingV1 invalid_binding;
    ProceduralSurfaceCageContract cage = {
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 8.0,
        .height_units = 8.0,
        .depth_units = 0.8,
        .target_edge_length_units = 0.25};
    CompiledShell first;
    CompiledShell repeat;
    CompiledShell control;
    ProceduralSurfacePrismFace parsed_face =
        PROCEDURAL_SURFACE_PRISM_FACE_COUNT;

    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIELD_PRESET_ROOT "/central_mountain_peak.json",
        &graph, &graph_report));
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &recipe, &recipe_report));
    snprintf(recipe.recipe_id, sizeof(recipe.recipe_id), "psg18_mountain");
    recipe.target_edge_length_units = cage.target_edge_length_units;
    recipe.displacement_amplitude_units = 2.2;
    recipe.edge_lock_width_units = 0.36;
    assert(ProceduralSurfaceRecipeV1_Validate(&recipe, &recipe_report));
    binding_init(&binding, &graph);

    first = compile_shell(
        &cage, &recipe, &graph, &binding, "mountain_shell");
    repeat = compile_shell(
        &cage, &recipe, &graph, &binding, "mountain_shell");
    assert(strcmp(first.summary.mesh_digest_sha256,
                  repeat.summary.mesh_digest_sha256) == 0);
    assert(memcmp(&first.receipt, &repeat.receipt,
                  sizeof(first.receipt)) == 0);
    assert(first.receipt.source_triangle_count == 12u);
    assert(first.receipt.source_selected_face_triangle_count == 2u);
    assert(first.receipt.derived_selected_face_triangle_count > 2u);
    assert(first.receipt.closure_support_triangle_count > 0u);
    assert(first.receipt.maximum_selected_face_absolute_displacement_units >
           1.5);
    assert(first.receipt.maximum_unselected_face_absolute_displacement_units ==
           0.0);
    assert(first.receipt.geometry_displacement_active);
    assert(first.receipt.source_semantic_identity_retained);
    assert(first.receipt.replaceable_derived_geometry);
    assert(first.summary.boundary_edge_count == 0u);
    assert(first.summary.connected_component_count == 1u);
    assert(first.summary.euler_characteristic == 2);
    assert(first.summary.maximum_edge_absolute_displacement_units == 0.0);
    assert(first.summary.signed_volume_units3 > 0.0);
    assert(first.summary.minimum_outward_winding_dot > 0.0);

    recipe.displacement_amplitude_units = 0.0;
    control = compile_shell(
        &cage, &recipe, &graph, &binding, "flat_refined_shell");
    assert(!control.receipt.geometry_displacement_active);
    assert(control.receipt.maximum_selected_face_absolute_displacement_units ==
           0.0);
    assert(control.summary.vertex_count == first.summary.vertex_count);
    assert(control.summary.triangle_count == first.summary.triangle_count);
    assert(strcmp(control.summary.mesh_digest_sha256,
                  first.summary.mesh_digest_sha256) != 0);

    assert(ProceduralSurfacePrismFace_Parse("positive_z", &parsed_face));
    assert(parsed_face == PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z);
    assert(!ProceduralSurfacePrismFace_Parse("top-ish", &parsed_face));

    invalid_binding = binding;
    invalid_binding.selector = PROCEDURAL_SURFACE_SELECTOR_UPWARD_FACING;
    {
        ProceduralSurfaceSelectedFaceShellReceipt sentinel;
        ProceduralSurfaceSelectedFaceShellReport report;
        ProceduralSurfaceFieldBudget budget = {
            .max_evaluations = recipe.quality.max_field_evaluations};
        ProceduralSurfacePrismMeshRequirements requirements =
            first.requirements;
        ProceduralSurfacePrismMeshSummary summary = first.summary;
        ProceduralSurfacePrismMeshBuffers buffers = first.buffers;
        const ProceduralSurfaceSelectedFaceShellRequest request = {
            .source_asset_id = "semantic_prism",
            .derived_asset_id = "invalid_shell",
            .selected_face = PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z,
            .cage = &cage,
            .recipe = &recipe,
            .graph = &graph,
            .binding = &invalid_binding,
            .quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL};
        memset(&sentinel, 0x5a, sizeof(sentinel));
        assert(!ProceduralSurfaceSelectedFaceShell_Compile(
            &request, &budget, &buffers, &requirements, &summary, &sentinel,
            &report));
        assert(report.status ==
               PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_BINDING);
        for (size_t i = 0u; i < sizeof(sentinel); ++i) {
            assert(((const unsigned char *)&sentinel)[i] == 0x5a);
        }
    }

    printf(
        "PSG-18 selected-face shell passed: selected=%llu support=%llu "
        "max=%.9f triangles=%llu digest=%s\n",
        (unsigned long long)
            first.receipt.derived_selected_face_triangle_count,
        (unsigned long long)first.receipt.closure_support_triangle_count,
        first.receipt.maximum_selected_face_absolute_displacement_units,
        (unsigned long long)first.summary.triangle_count,
        first.summary.mesh_digest_sha256);
    shell_free(&control);
    shell_free(&repeat);
    shell_free(&first);
    return 0;
}
