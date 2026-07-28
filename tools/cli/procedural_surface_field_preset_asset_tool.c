#include "procedural/procedural_surface_derived_asset.h"
#include "procedural/procedural_surface_field_graph.h"
#include "procedural/procedural_surface_material.h"
#include "procedural/procedural_surface_mesh_asset_adapter.h"
#include "procedural/procedural_surface_prism_binding.h"
#include "procedural/procedural_surface_prism_mesh.h"
#include "procedural/procedural_surface_selected_face_shell.h"

#include <json-c/json.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ToolOptions {
    const char *graph_path;
    const char *binding_path;
    const char *base_recipe_path;
    const char *recipe_path;
    const char *asset_path;
    const char *material_path;
    const char *manifest_path;
    const char *summary_path;
    const char *asset_id;
    const char *source_asset_id;
    const char *selected_face_name;
    double width;
    double height;
    double depth;
    double target_edge;
    double amplitude;
    double edge_lock;
} ToolOptions;

typedef struct MaterialStats {
    double min_height;
    double max_height;
    double mean_height;
    double mean_cavity;
    double min_roughness;
    double max_roughness;
    double mean_r;
    double mean_g;
    double mean_b;
    double mean_application_weight;
    double min_snow_likelihood;
    double max_snow_likelihood;
    double mean_snow_likelihood;
} MaterialStats;

static void usage(const char *program) {
    fprintf(
        stderr,
        "usage: %s --graph PATH [--binding PATH] --base-recipe PATH --recipe-out PATH "
        "--asset-out PATH --material-out PATH --manifest-out PATH "
        "--summary-out PATH --width N --height N --depth N "
        "--target-edge N --amplitude N --edge-lock N "
        "[--asset-id ID] [--source-asset-id ID] "
        "[--selected-face FACE]\n",
        program);
}

