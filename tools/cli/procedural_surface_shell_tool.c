#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include "core_mesh_asset.h"
#include "procedural/procedural_surface_shell.h"

#include <json-c/json.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *source;
    const char *graph;
    const char *binding;
    const char *output;
    const char *summary;
    const char *asset_id;
    const char *expected_source_digest;
    const char *expected_graph_digest;
    const char *expected_binding_digest;
    double target_edge;
    double max_displacement_ratio;
    size_t max_vertices;
    size_t max_triangles;
    unsigned int max_levels;
} Options;

static int parse_size(const char *text, size_t *out) {
    char *end = NULL;
    unsigned long long value;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || !end || *end || value > SIZE_MAX) return 0;
    *out = (size_t)value;
    return 1;
}

static int parse_uint(const char *text, unsigned int *out) {
    size_t value;
    if (!parse_size(text, &value) || value > UINT_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}

static int parse_double(const char *text, double *out) {
    char *end = NULL;
    errno = 0;
    *out = strtod(text, &end);
    return !errno && end && !*end;
}

static int parse_options(int argc, char **argv, Options *options) {
    *options = (Options){
        .target_edge = 0.1,
        .max_displacement_ratio = 0.45,
        .max_vertices = 1000000u,
        .max_triangles = 2000000u,
        .max_levels = 6u
    };
#define STRING_OPTION(NAME, FIELD)                                             \
    if (strcmp(argv[i], (NAME)) == 0 && i + 1 < argc) {                       \
        options->FIELD = argv[++i];                                            \
    }
    for (int i = 1; i < argc; ++i) {
        STRING_OPTION("--source", source)
        else STRING_OPTION("--graph", graph)
        else STRING_OPTION("--binding", binding)
        else STRING_OPTION("--out", output)
        else STRING_OPTION("--summary-out", summary)
        else STRING_OPTION("--asset-id", asset_id)
        else STRING_OPTION("--expected-source-digest", expected_source_digest)
        else STRING_OPTION("--expected-graph-digest", expected_graph_digest)
        else STRING_OPTION("--expected-binding-digest", expected_binding_digest)
        else if (strcmp(argv[i], "--target-edge") == 0 && i + 1 < argc) {
            if (!parse_double(argv[++i], &options->target_edge)) return 0;
        } else if (strcmp(argv[i], "--max-displacement-ratio") == 0 &&
                   i + 1 < argc) {
            if (!parse_double(argv[++i],
                              &options->max_displacement_ratio)) return 0;
        } else if (strcmp(argv[i], "--max-vertices") == 0 && i + 1 < argc) {
            if (!parse_size(argv[++i], &options->max_vertices)) return 0;
        } else if (strcmp(argv[i], "--max-triangles") == 0 && i + 1 < argc) {
            if (!parse_size(argv[++i], &options->max_triangles)) return 0;
        } else if (strcmp(argv[i], "--max-levels") == 0 && i + 1 < argc) {
            if (!parse_uint(argv[++i], &options->max_levels)) return 0;
        } else {
            return 0;
        }
    }
#undef STRING_OPTION
    return options->source && options->graph && options->binding &&
           options->output && options->summary && options->asset_id &&
           options->expected_source_digest &&
           options->expected_graph_digest &&
           options->expected_binding_digest;
}

static int digest_matches(const char *actual, const char *expected) {
    return ray_tracing_sha256_is_valid_hex(expected) &&
           strcmp(actual, expected) == 0;
}

static int write_summary(const Options *options,
                         const ProceduralSurfaceShellSummary *summary,
                         const char *source_digest,
                         const char *graph_digest,
                         const char *binding_digest) {
    json_object *root = json_object_new_object();
    const char *serialized;
    CoreResult write_result;
    if (!root) return 0;
#define ADD_INT(KEY, VALUE)                                                     \
    json_object_object_add(root, (KEY), json_object_new_int64((int64_t)(VALUE)))
#define ADD_DOUBLE(KEY, VALUE)                                                  \
    json_object_object_add(root, (KEY), json_object_new_double((VALUE)))
    json_object_object_add(
        root, "schema",
        json_object_new_string("ray_tracing.procedural_surface_shell_receipt"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "asset_id",
                           json_object_new_string(options->asset_id));
    json_object_object_add(root, "source_digest_sha256",
                           json_object_new_string(source_digest));
    json_object_object_add(root, "field_graph_digest_sha256",
                           json_object_new_string(graph_digest));
    json_object_object_add(root, "binding_digest_sha256",
                           json_object_new_string(binding_digest));
    ADD_INT("source_vertex_count", summary->source_vertex_count);
    ADD_INT("source_triangle_count", summary->source_triangle_count);
    ADD_INT("vertex_count", summary->vertex_count);
    ADD_INT("triangle_count", summary->triangle_count);
    ADD_INT("unique_edge_count", summary->unique_edge_count);
    ADD_INT("boundary_edge_count", summary->boundary_edge_count);
    ADD_INT("nonmanifold_edge_count", summary->nonmanifold_edge_count);
    ADD_INT("connected_component_count", summary->connected_component_count);
    ADD_INT("euler_characteristic", summary->euler_characteristic);
    ADD_INT("refinement_levels", summary->refinement_levels);
    ADD_DOUBLE("source_max_edge_length_units",
               summary->source_max_edge_length_units);
    ADD_DOUBLE("final_max_edge_length_units",
               summary->final_max_edge_length_units);
    ADD_DOUBLE("max_abs_displacement_units",
               summary->max_abs_displacement_units);
    ADD_DOUBLE("signed_volume_units3", summary->signed_volume_units3);
#undef ADD_DOUBLE
#undef ADD_INT
    serialized = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    write_result = core_io_write_all_atomic(
        options->summary, serialized, strlen(serialized));
    json_object_put(root);
    return write_result.code == CORE_OK;
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument source;
    CoreMeshAssetRuntimeDocument output;
    ProceduralSurfaceMaterialSample *materials = NULL;
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfaceShellConfig config;
    ProceduralSurfaceShellSummary summary;
    ProceduralSurfaceShellReport shell_report;
    char source_digest[RAY_TRACING_SHA256_HEX_SIZE];
    char graph_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char binding_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY];
    CoreResult core_result;
    int result = 1;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr,
                "usage: %s --source FILE --graph FILE --binding FILE "
                "--out FILE --summary-out FILE --asset-id ID "
                "--expected-source-digest HEX --expected-graph-digest HEX "
                "--expected-binding-digest HEX [--target-edge N] "
                "[--max-displacement-ratio N] [--max-levels N] "
                "[--max-vertices N] [--max-triangles N]\n",
                argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&source);
    core_mesh_asset_runtime_document_init(&output);
    core_result = core_mesh_asset_runtime_document_load_file(
        options.source, &source);
    if (core_result.code != CORE_OK ||
        !ray_tracing_sha256_file(options.source, source_digest) ||
        !digest_matches(source_digest, options.expected_source_digest)) {
        fprintf(stderr, "source asset invalid or stale\n");
        goto cleanup;
    }
    if (!ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options.graph, &graph, &graph_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            &graph, graph_digest, &graph_report) ||
        !digest_matches(graph_digest, options.expected_graph_digest)) {
        fprintf(stderr, "field graph invalid or stale\n");
        goto cleanup;
    }
    if (!ProceduralSurfaceBindingV1_LoadJsonFile(
            options.binding, &binding, &binding_report) ||
        !ProceduralSurfaceBindingV1_Validate(
            &binding, &graph, &binding_report) ||
        !ProceduralSurfaceBindingV1_Digest(
            &binding, binding_digest, &binding_report) ||
        !digest_matches(binding_digest, options.expected_binding_digest)) {
        fprintf(stderr, "surface binding invalid or stale\n");
        goto cleanup;
    }
    ProceduralSurfaceShellConfig_Init(&config);
    config.target_edge_length_units = options.target_edge;
    config.max_displacement_to_source_edge_ratio =
        options.max_displacement_ratio;
    config.max_refinement_levels = options.max_levels;
    config.max_vertices = options.max_vertices;
    config.max_triangles = options.max_triangles;
    if (!ProceduralSurfaceShell_Compile(
            &source, &graph, &binding, &config, options.asset_id,
            &output, &materials, &summary, &shell_report)) {
        fprintf(stderr, "shell compile failed: %s (%s)\n",
                shell_report.message,
                ProceduralSurfaceShellStatus_Name(shell_report.status));
        goto cleanup;
    }
    core_result = core_mesh_asset_runtime_document_save_file(
        &output, options.output);
    if (core_result.code != CORE_OK ||
        !write_summary(&options, &summary, source_digest, graph_digest,
                       binding_digest)) {
        fprintf(stderr, "unable to save shell output or receipt\n");
        goto cleanup;
    }
    printf("%s\n", options.summary);
    result = 0;
cleanup:
    free(materials);
    core_mesh_asset_runtime_document_free(&source);
    core_mesh_asset_runtime_document_free(&output);
    return result;
}
