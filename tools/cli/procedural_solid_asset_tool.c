#include "core_io.h"
#include "core_mesh_asset.h"
#include "procedural/procedural_solid_graph.h"
#include "procedural/procedural_solid_local_remesh.h"
#include "procedural/procedural_solid_mesh.h"
#include "procedural/procedural_solid_quality.h"
#include "procedural/procedural_solid_regions.h"
#include "procedural/procedural_solid_remesh.h"
#include "procedural/procedural_solid_source_accel.h"

#include <json-c/json.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SourceOption {
    char id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    const char *path;
    CoreMeshAssetRuntimeDocument document;
    ProceduralSolidSourceAccel accel;
} SourceOption;

typedef struct Options {
    const char *graph;
    const char *output;
    const char *summary;
    const char *asset_id;
    CoreObjectVec3 bounds_min;
    CoreObjectVec3 bounds_max;
    unsigned int cells;
    unsigned int maximum_cells;
    unsigned int quality_maximum_cells;
    double requested_feature_size;
    int adaptive;
    int local_adaptive;
    int quality_adaptive;
    int assign_regions;
    double surface_band_cells;
    double crease_angle_degrees;
    size_t maximum_output_vertices;
    size_t min_components;
    size_t max_components;
    ProceduralSolidCollisionAuthority collision_authority;
    size_t source_count;
    SourceOption sources[PROCEDURAL_SOLID_GRAPH_MAX_SOURCES];
} Options;

static int parse_double(const char *text, double *out) {
    char *end = NULL;
    errno = 0;
    *out = strtod(text, &end);
    return !errno && end && end != text && !*end;
}

static int parse_size(const char *text, size_t *out) {
    char *end = NULL;
    unsigned long long value;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || !end || end == text || *end || value > SIZE_MAX) return 0;
    *out = (size_t)value;
    return 1;
}