static bool parse_positive(const char *text, double *out) {
    char *end = NULL;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (errno || !end || *end != '\0' || !isfinite(value) ||
        !(value > 0.0)) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_nonnegative(const char *text, double *out) {
    char *end = NULL;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (errno || !end || *end != '\0' || !isfinite(value) || value < 0.0) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_options(
    int argc,
    char **argv,
    ToolOptions *options) {
    memset(options, 0, sizeof(*options));
    options->asset_id = "procedural_surface_field_preset";
    options->source_asset_id = "procedural_surface_field_preset_cage";
    for (int i = 1; i < argc; ++i) {
        const char *flag = argv[i];
        if (i + 1 >= argc) return false;
#define STRING_OPTION(name, member) \
        if (strcmp(flag, name) == 0) { options->member = argv[++i]; }
        STRING_OPTION("--graph", graph_path)
        else STRING_OPTION("--binding", binding_path)
        else STRING_OPTION("--base-recipe", base_recipe_path)
        else STRING_OPTION("--recipe-out", recipe_path)
        else STRING_OPTION("--asset-out", asset_path)
        else STRING_OPTION("--material-out", material_path)
        else STRING_OPTION("--manifest-out", manifest_path)
        else STRING_OPTION("--summary-out", summary_path)
        else STRING_OPTION("--asset-id", asset_id)
        else STRING_OPTION("--source-asset-id", source_asset_id)
        else STRING_OPTION("--selected-face", selected_face_name)
#undef STRING_OPTION
        else if (strcmp(flag, "--width") == 0) {
            if (!parse_positive(argv[++i], &options->width)) return false;
        } else if (strcmp(flag, "--height") == 0) {
            if (!parse_positive(argv[++i], &options->height)) return false;
        } else if (strcmp(flag, "--depth") == 0) {
            if (!parse_positive(argv[++i], &options->depth)) return false;
        } else if (strcmp(flag, "--target-edge") == 0) {
            if (!parse_positive(argv[++i], &options->target_edge)) return false;
        } else if (strcmp(flag, "--amplitude") == 0) {
            if (!parse_nonnegative(argv[++i], &options->amplitude)) return false;
        } else if (strcmp(flag, "--edge-lock") == 0) {
            if (!parse_nonnegative(argv[++i], &options->edge_lock)) return false;
        } else {
            return false;
        }
    }
    return options->graph_path && options->base_recipe_path &&
           options->recipe_path && options->asset_path &&
           options->material_path && options->manifest_path &&
           options->summary_path && options->width > 0.0 &&
           options->height > 0.0 && options->depth > 0.0 &&
           options->target_edge > 0.0;
}

static bool evaluate_graph_legacy(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    return ProceduralSurfaceFieldGraphV1_EvaluateLegacy(
        context, point, budget, out_field, report);
}

static struct json_object *new_vec3(
    ProceduralSurfaceFieldPoint3D value) {
    struct json_object *array = json_object_new_array();
    json_object_array_add(array, json_object_new_double(value.x));
    json_object_array_add(array, json_object_new_double(value.y));
    json_object_array_add(array, json_object_new_double(value.z));
    return array;
}

static void add_vec3_object(
    struct json_object *root,
    const char *key,
    ProceduralSurfaceFieldPoint3D value) {
    struct json_object *object = json_object_new_object();
    json_object_object_add(object, "x", json_object_new_double(value.x));
    json_object_object_add(object, "y", json_object_new_double(value.y));
    json_object_object_add(object, "z", json_object_new_double(value.z));
    json_object_object_add(root, key, object);
}

static bool write_material(
    const ToolOptions *options,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfacePrismBindingContext *binding_context,
    const ProceduralSurfacePrismMesh *mesh,
    const ProceduralSurfacePrismMeshSummary *summary,
    char out_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY],
    MaterialStats *out_stats) {
    struct json_object *root = NULL;
    struct json_object *vertices = NULL;
    struct json_object *triangles = NULL;
    ProceduralSurfaceMaterialSample *samples = NULL;
    const char **sample_ids = NULL;
    char **owned_ids = NULL;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceMaterialReport material_report;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceFieldBudget budget = {
        .max_evaluations = mesh->vertex_count
    };
    MaterialStats stats = {
        .min_height = INFINITY,
        .max_height = -INFINITY,
        .min_roughness = INFINITY,
        .max_roughness = -INFINITY,
        .min_snow_likelihood = INFINITY,
        .max_snow_likelihood = -INFINITY
    };
    bool result = false;
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
        ProceduralSurfaceFieldGraphSample graph_sample;
        ProceduralSurfaceBoundSample bound_sample;
        ProceduralSurfaceMaterialSample *sample = &samples[i];
        struct json_object *entry = NULL;
        struct json_object *color = NULL;
        owned_ids[i] = calloc(32u, 1u);
        if (!owned_ids[i]) goto cleanup;
        if (binding_context) {
            ProceduralSurfaceBindingReport binding_report;
            if (!ProceduralSurfacePrismBinding_EvaluateSample(
                    binding_context, vertex->cage_position, &budget,
                    &bound_sample, &binding_report)) {
                fprintf(stderr,
                        "bound material field evaluation failed at %zu: %s\n",
                        i, binding_report.message);
                goto cleanup;
            }
            graph_sample = bound_sample.graph_sample;
            stats.mean_application_weight +=
                bound_sample.application_weight;
        } else if (!ProceduralSurfaceFieldGraphV1_Evaluate(
                       graph, vertex->cage_position, &budget, &graph_sample,
                       &graph_report)) {
            fprintf(stderr, "material field evaluation failed at %zu: %s\n",
                    i, graph_report.message);
            goto cleanup;
        } else {
            stats.mean_application_weight += 1.0;
        }
        snprintf(owned_ids[i], 32u, "vertex_%06zu", i);
        sample_ids[i] = owned_ids[i];
        if (!ProceduralSurfaceMaterial_Evaluate(
                recipe, &vertex->field, vertex->position, vertex->normal,
                sample, &material_report)) {
            fprintf(stderr,
                    "geometry-coupled material evaluation failed at %zu: %s\n",
                    i, material_report.message);
            goto cleanup;
        }
        stats.min_height = fmin(stats.min_height, graph_sample.height);
        stats.max_height = fmax(stats.max_height, graph_sample.height);
        stats.mean_height += graph_sample.height;
        stats.mean_cavity += graph_sample.cavity;
        stats.min_roughness =
            fmin(stats.min_roughness, graph_sample.roughness);
        stats.max_roughness =
            fmax(stats.max_roughness, graph_sample.roughness);
        stats.mean_r += graph_sample.color_r;
        stats.mean_g += graph_sample.color_g;
        stats.mean_b += graph_sample.color_b;
        stats.min_snow_likelihood =
            fmin(stats.min_snow_likelihood, sample->snow_likelihood);
        stats.max_snow_likelihood =
            fmax(stats.max_snow_likelihood, sample->snow_likelihood);
        stats.mean_snow_likelihood += sample->snow_likelihood;
        entry = json_object_new_object();
        color = json_object_new_array();
        json_object_object_add(entry, "position", new_vec3(vertex->position));
        json_object_object_add(entry, "normal", new_vec3(vertex->normal));
        json_object_array_add(
            color, json_object_new_double(sample->final_color_r));
        json_object_array_add(
            color, json_object_new_double(sample->final_color_g));
        json_object_array_add(
            color, json_object_new_double(sample->final_color_b));
        json_object_object_add(entry, "color", color);
        json_object_object_add(
            entry, "roughness",
            json_object_new_double(sample->final_roughness));
        json_object_object_add(
            entry, "snow_likelihood",
            json_object_new_double(sample->snow_likelihood));
        json_object_array_add(vertices, entry);
    }
    if (!ProceduralSurfaceMaterial_SummaryDigest(
            recipe_digest, summary->mesh_digest_sha256, sample_ids, samples,
            mesh->vertex_count, out_digest, &material_report)) {
        goto cleanup;
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *triangle = &mesh->triangles[i];
        struct json_object *entry = json_object_new_array();
        json_object_array_add(entry, json_object_new_int64(triangle->a));
        json_object_array_add(entry, json_object_new_int64(triangle->b));
        json_object_array_add(entry, json_object_new_int64(triangle->c));
        json_object_array_add(triangles, entry);
    }
    json_object_object_add(
        root, "schema_version",
        json_object_new_string("procedural_surface_material_artifact_v1"));
    json_object_object_add(
        root, "coordinate_space", json_object_new_string("object"));
    json_object_object_add(
        root, "recipe_digest_sha256", json_object_new_string(recipe_digest));
    json_object_object_add(
        root, "shell_digest_sha256",
        json_object_new_string(summary->mesh_digest_sha256));
    json_object_object_add(
        root, "material_digest_sha256", json_object_new_string(out_digest));
    json_object_object_add(root, "vertices", vertices);
    vertices = NULL;
    json_object_object_add(root, "triangles", triangles);
    triangles = NULL;
    result = json_object_to_file_ext(
                 options->material_path, root,
                 JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED) == 0;
    if (result) {
        const double count = (double)mesh->vertex_count;
        stats.mean_height /= count;
        stats.mean_cavity /= count;
        stats.mean_r /= count;
        stats.mean_g /= count;
        stats.mean_b /= count;
        stats.mean_application_weight /= count;
        stats.mean_snow_likelihood /= count;
        *out_stats = stats;
    }

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
    return result;
}

