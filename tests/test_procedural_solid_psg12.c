#include "procedural/procedural_solid_quality.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(int condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

static int load_graph(const char *name, ProceduralSolidGraphV1 *graph) {
    char path[512];
    ProceduralSolidGraphReport report;
    snprintf(path, sizeof(path),
             "tests/fixtures/procedural_solid_graphs/%s", name);
    return ProceduralSolidGraphV1_LoadJsonFile(path, graph, &report);
}

static void configure_quality(ProceduralSolidQualityConfig *config) {
    ProceduralSolidQualityConfig_Init(config);
    config->local.mesh.bounds_min =
        (CoreObjectVec3){-2.4, -2.4, -2.4};
    config->local.mesh.bounds_max =
        (CoreObjectVec3){2.4, 2.4, 2.4};
    config->local.base_cells = 16u;
    config->baseline_maximum_cells = 32u;
    config->quality_maximum_cells = 64u;
    config->local.maximum_passes = 2u;
    config->local.maximum_active_cell_ratio = 0.90;
    config->local.maximum_relative_volume_delta = 0.08;
    config->local.maximum_bounds_delta_units = 0.12;
    config->local.feature.maximum_projection_step_units = 0.08;
    config->maximum_signed_distance_rms_units = 0.0;
    config->maximum_signed_distance_max_units = 0.0;
    config->maximum_face_gradient_rms_degrees = 0.0;
    config->minimum_refinement_improvement_ratio = 0.01;
    config->crease.maximum_position_delta_units = 0.08;
    config->crease.maximum_relative_volume_delta = 0.05;
    config->crease.minimum_qef_improvement_ratio = 0.01;
    config->shading.minimum_hard_corner_improvement_ratio = 0.10;
}

static int compile_quality(
    const char *fixture,
    const char *asset_id,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidQualitySummary *summary) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidQualityConfig config;
    ProceduralSolidMeshReport report;
    if (!load_graph(fixture, &graph)) return 0;
    configure_quality(&config);
    core_mesh_asset_runtime_document_init(document);
    if (!ProceduralSolidQuality_Compile(
            &graph, NULL, &config, asset_id, document, summary, &report)) {
        fprintf(stderr, "quality compile failed status=%s field=%s: %s\n",
                ProceduralSolidMeshStatus_Name(report.status),
                report.field, report.message);
        return 0;
    }
    return 1;
}

static void test_feature_quality_and_determinism(void) {
    CoreMeshAssetRuntimeDocument first;
    CoreMeshAssetRuntimeDocument repeat;
    ProceduralSolidQualitySummary first_summary;
    ProceduralSolidQualitySummary repeat_summary;
    expect_true(
        compile_quality(
            "transformed_box.json", "psg12_transformed_box",
            &first, &first_summary),
        "PSG-12 transformed box compiles");
    if (failures) return;
    expect_true(
        first_summary.refinement_triggered &&
        first_summary.refinement_selected &&
        first_summary.baseline_cells == 32u &&
        first_summary.selected_cells == 64u &&
        first_summary.refinement_improvement_ratio > 0.01,
        "surface-error receipts select the finer local pass");
    expect_true(
        first_summary.selected_error.signed_distance_rms_units <
            first_summary.baseline_error.signed_distance_rms_units &&
        first_summary.selected_error.face_gradient_rms_degrees <
            first_summary.baseline_error.face_gradient_rms_degrees,
        "selected pass lowers geometric and normal-field error");
    expect_true(
        first_summary.crease.candidate_vertex_count > 0u &&
        first_summary.crease.optimized_vertex_count > 0u &&
        first_summary.crease.qef_improvement_ratio > 0.01 &&
        first_summary.crease.topology_preserved,
        "QEF crease optimization measurably improves feature vertices");
    expect_true(
        first_summary.shading.hard_vertex_count > 0u &&
        first_summary.shading.split_vertex_count > 0u &&
        first_summary.shading.hard_corner_count > 0u &&
        first_summary.shading.hard_corner_rms_degrees_after <
            first_summary.shading.hard_corner_rms_degrees_before &&
        first_summary.shading.measurable_improvement &&
        first.normal_provenance ==
            CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE,
        "split normal islands improve hard-corner shading");
    expect_true(
        first_summary.local.selected_mesh.boundary_edge_count == 0u &&
        first_summary.local.selected_mesh.nonmanifold_edge_count == 0u &&
        first_summary.local.selected_mesh.euler_characteristic == 2 &&
        first_summary.local.selected_regions.region_count > 0u &&
        core_mesh_asset_runtime_document_validate(&first).code == CORE_OK,
        "split shading preserves geometric shell and runtime contract");
    expect_true(
        compile_quality(
            "transformed_box.json", "psg12_transformed_box",
            &repeat, &repeat_summary),
        "PSG-12 transformed box repeat compiles");
    expect_true(
        strcmp(
            first_summary.local.selected_mesh.mesh_digest_sha256,
            repeat_summary.local.selected_mesh.mesh_digest_sha256) == 0 &&
        first.vertex_count == repeat.vertex_count &&
        first_summary.shading.split_vertex_count ==
            repeat_summary.shading.split_vertex_count &&
        first_summary.selected_error.composite_score ==
            repeat_summary.selected_error.composite_score,
        "PSG-12 geometry, normals, and receipts are deterministic");
    core_mesh_asset_runtime_document_free(&first);
    core_mesh_asset_runtime_document_free(&repeat);
}

static void test_hostile_split_budget(void) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidQualityConfig config;
    ProceduralSolidQualitySummary summary;
    CoreMeshAssetRuntimeDocument output;
    ProceduralSolidMeshReport report;
    expect_true(load_graph("transformed_box.json", &graph),
                "hostile fixture graph loads");
    configure_quality(&config);
    config.shading.maximum_output_vertices = 4u;
    core_mesh_asset_runtime_document_init(&output);
    expect_true(
        !ProceduralSolidQuality_Compile(
            &graph, NULL, &config, "psg12_hostile",
            &output, &summary, &report) &&
        report.status == PROCEDURAL_SOLID_MESH_STATUS_CAPACITY &&
        output.vertex_count == 0u && output.triangle_count == 0u,
        "hostile split budget fails without publishing a partial asset");
    core_mesh_asset_runtime_document_free(&output);
}

int main(void) {
    test_feature_quality_and_determinism();
    test_hostile_split_budget();
    if (failures) {
        fprintf(stderr, "%d PSG-12 failures\n", failures);
        return 1;
    }
    printf("PSG-12 feature quality and split-normal tests passed\n");
    return 0;
}
