#include "procedural/procedural_surface_mesh_asset_adapter.h"
#include "procedural/procedural_surface_material.h"
#include "procedural/procedural_surface_derived_asset.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <json-c/json.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ToolOptions {
    const char *recipe_path;
    const char *asset_path;
    const char *summary_path;
    const char *material_path;
    const char *manifest_path;
    const char *asset_id;
    const char *source_asset_id;
    double width;
    double height;
    double depth;
} ToolOptions;

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --recipe PATH --asset-out PATH --summary-out PATH "
            "--width N --height N --depth N "
            "[--material-out PATH] [--manifest-out PATH] "
            "[--asset-id ID] [--source-asset-id ID]\n",
            program);
}

static int parse_positive_double(const char *text, double *out_value) {
    char *end = NULL;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || !end || *end != '\0' || !(value > 0.0)) return 0;
    *out_value = value;
    return 1;
}

static int parse_options(int argc, char **argv, ToolOptions *options) {
    memset(options, 0, sizeof(*options));
    options->asset_id = "procedural_surface_preview_asset";
    options->source_asset_id = "procedural_surface_preview_cage";
    for (int i = 1; i < argc; ++i) {
        const char *flag = argv[i];
        if (i + 1 >= argc) return 0;
        if (strcmp(flag, "--recipe") == 0) {
            options->recipe_path = argv[++i];
        } else if (strcmp(flag, "--asset-out") == 0) {
            options->asset_path = argv[++i];
        } else if (strcmp(flag, "--summary-out") == 0) {
            options->summary_path = argv[++i];
        } else if (strcmp(flag, "--material-out") == 0) {
            options->material_path = argv[++i];
        } else if (strcmp(flag, "--manifest-out") == 0) {
            options->manifest_path = argv[++i];
        } else if (strcmp(flag, "--asset-id") == 0) {
            options->asset_id = argv[++i];
        } else if (strcmp(flag, "--source-asset-id") == 0) {
            options->source_asset_id = argv[++i];
        } else if (strcmp(flag, "--width") == 0) {
            if (!parse_positive_double(argv[++i], &options->width)) return 0;
        } else if (strcmp(flag, "--height") == 0) {
            if (!parse_positive_double(argv[++i], &options->height)) return 0;
        } else if (strcmp(flag, "--depth") == 0) {
            if (!parse_positive_double(argv[++i], &options->depth)) return 0;
        } else {
            return 0;
        }
    }
    return options->recipe_path && options->asset_path &&
           options->summary_path && options->width > 0.0 &&
           options->height > 0.0 && options->depth > 0.0 &&
           (!options->manifest_path || options->material_path);
}

static struct json_object *new_vec3(ProceduralSurfaceFieldPoint3D value) {
    struct json_object *array = json_object_new_array();
    json_object_array_add(array, json_object_new_double(value.x));
    json_object_array_add(array, json_object_new_double(value.y));
    json_object_array_add(array, json_object_new_double(value.z));
    return array;
}

