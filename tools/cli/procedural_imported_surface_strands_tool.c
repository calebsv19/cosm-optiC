#include "procedural/procedural_imported_surface_strands.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *mesh_path;
    const char *region_path;
    const char *tube_path;
    const char *strand_path;
    const char *strand_asset_id;
    const char *summary_path;
    const char *provenance_path;
    double threshold;
    double length;
    double root_radius;
    double tip_radius;
    double root_penetration;
    double bend;
    double curl;
    size_t max_strands;
    bool has_threshold;
    bool has_length;
    bool has_root_radius;
    bool has_tip_radius;
    bool has_root_penetration;
    bool has_bend;
    bool has_curl;
    bool has_max_strands;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --mesh FILE --region FILE --tube-out FILE "
            "--strand-out FILE --strand-asset-id ID --summary-out FILE "
            "--provenance-out FILE [--threshold N] [--length N] "
            "[--root-radius N] [--tip-radius N] [--root-penetration N] "
            "[--bend N] [--curl N] [--max-strands N]\n",
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
        VALUE_OPTION("--tube-out", tube_path)
        VALUE_OPTION("--strand-out", strand_path)
        VALUE_OPTION("--strand-asset-id", strand_asset_id)
        VALUE_OPTION("--summary-out", summary_path)
        VALUE_OPTION("--provenance-out", provenance_path)
#undef VALUE_OPTION
#define DOUBLE_OPTION(flag_, field_, has_) \
        if (strcmp(argv[i], (flag_)) == 0 && i + 1 < argc) { \
            options.has_ = parse_double_value(argv[++i], &options.field_); \
            if (!options.has_) return false; \
            continue; \
        }
        DOUBLE_OPTION("--threshold", threshold, has_threshold)
        DOUBLE_OPTION("--length", length, has_length)
        DOUBLE_OPTION("--root-radius", root_radius, has_root_radius)
        DOUBLE_OPTION("--tip-radius", tip_radius, has_tip_radius)
        DOUBLE_OPTION("--root-penetration", root_penetration,
                      has_root_penetration)
        DOUBLE_OPTION("--bend", bend, has_bend)
        DOUBLE_OPTION("--curl", curl, has_curl)
#undef DOUBLE_OPTION
        if (strcmp(argv[i], "--max-strands") == 0 && i + 1 < argc) {
            options.has_max_strands =
                parse_size_value(argv[++i], &options.max_strands);
            if (!options.has_max_strands) return false;
            continue;
        }
        return false;
    }
    if (!options.mesh_path || !options.region_path || !options.tube_path ||
        !options.strand_path || !options.strand_asset_id ||
        !options.summary_path || !options.provenance_path) return false;
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

static json_object *vec3_json(CoreObjectVec3 value) {
    json_object *array = json_object_new_array();
    if (!array) return NULL;
    json_object_array_add(array, json_object_new_double(value.x));
    json_object_array_add(array, json_object_new_double(value.y));
    json_object_array_add(array, json_object_new_double(value.z));
    return array;
}

static json_object *receipt_json(
    const ProceduralImportedSurfaceStrandReceipt *receipt) {
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    add_string(root, "schema", PROCEDURAL_IMPORTED_SURFACE_STRANDS_SCHEMA);
    add_size(root, "schema_version", receipt->schema_version);
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "semantic_source_id", receipt->semantic_source_id);
    add_string(root, "strand_asset_id", receipt->strand_asset_id);
    add_string(root, "region_id", receipt->region_id);
#define ADD_STRING_FIELD(name_) add_string(root, #name_, receipt->name_)
    ADD_STRING_FIELD(source_mesh_digest_sha256);
    ADD_STRING_FIELD(source_file_digest_sha256);
    ADD_STRING_FIELD(carrier_value_digest_sha256);
    ADD_STRING_FIELD(carrier_file_digest_sha256);
    ADD_STRING_FIELD(config_digest_sha256);
    ADD_STRING_FIELD(strand_data_digest_sha256);
    ADD_STRING_FIELD(tube_mesh_digest_sha256);
    ADD_STRING_FIELD(provenance_digest_sha256);
