#include "procedural/procedural_solid_graph.h"
#include "procedural/procedural_solid_mesh.h"
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

static int make_cube_source(CoreMeshAssetRuntimeDocument *document) {
    static const CoreObjectVec3 positions[8] = {
        {-0.7, -0.55, -0.8}, {0.7, -0.55, -0.8},
        {0.7, 0.55, -0.8}, {-0.7, 0.55, -0.8},
        {-0.7, -0.55, 0.8}, {0.7, -0.55, 0.8},
        {0.7, 0.55, 0.8}, {-0.7, 0.55, 0.8}};
    static const size_t triangles[12][3] = {
        {0u, 2u, 1u}, {0u, 3u, 2u},
        {4u, 5u, 6u}, {4u, 6u, 7u},
        {0u, 1u, 5u}, {0u, 5u, 4u},
        {1u, 2u, 6u}, {1u, 6u, 5u},
        {2u, 3u, 7u}, {2u, 7u, 6u},
        {3u, 0u, 4u}, {3u, 4u, 7u}};
    core_mesh_asset_runtime_document_init(document);
    if (core_mesh_asset_runtime_contract_set_asset_id(
            &document->contract, "fixture_cube_source").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(
            &document->contract, "fixture_cube_seed").code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            document, 8u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            document, 12u).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            document, 1u).code != CORE_OK) {
        return 0;
    }
    for (size_t i = 0u; i < 8u; ++i) {
        document->vertices[i].position = positions[i];
    }
    for (size_t i = 0u; i < 12u; ++i) {
        document->triangles[i].a = triangles[i][0];
        document->triangles[i].b = triangles[i][1];
        document->triangles[i].c = triangles[i][2];
        snprintf(document->triangles[i].surface_group_id,
                 sizeof(document->triangles[i].surface_group_id),
                 "cube_shell");
    }
    snprintf(document->surface_groups[0].group_id,
             sizeof(document->surface_groups[0].group_id), "cube_shell");
    document->surface_groups[0].triangle_count = 12u;
    document->contract.local_bounds.min = positions[0];
    document->contract.local_bounds.max = positions[6];
    document->contract.topology_closed_volume = true;
    document->contract.topology_manifold_expected = true;
    return core_mesh_asset_runtime_document_validate(document).code == CORE_OK;
}

static int load_graph(const char *name, ProceduralSolidGraphV1 *graph) {
    char path[512];
    ProceduralSolidGraphReport report;
    snprintf(path, sizeof(path),
             "tests/fixtures/procedural_solid_graphs/%s", name);
    if (ProceduralSolidGraphV1_LoadJsonFile(path, graph, &report)) return 1;
    fprintf(stderr, "graph load failed %s: %s %s\n",
            name, report.field, report.message);
    return 0;
}

static void expect_shell(const ProceduralSolidMeshSummary *summary,
                         int expected_euler,
                         const char *message) {
    expect_true(
        summary->vertex_count > 0u && summary->triangle_count > 0u &&
        summary->boundary_edge_count == 0u &&
        summary->nonmanifold_edge_count == 0u &&
        summary->connected_component_count == 1u &&
        summary->euler_characteristic == expected_euler &&
        summary->signed_volume_units3 > 0.0 &&
        summary->minimum_triangle_area2 > 0.0 &&
        summary->boundary_min_signed_distance > 0.0 &&
        summary->conforming_cell_self_intersection_free,
        message);
}

