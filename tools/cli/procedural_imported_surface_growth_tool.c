#include "procedural/procedural_imported_surface_growth.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *mesh_path;
    const char *region_path;
    const char *output_path;
    const char *growth_asset_id;
    const char *summary_path;
    const char *provenance_path;
    double threshold;
    double radius;
    double height;
    double attachment_depth;
    size_t max_elements;
    bool has_threshold;
    bool has_radius;
    bool has_height;
    bool has_attachment_depth;
    bool has_max_elements;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --mesh FILE --region FILE --out FILE "
            "--growth-asset-id ID --summary-out FILE "
            "--provenance-out FILE [--threshold N] [--radius N] "
            "[--height N] [--attachment-depth N] [--max-elements N]\n",
            program);
}

static bool parse_double_value(const char *text, double *out) {
    char *end = NULL;
    double value;
    if (!text || !out) return false;
    value = strtod(text, &end);
    if (!end || *end != '\0') return false;
    *out = value;
    return true;
}

static bool parse_size_value(const char *text, size_t *out) {
    char *end = NULL;
    unsigned long long value;
    if (!text || !out || text[0] == '-') return false;
    value = strtoull(text, &end, 10);
    if (!end || *end != '\0' || value > SIZE_MAX) return false;
    *out = (size_t)value;
    return true;
}

static bool parse_options(int argc, char **argv, Options *out) {
    Options options = {0};
    if (!out) return false;
    for (int i = 1; i < argc; ++i) {
#define VALUE_OPTION(flag_, field_) \
        if (strcmp(argv[i], (flag_)) == 0 && i + 1 < argc) { \
            options.field_ = argv[++i]; \
            continue; \
        }
        VALUE_OPTION("--mesh", mesh_path)
        VALUE_OPTION("--region", region_path)
        VALUE_OPTION("--out", output_path)
        VALUE_OPTION("--growth-asset-id", growth_asset_id)
        VALUE_OPTION("--summary-out", summary_path)
        VALUE_OPTION("--provenance-out", provenance_path)
#undef VALUE_OPTION
        if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            options.has_threshold =
                parse_double_value(argv[++i], &options.threshold);
            if (!options.has_threshold) return false;
            continue;
        }
        if (strcmp(argv[i], "--radius") == 0 && i + 1 < argc) {
            options.has_radius =
                parse_double_value(argv[++i], &options.radius);
            if (!options.has_radius) return false;
            continue;
        }
        if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            options.has_height =
                parse_double_value(argv[++i], &options.height);
            if (!options.has_height) return false;
            continue;
        }
        if (strcmp(argv[i], "--attachment-depth") == 0 &&
            i + 1 < argc) {
            options.has_attachment_depth = parse_double_value(
                argv[++i], &options.attachment_depth);
            if (!options.has_attachment_depth) return false;
            continue;
        }
        if (strcmp(argv[i], "--max-elements") == 0 && i + 1 < argc) {
            options.has_max_elements =
                parse_size_value(argv[++i], &options.max_elements);
            if (!options.has_max_elements) return false;
            continue;
        }
        return false;
    }
    if (!options.mesh_path || !options.region_path || !options.output_path ||
        !options.growth_asset_id || !options.summary_path ||
        !options.provenance_path) return false;
    *out = options;
    return true;
}

static void add_string(
    json_object *root, const char *key, const char *value) {
    json_object_object_add(root, key, json_object_new_string(value));
}

static void add_size(json_object *root, const char *key, size_t value) {
    json_object_object_add(
        root, key, json_object_new_int64((int64_t)value));
}

static void add_double(json_object *root, const char *key, double value) {
    json_object_object_add(root, key, json_object_new_double(value));
}

static void add_bool(json_object *root, const char *key, bool value) {
    json_object_object_add(root, key, json_object_new_boolean(value));
}

static json_object *receipt_json(
    const ProceduralImportedSurfaceGrowthReceipt *receipt) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    add_string(root, "schema", PROCEDURAL_IMPORTED_SURFACE_GROWTH_SCHEMA);
    add_size(root, "schema_version", receipt->schema_version);
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "semantic_source_id", receipt->semantic_source_id);
    add_string(root, "growth_asset_id", receipt->growth_asset_id);
    add_string(root, "region_id", receipt->region_id);
    add_string(root, "source_mesh_digest_sha256",
               receipt->source_mesh_digest_sha256);
    add_string(root, "source_file_digest_sha256",
               receipt->source_file_digest_sha256);
    add_string(root, "carrier_value_digest_sha256",
               receipt->carrier_value_digest_sha256);
    add_string(root, "carrier_file_digest_sha256",
               receipt->carrier_file_digest_sha256);
    add_string(root, "config_digest_sha256", receipt->config_digest_sha256);
    add_string(root, "growth_mesh_digest_sha256",
               receipt->growth_mesh_digest_sha256);
    add_string(root, "provenance_digest_sha256",
               receipt->provenance_digest_sha256);
#define ADD_SIZE_FIELD(name_) add_size(root, #name_, receipt->name_)
    ADD_SIZE_FIELD(source_vertex_count);
    ADD_SIZE_FIELD(source_triangle_count);
    ADD_SIZE_FIELD(candidate_triangle_count);
    ADD_SIZE_FIELD(growth_element_count);
    ADD_SIZE_FIELD(rejected_clearance_candidate_count);
    ADD_SIZE_FIELD(exposed_growth_triangle_count);
    ADD_SIZE_FIELD(attachment_base_triangle_count);
    ADD_SIZE_FIELD(growth_vertex_count);
    ADD_SIZE_FIELD(growth_triangle_count);
    ADD_SIZE_FIELD(unique_edge_count);
    ADD_SIZE_FIELD(boundary_edge_count);
    ADD_SIZE_FIELD(nonmanifold_edge_count);
    ADD_SIZE_FIELD(connected_component_count);
    ADD_SIZE_FIELD(inter_element_overlap_pair_count);
    ADD_SIZE_FIELD(self_intersection_pair_count);