static int parse_uint(const char *text, unsigned int *out) {
    size_t value;
    if (!parse_size(text, &value) || value > UINT_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}

static int parse_vec3(const char *text, CoreObjectVec3 *out) {
    char buffer[192];
    char *first;
    char *second;
    if (strlen(text) >= sizeof(buffer)) return 0;
    snprintf(buffer, sizeof(buffer), "%s", text);
    first = strchr(buffer, ',');
    if (!first) return 0;
    *first++ = '\0';
    second = strchr(first, ',');
    if (!second) return 0;
    *second++ = '\0';
    return !strchr(second, ',') &&
           parse_double(buffer, &out->x) &&
           parse_double(first, &out->y) &&
           parse_double(second, &out->z);
}

static int parse_source(const char *text, SourceOption *out) {
    const char *equals = strchr(text, '=');
    size_t id_length;
    if (!equals || equals == text || !equals[1]) return 0;
    id_length = (size_t)(equals - text);
    if (id_length >= sizeof(out->id)) return 0;
    memcpy(out->id, text, id_length);
    out->id[id_length] = '\0';
    out->path = equals + 1;
    return 1;
}

static int parse_options(int argc, char **argv, Options *options) {
    *options = (Options){
        .bounds_min = {-2.4, -2.4, -2.4},
        .bounds_max = {2.4, 2.4, 2.4},
        .cells = 30u,
        .maximum_cells = 72u,
        .quality_maximum_cells = 96u,
        .requested_feature_size = 0.18,
        .surface_band_cells = 0.75,
        .crease_angle_degrees = 38.0,
        .maximum_output_vertices = 1000000u,
        .min_components = 1u,
        .max_components = 1u,
        .collision_authority =
            PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--graph") == 0 && i + 1 < argc) {
            options->graph = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->output = argv[++i];
        } else if (strcmp(argv[i], "--summary-out") == 0 &&
                   i + 1 < argc) {
            options->summary = argv[++i];
        } else if (strcmp(argv[i], "--asset-id") == 0 && i + 1 < argc) {
            options->asset_id = argv[++i];
        } else if (strcmp(argv[i], "--bounds-min") == 0 &&
                   i + 1 < argc) {
            if (!parse_vec3(argv[++i], &options->bounds_min)) return 0;
        } else if (strcmp(argv[i], "--bounds-max") == 0 &&
                   i + 1 < argc) {
            if (!parse_vec3(argv[++i], &options->bounds_max)) return 0;
        } else if (strcmp(argv[i], "--cells") == 0 && i + 1 < argc) {
            if (!parse_uint(argv[++i], &options->cells)) return 0;
        } else if (strcmp(argv[i], "--adaptive") == 0) {
            options->adaptive = 1;
        } else if (strcmp(argv[i], "--local-adaptive") == 0) {
            options->local_adaptive = 1;
        } else if (strcmp(argv[i], "--quality-adaptive") == 0) {
            options->quality_adaptive = 1;
        } else if (strcmp(argv[i], "--assign-regions") == 0) {
            options->assign_regions = 1;
        } else if (strcmp(argv[i], "--maximum-cells") == 0 &&
                   i + 1 < argc) {
            if (!parse_uint(argv[++i], &options->maximum_cells)) return 0;
        } else if (strcmp(argv[i], "--quality-maximum-cells") == 0 &&
                   i + 1 < argc) {
            if (!parse_uint(
                    argv[++i], &options->quality_maximum_cells)) return 0;
        } else if (strcmp(argv[i], "--feature-size") == 0 &&
                   i + 1 < argc) {
            if (!parse_double(
                    argv[++i], &options->requested_feature_size)) return 0;
        } else if (strcmp(argv[i], "--surface-band-cells") == 0 &&
                   i + 1 < argc) {
            if (!parse_double(
                    argv[++i], &options->surface_band_cells)) return 0;
        } else if (strcmp(argv[i], "--crease-angle-degrees") == 0 &&
                   i + 1 < argc) {
            if (!parse_double(
                    argv[++i], &options->crease_angle_degrees)) return 0;
        } else if (strcmp(argv[i], "--maximum-output-vertices") == 0 &&
                   i + 1 < argc) {
            if (!parse_size(
                    argv[++i], &options->maximum_output_vertices)) return 0;
        } else if (strcmp(argv[i], "--min-components") == 0 &&
                   i + 1 < argc) {
            if (!parse_size(argv[++i], &options->min_components)) return 0;
        } else if (strcmp(argv[i], "--max-components") == 0 &&
                   i + 1 < argc) {
            if (!parse_size(argv[++i], &options->max_components)) return 0;
        } else if (strcmp(argv[i], "--collision-authority") == 0 &&
                   i + 1 < argc) {
            const char *value = argv[++i];
            if (strcmp(value, "semantic_source") == 0) {
                options->collision_authority =
                    PROCEDURAL_SOLID_COLLISION_AUTHORITY_SEMANTIC_SOURCE;
            } else if (strcmp(value, "derived_shell") == 0) {
                options->collision_authority =
                    PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL;
            } else {
                return 0;
            }
        } else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
            if (options->source_count >=
                    PROCEDURAL_SOLID_GRAPH_MAX_SOURCES ||
                !parse_source(argv[++i],
                              &options->sources[options->source_count])) {
                return 0;
            }
            ++options->source_count;
        } else {
            return 0;
        }
    }
    return options->graph && options->output && options->summary &&
           options->asset_id &&
           options->maximum_output_vertices > 0u &&
           options->adaptive + options->local_adaptive +
               options->quality_adaptive <= 1;
}

static int add_vec3(json_object *root,
                    const char *key,
                    CoreObjectVec3 value) {
    json_object *object = json_object_new_object();
    if (!object) return 0;
    json_object_object_add(object, "x", json_object_new_double(value.x));
    json_object_object_add(object, "y", json_object_new_double(value.y));
    json_object_object_add(object, "z", json_object_new_double(value.z));
    json_object_object_add(root, key, object);
    return 1;
}