static ProceduralSolidGraphV1 disconnected_spheres_graph(void) {
    ProceduralSolidGraphV1 graph;
    ProceduralSolidGraphV1_Init(&graph);
    snprintf(graph.graph_id, sizeof(graph.graph_id), "disconnected_spheres");
    snprintf(graph.semantic_source_id, sizeof(graph.semantic_source_id),
             "sphere_pair");
    graph.max_node_evaluations = 16u;
    graph.node_count = 5u;
    snprintf(graph.nodes[0].id, sizeof(graph.nodes[0].id), "sphere_a");
    graph.nodes[0].op = PROCEDURAL_SOLID_NODE_SPHERE;
    graph.nodes[0].scalar_a = 0.45;
    snprintf(graph.nodes[1].id, sizeof(graph.nodes[1].id), "move_a");
    graph.nodes[1].op = PROCEDURAL_SOLID_NODE_TRANSFORM;
    graph.nodes[1].input_count = 1u;
    snprintf(graph.nodes[1].inputs[0], sizeof(graph.nodes[1].inputs[0]),
             "sphere_a");
    graph.nodes[1].vector_a.x = -0.9;
    graph.nodes[1].vector_c = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(graph.nodes[2].id, sizeof(graph.nodes[2].id), "sphere_b");
    graph.nodes[2].op = PROCEDURAL_SOLID_NODE_SPHERE;
    graph.nodes[2].scalar_a = 0.45;
    snprintf(graph.nodes[3].id, sizeof(graph.nodes[3].id), "move_b");
    graph.nodes[3].op = PROCEDURAL_SOLID_NODE_TRANSFORM;
    graph.nodes[3].input_count = 1u;
    snprintf(graph.nodes[3].inputs[0], sizeof(graph.nodes[3].inputs[0]),
             "sphere_b");
    graph.nodes[3].vector_a.x = 0.9;
    graph.nodes[3].vector_c = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(graph.nodes[4].id, sizeof(graph.nodes[4].id), "union");
    graph.nodes[4].op = PROCEDURAL_SOLID_NODE_UNION;
    graph.nodes[4].input_count = 2u;
    snprintf(graph.nodes[4].inputs[0], sizeof(graph.nodes[4].inputs[0]),
             "move_a");
    snprintf(graph.nodes[4].inputs[1], sizeof(graph.nodes[4].inputs[1]),
             "move_b");
    snprintf(graph.output, sizeof(graph.output), "union");
    return graph;
}

