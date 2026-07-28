#include "procedural/procedural_solid_authoring.h"
#include "procedural/procedural_solid_remesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static const ProceduralSolidParameter *find_parameter(
    const ProceduralSolidAuthoringView *view,
    const char *id) {
    for (size_t i = 0u; i < view->parameter_count; ++i) {
        if (strcmp(view->parameters[i].id, id) == 0) {
            return &view->parameters[i];
        }
    }
    return NULL;
}

int main(void) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidGraphV1 edited;
    ProceduralSolidGraphV1 restored;
    ProceduralSolidAuthoringView view;
    ProceduralSolidAuthoringReport authoring_report;
    ProceduralSolidGraphReport graph_report;
    char original_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char edited_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char restored_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char save_path[256];

    expect_true(load_graph("twisted_tapered_column.json", &graph),
                "solid authoring fixture loads");
    expect_true(ProceduralSolidAuthoring_Inspect(
                    &graph, &view, &authoring_report),
                "typed solid graph inspect succeeds");
    expect_true(view.node_count == 4u && view.connection_count == 3u &&
                    view.parameter_count >= 7u,
                "inspect exposes nodes, ports, and typed parameters");
    expect_true(find_parameter(&view, "twisted.scalar_a") != NULL &&
                    find_parameter(&view, "column.vector_a.x") != NULL,
                "inspect exposes semantic editable parameters");
    snprintf(original_digest, sizeof(original_digest), "%s",
             view.graph_digest_sha256);

    expect_true(ProceduralSolidAuthoring_ApplyParameter(
                    &graph, original_digest, "twisted.scalar_a", 0.55,
                    &edited, &authoring_report),
                "digest-bound typed edit succeeds");
    snprintf(edited_digest, sizeof(edited_digest), "%s",
             authoring_report.result_graph_digest_sha256);
    expect_true(strcmp(original_digest, edited_digest) != 0,
                "typed edit changes canonical graph identity");
    expect_true(!ProceduralSolidAuthoring_ApplyParameter(
                    &graph, edited_digest, "twisted.scalar_a", 0.2,
                    &restored, &authoring_report) &&
                    authoring_report.status ==
                        PROCEDURAL_SOLID_AUTHORING_STATUS_BASE_DIGEST,
                "stale agent edit fails closed");
    expect_true(!ProceduralSolidAuthoring_ApplyParameter(
                    &graph, original_digest, "tapered.scalar_a", 5.0,
                    &restored, &authoring_report) &&
                    authoring_report.status ==
                        PROCEDURAL_SOLID_AUTHORING_STATUS_RANGE,
                "typed edit enforces semantic range");

    snprintf(save_path, sizeof(save_path),
             "/tmp/procedural_solid_authoring_%ld.json", (long)getpid());
    expect_true(ProceduralSolidAuthoring_SaveGraphAtomic(
                    save_path, &edited, &authoring_report) &&
                    ProceduralSolidGraphV1_LoadJsonFile(
                        save_path, &restored, &graph_report) &&
                    ProceduralSolidGraphV1_Digest(
                        &restored, restored_digest, &graph_report) &&
                    strcmp(edited_digest, restored_digest) == 0,
                "atomic save preserves exact edited graph");
    expect_true(ProceduralSolidAuthoring_SaveGraphAtomic(
                    save_path, &graph, &authoring_report) &&
                    ProceduralSolidGraphV1_LoadJsonFile(
                        save_path, &restored, &graph_report) &&
                    ProceduralSolidGraphV1_Digest(
                        &restored, restored_digest, &graph_report) &&
                    strcmp(original_digest, restored_digest) == 0,
                "exact prior graph provides deterministic undo");
    unlink(save_path);

    {
        ProceduralSolidRemeshConfig config;
        ProceduralSolidRemeshSummary summary;
        ProceduralSolidRemeshSummary repeat_summary;
        ProceduralSolidMeshReport mesh_report;
        CoreMeshAssetRuntimeDocument document;
        CoreMeshAssetRuntimeDocument repeat;
        expect_true(load_graph("blended_double_sphere.json", &graph),
                    "adaptive fixture loads");
        ProceduralSolidRemeshConfig_Init(&config);
        config.mesh.bounds_min = (CoreObjectVec3){-2.0, -2.0, -2.0};
        config.mesh.bounds_max = (CoreObjectVec3){2.0, 2.0, 2.0};
        config.base_cells = 12u;
        config.maximum_cells = 48u;
        config.maximum_passes = 3u;
        config.requested_feature_size_units = 0.34;
        config.maximum_relative_volume_delta = 0.10;
        config.maximum_bounds_delta_units = 0.10;
        core_mesh_asset_runtime_document_init(&document);
        core_mesh_asset_runtime_document_init(&repeat);
        expect_true(ProceduralSolidRemesh_CompileAdaptive(
                        &graph, NULL, &config, "adaptive_blend",
                        &document, &summary, &mesh_report),
                    "adaptive remesh converges");
        expect_true(summary.converged && summary.pass_count >= 2u &&
                        summary.selected_pass + 1u == summary.pass_count,
                    "adaptive remesh selects first passing convergence level");
        expect_true(
            summary.passes[summary.selected_pass].topology_matches_previous &&
                summary.selected_mesh.boundary_edge_count == 0u &&
                summary.selected_mesh.nonmanifold_edge_count == 0u &&
                summary.selected_mesh.thin_feature_floor_units <=
                    config.requested_feature_size_units,
            "adaptive remesh preserves shell topology and feature floor");
        expect_true(ProceduralSolidRemesh_CompileAdaptive(
                        &graph, NULL, &config, "adaptive_blend",
                        &repeat, &repeat_summary, &mesh_report) &&
                        summary.selected_pass == repeat_summary.selected_pass &&
                        strcmp(summary.selected_mesh.mesh_digest_sha256,
                               repeat_summary.selected_mesh
                                   .mesh_digest_sha256) == 0,
                    "adaptive convergence and selected mesh are deterministic");
        core_mesh_asset_runtime_document_free(&document);
        core_mesh_asset_runtime_document_free(&repeat);
    }

    if (failures) {
        fprintf(stderr, "%d procedural solid authoring failures\n", failures);
        return 1;
    }
    printf("procedural solid authoring and adaptive remesh tests passed\n");
    return 0;
}
