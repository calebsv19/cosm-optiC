#include "procedural/procedural_imported_surface_inset.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"

#include <json-c/json.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *mesh_path;
    const char *region_path;
    const char *output_path;
    const char *summary_path;
    const char *provenance_path;
    const char *solid_receipt_path;
    const char *derived_asset_id;
    double threshold;
    double depth;
    double depth_variation;
    double target_boundary_edge_length;
    size_t adaptive_passes;
    size_t minimum_component_triangles;
    bool has_threshold;
    bool has_depth;
    bool has_depth_variation;
    bool has_target_boundary_edge_length;
    bool has_adaptive_passes;
    bool has_minimum_component_triangles;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
        "usage: %s --mesh PATH --region PATH --out PATH "
        "--derived-asset-id ID [--summary-out PATH] "
        "[--provenance-out PATH] [--solid-receipt-out PATH] "
        "[--threshold VALUE] [--depth VALUE] "
        "[--depth-variation VALUE] [--adaptive-passes COUNT] "
        "[--target-boundary-edge-length VALUE] "
        "[--minimum-component-triangles COUNT]\n", program);
}

static bool parse_double(const char *text, double *out) {
    char *end = NULL;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || !end || end == text || *end != '\0') return false;
    *out = value;
    return true;
}

static bool parse_size(const char *text, size_t *out) {
    char *end = NULL;
    unsigned long long value;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || end == text || *end != '\0' ||
        value > SIZE_MAX) return false;
    *out = (size_t)value;
    return true;
}

static bool parse_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc) return false;
        if (strcmp(argv[i], "--mesh") == 0)
            options->mesh_path = argv[++i];
        else if (strcmp(argv[i], "--region") == 0)
            options->region_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0)
            options->output_path = argv[++i];
        else if (strcmp(argv[i], "--summary-out") == 0)
            options->summary_path = argv[++i];
        else if (strcmp(argv[i], "--provenance-out") == 0)
            options->provenance_path = argv[++i];
        else if (strcmp(argv[i], "--solid-receipt-out") == 0)
            options->solid_receipt_path = argv[++i];
        else if (strcmp(argv[i], "--derived-asset-id") == 0)
            options->derived_asset_id = argv[++i];
        else if (strcmp(argv[i], "--threshold") == 0) {
            options->has_threshold =
                parse_double(argv[++i], &options->threshold);
            if (!options->has_threshold) return false;
        } else if (strcmp(argv[i], "--depth") == 0) {
            options->has_depth = parse_double(argv[++i], &options->depth);
            if (!options->has_depth) return false;
        } else if (strcmp(argv[i], "--depth-variation") == 0) {
            options->has_depth_variation =
                parse_double(argv[++i], &options->depth_variation);
            if (!options->has_depth_variation) return false;
        } else if (strcmp(argv[i], "--adaptive-passes") == 0) {
            options->has_adaptive_passes =
                parse_size(argv[++i], &options->adaptive_passes);
            if (!options->has_adaptive_passes) return false;
        } else if (strcmp(
                       argv[i], "--target-boundary-edge-length") == 0) {
            options->has_target_boundary_edge_length = parse_double(
                argv[++i], &options->target_boundary_edge_length);
            if (!options->has_target_boundary_edge_length) return false;
        } else if (strcmp(
                       argv[i], "--minimum-component-triangles") == 0) {
            options->has_minimum_component_triangles = parse_size(
                argv[++i], &options->minimum_component_triangles);
            if (!options->has_minimum_component_triangles) return false;
        } else {
            return false;
        }
    }
    return options->mesh_path && options->region_path &&
        options->output_path && options->derived_asset_id;
}

static bool write_json(const char *path, json_object *root) {
    const char *text;
    CoreResult result;
    if (!path) return true;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    return result.code == CORE_OK;
}

static void add_string(
    json_object *root,
    const char *key,
    const char *value) {
    json_object_object_add(root, key, json_object_new_string(value));
}

static void add_size(
    json_object *root,
    const char *key,
    size_t value) {
    json_object_object_add(
        root, key, json_object_new_int64((int64_t)value));
}

