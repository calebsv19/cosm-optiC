#include "procedural/procedural_imported_surface_region.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    const char *mesh_path;
    const char *recipe_path;
    const char *output_path;
    const char *summary_path;
    const char *solid_receipt_path;
} Options;

static void usage(const char *program) {
    fprintf(stderr,
        "usage: %s --mesh PATH --recipe PATH --out PATH "
        "[--summary-out PATH] [--solid-receipt-out PATH]\n", program);
}

static bool parse_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc) return false;
        if (strcmp(argv[i], "--mesh") == 0)
            options->mesh_path = argv[++i];
        else if (strcmp(argv[i], "--recipe") == 0)
            options->recipe_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0)
            options->output_path = argv[++i];
        else if (strcmp(argv[i], "--summary-out") == 0)
            options->summary_path = argv[++i];
        else if (strcmp(argv[i], "--solid-receipt-out") == 0)
            options->solid_receipt_path = argv[++i];
        else return false;
    }
    return options->mesh_path && options->recipe_path && options->output_path;
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

static json_object *summary_json(
    const ProceduralImportedSurfaceRegionV1 *region) {
    json_object *root = json_object_new_object();
#define ADD_STRING(key, value) \
    json_object_object_add(root, key, json_object_new_string(value))
#define ADD_SIZE(key, value) \
    json_object_object_add(root, key, json_object_new_int64((int64_t)(value)))
    ADD_STRING("schema", "ray_tracing.procedural_imported_surface_region_receipt");
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    ADD_STRING("region_id", region->region_id);
    ADD_STRING("source_asset_id", region->source_asset_id);
    ADD_STRING("source_mesh_digest_sha256",
               region->source_mesh_digest_sha256);
    ADD_STRING("source_file_digest_sha256",
               region->source_file_digest_sha256);
    ADD_STRING("recipe_digest_sha256", region->recipe_digest_sha256);
    ADD_STRING("value_digest_sha256", region->value_digest_sha256);
    ADD_SIZE("vertex_count", region->vertex_count);
    ADD_SIZE("triangle_count", region->triangle_count);
    ADD_SIZE("transition_vertex_count", region->transition_vertex_count);
    json_object_object_add(root, "minimum",
        json_object_new_double(region->minimum));
    json_object_object_add(root, "maximum",
        json_object_new_double(region->maximum));
    json_object_object_add(root, "mean",
        json_object_new_double(region->mean));
    json_object_object_add(root, "topology_unchanged",
        json_object_new_boolean(region->topology_unchanged));
    json_object_object_add(root, "source_triangle_provenance_retained",
        json_object_new_boolean(region->source_triangle_provenance_retained));
#undef ADD_SIZE
#undef ADD_STRING
    return root;
}

static bool write_solid_receipt(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const ProceduralImportedSurfaceRegionV1 *region) {
    json_object *root;
    json_object *regions;
    json_object *entry;
    char canonical[256];
    char region_digest[RAY_TRACING_SHA256_HEX_SIZE] = {0};
    int count;
    if (!path || mesh->surface_group_count != 1u) return path == NULL;
    count = snprintf(canonical, sizeof(canonical), "%s|retained|source_mesh||%zu;",
                     mesh->surface_groups[0].group_id,
                     mesh->surface_groups[0].triangle_count);
    if (count < 0 || (size_t)count >= sizeof(canonical) ||
        !ray_tracing_sha256_bytes(canonical, (size_t)count, region_digest))
        return false;
    root = json_object_new_object();
    regions = json_object_new_array();
    entry = json_object_new_object();
    if (!root || !regions || !entry) return false;
    json_object_object_add(root, "schema",
        json_object_new_string("ray_tracing.procedural_solid_receipt"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "asset_id",
        json_object_new_string(mesh->contract.asset_id));
    json_object_object_add(root, "semantic_source_id",
        json_object_new_string(mesh->contract.source_asset_id));
    json_object_object_add(root, "mesh_digest_sha256",
        json_object_new_string(region->source_mesh_digest_sha256));
    json_object_object_add(root, "region_digest_sha256",
        json_object_new_string(region_digest));
    json_object_object_add(entry, "region_id",
        json_object_new_string(mesh->surface_groups[0].group_id));
    json_object_object_add(entry, "kind",
        json_object_new_string("retained"));
    json_object_object_add(entry, "primary_node_id",
        json_object_new_string("source_mesh"));
    json_object_object_add(entry, "secondary_node_id",
        json_object_new_string(""));
    json_object_object_add(entry, "triangle_count",
        json_object_new_int64(
            (int64_t)mesh->surface_groups[0].triangle_count));
    json_object_array_add(regions, entry);
    json_object_object_add(root, "regions", regions);
    {
        bool ok = write_json(path, root);
        json_object_put(root);
        return ok;
    }
}

int main(int argc, char **argv) {
    Options options;
    CoreMeshAssetRuntimeDocument mesh;
    ProceduralImportedSurfaceRegionRecipeV1 recipe;
    ProceduralImportedSurfaceRegionV1 region;
    ProceduralImportedSurfaceRegionReport report = {0};
    json_object *summary;
    CoreResult mesh_result;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&mesh);
    ProceduralImportedSurfaceRegionV1_Init(&region);
    mesh_result = core_mesh_asset_runtime_document_load_file(
        options.mesh_path, &mesh);
    if (mesh_result.code != CORE_OK ||
        !ProceduralImportedSurfaceRegionRecipeV1_LoadJsonFile(
            options.recipe_path, &recipe, &report) ||
        !ProceduralImportedSurfaceRegionV1_Compile(
            &recipe, &mesh, options.mesh_path, &region, &report) ||
        !ProceduralImportedSurfaceRegionV1_SaveJsonFileAtomic(
            options.output_path, &region, &report)) {
        fprintf(stderr, "region compile failed: %s%s%s\n",
                mesh_result.code != CORE_OK ? mesh_result.message : "",
                mesh_result.code != CORE_OK ? "; " : "",
                report.message);
        core_mesh_asset_runtime_document_free(&mesh);
        ProceduralImportedSurfaceRegionV1_Free(&region);
        return 1;
    }
    summary = summary_json(&region);
    if (!summary || !write_json(options.summary_path, summary) ||
        !write_solid_receipt(
            options.solid_receipt_path, &mesh, &region)) {
        fprintf(stderr, "region receipt write failed\n");
        if (summary) json_object_put(summary);
        core_mesh_asset_runtime_document_free(&mesh);
        ProceduralImportedSurfaceRegionV1_Free(&region);
        return 1;
    }
    printf("%s\n", json_object_to_json_string_ext(
        summary, JSON_C_TO_STRING_PLAIN));
    json_object_put(summary);
    core_mesh_asset_runtime_document_free(&mesh);
    ProceduralImportedSurfaceRegionV1_Free(&region);
    return 0;
}