static int write_material_artifact(
    const ToolOptions *options,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePrismMesh *mesh,
    const ProceduralSurfacePrismMeshSummary *summary,
    char out_material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY]) {
    struct json_object *root = NULL;
    struct json_object *vertices = NULL;
    struct json_object *triangles = NULL;
    ProceduralSurfaceMaterialSample *samples = NULL;
    const char **sample_ids = NULL;
    char **owned_ids = NULL;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY];
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceMaterialReport material_report;
    int result = -1;
    if (out_material_digest) out_material_digest[0] = '\0';
    if (!options || !options->material_path || !recipe || !mesh || !summary ||
        !out_material_digest) {
        return 0;
    }
    samples = calloc(mesh->vertex_count, sizeof(*samples));
    sample_ids = calloc(mesh->vertex_count, sizeof(*sample_ids));
    owned_ids = calloc(mesh->vertex_count, sizeof(*owned_ids));
    if (!samples || !sample_ids || !owned_ids ||
        !ProceduralSurfaceRecipeV1_Digest(
            recipe, recipe_digest, &recipe_report)) {
        goto cleanup;
    }
    root = json_object_new_object();
    vertices = json_object_new_array();
    triangles = json_object_new_array();
    if (!root || !vertices || !triangles) goto cleanup;
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *vertex = &mesh->vertices[i];
        const ProceduralSurfaceMaterialSample *sample = &samples[i];
        struct json_object *entry = NULL;
        struct json_object *color = NULL;
        owned_ids[i] = calloc(32u, 1u);
        if (!owned_ids[i]) goto cleanup;
        snprintf(owned_ids[i], 32u, "vertex_%04zu", i);
        sample_ids[i] = owned_ids[i];
        if (!ProceduralSurfaceMaterial_Evaluate(
                recipe, &vertex->field, vertex->position, vertex->normal,
                &samples[i], &material_report)) {
            fprintf(stderr, "material evaluation failed at vertex %zu: %s (%s)\n",
                    i, material_report.message, material_report.field);
            goto cleanup;
        }
        entry = json_object_new_object();
        color = json_object_new_array();
        json_object_object_add(entry, "position", new_vec3(vertex->position));
        json_object_object_add(entry, "normal", new_vec3(vertex->normal));
        json_object_array_add(color, json_object_new_double(sample->final_color_r));
        json_object_array_add(color, json_object_new_double(sample->final_color_g));
        json_object_array_add(color, json_object_new_double(sample->final_color_b));
        json_object_object_add(entry, "color", color);
        json_object_object_add(entry, "roughness",
                               json_object_new_double(sample->final_roughness));
        json_object_object_add(entry, "snow_likelihood",
                               json_object_new_double(sample->snow_likelihood));
        json_object_array_add(vertices, entry);
    }
    if (!ProceduralSurfaceMaterial_SummaryDigest(
            recipe_digest, summary->mesh_digest_sha256, sample_ids, samples,
            mesh->vertex_count, material_digest, &material_report)) {
        goto cleanup;
    }
    snprintf(out_material_digest,
             PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY,
             "%s", material_digest);
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *triangle = &mesh->triangles[i];
        struct json_object *entry = json_object_new_array();
        json_object_array_add(entry, json_object_new_int64(triangle->a));
        json_object_array_add(entry, json_object_new_int64(triangle->b));
        json_object_array_add(entry, json_object_new_int64(triangle->c));
        json_object_array_add(triangles, entry);
    }
    json_object_object_add(root, "schema_version",
                           json_object_new_string(
                               "procedural_surface_material_artifact_v1"));
    json_object_object_add(root, "coordinate_space",
                           json_object_new_string("object"));
    json_object_object_add(root, "recipe_digest_sha256",
                           json_object_new_string(recipe_digest));
    json_object_object_add(root, "shell_digest_sha256",
                           json_object_new_string(summary->mesh_digest_sha256));
    json_object_object_add(root, "material_digest_sha256",
                           json_object_new_string(material_digest));
    json_object_object_add(root, "vertices", vertices);
    vertices = NULL;
    json_object_object_add(root, "triangles", triangles);
    triangles = NULL;
    result = json_object_to_file_ext(
        options->material_path, root,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);

cleanup:
    if (triangles) json_object_put(triangles);
    if (vertices) json_object_put(vertices);
    if (root) json_object_put(root);
    if (owned_ids) {
        for (size_t i = 0u; i < mesh->vertex_count; ++i) free(owned_ids[i]);
    }
    free(owned_ids);
    free(sample_ids);
    free(samples);
    return result == 0;
}