#undef ADD_STRING_FIELD
#define ADD_SIZE_FIELD(name_) add_size(root, #name_, receipt->name_)
    ADD_SIZE_FIELD(source_vertex_count);
    ADD_SIZE_FIELD(source_triangle_count);
    ADD_SIZE_FIELD(candidate_triangle_count);
    ADD_SIZE_FIELD(rejected_clearance_candidate_count);
    ADD_SIZE_FIELD(strand_count);
    ADD_SIZE_FIELD(control_point_count);
    ADD_SIZE_FIELD(root_cap_triangle_count);
    ADD_SIZE_FIELD(shaft_triangle_count);
    ADD_SIZE_FIELD(tip_cap_triangle_count);
    ADD_SIZE_FIELD(tube_vertex_count);
    ADD_SIZE_FIELD(tube_triangle_count);
    ADD_SIZE_FIELD(boundary_edge_count);
    ADD_SIZE_FIELD(nonmanifold_edge_count);
    ADD_SIZE_FIELD(connected_component_count);
    ADD_SIZE_FIELD(inter_strand_overlap_pair_count);
    ADD_SIZE_FIELD(strand_self_intersection_pair_count);
#undef ADD_SIZE_FIELD
    json_object_object_add(
        root, "euler_characteristic",
        json_object_new_int(receipt->euler_characteristic));
#define ADD_DOUBLE_FIELD(name_) add_double(root, #name_, receipt->name_)
    ADD_DOUBLE_FIELD(signed_volume_units3);
    ADD_DOUBLE_FIELD(minimum_root_penetration_units);
    ADD_DOUBLE_FIELD(minimum_root_clearance_units);
    ADD_DOUBLE_FIELD(minimum_strand_length_units);
    ADD_DOUBLE_FIELD(maximum_strand_length_units);
#undef ADD_DOUBLE_FIELD
#define ADD_BOOL_FIELD(name_) add_bool(root, #name_, receipt->name_)
    ADD_BOOL_FIELD(source_mesh_immutable);
    ADD_BOOL_FIELD(exact_source_and_carrier_binding);
    ADD_BOOL_FIELD(root_triangle_mapping_retained);
    ADD_BOOL_FIELD(root_barycentrics_valid);
    ADD_BOOL_FIELD(root_attachment_verified);
    ADD_BOOL_FIELD(finite_continuous_strands);
    ADD_BOOL_FIELD(overlap_gate_passed);
    ADD_BOOL_FIELD(self_intersection_gate_passed);
    ADD_BOOL_FIELD(closed_valid_tube_shells);
    ADD_BOOL_FIELD(replaceable_strand_asset);
    ADD_BOOL_FIELD(triangle_tube_proof_backend);
#undef ADD_BOOL_FIELD
    return root;
}

static bool write_json(const char *path, json_object *root) {
    return path && root && json_object_to_file_ext(
        path, root,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED) == 0;
}

static bool write_strands(
    const char *path,
    const ProceduralImportedSurfaceStrandReceipt *receipt,
    const ProceduralImportedSurfaceStrandAsset *asset) {
    json_object *root = json_object_new_object();
    json_object *strands = json_object_new_array();
    if (!root || !strands) goto fail;
    add_string(root, "schema",
               "ray_tracing.procedural_imported_surface_strand_asset");
    add_size(root, "schema_version", 1u);
    add_string(root, "strand_asset_id", receipt->strand_asset_id);
    add_string(root, "source_asset_id", receipt->source_asset_id);
    add_string(root, "strand_data_digest_sha256",
               receipt->strand_data_digest_sha256);
    add_size(root, "strand_count", asset->strand_count);
    add_size(root, "points_per_strand", asset->points_per_strand);
    for (size_t strand = 0u; strand < asset->strand_count; ++strand) {
        json_object *entry = json_object_new_object();
        json_object *points = json_object_new_array();
        if (!entry || !points) goto fail;
        add_size(entry, "strand_index", strand);
        add_size(entry, "source_triangle_index",
                 asset->source_triangle_indices[strand]);
        json_object_object_add(
            entry, "root_barycentrics",
            vec3_json(asset->root_barycentrics[strand]));
        json_object_object_add(
            entry, "root_normal", vec3_json(asset->root_normals[strand]));
        json_object_object_add(
            entry, "root_tangent", vec3_json(asset->root_tangents[strand]));
        for (size_t point = 0u;
             point < asset->points_per_strand; ++point) {
            const size_t index = strand * asset->points_per_strand + point;
            json_object *point_entry = json_object_new_object();
            if (!point_entry) goto fail;
            add_size(point_entry, "index", point);
            json_object_object_add(
                point_entry, "position", vec3_json(asset->points[index]));
            add_double(point_entry, "radius", asset->radii[index]);
            json_object_array_add(points, point_entry);
        }
        json_object_object_add(entry, "points", points);
        json_object_array_add(strands, entry);
    }
    json_object_object_add(root, "strands", strands);
    strands = NULL;
    {
        const bool ok = write_json(path, root);
        json_object_put(root);
        return ok;
    }
fail:
    if (strands) json_object_put(strands);
    if (root) json_object_put(root);
    return false;
}