static json_object *receipt_json(
    const ProceduralImportedSurfaceInsetReceipt *receipt) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    add_string(root, "schema", PROCEDURAL_IMPORTED_SURFACE_INSET_SCHEMA);
    json_object_object_add(root, "schema_version",
        json_object_new_int((int)receipt->schema_version));
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "semantic_source_id", receipt->semantic_source_id);
    add_string(root, "derived_asset_id", receipt->derived_asset_id);
    add_string(root, "region_id", receipt->region_id);
    add_string(root, "source_mesh_digest_sha256",
               receipt->source_mesh_digest_sha256);
    add_string(root, "source_file_digest_sha256",
               receipt->source_file_digest_sha256);
    add_string(root, "carrier_value_digest_sha256",
               receipt->carrier_value_digest_sha256);
    add_string(root, "carrier_file_digest_sha256",
               receipt->carrier_file_digest_sha256);
    add_string(root, "config_digest_sha256",
               receipt->config_digest_sha256);
    add_string(root, "derived_mesh_digest_sha256",
               receipt->derived_mesh_digest_sha256);
    add_string(root, "provenance_digest_sha256",
               receipt->provenance_digest_sha256);
    add_size(root, "source_vertex_count", receipt->source_vertex_count);
    add_size(root, "source_triangle_count", receipt->source_triangle_count);
    add_size(root, "refined_vertex_count", receipt->refined_vertex_count);
    add_size(root, "refined_triangle_count", receipt->refined_triangle_count);
    add_size(root, "transition_source_triangle_count",
             receipt->transition_source_triangle_count);
    add_size(root, "adaptive_refinement_pass_count",
             receipt->adaptive_refinement_pass_count);
    add_size(root, "discarded_candidate_triangle_count",
             receipt->discarded_candidate_triangle_count);
    add_size(root, "selected_component_count",
             receipt->selected_component_count);
    add_size(root, "selected_refined_triangle_count",
             receipt->selected_refined_triangle_count);
    add_size(root, "retained_triangle_count",
             receipt->retained_triangle_count);
    add_size(root, "transition_wall_triangle_count",
             receipt->transition_wall_triangle_count);
    add_size(root, "inset_floor_triangle_count",
             receipt->inset_floor_triangle_count);
    add_size(root, "boundary_loop_count", receipt->boundary_loop_count);
    add_size(root, "boundary_ring_edge_count",
             receipt->boundary_ring_edge_count);
    add_size(root, "derived_vertex_count", receipt->derived_vertex_count);
    add_size(root, "derived_triangle_count", receipt->derived_triangle_count);
    add_size(root, "unique_edge_count", receipt->unique_edge_count);
    add_size(root, "boundary_edge_count", receipt->boundary_edge_count);
    add_size(root, "nonmanifold_edge_count",
             receipt->nonmanifold_edge_count);
    add_size(root, "connected_component_count",
             receipt->connected_component_count);
    json_object_object_add(root, "euler_characteristic",
        json_object_new_int(receipt->euler_characteristic));
    json_object_object_add(root, "signed_volume_units3",
        json_object_new_double(receipt->signed_volume_units3));
    json_object_object_add(root, "minimum_inset_depth_units",
        json_object_new_double(receipt->minimum_inset_depth_units));
    json_object_object_add(root, "maximum_inset_depth_units",
        json_object_new_double(receipt->maximum_inset_depth_units));
    json_object_object_add(root, "target_boundary_edge_length_units",
        json_object_new_double(receipt->target_boundary_edge_length_units));
    json_object_object_add(root, "initial_max_boundary_edge_length_units",
        json_object_new_double(
            receipt->initial_max_boundary_edge_length_units));
    json_object_object_add(root, "final_max_boundary_edge_length_units",
        json_object_new_double(receipt->final_max_boundary_edge_length_units));