#undef ADD_SIZE_FIELD
    json_object_object_add(
        root, "euler_characteristic",
        json_object_new_int(receipt->euler_characteristic));
    add_double(root, "signed_volume_units3",
               receipt->signed_volume_units3);
    add_double(root, "minimum_attachment_depth_units",
               receipt->minimum_attachment_depth_units);
    add_double(root, "maximum_growth_height_units",
               receipt->maximum_growth_height_units);
    add_double(root, "minimum_inter_element_clearance_units",
               receipt->minimum_inter_element_clearance_units);
#define ADD_BOOL_FIELD(name_) add_bool(root, #name_, receipt->name_)
    ADD_BOOL_FIELD(source_mesh_immutable);
    ADD_BOOL_FIELD(exact_source_and_carrier_binding);
    ADD_BOOL_FIELD(source_triangle_mapping_retained);
    ADD_BOOL_FIELD(attachment_penetration_verified);
    ADD_BOOL_FIELD(overlap_gate_passed);
    ADD_BOOL_FIELD(self_intersection_gate_passed);
    ADD_BOOL_FIELD(closed_valid_growth_shells);
    ADD_BOOL_FIELD(replaceable_attached_geometry);
#undef ADD_BOOL_FIELD
    return root;
}

static bool write_json(const char *path, json_object *root) {
    return path && root &&
        json_object_to_file_ext(
            path, root,
            JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED) == 0;
}

static bool write_provenance(
    const char *path,
    const ProceduralImportedSurfaceGrowthReceipt *receipt,
    const ProceduralImportedSurfaceGrowthProvenance *provenance) {
    json_object *root = json_object_new_object();
    json_object *triangles = json_object_new_array();
    if (!root || !triangles) goto fail;
    add_string(root, "schema",
               "ray_tracing.procedural_imported_surface_growth_provenance");
    add_size(root, "schema_version", 1u);
    add_string(root, "growth_asset_id", receipt->growth_asset_id);
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "provenance_digest_sha256",
               receipt->provenance_digest_sha256);
    add_size(root, "triangle_count", provenance->triangle_count);
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        json_object *entry = json_object_new_object();
        if (!entry) goto fail;
        add_size(entry, "triangle_index", i);
        add_size(entry, "source_triangle_index",
                 provenance->source_triangle_indices[i]);
        add_size(entry, "growth_element_index",
                 provenance->growth_element_indices[i]);
        add_string(entry, "role",
                   ProceduralImportedSurfaceGrowthRole_Name(
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

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument source;
    CoreMeshAssetRuntimeDocument growth;
    ProceduralImportedSurfaceRegionV1 region;
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralImportedSurfaceGrowthConfig config;
    ProceduralImportedSurfaceGrowthProvenance provenance;
    ProceduralImportedSurfaceGrowthReceipt growth_receipt = {0};
    ProceduralImportedSurfaceGrowthReport report = {0};
    CoreResult core_result;
    json_object *summary = NULL;
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&source);
    core_mesh_asset_runtime_document_init(&growth);
    ProceduralImportedSurfaceRegionV1_Init(&region);
    ProceduralImportedSurfaceGrowthConfig_Init(&config);
    ProceduralImportedSurfaceGrowthProvenance_Init(&provenance);
    if (options.has_threshold) config.selection_threshold = options.threshold;
    if (options.has_radius) config.mound_radius_units = options.radius;
    if (options.has_height) config.mound_height_units = options.height;
    if (options.has_attachment_depth)
        config.attachment_depth_units = options.attachment_depth;
    if (options.has_max_elements)
        config.max_growth_elements = options.max_elements;
    core_result = core_mesh_asset_runtime_document_load_file(
        options.mesh_path, &source);
    if (core_result.code != CORE_OK ||
        !ProceduralImportedSurfaceRegionV1_LoadJsonFile(
            options.region_path, &source, options.mesh_path,
            &region, &region_report) ||
        !ProceduralImportedSurfaceGrowth_Compile(
            &source, options.mesh_path, &region, options.region_path,
            &config, options.growth_asset_id, &growth, &provenance,
            &growth_receipt, &report)) {
        fprintf(stderr, "growth compile failed: %s%s%s\n",
                core_result.code != CORE_OK ? core_result.message : "",
                core_result.code != CORE_OK ? "; " : "",
                report.message[0] ? report.message : region_report.message);
        goto cleanup;
    }
    core_result = core_mesh_asset_runtime_document_save_file(
        &growth, options.output_path);
    summary = receipt_json(&growth_receipt);
    if (core_result.code != CORE_OK || !summary ||
        !write_json(options.summary_path, summary) ||
        !write_provenance(
            options.provenance_path, &growth_receipt, &provenance)) {
        fprintf(stderr, "growth artifact write failed: %s\n",
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
    core_mesh_asset_runtime_document_free(&growth);
    ProceduralImportedSurfaceRegionV1_Free(&region);
    ProceduralImportedSurfaceGrowthProvenance_Free(&provenance);
    return exit_code;
}