static int write_summary(const char *path,
                         const char *asset_id,
                         const char *semantic_source_id,
                         const ProceduralSolidMeshSummary *summary,
                         const ProceduralSolidRemeshSummary *remesh,
                         const ProceduralSolidLocalRemeshSummary *local,
                         const ProceduralSolidQualitySummary *quality,
                         const ProceduralSolidRegionSummary *regions) {
    json_object *root = json_object_new_object();
    const char *serialized;
    CoreResult result;
    if (!root) return 0;
#define ADD_INT(KEY, VALUE) \
    json_object_object_add(root, (KEY), json_object_new_int64((int64_t)(VALUE)))
#define ADD_DOUBLE(KEY, VALUE) \
    json_object_object_add(root, (KEY), json_object_new_double((VALUE)))
    json_object_object_add(
        root, "schema",
        json_object_new_string("ray_tracing.procedural_solid_receipt"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "asset_id", json_object_new_string(asset_id));
    json_object_object_add(
        root, "semantic_source_id",
        json_object_new_string(semantic_source_id));
    json_object_object_add(
        root, "collision_authority",
        json_object_new_string(ProceduralSolidCollisionAuthority_Name(
            summary->collision_authority)));
    json_object_object_add(
        root, "graph_digest_sha256",
        json_object_new_string(summary->graph_digest_sha256));
    json_object_object_add(
        root, "mesh_digest_sha256",
        json_object_new_string(summary->mesh_digest_sha256));
    ADD_INT("samples_x", summary->samples_x);
    ADD_INT("samples_y", summary->samples_y);
    ADD_INT("samples_z", summary->samples_z);
    ADD_INT("sample_count", summary->sample_count);
    ADD_INT("evaluated_sample_count", summary->evaluated_sample_count);
    ADD_INT("inside_sample_count", summary->inside_sample_count);
    ADD_INT("total_cell_count", summary->total_cell_count);
    ADD_INT("active_cell_count", summary->active_cell_count);
    ADD_INT("source_query_count", summary->source_query_count);
    ADD_INT("accelerated_source_query_count",
            summary->accelerated_source_query_count);
    ADD_INT("source_triangle_tests", summary->source_triangle_tests);
    ADD_INT("vertex_count", summary->vertex_count);
    ADD_INT("triangle_count", summary->triangle_count);
    ADD_INT("unique_edge_count", summary->unique_edge_count);
    ADD_INT("boundary_edge_count", summary->boundary_edge_count);
    ADD_INT("nonmanifold_edge_count", summary->nonmanifold_edge_count);
    ADD_INT("connected_component_count", summary->connected_component_count);
    ADD_INT("euler_characteristic", summary->euler_characteristic);
    ADD_DOUBLE("signed_volume_units3", summary->signed_volume_units3);
    ADD_DOUBLE("minimum_triangle_area2", summary->minimum_triangle_area2);
    ADD_DOUBLE("minimum_edge_length_units",
               summary->minimum_edge_length_units);
    ADD_DOUBLE("maximum_edge_length_units",
               summary->maximum_edge_length_units);
    ADD_DOUBLE("maximum_cell_size_units",
               summary->maximum_cell_size_units);
    ADD_DOUBLE("thin_feature_floor_units", summary->thin_feature_floor_units);
    ADD_DOUBLE("boundary_min_signed_distance",
               summary->boundary_min_signed_distance);
    json_object_object_add(
        root, "conforming_cell_self_intersection_free",
        json_object_new_boolean(
            summary->conforming_cell_self_intersection_free));
    if (local) {
        json_object *passes = json_object_new_array();
        const ProceduralSolidFeatureSummary *feature =
            &local->selected_feature;
        json_object_object_add(
            root, "adaptive", json_object_new_boolean(0));
        json_object_object_add(
            root, "local_adaptive", json_object_new_boolean(!quality));
        json_object_object_add(
            root, "quality_adaptive", json_object_new_boolean(quality != NULL));
        ADD_INT("local_pass_count", local->pass_count);
        ADD_INT("local_selected_pass", local->selected_pass);
        json_object_object_add(
            root, "local_converged",
            json_object_new_boolean(local->converged));
        ADD_INT("feature_projected_vertex_count",
                feature->projected_vertex_count);
        ADD_INT("feature_vertex_count", feature->feature_vertex_count);
        ADD_DOUBLE("feature_residual_rms_before",
                   feature->residual_rms_before);
        ADD_DOUBLE("feature_residual_rms_after",
                   feature->residual_rms_after);
        ADD_DOUBLE("feature_improvement_ratio",
                   feature->improvement_ratio);
        ADD_DOUBLE("feature_maximum_position_delta_units",
                   feature->maximum_position_delta_units);
        json_object_object_add(
            root, "feature_topology_preserved",
            json_object_new_boolean(feature->topology_preserved));
        for (size_t i = 0u; i < local->pass_count; ++i) {
            const ProceduralSolidLocalRemeshPass *pass =
                &local->passes[i];
            json_object *entry = json_object_new_object();
            json_object_object_add(
                entry, "coarse_cells",
                json_object_new_int64(pass->coarse_cells));
            json_object_object_add(
                entry, "fine_cells",
                json_object_new_int64(pass->fine_cells));
            json_object_object_add(
                entry, "coarse_sample_count",
                json_object_new_int64(
                    (int64_t)pass->coarse_sample_count));
            json_object_object_add(
                entry, "total_fine_cell_count",
                json_object_new_int64(
                    (int64_t)pass->total_fine_cell_count));
            json_object_object_add(
                entry, "active_fine_cell_count",
                json_object_new_int64(
                    (int64_t)pass->active_fine_cell_count));
            json_object_object_add(
                entry, "active_cell_ratio",
                json_object_new_double(pass->active_cell_ratio));
            json_object_object_add(
                entry, "inactive_interface_face_count",
                json_object_new_int64(
                    (int64_t)pass->inactive_interface_face_count));
            json_object_object_add(
                entry, "transition_surface_crossing_count",
                json_object_new_int64(
                    (int64_t)pass->transition_surface_crossing_count));
            json_object_object_add(
                entry, "relative_volume_delta",
                json_object_new_double(pass->relative_volume_delta));
            json_object_object_add(
                entry, "bounds_delta_units",
                json_object_new_double(pass->bounds_delta_units));
            json_object_object_add(
                entry, "topology_matches_previous",
                json_object_new_boolean(pass->topology_matches_previous));
            json_object_object_add(
                entry, "converged",
                json_object_new_boolean(pass->converged));
            json_object_object_add(
                entry, "mesh_digest_sha256",
                json_object_new_string(
                    pass->mesh.mesh_digest_sha256));
            json_object_array_add(passes, entry);
        }
        json_object_object_add(root, "local_passes", passes);
        if (quality) {
            json_object_object_add(
                root, "quality_refinement_triggered",
                json_object_new_boolean(quality->refinement_triggered));
            json_object_object_add(
                root, "quality_refinement_selected",
                json_object_new_boolean(quality->refinement_selected));
            ADD_INT("quality_baseline_cells", quality->baseline_cells);
            ADD_INT("quality_selected_cells", quality->selected_cells);
            ADD_DOUBLE("quality_refinement_improvement_ratio",
                       quality->refinement_improvement_ratio);
            ADD_DOUBLE("quality_baseline_signed_distance_rms_units",
                       quality->baseline_error.signed_distance_rms_units);
            ADD_DOUBLE("quality_selected_signed_distance_rms_units",
                       quality->selected_error.signed_distance_rms_units);
            ADD_DOUBLE("quality_baseline_face_gradient_rms_degrees",
                       quality->baseline_error.face_gradient_rms_degrees);
            ADD_DOUBLE("quality_selected_face_gradient_rms_degrees",
                       quality->selected_error.face_gradient_rms_degrees);
            ADD_DOUBLE("quality_baseline_composite_score",
                       quality->baseline_error.composite_score);
            ADD_DOUBLE("quality_selected_composite_score",
                       quality->selected_error.composite_score);
            ADD_INT("crease_candidate_vertex_count",
                    quality->crease.candidate_vertex_count);
            ADD_INT("crease_optimized_vertex_count",
                    quality->crease.optimized_vertex_count);
            ADD_DOUBLE("crease_qef_rms_before",
                       quality->crease.qef_rms_before);
            ADD_DOUBLE("crease_qef_rms_after",
                       quality->crease.qef_rms_after);
            ADD_DOUBLE("crease_qef_improvement_ratio",
                       quality->crease.qef_improvement_ratio);
            ADD_DOUBLE("crease_relative_volume_delta",
                       quality->crease.relative_volume_delta);
            json_object_object_add(
                root, "crease_topology_preserved",
                json_object_new_boolean(
                    quality->crease.topology_preserved));
            ADD_INT("shading_source_vertex_count",
                    quality->shading.source_vertex_count);
            ADD_INT("shading_output_vertex_count",
                    quality->shading.output_vertex_count);
            ADD_INT("shading_split_vertex_count",
                    quality->shading.split_vertex_count);
            ADD_INT("shading_hard_vertex_count",
                    quality->shading.hard_vertex_count);
            ADD_INT("shading_normal_island_count",
                    quality->shading.normal_island_count);
            ADD_DOUBLE("shading_hard_corner_rms_degrees_before",
                       quality->shading.hard_corner_rms_degrees_before);
            ADD_DOUBLE("shading_hard_corner_rms_degrees_after",
                       quality->shading.hard_corner_rms_degrees_after);
            ADD_DOUBLE("shading_hard_corner_improvement_ratio",
                       quality->shading.hard_corner_improvement_ratio);
            json_object_object_add(
                root, "shading_geometric_topology_preserved",
                json_object_new_boolean(
                    quality->shading.geometric_topology_preserved));
        }
    } else if (remesh) {
        json_object *passes = json_object_new_array();
        json_object_object_add(root, "adaptive",
                               json_object_new_boolean(1));
        json_object_object_add(root, "local_adaptive",
                               json_object_new_boolean(0));
        json_object_object_add(root, "quality_adaptive",
                               json_object_new_boolean(0));
        ADD_INT("adaptive_pass_count", remesh->pass_count);
        ADD_INT("adaptive_selected_pass", remesh->selected_pass);
        json_object_object_add(root, "adaptive_converged",
                               json_object_new_boolean(remesh->converged));
        for (size_t i = 0u; i < remesh->pass_count; ++i) {
            const ProceduralSolidRemeshPass *pass = &remesh->passes[i];
            json_object *entry = json_object_new_object();
            json_object_object_add(
                entry, "cells", json_object_new_int64(pass->cells));
            json_object_object_add(
                entry, "vertex_count",
                json_object_new_int64((int64_t)pass->vertex_count));
            json_object_object_add(
                entry, "triangle_count",
                json_object_new_int64((int64_t)pass->triangle_count));
            json_object_object_add(
                entry, "euler_characteristic",
                json_object_new_int(pass->euler_characteristic));
            json_object_object_add(
                entry, "relative_volume_delta",
                json_object_new_double(pass->relative_volume_delta));
            json_object_object_add(
                entry, "bounds_delta_units",
                json_object_new_double(pass->bounds_delta_units));
            json_object_object_add(
                entry, "thin_feature_floor_units",
                json_object_new_double(pass->thin_feature_floor_units));
            json_object_object_add(
                entry, "topology_matches_previous",
                json_object_new_boolean(pass->topology_matches_previous));
            json_object_object_add(
                entry, "converged",
                json_object_new_boolean(pass->converged));
            json_object_object_add(
                entry, "mesh_digest_sha256",
                json_object_new_string(pass->mesh_digest_sha256));
            json_object_array_add(passes, entry);
        }
        json_object_object_add(root, "adaptive_passes", passes);
    } else {
        json_object_object_add(root, "adaptive",
                               json_object_new_boolean(0));
        json_object_object_add(root, "local_adaptive",
                               json_object_new_boolean(0));
        json_object_object_add(root, "quality_adaptive",
                               json_object_new_boolean(0));
    }
    if (regions) {
        ADD_INT("region_count", regions->region_count);
        ADD_INT("retained_triangle_count",
                regions->retained_triangle_count);
        ADD_INT("cut_triangle_count", regions->cut_triangle_count);
        ADD_INT("blend_triangle_count", regions->blend_triangle_count);
        json_object_object_add(
            root, "region_digest_sha256",
            json_object_new_string(regions->region_digest_sha256));
        {
            json_object *region_array = json_object_new_array();
            for (size_t i = 0u; i < regions->region_count; ++i) {
                const ProceduralSolidRegionRecord *record =
                    &regions->regions[i];
                json_object *entry = json_object_new_object();
                json_object_object_add(
                    entry, "region_id",
                    json_object_new_string(record->region_id));
                json_object_object_add(
                    entry, "kind",
                    json_object_new_string(
                        ProceduralSolidRegionKind_Name(record->kind)));
                json_object_object_add(
                    entry, "primary_node_id",
                    json_object_new_string(record->primary_node_id));
                json_object_object_add(
                    entry, "secondary_node_id",
                    json_object_new_string(record->secondary_node_id));
                json_object_object_add(
                    entry, "triangle_count",
                    json_object_new_int64(
                        (int64_t)record->triangle_count));
                json_object_array_add(region_array, entry);
            }
            json_object_object_add(root, "regions", region_array);
        }
    }
    if (!add_vec3(root, "bounds_min", summary->bounds_min) ||
        !add_vec3(root, "bounds_max", summary->bounds_max)) {
        json_object_put(root);
        return 0;
    }
#undef ADD_DOUBLE
#undef ADD_INT
    serialized = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, serialized, strlen(serialized));
    json_object_put(root);
    return result.code == CORE_OK;
}