#define ADD_BOOL(name) \
    json_object_object_add(root, #name, \
        json_object_new_boolean(receipt->name))
    ADD_BOOL(transition_refinement_active);
    ADD_BOOL(adaptive_refinement_active);
    ADD_BOOL(adaptive_refinement_converged);
    ADD_BOOL(source_mesh_immutable);
    ADD_BOOL(exact_source_and_carrier_binding);
    ADD_BOOL(source_triangle_mapping_retained);
    ADD_BOOL(explicit_region_transition_topology);
    ADD_BOOL(replaceable_derived_geometry);
    ADD_BOOL(closed_valid_shell);
#undef ADD_BOOL
    return root;
}

static bool write_provenance(
    const char *path,
    const ProceduralImportedSurfaceInsetReceipt *receipt,
    const ProceduralImportedSurfaceInsetProvenance *provenance) {
    json_object *root;
    json_object *triangles;
    if (!path) return true;
    root = json_object_new_object();
    triangles = json_object_new_array();
    if (!root || !triangles) goto fail;
    add_string(root, "schema",
               "ray_tracing.procedural_imported_surface_inset_provenance");
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "derived_asset_id", receipt->derived_asset_id);
    add_string(root, "source_mesh_digest_sha256",
               receipt->source_mesh_digest_sha256);
    add_string(root, "carrier_value_digest_sha256",
               receipt->carrier_value_digest_sha256);
    add_string(root, "derived_mesh_digest_sha256",
               receipt->derived_mesh_digest_sha256);
    add_string(root, "provenance_digest_sha256",
               receipt->provenance_digest_sha256);
    add_size(root, "triangle_count", provenance->triangle_count);
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        json_object *entry = json_object_new_object();
        if (!entry) goto fail;
        add_size(entry, "derived_triangle_index", i);
        add_size(entry, "source_triangle_index",
                 provenance->source_triangle_indices[i]);
        add_string(entry, "role",
                   ProceduralImportedSurfaceInsetRole_Name(
                       provenance->roles[i]));
        json_object_array_add(triangles, entry);
    }
    json_object_object_add(root, "triangles", triangles);
    triangles = NULL;
    {
        const bool ok = write_json(path, root);
        json_object_put(root);
        return ok;
    }
fail:
    if (triangles) json_object_put(triangles);
    if (root) json_object_put(root);
    return false;
}