static bool write_manifest(
    const ToolOptions *options,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfacePrismMeshSummary *summary,
    const char *material_digest) {
    ProceduralSurfaceDerivedAssetManifest manifest;
    ProceduralSurfaceDerivedAssetReport report;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport binding_report;
    memset(&manifest, 0, sizeof(manifest));
    manifest.schema_version = binding
        ? PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION
        : PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION_V1;
    snprintf(manifest.asset_id, sizeof(manifest.asset_id), "%s",
             options->asset_id);
    snprintf(manifest.source_asset_id, sizeof(manifest.source_asset_id), "%s",
             options->source_asset_id);
    snprintf(manifest.recipe_path, sizeof(manifest.recipe_path), "%s",
             options->recipe_path);
    if (binding) {
        snprintf(manifest.field_graph_path, sizeof(manifest.field_graph_path),
                 "%s", options->graph_path);
        snprintf(manifest.binding_path, sizeof(manifest.binding_path), "%s",
                 options->binding_path);
    }
    snprintf(manifest.mesh_path, sizeof(manifest.mesh_path), "%s",
             options->asset_path);
    snprintf(manifest.material_path, sizeof(manifest.material_path), "%s",
             options->material_path);
    manifest.quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL;
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
    return ProceduralSurfaceRecipeV1_Digest(
               recipe, manifest.recipe_digest_sha256, &recipe_report) &&
           (!binding ||
            (ProceduralSurfaceFieldGraphV1_Digest(
                 graph, manifest.field_graph_digest_sha256, &graph_report) &&
             ProceduralSurfaceBindingV1_Digest(
                 binding, manifest.binding_digest_sha256, &binding_report))) &&
           ProceduralSurfaceDerivedAsset_CageDigest(
               manifest.cage_kind, manifest.cage_width_units,
               manifest.cage_height_units, manifest.cage_depth_units,
               manifest.cage_digest_sha256, &report) &&
           ((!binding &&
             ProceduralSurfaceDerivedAsset_CacheIdentity(
                 manifest.recipe_digest_sha256, manifest.cage_digest_sha256,
                 manifest.quality, manifest.shell_digest_sha256,
                 manifest.material_digest_sha256,
                 manifest.cache_identity_sha256, &report)) ||
            (binding &&
             ProceduralSurfaceDerivedAsset_CacheIdentityV2(
                 manifest.recipe_digest_sha256,
                 manifest.field_graph_digest_sha256,
                 manifest.binding_digest_sha256,
                 manifest.cage_digest_sha256, manifest.quality,
                 manifest.shell_digest_sha256,
                 manifest.material_digest_sha256,
                 manifest.cache_identity_sha256, &report))) &&
           ProceduralSurfaceDerivedAssetManifest_SaveJsonFile(
               options->manifest_path, &manifest, &report);
}