int main(void) {
    const char *fixture_names[] = {
        "transformed_box.json",
        "twisted_tapered_column.json",
        "rounded_block_with_tunnel.json",
        "blended_double_sphere.json",
        "source_mesh_twist.json"};
    ProceduralSolidGraphV1 graphs[5];
    ProceduralSolidGraphReport graph_report;
    char digests[5][PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    CoreMeshAssetRuntimeDocument source;
    ProceduralSolidSourceSet sources;
    ProceduralSolidSample sample;
    ProceduralSolidMeshConfig config;
    CoreMeshAssetRuntimeDocument documents[5];
    ProceduralSolidMeshSummary summaries[5];
    ProceduralSolidMeshReport mesh_report;
    CoreMeshAssetRuntimeDocument repeat;
    ProceduralSolidMeshSummary repeat_summary;

    expect_true(make_cube_source(&source), "source cube fixture validates");
    memset(&sources, 0, sizeof(sources));
    sources.source_count = 1u;
    snprintf(sources.sources[0].source_id,
             sizeof(sources.sources[0].source_id), "source_cube");
    sources.sources[0].mesh = &source;

    for (size_t i = 0u; i < 5u; ++i) {
        expect_true(load_graph(fixture_names[i], &graphs[i]),
                    "solid graph fixture loads");
        expect_true(ProceduralSolidGraphV1_Digest(
                        &graphs[i], digests[i], &graph_report),
                    "solid graph digest succeeds");
        for (size_t j = 0u; j < i; ++j) {
            expect_true(strcmp(digests[i], digests[j]) != 0,
                        "solid graph fixture digests are distinct");
        }
    }
    {
        ProceduralSolidGraphV1 reordered = graphs[0];
        ProceduralSolidGraphNode swap = reordered.nodes[0];
        char reordered_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
        reordered.nodes[0] = reordered.nodes[1];
        reordered.nodes[1] = swap;
        expect_true(
            ProceduralSolidGraphV1_Digest(
                &reordered, reordered_digest, &graph_report) &&
            strcmp(reordered_digest, digests[0]) == 0,
            "canonical graph identity is independent of node array order");
    }

    expect_true(ProceduralSolidGraphV1_Evaluate(
                    &graphs[0], &sources,
                    (CoreObjectVec3){0.35, -0.2, 0.1},
                    &sample, &graph_report) &&
                    sample.signed_distance < 0.0,
                "transformed object center remains inside");
    expect_true(ProceduralSolidGraphV1_Evaluate(
                    &graphs[0], &sources,
                    (CoreObjectVec3){3.0, 3.0, 3.0},
                    &sample, &graph_report) &&
                    sample.signed_distance > 0.0,
                "transformed object far sample remains outside");
    expect_true(ProceduralSolidGraphV1_Evaluate(
                    &graphs[2], &sources,
                    (CoreObjectVec3){0.0, 0.0, 0.0},
                    &sample, &graph_report) &&
                    sample.signed_distance > 0.0,
                "difference removes the tunnel center");
    expect_true(ProceduralSolidGraphV1_Evaluate(
                    &graphs[2], &sources,
                    (CoreObjectVec3){0.9, 0.0, 0.0},
                    &sample, &graph_report) &&
                    sample.signed_distance < 0.0,
                "difference retains block material outside tunnel");
    expect_true(ProceduralSolidGraphV1_Evaluate(
                    &graphs[4], &sources,
                    (CoreObjectVec3){0.15, 0.05, -0.1},
                    &sample, &graph_report) &&
                    sample.signed_distance < 0.0,
                "registered arbitrary source mesh evaluates through transforms");
    expect_true(!ProceduralSolidGraphV1_Evaluate(
                    &graphs[4], NULL,
                    (CoreObjectVec3){0.0, 0.0, 0.0},
                    &sample, &graph_report) &&
                    graph_report.status ==
                        PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE,
                "unregistered source mesh is rejected");
    {
        ProceduralSolidSourceSet duplicate_sources = sources;
        duplicate_sources.source_count = 2u;
        duplicate_sources.sources[1] = duplicate_sources.sources[0];
        expect_true(
            !ProceduralSolidGraphV1_Evaluate(
                &graphs[4], &duplicate_sources,
                (CoreObjectVec3){0.0, 0.0, 0.0},
                &sample, &graph_report) &&
            graph_report.status == PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE,
            "ambiguous duplicate source registration is rejected");
    }
    {
        ProceduralSolidGraphV1 intersection =
            disconnected_spheres_graph();
        intersection.nodes[1].vector_a.x = -0.2;
        intersection.nodes[3].vector_a.x = 0.2;
        intersection.nodes[4].op = PROCEDURAL_SOLID_NODE_INTERSECTION;
        snprintf(intersection.graph_id, sizeof(intersection.graph_id),
                 "overlapping_sphere_intersection");
        expect_true(
            ProceduralSolidGraphV1_Evaluate(
                &intersection, NULL, (CoreObjectVec3){0.0, 0.0, 0.0},
                &sample, &graph_report) &&
            sample.signed_distance < 0.0,
            "solid intersection retains shared interior");
        expect_true(
            ProceduralSolidGraphV1_Evaluate(
                &intersection, NULL, (CoreObjectVec3){0.6, 0.0, 0.0},
                &sample, &graph_report) &&
            sample.signed_distance > 0.0,
            "solid intersection removes non-shared material");
    }

    {
        ProceduralSolidGraphV1 invalid = graphs[0];
        snprintf(invalid.nodes[0].inputs[0],
                 sizeof(invalid.nodes[0].inputs[0]), "object_transform");
        invalid.nodes[0].input_count = 1u;
        invalid.nodes[0].op = PROCEDURAL_SOLID_NODE_TWIST_Z;
        expect_true(!ProceduralSolidGraphV1_Validate(
                        &invalid, &graph_report) &&
                        graph_report.status ==
                            PROCEDURAL_SOLID_GRAPH_STATUS_CYCLE,
                    "solid graph cycle is rejected");
    }
    {
        ProceduralSolidGraphV1 invalid = graphs[0];
        invalid.max_node_evaluations = 1u;
        expect_true(!ProceduralSolidGraphV1_Validate(
                        &invalid, &graph_report) &&
                        graph_report.status ==
                            PROCEDURAL_SOLID_GRAPH_STATUS_CAPACITY,
                    "undersized solid graph budget is rejected");
    }

    ProceduralSolidMeshConfig_Init(&config);
    config.bounds_min = (CoreObjectVec3){-2.4, -2.4, -2.4};
    config.bounds_max = (CoreObjectVec3){2.4, 2.4, 2.4};
    config.cells_x = 30u;
    config.cells_y = 30u;
    config.cells_z = 30u;
    config.gradient_step_units = 0.002;
    config.collision_authority =
        PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL;
    for (size_t i = 0u; i < 5u; ++i) {
        char asset_id[64];
        int compiled;
        core_mesh_asset_runtime_document_init(&documents[i]);
        snprintf(asset_id, sizeof(asset_id), "psg9_fixture_%zu", i);
        compiled = ProceduralSolidMesh_Compile(
            &graphs[i], &sources, &config, asset_id,
            &documents[i], &summaries[i], &mesh_report);
        if (!compiled) {
            fprintf(stderr, "compile %s failed: %s %s %s\n",
                    fixture_names[i],
                    ProceduralSolidMeshStatus_Name(mesh_report.status),
                    mesh_report.field, mesh_report.message);
        }
        expect_true(compiled,
                    "solid graph compiles to fresh runtime shell");
        expect_shell(&summaries[i], i == 2u ? 0 : 2,
                     "fresh solid shell satisfies topology contract");
        expect_true(
            summaries[i].collision_authority ==
                PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL &&
            summaries[i].thin_feature_floor_units > 0.0 &&
            strcmp(summaries[i].graph_digest_sha256, digests[i]) == 0,
            "solid resolution, collision authority, and identity persist");
    }
    expect_true(
        summaries[0].bounds_min.x > -2.0 &&
        summaries[0].bounds_max.x > 1.0,
        "object transform changes derived bounds");
    expect_true(summaries[2].euler_characteristic == 0,
                "through-tunnel produces genus-one topology");
    {
        ProceduralSurfaceFieldGraphV1 surface_graph;
        ProceduralSurfaceBindingV1 binding;
        ProceduralSurfaceFieldGraphReport field_report;
        ProceduralSurfaceBindingReport binding_report;
        ProceduralSurfaceShellConfig shell_config;
        ProceduralSurfaceShellSummary shell_summary;
        ProceduralSurfaceShellReport shell_report;
        ProceduralSurfaceMaterialSample *materials = NULL;
        CoreMeshAssetRuntimeDocument textured;
        int shell_compiled;
        core_mesh_asset_runtime_document_init(&textured);
        expect_true(
            ProceduralSurfaceFieldGraphV1_LoadJsonFile(
                "tests/fixtures/procedural_surface_field_presets/"
                "pitted_concrete.json",
                &surface_graph, &field_report) &&
            ProceduralSurfaceBindingV1_LoadJsonFile(
                "tests/fixtures/procedural_surface_field_presets/"
                "pitted_concrete.all.binding.json",
                &binding, &binding_report),
            "PSG-8 field and binding load for PSG-9 shell");
        binding.displacement_scale = 0.01;
        expect_true(ProceduralSurfaceBindingV1_Validate(
                        &binding, &surface_graph, &binding_report),
                    "adjusted pitted-concrete binding validates");
        ProceduralSurfaceShellConfig_Init(&shell_config);
        shell_config.target_edge_length_units = 1.0;
        shell_config.max_refinement_levels = 0u;
        shell_compiled = ProceduralSurfaceShell_Compile(
            &documents[2], &surface_graph, &binding, &shell_config,
            "psg9_tunnel_pitted_concrete", &textured, &materials,
            &shell_summary, &shell_report);
        if (!shell_compiled) {
            fprintf(stderr, "PSG-9 surface integration failed: %s %s %s\n",
                    ProceduralSurfaceShellStatus_Name(shell_report.status),
                    shell_report.field, shell_report.message);
        }
        expect_true(
            shell_compiled &&
            materials != NULL &&
            shell_summary.vertex_count == summaries[2].vertex_count &&
            shell_summary.triangle_count == summaries[2].triangle_count &&
            shell_summary.boundary_edge_count == 0u &&
            shell_summary.nonmanifold_edge_count == 0u &&
            shell_summary.connected_component_count == 1u &&
            shell_summary.euler_characteristic == 0 &&
            shell_summary.max_abs_displacement_units > 0.0,
            "fresh PSG-9 shell accepts PSG-8 field, material, and displacement");
        free(materials);
        core_mesh_asset_runtime_document_free(&textured);
    }

    core_mesh_asset_runtime_document_init(&repeat);
    expect_true(ProceduralSolidMesh_Compile(
                    &graphs[2], &sources, &config, "psg9_fixture_2",
                    &repeat, &repeat_summary, &mesh_report),
                "repeat solid compile succeeds");
    expect_true(
        repeat.vertex_count == documents[2].vertex_count &&
        repeat.triangle_count == documents[2].triangle_count &&
        memcmp(repeat.vertices, documents[2].vertices,
               repeat.vertex_count * sizeof(*repeat.vertices)) == 0 &&
        memcmp(repeat.triangles, documents[2].triangles,
               repeat.triangle_count * sizeof(*repeat.triangles)) == 0 &&
        strcmp(repeat_summary.mesh_digest_sha256,
               summaries[2].mesh_digest_sha256) == 0,
        "repeat solid extraction is byte deterministic");

    {
        ProceduralSolidGraphV1 split = disconnected_spheres_graph();
        CoreMeshAssetRuntimeDocument split_mesh;
        ProceduralSolidMeshSummary split_summary;
        core_mesh_asset_runtime_document_init(&split_mesh);
        expect_true(!ProceduralSolidMesh_Compile(
                        &split, NULL, &config, "split_rejected",
                        &split_mesh, &split_summary, &mesh_report) &&
                        mesh_report.status ==
                            PROCEDURAL_SOLID_MESH_STATUS_COMPONENT_POLICY,
                    "default one-component policy rejects split result");
        config.max_components = 2u;
        expect_true(ProceduralSolidMesh_Compile(
                        &split, NULL, &config, "split_allowed",
                        &split_mesh, &split_summary, &mesh_report) &&
                        split_summary.connected_component_count == 2u &&
                        split_summary.euler_characteristic == 4,
                    "explicit component policy permits two derived solids");
        core_mesh_asset_runtime_document_free(&split_mesh);
        config.max_components = 1u;
    }
    {
        ProceduralSolidMeshConfig clipped = config;
        CoreMeshAssetRuntimeDocument clipped_mesh;
        ProceduralSolidMeshSummary clipped_summary;
        clipped.bounds_min = (CoreObjectVec3){-0.4, -0.4, -0.4};
        clipped.bounds_max = (CoreObjectVec3){0.4, 0.4, 0.4};
        core_mesh_asset_runtime_document_init(&clipped_mesh);
        expect_true(!ProceduralSolidMesh_Compile(
                        &graphs[3], &sources, &clipped, "clipped",
                        &clipped_mesh, &clipped_summary, &mesh_report) &&
                        mesh_report.status ==
                            PROCEDURAL_SOLID_MESH_STATUS_DOMAIN_CLIPPED,
                    "sampling-domain clipping is rejected transactionally");
        core_mesh_asset_runtime_document_free(&clipped_mesh);
    }
    {
        ProceduralSolidMeshConfig bounded = config;
        CoreMeshAssetRuntimeDocument bounded_mesh;
        ProceduralSolidMeshSummary bounded_summary;
        bounded.max_vertices = 32u;
        core_mesh_asset_runtime_document_init(&bounded_mesh);
        expect_true(!ProceduralSolidMesh_Compile(
                        &graphs[0], &sources, &bounded, "capacity_rejected",
                        &bounded_mesh, &bounded_summary, &mesh_report) &&
                        mesh_report.status ==
                            PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
                    "derived-shell vertex capacity is enforced");
        core_mesh_asset_runtime_document_free(&bounded_mesh);
    }

    printf("PSG-9 solid summary: transformed=%zu/%zu twisted=%zu/%zu "
           "tunnel=%zu/%zu euler=%d source=%zu/%zu digest=%s\n",
           summaries[0].vertex_count, summaries[0].triangle_count,
           summaries[1].vertex_count, summaries[1].triangle_count,
           summaries[2].vertex_count, summaries[2].triangle_count,
           summaries[2].euler_characteristic,
           summaries[4].vertex_count, summaries[4].triangle_count,
           summaries[2].mesh_digest_sha256);

    core_mesh_asset_runtime_document_free(&repeat);
    for (size_t i = 0u; i < 5u; ++i) {
        core_mesh_asset_runtime_document_free(&documents[i]);
    }
    core_mesh_asset_runtime_document_free(&source);
    return failures == 0 ? 0 : 1;
}