static bool write_solid_receipt(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const ProceduralImportedSurfaceInsetReceipt *receipt) {
    json_object *root;
    json_object *regions;
    char canonical[1024];
    char region_digest[RAY_TRACING_SHA256_HEX_SIZE] = {0};
    size_t canonical_length = 0u;
    if (!path) return true;
    for (size_t i = 0u; i < mesh->surface_group_count; ++i) {
        const char *group_id = mesh->surface_groups[i].group_id;
        const char *kind =
            strcmp(group_id, "retained_surface") == 0 ? "retained" :
            (strcmp(group_id, "transition_wall") == 0 ? "blend" : "cut");
        const int count = snprintf(
            canonical + canonical_length,
            sizeof(canonical) - canonical_length,
            "%s|%s|%s|%s|%zu;",
            mesh->surface_groups[i].group_id,
            kind,
            "imported_surface_inset",
            receipt->region_id,
            mesh->surface_groups[i].triangle_count);
        if (count < 0 ||
            (size_t)count >= sizeof(canonical) - canonical_length) {
            return false;
        }
        canonical_length += (size_t)count;
    }
    if (!ray_tracing_sha256_bytes(
            canonical, canonical_length, region_digest)) {
        return false;
    }
    root = json_object_new_object();
    regions = json_object_new_array();
    if (!root || !regions) goto fail;
    add_string(root, "schema", "ray_tracing.procedural_solid_receipt");
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    add_string(root, "asset_id", mesh->contract.asset_id);
    add_string(root, "semantic_source_id", receipt->source_asset_id);
    add_string(root, "mesh_digest_sha256",
               receipt->derived_mesh_digest_sha256);
    add_string(root, "region_digest_sha256", region_digest);
    for (size_t i = 0u; i < mesh->surface_group_count; ++i) {
        json_object *entry = json_object_new_object();
        const char *group_id = mesh->surface_groups[i].group_id;
        const char *kind =
            strcmp(group_id, "retained_surface") == 0 ? "retained" :
            (strcmp(group_id, "transition_wall") == 0 ? "blend" : "cut");
        if (!entry) goto fail;
        add_string(entry, "region_id", mesh->surface_groups[i].group_id);
        add_string(entry, "kind", kind);
        add_string(entry, "primary_node_id", "imported_surface_inset");
        add_string(entry, "secondary_node_id", receipt->region_id);
        add_size(entry, "triangle_count",
                 mesh->surface_groups[i].triangle_count);
        json_object_array_add(regions, entry);
    }
    json_object_object_add(root, "regions", regions);
    regions = NULL;
    {
        const bool ok = write_json(path, root);
        json_object_put(root);
        return ok;
    }
fail:
    if (regions) json_object_put(regions);
    if (root) json_object_put(root);
    return false;
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument source;
    CoreMeshAssetRuntimeDocument derived;
    ProceduralImportedSurfaceRegionV1 region;
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralImportedSurfaceInsetConfig config;
    ProceduralImportedSurfaceInsetProvenance provenance;
    ProceduralImportedSurfaceInsetReceipt receipt = {0};
    ProceduralImportedSurfaceInsetReport report = {0};
    CoreResult core_result;
    json_object *summary = NULL;
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&source);
    core_mesh_asset_runtime_document_init(&derived);
    ProceduralImportedSurfaceRegionV1_Init(&region);
    ProceduralImportedSurfaceInsetConfig_Init(&config);
    ProceduralImportedSurfaceInsetProvenance_Init(&provenance);
    if (options.has_threshold) config.selection_threshold = options.threshold;
    if (options.has_depth) config.depth_units = options.depth;
    if (options.has_depth_variation)
        config.depth_variation = options.depth_variation;
    if (options.has_adaptive_passes)
        config.max_adaptive_refinement_passes = options.adaptive_passes;
    if (options.has_target_boundary_edge_length)
        config.target_boundary_edge_length_units =
            options.target_boundary_edge_length;
    if (options.has_minimum_component_triangles)
        config.minimum_selected_component_triangles =
            options.minimum_component_triangles;
    core_result = core_mesh_asset_runtime_document_load_file(
        options.mesh_path, &source);
    if (core_result.code != CORE_OK ||
        !ProceduralImportedSurfaceRegionV1_LoadJsonFile(
            options.region_path, &source, options.mesh_path,
            &region, &region_report) ||
        !ProceduralImportedSurfaceInset_Compile(
            &source, options.mesh_path, &region, options.region_path,
            &config, options.derived_asset_id, &derived, &provenance,
            &receipt, &report)) {
        fprintf(stderr, "inset compile failed: %s%s%s\n",
                core_result.code != CORE_OK ? core_result.message : "",
                core_result.code != CORE_OK ? "; " : "",
                report.message[0] ? report.message : region_report.message);
        goto cleanup;
    }
    core_result = core_mesh_asset_runtime_document_save_file(
        &derived, options.output_path);
    summary = receipt_json(&receipt);
    if (core_result.code != CORE_OK || !summary ||
        !write_json(options.summary_path, summary) ||
        !write_provenance(
            options.provenance_path, &receipt, &provenance) ||
        !write_solid_receipt(
            options.solid_receipt_path, &derived, &receipt)) {
        fprintf(stderr, "inset artifact write failed: %s\n",
                core_result.code != CORE_OK ? core_result.message :
                "receipt or provenance write failed");
        goto cleanup;
    }
    printf("%s\n", json_object_to_json_string_ext(
        summary, JSON_C_TO_STRING_PLAIN));
    exit_code = 0;
cleanup:
    if (summary) json_object_put(summary);
    core_mesh_asset_runtime_document_free(&source);
    core_mesh_asset_runtime_document_free(&derived);
    ProceduralImportedSurfaceRegionV1_Free(&region);
    ProceduralImportedSurfaceInsetProvenance_Free(&provenance);
    return exit_code;
}
