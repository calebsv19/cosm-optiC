#include "procedural/procedural_surface_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void expect_true(int condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

static int make_tetrahedron(CoreMeshAssetRuntimeDocument *document) {
    static const CoreObjectVec3 positions[4] = {
        {1.0, 1.0, 1.0},
        {-1.0, -1.0, 1.0},
        {-1.0, 1.0, -1.0},
        {1.0, -1.0, -1.0}
    };
    static const size_t indices[4][3] = {
        {0u, 2u, 1u},
        {0u, 1u, 3u},
        {0u, 3u, 2u},
        {1u, 2u, 3u}
    };
    core_mesh_asset_runtime_document_init(document);
    if (core_mesh_asset_runtime_contract_set_asset_id(
            &document->contract, "tetra_source").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(
            &document->contract, "tetra_seed").code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            document, 4u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            document, 4u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            document, 1u).code != CORE_OK) {
        return 0;
    }
    for (size_t i = 0u; i < 4u; ++i) {
        document->vertices[i].position = positions[i];
        document->triangles[i].a = indices[i][0];
        document->triangles[i].b = indices[i][1];
        document->triangles[i].c = indices[i][2];
        snprintf(document->triangles[i].surface_group_id,
                 sizeof(document->triangles[i].surface_group_id),
                 "tetra_shell");
    }
    snprintf(document->surface_groups[0].group_id,
             sizeof(document->surface_groups[0].group_id), "tetra_shell");
    document->surface_groups[0].triangle_count = 4u;
    document->contract.local_bounds.min =
        (CoreObjectVec3){-1.0, -1.0, -1.0};
    document->contract.local_bounds.max =
        (CoreObjectVec3){1.0, 1.0, 1.0};
    document->contract.topology_closed_volume = true;
    document->contract.topology_manifold_expected = true;
    return 1;
}

int main(void) {
    CoreMeshAssetRuntimeDocument source;
    CoreMeshAssetRuntimeDocument first;
    CoreMeshAssetRuntimeDocument second;
    CoreMeshAssetRuntimeDocument open;
    ProceduralSurfaceMaterialSample *first_materials = NULL;
    ProceduralSurfaceMaterialSample *second_materials = NULL;
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfaceShellConfig config;
    ProceduralSurfaceShellSummary first_summary;
    ProceduralSurfaceShellSummary second_summary;
    ProceduralSurfaceShellReport shell_report;

    expect_true(make_tetrahedron(&source), "tetrahedron fixture initializes");
    expect_true(
        ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            "tests/fixtures/procedural_surface_field_presets/"
            "pitted_concrete.json",
            &graph, &graph_report),
        "field graph loads");
    expect_true(
        ProceduralSurfaceBindingV1_LoadJsonFile(
            "tests/fixtures/procedural_surface_field_presets/"
            "pitted_concrete.all.binding.json",
            &binding, &binding_report),
        "surface binding loads");
    binding.displacement_scale = 0.05;
    expect_true(ProceduralSurfaceBindingV1_Validate(
                    &binding, &graph, &binding_report),
                "adjusted binding validates");
    ProceduralSurfaceShellConfig_Init(&config);
    config.target_edge_length_units = 1.0;
    config.max_refinement_levels = 3u;

    core_mesh_asset_runtime_document_init(&first);
    core_mesh_asset_runtime_document_init(&second);
    expect_true(
        ProceduralSurfaceShell_Compile(
            &source, &graph, &binding, &config, "tetra_derived_a",
            &first, &first_materials, &first_summary, &shell_report),
        "arbitrary closed shell compiles");
    expect_true(first_summary.refinement_levels == 2u,
                "unit-scale target derives two uniform refinements");
    expect_true(first_summary.vertex_count == 34u &&
                    first_summary.triangle_count == 64u,
                "shared-edge subdivision has deterministic counts");
    expect_true(first_summary.boundary_edge_count == 0u &&
                    first_summary.nonmanifold_edge_count == 0u &&
                    first_summary.connected_component_count == 1u &&
                    first_summary.euler_characteristic == 2 &&
                    first_summary.signed_volume_units3 > 0.0,
                "derived result remains one valid closed shell");

    expect_true(
        ProceduralSurfaceShell_Compile(
            &source, &graph, &binding, &config, "tetra_derived_a",
            &second, &second_materials, &second_summary, &shell_report),
        "repeat compile succeeds");
    expect_true(first.vertex_count == second.vertex_count &&
                    first.triangle_count == second.triangle_count &&
                    memcmp(first.vertices, second.vertices,
                           first.vertex_count * sizeof(*first.vertices)) == 0 &&
                    memcmp(first.triangles, second.triangles,
                           first.triangle_count * sizeof(*first.triangles)) == 0 &&
                    memcmp(first_materials, second_materials,
                           first.vertex_count * sizeof(*first_materials)) == 0,
                "repeat compile is byte deterministic");

    expect_true(make_tetrahedron(&open), "open fixture initializes");
    open.triangle_count = 3u;
    open.surface_groups[0].triangle_count = 3u;
    open.contract.triangle_count = 3u;
    open.contract.topology_closed_volume = false;
    expect_true(
        !ProceduralSurfaceShell_Compile(
            &open, &graph, &binding, &config, "open_rejected",
            &second, &second_materials, &second_summary, &shell_report) &&
            shell_report.status == PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_OPEN,
        "open source shell is rejected transactionally");

    free(first_materials);
    free(second_materials);
    core_mesh_asset_runtime_document_free(&source);
    core_mesh_asset_runtime_document_free(&open);
    core_mesh_asset_runtime_document_free(&first);
    core_mesh_asset_runtime_document_free(&second);
    if (failures != 0) return 1;
    printf("procedural surface arbitrary shell contract passed "
           "(vertices=%zu triangles=%zu volume=%.9f)\n",
           first_summary.vertex_count, first_summary.triangle_count,
           first_summary.signed_volume_units3);
    return 0;
}