static int write_derived_asset_manifest(
    const ToolOptions *options,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePrismMeshSummary *summary,
    const char *material_digest) {
    ProceduralSurfaceDerivedAssetManifest manifest;
    ProceduralSurfaceDerivedAssetReport report;
    ProceduralSurfaceRecipeReport recipe_report;
    memset(&manifest, 0, sizeof(manifest));
    manifest.schema_version =
        PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION_V1;
    snprintf(manifest.asset_id, sizeof(manifest.asset_id), "%s",
             options->asset_id);
    snprintf(manifest.source_asset_id, sizeof(manifest.source_asset_id), "%s",
             options->source_asset_id);
    snprintf(manifest.recipe_path, sizeof(manifest.recipe_path), "%s",
             options->recipe_path);
    snprintf(manifest.mesh_path, sizeof(manifest.mesh_path), "%s",
             options->asset_path);
    snprintf(manifest.material_path, sizeof(manifest.material_path), "%s",
             options->material_path);
    manifest.quality = PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW;
    manifest.cage_kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM;
    manifest.cage_width_units = options->width;
    manifest.cage_height_units = options->height;
    manifest.cage_depth_units = options->depth;
    snprintf(manifest.shell_digest_sha256,
             sizeof(manifest.shell_digest_sha256), "%s",
             summary->mesh_digest_sha256);
    snprintf(manifest.material_digest_sha256,
             sizeof(manifest.material_digest_sha256), "%s", material_digest);
    snprintf(manifest.collision_owner, sizeof(manifest.collision_owner),
             "semantic_cage");
    if (!ProceduralSurfaceRecipeV1_Digest(
            recipe, manifest.recipe_digest_sha256, &recipe_report) ||
        !ProceduralSurfaceDerivedAsset_CageDigest(
            manifest.cage_kind, manifest.cage_width_units,
            manifest.cage_height_units, manifest.cage_depth_units,
            manifest.cage_digest_sha256, &report) ||
        !ProceduralSurfaceDerivedAsset_CacheIdentity(
            manifest.recipe_digest_sha256, manifest.cage_digest_sha256,
            manifest.quality, manifest.shell_digest_sha256,
            manifest.material_digest_sha256,
            manifest.cache_identity_sha256, &report) ||
        !ProceduralSurfaceDerivedAssetManifest_SaveJsonFile(
            options->manifest_path, &manifest, &report)) {
        fprintf(stderr, "derived asset manifest save failed: %s (%s)\n",
                report.message, report.field);
        return 0;
    }
    return 1;
}

static void add_vec3(struct json_object *root,
                     const char *key,
                     ProceduralSurfaceFieldPoint3D value) {
    struct json_object *object = json_object_new_object();
    json_object_object_add(object, "x", json_object_new_double(value.x));
    json_object_object_add(object, "y", json_object_new_double(value.y));
    json_object_object_add(object, "z", json_object_new_double(value.z));
    json_object_object_add(root, key, object);
}