static bool write_summary(
    const ToolOptions *options,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfacePrismMeshRequirements *requirements,
    const ProceduralSurfacePrismMeshSummary *summary,
    const char *material_digest,
    const MaterialStats *stats,
    const ProceduralSurfaceSelectedFaceShellReceipt *selected_receipt) {
    char graph_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char binding_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY] = "";
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport binding_report;
    struct json_object *root = json_object_new_object();
    struct json_object *color = json_object_new_array();
    bool result;
    if (!root || !color ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            graph, graph_digest, &graph_report) ||
        (binding && !ProceduralSurfaceBindingV1_Digest(
             binding, binding_digest, &binding_report))) {
        if (color) json_object_put(color);
        if (root) json_object_put(root);
        return false;
    }
    json_object_object_add(
        root, "schema_version",
        json_object_new_string("procedural_surface_field_preset_summary_v1"));
    json_object_object_add(
        root, "program_id", json_object_new_string(graph->program_id));
    json_object_object_add(
        root, "field_graph_digest_sha256",
        json_object_new_string(graph_digest));
    if (binding) {
        json_object_object_add(
            root, "surface_binding_id",
            json_object_new_string(binding->binding_id));
        json_object_object_add(
            root, "surface_binding_digest_sha256",
            json_object_new_string(binding_digest));
        json_object_object_add(
            root, "surface_selector",
            json_object_new_string(
                ProceduralSurfaceSelectorKind_Name(binding->selector)));
        json_object_object_add(
            root, "surface_projection",
            json_object_new_string(
                ProceduralSurfaceProjectionKind_Name(binding->projection)));
    }
    json_object_object_add(
        root, "mesh_digest_sha256",
        json_object_new_string(summary->mesh_digest_sha256));
    json_object_object_add(
        root, "material_digest_sha256",
        json_object_new_string(material_digest));
    if (selected_receipt) {
        struct json_object *receipt = json_object_new_object();
        json_object_object_add(
            receipt, "schema",
            json_object_new_string(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_SCHEMA));
        json_object_object_add(
            receipt, "schema_version",
            json_object_new_int(selected_receipt->schema_version));
        json_object_object_add(
            receipt, "source_asset_id",
            json_object_new_string(selected_receipt->source_asset_id));
        json_object_object_add(
            receipt, "derived_asset_id",
            json_object_new_string(selected_receipt->derived_asset_id));
        json_object_object_add(
            receipt, "selected_face",
            json_object_new_string(ProceduralSurfacePrismFace_Name(
                selected_receipt->selected_face)));
        json_object_object_add(
            receipt, "source_triangle_count",
            json_object_new_int64(selected_receipt->source_triangle_count));
        json_object_object_add(
            receipt, "source_selected_face_triangle_count",
            json_object_new_int64(
                selected_receipt->source_selected_face_triangle_count));
        json_object_object_add(
            receipt, "derived_selected_face_triangle_count",
            json_object_new_int64(
                selected_receipt->derived_selected_face_triangle_count));
        json_object_object_add(
            receipt, "closure_support_triangle_count",
            json_object_new_int64(
                selected_receipt->closure_support_triangle_count));
        json_object_object_add(
            receipt, "maximum_selected_face_absolute_displacement_units",
            json_object_new_double(
                selected_receipt
                    ->maximum_selected_face_absolute_displacement_units));
        json_object_object_add(
            receipt, "maximum_unselected_face_absolute_displacement_units",
            json_object_new_double(
                selected_receipt
                    ->maximum_unselected_face_absolute_displacement_units));
        json_object_object_add(
            receipt, "geometry_displacement_active",
            json_object_new_boolean(
                selected_receipt->geometry_displacement_active));
        json_object_object_add(
            receipt, "source_semantic_identity_retained",
            json_object_new_boolean(
                selected_receipt->source_semantic_identity_retained));
        json_object_object_add(
            receipt, "replaceable_derived_geometry",
            json_object_new_boolean(
                selected_receipt->replaceable_derived_geometry));
        json_object_object_add(
            receipt, "recipe_digest_sha256",
            json_object_new_string(selected_receipt->recipe_digest_sha256));
        json_object_object_add(
            receipt, "field_graph_digest_sha256",
            json_object_new_string(
                selected_receipt->field_graph_digest_sha256));
        json_object_object_add(
            receipt, "binding_digest_sha256",
            json_object_new_string(selected_receipt->binding_digest_sha256));
        json_object_object_add(
            receipt, "mesh_digest_sha256",
            json_object_new_string(selected_receipt->mesh_digest_sha256));
        json_object_object_add(root, "selected_face_shell", receipt);
    }
    json_object_object_add(
        root, "vertex_count", json_object_new_int64(summary->vertex_count));
    json_object_object_add(
        root, "triangle_count", json_object_new_int64(summary->triangle_count));
    json_object_object_add(
        root, "boundary_edge_count",
        json_object_new_int64(summary->boundary_edge_count));
    json_object_object_add(
        root, "connected_component_count",
        json_object_new_int64(summary->connected_component_count));
    json_object_object_add(
        root, "euler_characteristic",
        json_object_new_int(summary->euler_characteristic));
    json_object_object_add(
        root, "subdivisions_x",
        json_object_new_int64(requirements->subdivisions_x));
    json_object_object_add(
        root, "subdivisions_y",
        json_object_new_int64(requirements->subdivisions_y));
    json_object_object_add(
        root, "subdivisions_z",
        json_object_new_int64(requirements->subdivisions_z));
    json_object_object_add(
        root, "maximum_absolute_displacement_units",
        json_object_new_double(summary->maximum_absolute_displacement_units));
    add_vec3_object(root, "bounds_min", summary->bounds_min);
    add_vec3_object(root, "bounds_max", summary->bounds_max);
    json_object_object_add(
        root, "field_height_min", json_object_new_double(stats->min_height));
    json_object_object_add(
        root, "field_height_max", json_object_new_double(stats->max_height));
    json_object_object_add(
        root, "field_height_mean", json_object_new_double(stats->mean_height));
    json_object_object_add(
        root, "mean_application_weight",
        json_object_new_double(stats->mean_application_weight));
    json_object_object_add(
        root, "cavity_mean", json_object_new_double(stats->mean_cavity));
    json_object_object_add(
        root, "roughness_min", json_object_new_double(stats->min_roughness));
    json_object_object_add(
        root, "roughness_max", json_object_new_double(stats->max_roughness));
    json_object_object_add(
        root, "snow_likelihood_min",
        json_object_new_double(stats->min_snow_likelihood));
    json_object_object_add(
        root, "snow_likelihood_max",
        json_object_new_double(stats->max_snow_likelihood));
    json_object_object_add(
        root, "snow_likelihood_mean",
        json_object_new_double(stats->mean_snow_likelihood));
    json_object_array_add(color, json_object_new_double(stats->mean_r));
    json_object_array_add(color, json_object_new_double(stats->mean_g));
    json_object_array_add(color, json_object_new_double(stats->mean_b));
    json_object_object_add(root, "mean_color", color);
    result = json_object_to_file_ext(
                 options->summary_path, root,
                 JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED) == 0;
    json_object_put(root);
    return result;
}