int main(int argc, char **argv) {
    Options options;
    ProceduralSolidGraphV1 graph;
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidSourceSet source_set = {0};
    ProceduralSolidMeshConfig config;
    ProceduralSolidMeshSummary summary;
    ProceduralSolidRemeshSummary remesh_summary;
    ProceduralSolidLocalRemeshSummary local_summary;
    ProceduralSolidQualitySummary quality_summary;
    ProceduralSolidRegionSummary assigned_regions;
    ProceduralSolidMeshReport mesh_report;
    CoreMeshAssetRuntimeDocument output;
    CoreResult result;
    int exit_code = 1;
    if (!parse_options(argc, argv, &options)) {
        fprintf(
            stderr,
            "usage: %s --graph FILE --out FILE --summary-out FILE "
            "--asset-id ID [--source ID=FILE] [--bounds-min X,Y,Z] "
            "[--bounds-max X,Y,Z] [--cells N] [--adaptive] "
            "[--local-adaptive] [--quality-adaptive] [--assign-regions] "
            "[--maximum-cells N] [--quality-maximum-cells N] "
            "[--feature-size UNITS] "
            "[--surface-band-cells N] [--crease-angle-degrees N] "
            "[--maximum-output-vertices N] "
            "[--min-components N] "
            "[--max-components N] "
            "[--collision-authority semantic_source|derived_shell]\n",
            argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&output);
    memset(&assigned_regions, 0, sizeof(assigned_regions));
    if (!ProceduralSolidGraphV1_LoadJsonFile(
            options.graph, &graph, &graph_report)) {
        fprintf(stderr, "solid graph %s: %s\n",
                ProceduralSolidGraphStatus_Name(graph_report.status),
                graph_report.message);
        goto cleanup;
    }
    for (size_t i = 0u; i < options.source_count; ++i) {
        core_mesh_asset_runtime_document_init(&options.sources[i].document);
        ProceduralSolidSourceAccel_Init(&options.sources[i].accel);
        result = core_mesh_asset_runtime_document_load_file(
            options.sources[i].path, &options.sources[i].document);
        if (result.code != CORE_OK) {
            fprintf(stderr, "source mesh '%s' failed: %s\n",
                    options.sources[i].id, result.message);
            goto cleanup;
        }
        snprintf(source_set.sources[i].source_id,
                 sizeof(source_set.sources[i].source_id), "%s",
                 options.sources[i].id);
        source_set.sources[i].mesh = &options.sources[i].document;
        if (options.local_adaptive || options.quality_adaptive ||
            options.assign_regions) {
            if (!ProceduralSolidSourceAccel_Build(
                    &options.sources[i].document, 8u,
                    &options.sources[i].accel)) {
                fprintf(stderr, "source mesh '%s' acceleration failed\n",
                        options.sources[i].id);
                goto cleanup;
            }
            source_set.sources[i].accel = &options.sources[i].accel;
        }
        ++source_set.source_count;
    }
    ProceduralSolidMeshConfig_Init(&config);
    config.bounds_min = options.bounds_min;
    config.bounds_max = options.bounds_max;
    config.cells_x = options.cells;
    config.cells_y = options.cells;
    config.cells_z = options.cells;
    config.min_components = options.min_components;
    config.max_components = options.max_components;
    config.collision_authority = options.collision_authority;
    if (options.quality_adaptive) {
        ProceduralSolidQualityConfig quality_config;
        ProceduralSolidQualityConfig_Init(&quality_config);
        quality_config.local.mesh = config;
        quality_config.local.base_cells = options.cells;
        quality_config.local.surface_band_cells =
            options.surface_band_cells;
        quality_config.local.feature.crease_angle_degrees =
            options.crease_angle_degrees;
        quality_config.local.feature.maximum_projection_step_units =
            options.requested_feature_size * 0.5;
        quality_config.baseline_maximum_cells = options.maximum_cells;
        quality_config.quality_maximum_cells =
            options.quality_maximum_cells;
        quality_config.crease.crease_angle_degrees =
            options.crease_angle_degrees;
        quality_config.shading.crease_angle_degrees =
            options.crease_angle_degrees;
        quality_config.shading.maximum_output_vertices =
            options.maximum_output_vertices;
        if (!ProceduralSolidQuality_Compile(
                &graph, &source_set, &quality_config, options.asset_id,
                &output, &quality_summary, &mesh_report)) {
            fprintf(stderr, "solid quality remesh %s (%s): %s\n",
                    ProceduralSolidMeshStatus_Name(mesh_report.status),
                    mesh_report.field, mesh_report.message);
            goto cleanup;
        }
        local_summary = quality_summary.local;
        summary = quality_summary.local.selected_mesh;
    } else if (options.local_adaptive) {
        ProceduralSolidLocalRemeshConfig local_config;
        ProceduralSolidLocalRemeshConfig_Init(&local_config);
        local_config.mesh = config;
        local_config.base_cells = options.cells;
        local_config.maximum_cells = options.maximum_cells;
        local_config.maximum_passes =
            PROCEDURAL_SOLID_LOCAL_REMESH_MAX_PASSES;
        local_config.surface_band_cells = options.surface_band_cells;
        local_config.feature.crease_angle_degrees =
            options.crease_angle_degrees;
        local_config.feature.maximum_projection_step_units =
            options.requested_feature_size * 0.5;
        if (!ProceduralSolidLocalRemesh_Compile(
                &graph, &source_set, &local_config, options.asset_id,
                &output, &local_summary, &mesh_report)) {
            fprintf(stderr, "solid local remesh %s (%s): %s\n",
                    ProceduralSolidMeshStatus_Name(mesh_report.status),
                    mesh_report.field, mesh_report.message);
            goto cleanup;
        }
        summary = local_summary.selected_mesh;
    } else if (options.adaptive) {
        ProceduralSolidRemeshConfig remesh_config;
        ProceduralSolidRemeshConfig_Init(&remesh_config);
        remesh_config.mesh = config;
        remesh_config.base_cells = options.cells;
        remesh_config.maximum_cells = options.maximum_cells;
        remesh_config.maximum_passes =
            PROCEDURAL_SOLID_REMESH_MAX_PASSES;
        remesh_config.requested_feature_size_units =
            options.requested_feature_size;
        remesh_config.maximum_relative_volume_delta = 0.06;
        remesh_config.maximum_bounds_delta_units = 0.06;
        if (!ProceduralSolidRemesh_CompileAdaptive(
                &graph, &source_set, &remesh_config, options.asset_id,
                &output, &remesh_summary, &mesh_report)) {
            fprintf(stderr, "solid adaptive remesh %s (%s): %s\n",
                    ProceduralSolidMeshStatus_Name(mesh_report.status),
                    mesh_report.field, mesh_report.message);
            goto cleanup;
        }
        summary = remesh_summary.selected_mesh;
    } else if (!ProceduralSolidMesh_Compile(
                   &graph, &source_set, &config, options.asset_id, &output,
                   &summary, &mesh_report)) {
        fprintf(stderr, "solid mesh %s (%s): %s\n",
                ProceduralSolidMeshStatus_Name(mesh_report.status),
                mesh_report.field, mesh_report.message);
        goto cleanup;
    }
    if (options.assign_regions &&
        !options.local_adaptive && !options.quality_adaptive &&
        !ProceduralSolidRegions_Assign(
            &graph, &source_set, &config, &output, &summary,
            &assigned_regions, &mesh_report)) {
        fprintf(stderr, "solid region assignment %s (%s): %s\n",
                ProceduralSolidMeshStatus_Name(mesh_report.status),
                mesh_report.field, mesh_report.message);
        goto cleanup;
    }
    result = core_mesh_asset_runtime_document_save_file(
        &output, options.output);
    if (result.code != CORE_OK ||
        !write_summary(options.summary, options.asset_id,
                       graph.semantic_source_id, &summary,
                       options.adaptive ? &remesh_summary : NULL,
                       (options.local_adaptive ||
                        options.quality_adaptive)
                           ? &local_summary : NULL,
                       options.quality_adaptive
                           ? &quality_summary : NULL,
                       (options.local_adaptive ||
                        options.quality_adaptive)
                           ? &local_summary.selected_regions
                           : (options.assign_regions
                                  ? &assigned_regions
                                  : NULL))) {
        fprintf(stderr, "solid asset or receipt write failed\n");
        goto cleanup;
    }
    printf("%s\n", options.summary);
    exit_code = 0;
cleanup:
    core_mesh_asset_runtime_document_free(&output);
    for (size_t i = 0u; i < options.source_count; ++i) {
        ProceduralSolidSourceAccel_Free(&options.sources[i].accel);
        core_mesh_asset_runtime_document_free(&options.sources[i].document);
    }
    return exit_code;
}