static bool write_provenance(
    const char *path,
    const ProceduralImportedSurfaceStrandReceipt *receipt,
    const ProceduralImportedSurfaceStrandProvenance *provenance) {
    json_object *root = json_object_new_object();
    json_object *triangles = json_object_new_array();
    if (!root || !triangles) goto fail;
    add_string(root, "schema",
               "ray_tracing.procedural_imported_surface_strand_provenance");
    add_size(root, "schema_version", 1u);
    add_string(root, "strand_asset_id", receipt->strand_asset_id);
    add_string(root, "provenance_digest_sha256",
               receipt->provenance_digest_sha256);
    add_size(root, "triangle_count", provenance->triangle_count);
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        json_object *entry = json_object_new_object();
        if (!entry) goto fail;
        add_size(entry, "triangle_index", i);
        add_size(entry, "source_triangle_index",
                 provenance->source_triangle_indices[i]);
        add_size(entry, "strand_index", provenance->strand_indices[i]);
        add_size(entry, "segment_index", provenance->segment_indices[i]);
        add_string(entry, "role",
                   ProceduralImportedSurfaceStrandRole_Name(
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
    CoreMeshAssetRuntimeDocument tubes;
    ProceduralImportedSurfaceRegionV1 region;
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralImportedSurfaceStrandConfig config;
    ProceduralImportedSurfaceStrandAsset strands;
    ProceduralImportedSurfaceStrandProvenance provenance;
    ProceduralImportedSurfaceStrandReceipt receipt = {0};
    ProceduralImportedSurfaceStrandReport report = {0};
    CoreResult core_result;
    json_object *summary = NULL;
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&source);
    core_mesh_asset_runtime_document_init(&tubes);
    ProceduralImportedSurfaceRegionV1_Init(&region);
    ProceduralImportedSurfaceStrandConfig_Init(&config);
    ProceduralImportedSurfaceStrandAsset_Init(&strands);
    ProceduralImportedSurfaceStrandProvenance_Init(&provenance);
    if (options.has_threshold) config.selection_threshold = options.threshold;
    if (options.has_length) config.strand_length_units = options.length;
    if (options.has_root_radius)
        config.root_radius_units = options.root_radius;
    if (options.has_tip_radius) config.tip_radius_units = options.tip_radius;
    if (options.has_root_penetration)
        config.root_penetration_units = options.root_penetration;
    if (options.has_bend) config.bend_strength = options.bend;
    if (options.has_curl) config.curl_strength = options.curl;
    if (options.has_max_strands) config.max_strands = options.max_strands;
    core_result = core_mesh_asset_runtime_document_load_file(
        options.mesh_path, &source);
    if (core_result.code != CORE_OK ||
        !ProceduralImportedSurfaceRegionV1_LoadJsonFile(
            options.region_path, &source, options.mesh_path,
            &region, &region_report) ||
        !ProceduralImportedSurfaceStrands_Compile(
            &source, options.mesh_path, &region, options.region_path,
            &config, options.strand_asset_id, &strands, &tubes,
            &provenance, &receipt, &report)) {
        fprintf(stderr, "strand compile failed: %s%s%s\n",
                core_result.code != CORE_OK ? core_result.message : "",
                core_result.code != CORE_OK ? "; " : "",
                report.message[0] ? report.message : region_report.message);
        goto cleanup;
    }
    core_result = core_mesh_asset_runtime_document_save_file(
        &tubes, options.tube_path);
    summary = receipt_json(&receipt);
    if (core_result.code != CORE_OK || !summary ||
        !write_json(options.summary_path, summary) ||
        !write_strands(options.strand_path, &receipt, &strands) ||
        !write_provenance(options.provenance_path, &receipt, &provenance)) {
        fprintf(stderr, "strand artifact write failed: %s\n",
                core_result.code != CORE_OK ? core_result.message :
                "receipt, strand data, or provenance write failed");
        goto cleanup;
    }
    printf("%s\n", json_object_to_json_string_ext(
        summary, JSON_C_TO_STRING_PLAIN));
    exit_code = 0;
cleanup:
    if (summary) json_object_put(summary);
    core_mesh_asset_runtime_document_free(&source);
    core_mesh_asset_runtime_document_free(&tubes);
    ProceduralImportedSurfaceRegionV1_Free(&region);
    ProceduralImportedSurfaceStrandAsset_Free(&strands);
    ProceduralImportedSurfaceStrandProvenance_Free(&provenance);
    return exit_code;
}