int main(int argc, char **argv) {
    ToolOptions options;
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfacePrismBindingContext binding_context;
    const ProceduralSurfaceBindingV1 *active_binding = NULL;
    const ProceduralSurfacePrismBindingContext *active_binding_context = NULL;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceCageContract cage;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMesh mesh;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceSelectedFaceShellReceipt selected_receipt;
    ProceduralSurfaceSelectedFaceShellReport selected_report;
    ProceduralSurfacePrismFace selected_face =
        PROCEDURAL_SURFACE_PRISM_FACE_COUNT;
    const ProceduralSurfaceSelectedFaceShellReceipt *active_selected_receipt =
        NULL;
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfacePrismVertex *vertices = NULL;
    ProceduralSurfacePrismTriangle *triangles = NULL;
    CoreMeshAssetRuntimeDocument document;
    CoreResult core_result;
    MaterialStats stats;
    char material_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY] = {0};
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    if (!ProceduralSurfaceFieldGraphV1_LoadJsonFile(
            options.graph_path, &graph, &graph_report) ||
        !ProceduralSurfaceRecipeV1_LoadJsonFile(
            options.base_recipe_path, &recipe, &recipe_report)) {
        fprintf(stderr, "field graph or base recipe load failed\n");
        return 1;
    }
    if (options.binding_path) {
        if (!ProceduralSurfaceBindingV1_LoadJsonFile(
                options.binding_path, &binding, &binding_report) ||
            !ProceduralSurfaceBindingV1_Validate(
                &binding, &graph, &binding_report)) {
            fprintf(stderr, "surface binding load failed: %s\n",
                    binding_report.message);
            return 1;
        }
        active_binding = &binding;
    }
    if (options.selected_face_name &&
        (!active_binding ||
         !ProceduralSurfacePrismFace_Parse(
             options.selected_face_name, &selected_face))) {
        fprintf(stderr,
                "--selected-face requires a valid face name and --binding\n");
        return 1;
    }
    snprintf(recipe.recipe_id, sizeof(recipe.recipe_id), "%s_recipe",
             graph.program_id);
    recipe.target_edge_length_units = options.target_edge;
    recipe.displacement_amplitude_units = options.amplitude;
    recipe.edge_lock_width_units = options.edge_lock;
    if (!ProceduralSurfaceRecipeV1_Validate(&recipe, &recipe_report) ||
        !ProceduralSurfaceRecipeV1_SaveJsonFile(
            options.recipe_path, &recipe, &recipe_report)) {
        fprintf(stderr, "preset recipe write failed: %s\n",
                recipe_report.message);
        return 1;
    }
    cage = (ProceduralSurfaceCageContract){
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = options.width,
        .height_units = options.height,
        .depth_units = options.depth,
        .target_edge_length_units = options.target_edge
    };
    if (active_binding &&
        !ProceduralSurfacePrismBindingContext_Init(
            &binding_context, &cage, active_binding, &graph,
            &binding_report)) {
        fprintf(stderr, "surface binding context failed: %s\n",
                binding_report.message);
        return 1;
    }
    if (active_binding) active_binding_context = &binding_context;
    if (!ProceduralSurfacePrismMesh_DeriveRequirements(
            &cage, &recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
            &requirements, &mesh_report)) {
        fprintf(stderr, "preset requirements failed: %s\n",
                mesh_report.message);
        return 1;
    }
    vertices = calloc((size_t)requirements.vertex_count, sizeof(*vertices));
    triangles = calloc((size_t)requirements.triangle_count, sizeof(*triangles));
    if (!vertices || !triangles) goto cleanup;
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = triangles,
        .triangle_capacity = (size_t)requirements.triangle_count
    };
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = recipe.quality.max_field_evaluations
    };
    if (options.selected_face_name) {
        const ProceduralSurfaceSelectedFaceShellRequest request = {
            .source_asset_id = options.source_asset_id,
            .derived_asset_id = options.asset_id,
            .selected_face = selected_face,
            .cage = &cage,
            .recipe = &recipe,
            .graph = &graph,
            .binding = active_binding,
            .quality = PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL};
        if (!ProceduralSurfaceSelectedFaceShell_Compile(
                &request, &budget, &buffers, &requirements, &summary,
                &selected_receipt, &selected_report)) {
            fprintf(stderr,
                    "selected-face shell compilation failed: %s (%s)\n",
                    selected_report.message, selected_report.field);
            goto cleanup;
        }
        active_selected_receipt = &selected_receipt;
    } else if (!ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
                   &cage, &recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
                   active_binding_context
                       ? ProceduralSurfacePrismBinding_EvaluateLegacy
                       : evaluate_graph_legacy,
                   active_binding_context
                       ? (const void *)active_binding_context
                       : (const void *)&graph,
                   active_binding_context
                       ? ProceduralSurfacePrismBinding_ResolveDisplacementDirection
                       : NULL,
                   active_binding_context
                       ? (const void *)active_binding_context
                       : NULL,
                   &budget, &buffers, &summary, &mesh_report)) {
        fprintf(stderr, "preset mesh generation failed: %s (%s)\n",
                mesh_report.message, mesh_report.field);
        goto cleanup;
    }
    mesh = (ProceduralSurfacePrismMesh){
        .vertices = vertices,
        .vertex_count = buffers.vertex_count,
        .triangles = triangles,
        .triangle_count = buffers.triangle_count
    };
    core_mesh_asset_runtime_document_init(&document);
    core_result = ProceduralSurfaceMeshAsset_FromPrism(
        &mesh, &summary, options.asset_id, options.source_asset_id, &document);
    if (core_result.code != CORE_OK) {
        fprintf(stderr, "preset mesh adaptation failed: %s\n",
                core_result.message);
        core_mesh_asset_runtime_document_free(&document);
        goto cleanup;
    }
    core_result = core_mesh_asset_runtime_document_save_file(
        &document, options.asset_path);
    core_mesh_asset_runtime_document_free(&document);
    if (core_result.code != CORE_OK) {
        fprintf(stderr, "preset mesh asset write failed: %s\n",
                core_result.message);
        goto cleanup;
    }
    if (!write_material(
            &options, &recipe, &graph, active_binding_context,
            &mesh, &summary,
            material_digest, &stats)) {
        fprintf(stderr, "preset material artifact write failed\n");
        goto cleanup;
    }
    if (!write_manifest(
            &options, &recipe, &graph, active_binding, &summary,
            material_digest)) {
        fprintf(stderr, "preset manifest write failed\n");
        goto cleanup;
    }
    if (!write_summary(
            &options, &graph, active_binding, &requirements, &summary,
            material_digest, &stats, active_selected_receipt)) {
        fprintf(stderr, "preset summary write failed\n");
        goto cleanup;
    }
    printf("%s\n", options.summary_path);
    exit_code = 0;

cleanup:
    free(triangles);
    free(vertices);
    return exit_code;
}