static int write_summary(
    const ToolOptions *options,
    const ProceduralSurfacePrismMeshRequirements *requirements,
    const ProceduralSurfacePrismMeshSummary *summary) {
    struct json_object *root = json_object_new_object();
    int result;
    if (!root) {
        json_object_put(root);
        return 0;
    }
    json_object_object_add(root, "schema_version",
                           json_object_new_string(
                               "procedural_surface_preview_asset_summary_v1"));
    json_object_object_add(root, "asset_id",
                           json_object_new_string(options->asset_id));
    json_object_object_add(root, "source_asset_id",
                           json_object_new_string(options->source_asset_id));
    json_object_object_add(root, "recipe_path",
                           json_object_new_string(options->recipe_path));
    json_object_object_add(root, "asset_path",
                           json_object_new_string(options->asset_path));
    json_object_object_add(root, "width_units",
                           json_object_new_double(options->width));
    json_object_object_add(root, "height_units",
                           json_object_new_double(options->height));
    json_object_object_add(root, "depth_units",
                           json_object_new_double(options->depth));
    json_object_object_add(root, "subdivisions_x",
                           json_object_new_int64(requirements->subdivisions_x));
    json_object_object_add(root, "subdivisions_y",
                           json_object_new_int64(requirements->subdivisions_y));
    json_object_object_add(root, "subdivisions_z",
                           json_object_new_int64(requirements->subdivisions_z));
    json_object_object_add(root, "vertex_count",
                           json_object_new_int64(summary->vertex_count));
    json_object_object_add(root, "triangle_count",
                           json_object_new_int64(summary->triangle_count));
    json_object_object_add(root, "unique_edge_count",
                           json_object_new_int64(summary->unique_edge_count));
    json_object_object_add(root, "boundary_edge_count",
                           json_object_new_int64(summary->boundary_edge_count));
    json_object_object_add(
        root, "connected_component_count",
        json_object_new_int64(summary->connected_component_count));
    json_object_object_add(root, "euler_characteristic",
                           json_object_new_int(summary->euler_characteristic));
    json_object_object_add(root, "field_evaluation_count",
                           json_object_new_int64(
                               summary->field_evaluation_count));
    json_object_object_add(
        root, "maximum_absolute_displacement_units",
        json_object_new_double(summary->maximum_absolute_displacement_units));
    json_object_object_add(
        root, "maximum_edge_absolute_displacement_units",
        json_object_new_double(
            summary->maximum_edge_absolute_displacement_units));
    json_object_object_add(
        root, "minimum_twice_triangle_area_units2",
        json_object_new_double(summary->minimum_twice_triangle_area_units2));
    json_object_object_add(root, "total_surface_area_units2",
                           json_object_new_double(
                               summary->total_surface_area_units2));
    json_object_object_add(root, "signed_volume_units3",
                           json_object_new_double(
                               summary->signed_volume_units3));
    json_object_object_add(root, "minimum_outward_winding_dot",
                           json_object_new_double(
                               summary->minimum_outward_winding_dot));
    json_object_object_add(root, "mesh_digest_sha256",
                           json_object_new_string(
                               summary->mesh_digest_sha256));
    add_vec3(root, "bounds_min", summary->bounds_min);
    add_vec3(root, "bounds_max", summary->bounds_max);
    result = json_object_to_file_ext(
        options->summary_path, root,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    json_object_put(root);
    return result == 0;
}

int main(int argc, char **argv) {
    ToolOptions options;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceCageContract cage;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMesh mesh;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfacePrismVertex *vertices = NULL;
    ProceduralSurfacePrismTriangle *triangles = NULL;
    CoreMeshAssetRuntimeDocument document;
    CoreResult result;
    char material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY] = {0};
    int exit_code = 1;

    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!ProceduralSurfaceRecipeV1_LoadJsonFile(
            options.recipe_path, &recipe, &recipe_report)) {
        fprintf(stderr, "recipe load failed: %s (%s)\n",
                recipe_report.message, recipe_report.field);
        return 1;
    }
    cage = (ProceduralSurfaceCageContract){
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = options.width,
        .height_units = options.height,
        .depth_units = options.depth,
        .target_edge_length_units = recipe.target_edge_length_units};
    if (!ProceduralSurfacePrismMesh_DeriveRequirements(
            &cage, &recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &requirements, &mesh_report)) {
        fprintf(stderr, "requirement derivation failed: %s (%s)\n",
                mesh_report.message, mesh_report.field);
        return 1;
    }
    vertices = calloc((size_t)requirements.vertex_count, sizeof(*vertices));
    triangles = calloc((size_t)requirements.triangle_count, sizeof(*triangles));
    if (!vertices || !triangles) {
        fprintf(stderr, "mesh allocation failed\n");
        goto cleanup;
    }
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = recipe.quality.max_field_evaluations};
    if (!ProceduralSurfacePrismMesh_Generate(
            &cage, &recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &budget, &buffers, &summary, &mesh_report)) {
        fprintf(stderr, "mesh generation failed: %s (%s)\n",
                mesh_report.message, mesh_report.field);
        goto cleanup;
    }
    mesh = (ProceduralSurfacePrismMesh){
        .vertices = vertices,
        .vertex_count = buffers.vertex_count,
        .triangles = triangles,
        .triangle_count = buffers.triangle_count};
    core_mesh_asset_runtime_document_init(&document);
    result = ProceduralSurfaceMeshAsset_FromPrism(
        &mesh, &summary, options.asset_id, options.source_asset_id, &document);
    if (result.code != CORE_OK) {
        fprintf(stderr, "mesh asset adaptation failed: %s\n", result.message);
        core_mesh_asset_runtime_document_free(&document);
        goto cleanup;
    }
    result = core_mesh_asset_runtime_document_save_file(
        &document, options.asset_path);
    core_mesh_asset_runtime_document_free(&document);
    if (result.code != CORE_OK) {
        fprintf(stderr, "mesh asset save failed: %s\n", result.message);
        goto cleanup;
    }
    if (!write_summary(&options, &requirements, &summary)) {
        fprintf(stderr, "preview summary save failed: %s\n",
                options.summary_path);
        goto cleanup;
    }
    if (options.material_path &&
        !write_material_artifact(&options, &recipe, &mesh, &summary,
                                 material_digest)) {
        fprintf(stderr, "material artifact save failed: %s\n",
                options.material_path);
        goto cleanup;
    }
    if (options.manifest_path &&
        !write_derived_asset_manifest(&options, &recipe, &summary,
                                      material_digest)) {
        goto cleanup;
    }
    printf("%s\n", options.summary_path);
    exit_code = 0;
cleanup:
    free(triangles);
    free(vertices);
    return exit_code;
}
