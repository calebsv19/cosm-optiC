#include "procedural/procedural_surface_authoring.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static void test_manifest_and_transaction(void) {
    char graph_path[1024];
    char manifest_path[1024];
    char base_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char manifest_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphV1 edited;
    ProceduralSurfaceFieldGraphV1 sentinel;
    ProceduralSurfaceParameterManifestV1 manifest;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceAuthoringReport report;

    fixture_path(graph_path, sizeof(graph_path), "wind_shaped_sand.json");
    fixture_path(manifest_path, sizeof(manifest_path),
                 "wind_shaped_sand.parameters.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    assert(ProceduralSurfaceParameterManifestV1_LoadJsonFile(
        manifest_path, &manifest, &report));
    assert(ProceduralSurfaceParameterManifestV1_Validate(
        &manifest, &graph, &report));
    assert(ProceduralSurfaceParameterManifestV1_Digest(
        &manifest, manifest_digest, &report));
    assert(strlen(manifest_digest) == 64u);
    assert(ProceduralSurfaceFieldGraphV1_Digest(
        &graph, base_digest, &graph_report));

    memset(&edited, 0, sizeof(edited));
    assert(ProceduralSurfaceAuthoring_ApplyParameter(
        &graph, &manifest, base_digest, "dune_spacing", 5.25,
        &edited, &report));
    assert(strcmp(report.base_graph_digest_sha256, base_digest) == 0);
    assert(strcmp(report.result_graph_digest_sha256, base_digest) != 0);
    assert(strcmp(graph.program_id, "wind_shaped_sand_v1") == 0);
    {
        bool found = false;
        for (size_t i = 0u; i < edited.node_count; ++i) {
            if (strcmp(edited.nodes[i].id, "primary_x_frequency") == 0) {
                assert(edited.nodes[i].value == 5.25);
                found = true;
            }
        }
        assert(found);
    }

    memset(&sentinel, 0x5a, sizeof(sentinel));
    {
        ProceduralSurfaceFieldGraphV1 before = sentinel;
        assert(!ProceduralSurfaceAuthoring_ApplyParameter(
            &graph, &manifest,
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "dune_spacing", 7.0, &sentinel, &report));
        assert(report.status ==
               PROCEDURAL_SURFACE_AUTHORING_STATUS_BASE_DIGEST);
        assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
    }
    {
        ProceduralSurfaceFieldGraphV1 before = sentinel;
        assert(!ProceduralSurfaceAuthoring_ApplyParameter(
            &graph, &manifest, base_digest, "dune_spacing", 400.0,
            &sentinel, &report));
        assert(report.status == PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE);
        assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
    }
    printf("authoring manifest_digest=%s base=%s result=%s\n",
           manifest_digest, base_digest, report.result_graph_digest_sha256);
}

static void test_atomic_save_and_restore(void) {
    char graph_path[1024];
    char manifest_path[1024];
    char output_path[1024];
    char base_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char edited_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char loaded_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphV1 edited;
    ProceduralSurfaceFieldGraphV1 loaded;
    ProceduralSurfaceParameterManifestV1 manifest;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceAuthoringReport report;
    fixture_path(graph_path, sizeof(graph_path), "wind_shaped_sand.json");
    fixture_path(manifest_path, sizeof(manifest_path),
                 "wind_shaped_sand.parameters.json");
    snprintf(output_path, sizeof(output_path),
             "/tmp/procedural_surface_authoring_%ld.json", (long)getpid());
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    assert(ProceduralSurfaceParameterManifestV1_LoadJsonFile(
        manifest_path, &manifest, &report));
    assert(ProceduralSurfaceFieldGraphV1_Digest(
        &graph, base_digest, &graph_report));
    assert(ProceduralSurfaceAuthoring_ApplyParameter(
        &graph, &manifest, base_digest, "sand_roughness", 0.77,
        &edited, &report));
    snprintf(edited_digest, sizeof(edited_digest), "%s",
             report.result_graph_digest_sha256);
    assert(ProceduralSurfaceAuthoring_SaveGraphAtomic(
        output_path, &edited, &report));
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        output_path, &loaded, &graph_report));
    assert(ProceduralSurfaceFieldGraphV1_Digest(
        &loaded, loaded_digest, &graph_report));
    assert(strcmp(loaded_digest, edited_digest) == 0);

    /* The same atomic path is also the undo primitive: restore the prior graph. */
    assert(ProceduralSurfaceAuthoring_SaveGraphAtomic(
        output_path, &graph, &report));
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        output_path, &loaded, &graph_report));
    assert(ProceduralSurfaceFieldGraphV1_Digest(
        &loaded, loaded_digest, &graph_report));
    assert(strcmp(loaded_digest, base_digest) == 0);
    unlink(output_path);
}

int main(void) {
    test_manifest_and_transaction();
    test_atomic_save_and_restore();
    return 0;
}
