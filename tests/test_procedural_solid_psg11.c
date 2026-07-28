#include "procedural/procedural_solid_local_remesh.h"
#include "procedural/procedural_solid_source_accel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static int build_dense_cube(
    size_t subdivisions,
    CoreMeshAssetRuntimeDocument *document) {
    static const CoreObjectVec3 centers[6] = {
        {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
    static const CoreObjectVec3 axes_u[6] = {
        {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
        {-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}};
    static const CoreObjectVec3 axes_v[6] = {
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0}, {0.0, 1.0, 0.0}};
    const size_t side = subdivisions + 1u;
    const size_t face_vertices = side * side;
    const size_t vertex_count = 6u * face_vertices;
    const size_t triangle_count = 12u * subdivisions * subdivisions;
    CoreResult result;
    core_mesh_asset_runtime_document_init(document);
    result = core_mesh_asset_runtime_contract_set_asset_id(
        &document->contract, "dense_cube_runtime");
    if (result.code != CORE_OK) return 0;
    result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &document->contract, "dense_cube_source");
    if (result.code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            document, vertex_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            document, triangle_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            document, 1u).code != CORE_OK) {
        return 0;
    }
    for (size_t face = 0u; face < 6u; ++face) {
        for (size_t y = 0u; y < side; ++y) {
            for (size_t x = 0u; x < side; ++x) {
                const double u =
                    -1.0 + 2.0 * (double)x / (double)subdivisions;
                const double v =
                    -1.0 + 2.0 * (double)y / (double)subdivisions;
                const size_t index =
                    face * face_vertices + x + side * y;
                document->vertices[index].position = (CoreObjectVec3){
                    centers[face].x + axes_u[face].x * u +
                        axes_v[face].x * v,
                    centers[face].y + axes_u[face].y * u +
                        axes_v[face].y * v,
                    centers[face].z + axes_u[face].z * u +
                        axes_v[face].z * v};
                document->vertices[index].normal = centers[face];
            }
        }
    }
    {
        size_t cursor = 0u;
        for (size_t face = 0u; face < 6u; ++face) {
            const size_t base = face * face_vertices;
            for (size_t y = 0u; y < subdivisions; ++y) {
                for (size_t x = 0u; x < subdivisions; ++x) {
                    const size_t a = base + x + side * y;
                    const size_t b = a + 1u;
                    const size_t d = base + x + side * (y + 1u);
                    const size_t c = d + 1u;
                    document->triangles[cursor] =
                        (CoreMeshAssetRuntimeTriangle){a, b, c, ""};
                    snprintf(
                        document->triangles[cursor++].surface_group_id,
                        64u, "cube_shell");
                    document->triangles[cursor] =
                        (CoreMeshAssetRuntimeTriangle){a, c, d, ""};
                    snprintf(
                        document->triangles[cursor++].surface_group_id,
                        64u, "cube_shell");
                }
            }
        }
    }
    document->vertex_normal_count = vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_SOURCE;
    snprintf(document->surface_groups[0].group_id, 64u, "cube_shell");
    document->surface_groups[0].triangle_start = 0u;
    document->surface_groups[0].triangle_count = triangle_count;
    document->contract.local_bounds.min =
        (CoreObjectVec3){-1.0, -1.0, -1.0};
    document->contract.local_bounds.max =
        (CoreObjectVec3){1.0, 1.0, 1.0};
    document->contract.topology_closed_volume = true;
    document->contract.topology_manifold_expected = true;
    return core_mesh_asset_runtime_document_validate(document).code ==
        CORE_OK;
}

static void init_source_graph(ProceduralSolidGraphV1 *graph) {
    ProceduralSolidGraphV1_Init(graph);
    snprintf(graph->graph_id, sizeof(graph->graph_id), "dense_source_graph");
    snprintf(
        graph->semantic_source_id, sizeof(graph->semantic_source_id),
        "dense_cube_source");
    graph->max_node_evaluations = 8u;
    graph->node_count = 1u;
    snprintf(graph->nodes[0].id, sizeof(graph->nodes[0].id), "source");
    graph->nodes[0].op = PROCEDURAL_SOLID_NODE_SOURCE_MESH;
    snprintf(
        graph->nodes[0].source_id, sizeof(graph->nodes[0].source_id),
        "dense_cube");
    snprintf(graph->output, sizeof(graph->output), "source");
}

static void test_source_acceleration(void) {
    static const CoreObjectVec3 points[] = {
        {0.0, 0.0, 0.0}, {0.25, -0.3, 0.4}, {0.9, 0.1, -0.2},
        {1.4, 0.2, 0.3}, {-1.3, -0.4, 0.2}, {0.2, 1.7, -0.1},
        {0.1, 0.4, -1.6}, {1.5, 1.5, 0.0}};
    CoreMeshAssetRuntimeDocument cube;
    ProceduralSolidSourceAccel accel;
    ProceduralSolidGraphV1 graph;
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidSourceSet baseline = {0};
    ProceduralSolidSourceSet accelerated = {0};
    size_t baseline_tests = 0u;
    size_t accelerated_tests = 0u;
    ProceduralSolidSourceAccel_Init(&accel);
    expect_true(build_dense_cube(24u, &cube),
                "dense source cube fixture builds");
    expect_true(ProceduralSolidSourceAccel_Build(&cube, 8u, &accel),
                "dense source BVH builds");
    init_source_graph(&graph);
    expect_true(ProceduralSolidGraphV1_Validate(&graph, &graph_report),
                "dense source graph validates");
    baseline.source_count = 1u;
    snprintf(baseline.sources[0].source_id,
             sizeof(baseline.sources[0].source_id), "dense_cube");
    baseline.sources[0].mesh = &cube;
    accelerated = baseline;
    accelerated.sources[0].accel = &accel;
    for (size_t i = 0u; i < sizeof(points) / sizeof(points[0]); ++i) {
        ProceduralSolidSample exact;
        ProceduralSolidSample fast;
        expect_true(
            ProceduralSolidGraphV1_Evaluate(
                &graph, &baseline, points[i], &exact, &graph_report) &&
            ProceduralSolidGraphV1_Evaluate(
                &graph, &accelerated, points[i], &fast, &graph_report),
            "source queries evaluate through exact and BVH paths");
        expect_true(
            fabs(exact.signed_distance - fast.signed_distance) <= 1.0e-9,
            "accelerated source query preserves signed distance");
        expect_true(
            exact.region_kind == fast.region_kind &&
            strcmp(
                exact.contributing_node_id,
                fast.contributing_node_id) == 0,
            "accelerated source query preserves contributor semantics");
        baseline_tests += exact.source_triangle_tests;
        accelerated_tests += fast.source_triangle_tests;
    }
    expect_true(
        accelerated_tests * 4u < baseline_tests &&
        accel.maximum_depth > 1u,
        "BVH reduces dense-source triangle work by at least 75 percent");
    ProceduralSolidSourceAccel_Free(&accel);
    core_mesh_asset_runtime_document_free(&cube);
}

static int compile_local(
    const char *fixture,
    const char *asset_id,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidLocalRemeshSummary *summary) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidLocalRemeshConfig config;
    ProceduralSolidMeshReport report;
    if (!load_graph(fixture, &graph)) return 0;
    ProceduralSolidLocalRemeshConfig_Init(&config);
    config.mesh.bounds_min = (CoreObjectVec3){-2.4, -2.4, -2.4};
    config.mesh.bounds_max = (CoreObjectVec3){2.4, 2.4, 2.4};
    config.base_cells = 24u;
    config.maximum_cells = 48u;
    config.maximum_passes = 2u;
    config.feature.maximum_projection_step_units = 0.06;
    core_mesh_asset_runtime_document_init(document);
    return ProceduralSolidLocalRemesh_Compile(
        &graph, NULL, &config, asset_id, document, summary, &report);
}

static void test_local_feature_and_regions(void) {
    CoreMeshAssetRuntimeDocument blend;
    CoreMeshAssetRuntimeDocument repeat;
    CoreMeshAssetRuntimeDocument tunnel;
    ProceduralSolidLocalRemeshSummary blend_summary;
    ProceduralSolidLocalRemeshSummary repeat_summary;
    ProceduralSolidLocalRemeshSummary tunnel_summary;
    expect_true(
        compile_local(
            "blended_double_sphere.json", "psg11_blend",
            &blend, &blend_summary),
        "local blend remesh converges");
    expect_true(
        blend_summary.converged &&
        blend_summary.pass_count == 2u &&
        blend_summary.selected_pass == 1u,
        "local remesh selects the first converged refinement pass");
    {
        const ProceduralSolidLocalRemeshPass *pass =
            &blend_summary.passes[blend_summary.selected_pass];
        expect_true(
            pass->active_cell_ratio < 0.5 &&
            pass->mesh.evaluated_sample_count <
                pass->mesh.sample_count / 2u,
            "local remesh refines and evaluates less than half the domain");
        expect_true(
            pass->inactive_interface_face_count > 0u &&
            pass->transition_surface_crossing_count == 0u &&
            pass->mesh.boundary_edge_count == 0u &&
            pass->mesh.nonmanifold_edge_count == 0u,
            "inactive-band transition equivalent remains crack free");
        expect_true(
            pass->feature.measurable_improvement &&
            pass->feature.improvement_ratio > 0.5 &&
            pass->feature.feature_vertex_count > 0u &&
            pass->feature.topology_preserved &&
            blend.normal_provenance ==
                CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE,
            "feature projection measurably improves the PSG-10 cage");
        expect_true(
            pass->regions.region_count >= 3u &&
            pass->regions.blend_triangle_count > 0u &&
            pass->regions.retained_triangle_count > 0u,
            "smooth union emits retained and blend regions");
    }
    expect_true(
        compile_local(
            "blended_double_sphere.json", "psg11_blend",
            &repeat, &repeat_summary) &&
        repeat_summary.selected_pass == blend_summary.selected_pass &&
        strcmp(
            repeat_summary.selected_mesh.mesh_digest_sha256,
            blend_summary.selected_mesh.mesh_digest_sha256) == 0 &&
        strcmp(
            repeat_summary.selected_regions.region_digest_sha256,
            blend_summary.selected_regions.region_digest_sha256) == 0,
        "local remesh, features, and regions are deterministic");
    expect_true(
        compile_local(
            "rounded_block_with_tunnel.json", "psg11_tunnel",
            &tunnel, &tunnel_summary),
        "local Boolean tunnel remesh converges");
    expect_true(
        tunnel_summary.selected_regions.cut_triangle_count > 0u &&
        tunnel_summary.selected_regions.retained_triangle_count > 0u &&
        tunnel_summary.selected_mesh.euler_characteristic == 0 &&
        tunnel_summary.selected_mesh.boundary_edge_count == 0u &&
        tunnel_summary.selected_mesh.nonmanifold_edge_count == 0u,
        "difference emits retained/cut regions without changing tunnel genus");
    core_mesh_asset_runtime_document_free(&blend);
    core_mesh_asset_runtime_document_free(&repeat);
    core_mesh_asset_runtime_document_free(&tunnel);
}

int main(void) {
    test_source_acceleration();
    test_local_feature_and_regions();
    if (failures) {
        fprintf(stderr, "%d PSG-11 failures\n", failures);
        return 1;
    }
    printf(
        "PSG-11 local remesh, feature, region, and acceleration tests passed\n");
    return 0;
}
