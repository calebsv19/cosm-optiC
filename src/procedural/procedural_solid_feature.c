#include "procedural/procedural_solid_feature.h"

#include "procedural_solid_field_query.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CoreObjectVec3 vec_scale(CoreObjectVec3 value, double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static double vec_length(CoreObjectVec3 value) {
    return sqrt(vec_dot(value, value));
}

static bool vec_normalize(CoreObjectVec3 value, CoreObjectVec3 *out) {
    const double length = vec_length(value);
    if (!out || !isfinite(length) || length <= 1.0e-14) return false;
    *out = vec_scale(value, 1.0 / length);
    return true;
}

static void feature_report(
    ProceduralSolidMeshReport *report,
    ProceduralSolidMeshStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field);
    snprintf(report->message, sizeof(report->message), "%s", message);
}

void ProceduralSolidFeatureConfig_Init(ProceduralSolidFeatureConfig *config) {
    if (!config) return;
    *config = (ProceduralSolidFeatureConfig){
        .projection_iterations = 4u,
        .maximum_projection_step_units = 0.08,
        .crease_angle_degrees = 38.0,
        .minimum_improvement_ratio = 0.05};
}

static bool config_valid(const ProceduralSolidFeatureConfig *config) {
    return config && config->projection_iterations > 0u &&
        config->projection_iterations <= 16u &&
        isfinite(config->maximum_projection_step_units) &&
        config->maximum_projection_step_units > 0.0 &&
        isfinite(config->crease_angle_degrees) &&
        config->crease_angle_degrees > 0.0 &&
        config->crease_angle_degrees < 180.0 &&
        isfinite(config->minimum_improvement_ratio) &&
        config->minimum_improvement_ratio >= 0.0 &&
        config->minimum_improvement_ratio < 1.0;
}

bool ProceduralSolidFeature_Optimize(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidFeatureConfig *config,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidFeatureSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidFeatureSummary summary;
    CoreMeshAssetRuntimeVertex *old_vertices = NULL;
    CoreObjectVec3 *target_positions = NULL;
    CoreMeshAssetRuntimeContract old_contract;
    ProceduralSolidMeshSummary old_mesh_summary;
    CoreObjectVec3 *reference_normals = NULL;
    uint8_t *have_reference = NULL;
    double *minimum_dot = NULL;
    const size_t baseline_boundary = mesh_summary
        ? mesh_summary->boundary_edge_count : SIZE_MAX;
    const size_t baseline_nonmanifold = mesh_summary
        ? mesh_summary->nonmanifold_edge_count : SIZE_MAX;
    const int baseline_euler =
        mesh_summary ? mesh_summary->euler_characteristic : 0;
    const double gradient_step =
        mesh_config ? mesh_config->gradient_step_units : 0.0;
    if (!graph || !mesh_config || !document || !mesh_summary ||
        !out_summary || !config_valid(config) ||
        document->vertex_count == 0u || !document->vertices ||
        document->triangle_count == 0u || !document->triangles) {
        feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                       "feature_config",
                       "feature optimizer inputs are invalid");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    old_vertices = malloc(
        document->vertex_count * sizeof(*old_vertices));
    target_positions = calloc(
        document->vertex_count, sizeof(*target_positions));
    if (!old_vertices || !target_positions) {
        free(old_vertices);
        free(target_positions);
        feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                       "feature_projection",
                       "feature projection allocation failed");
        return false;
    }
    memcpy(old_vertices, document->vertices,
           document->vertex_count * sizeof(*old_vertices));
    old_contract = document->contract;
    old_mesh_summary = *mesh_summary;
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        CoreObjectVec3 point = document->vertices[i].position;
        double distance;
        if (!procedural_solid_field_distance(
                graph, sources, point, &distance)) {
            feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                           "feature_distance",
                           "feature residual evaluation failed");
            goto rollback;
        }
        summary.residual_rms_before += distance * distance;
        summary.residual_max_before =
            fmax(summary.residual_max_before, fabs(distance));
        for (size_t iteration = 0u;
             iteration < config->projection_iterations; ++iteration) {
            CoreObjectVec3 gradient;
            double step;
            if (!procedural_solid_field_gradient(
                    graph, sources, point, gradient_step, &gradient)) {
                feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                               "feature_gradient",
                               "feature gradient evaluation failed");
                goto rollback;
            }
            step = fmax(
                -config->maximum_projection_step_units,
                fmin(config->maximum_projection_step_units, distance));
            /*
             * Under-relax the Newton projection.  Marching-tetra vertices can
             * share extremely short feature edges; moving every endpoint all
             * the way to the nonlinear zero set in one step can collapse
             * otherwise valid triangles.  Repeated 0.35 steps still provide a
             * measurable residual reduction while preserving the mesh cage.
             */
            step *= 0.35;
            point = vec_sub(point, vec_scale(gradient, step));
            if (!procedural_solid_field_distance(
                    graph, sources, point, &distance)) {
                feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                               "feature_projection",
                               "feature projection evaluation failed");
                goto rollback;
            }
            if (fabs(distance) <= 1.0e-10) break;
        }
        target_positions[i] = point;
    }
    summary.residual_rms_before = sqrt(
        summary.residual_rms_before / (double)document->vertex_count);
    {
        bool accepted = false;
        double scale = 1.0;
        for (size_t attempt = 0u; attempt < 8u; ++attempt, scale *= 0.5) {
            double residual_square_sum = 0.0;
            size_t projected_count = 0u;
            double maximum_delta = 0.0;
            double residual_max = 0.0;
            memcpy(document->vertices, old_vertices,
                   document->vertex_count * sizeof(*old_vertices));
            document->contract = old_contract;
            *mesh_summary = old_mesh_summary;
            for (size_t i = 0u; i < document->vertex_count; ++i) {
                const CoreObjectVec3 delta = vec_scale(
                    vec_sub(target_positions[i],
                            old_vertices[i].position),
                    scale);
                double distance;
                document->vertices[i].position = vec_sub(
                    old_vertices[i].position, vec_scale(delta, -1.0));
                if (!procedural_solid_field_gradient(
                        graph, sources, document->vertices[i].position,
                        gradient_step, &document->vertices[i].normal) ||
                    !procedural_solid_field_distance(
                        graph, sources, document->vertices[i].position,
                        &distance)) {
                    feature_report(
                        report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                        "feature_line_search",
                        "feature line-search field evaluation failed");
                    goto rollback;
                }
                residual_square_sum += distance * distance;
                residual_max = fmax(residual_max, fabs(distance));
                maximum_delta = fmax(maximum_delta, vec_length(delta));
                if (vec_length(delta) > 1.0e-12) ++projected_count;
            }
            summary.residual_rms_after = sqrt(
                residual_square_sum / (double)document->vertex_count);
            summary.residual_max_after = residual_max;
            summary.projected_vertex_count = projected_count;
            summary.maximum_position_delta_units = maximum_delta;
            if (summary.residual_rms_before > 1.0e-15) {
                summary.improvement_ratio =
                    1.0 - summary.residual_rms_after /
                        summary.residual_rms_before;
            } else {
                summary.improvement_ratio =
                    summary.residual_rms_after <=
                        summary.residual_rms_before ? 1.0 : 0.0;
            }
            if (!ProceduralSolidMesh_Reanalyze(
                    mesh_config, document, mesh_summary, report)) {
                continue;
            }
            summary.topology_preserved =
                mesh_summary->boundary_edge_count == baseline_boundary &&
                mesh_summary->nonmanifold_edge_count ==
                    baseline_nonmanifold &&
                mesh_summary->euler_characteristic == baseline_euler;
            summary.measurable_improvement =
                summary.residual_rms_after <=
                    summary.residual_rms_before &&
                summary.improvement_ratio >=
                    config->minimum_improvement_ratio;
            if (summary.topology_preserved &&
                summary.measurable_improvement) {
                accepted = true;
                break;
            }
        }
        if (!accepted) {
            feature_report(
                report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                "feature_line_search",
                "no feature projection step preserved shell validity and improvement");
            goto rollback;
        }
    }

    reference_normals = calloc(
        document->vertex_count, sizeof(*reference_normals));
    have_reference = calloc(
        document->vertex_count, sizeof(*have_reference));
    minimum_dot = malloc(document->vertex_count * sizeof(*minimum_dot));
    if (!reference_normals || !have_reference || !minimum_dot) {
        free(reference_normals);
        free(have_reference);
        free(minimum_dot);
        feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                       "feature_classification",
                       "feature classification allocation failed");
        goto rollback;
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        minimum_dot[i] = 1.0;
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle =
            &document->triangles[i];
        const size_t ids[3] = {
            triangle->a, triangle->b, triangle->c};
        CoreObjectVec3 normal;
        if (!vec_normalize(
                vec_cross(
                    vec_sub(
                        document->vertices[triangle->b].position,
                        document->vertices[triangle->a].position),
                    vec_sub(
                        document->vertices[triangle->c].position,
                        document->vertices[triangle->a].position)),
                &normal)) {
            free(reference_normals);
            free(have_reference);
            free(minimum_dot);
            feature_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                           "feature_triangle",
                           "feature projection created a degenerate triangle");
            goto rollback;
        }
        for (size_t corner = 0u; corner < 3u; ++corner) {
            const size_t vertex = ids[corner];
            if (!have_reference[vertex]) {
                reference_normals[vertex] = normal;
                have_reference[vertex] = 1u;
            } else {
                minimum_dot[vertex] = fmin(
                    minimum_dot[vertex],
                    vec_dot(reference_normals[vertex], normal));
            }
        }
    }
    {
        const double cosine_threshold =
            cos(config->crease_angle_degrees * M_PI / 180.0);
        for (size_t i = 0u; i < document->vertex_count; ++i) {
            if (minimum_dot[i] < cosine_threshold) {
                ++summary.feature_vertex_count;
            }
        }
    }
    free(reference_normals);
    free(have_reference);
    free(minimum_dot);
    document->normal_provenance =
        summary.feature_vertex_count > 0u
            ? CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE
            : CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    if (!ProceduralSolidMesh_RefreshIdentity(
            document, mesh_summary, report)) {
        goto rollback;
    }
    *out_summary = summary;
    free(old_vertices);
    free(target_positions);
    return true;

rollback:
    memcpy(document->vertices, old_vertices,
           document->vertex_count * sizeof(*old_vertices));
    document->contract = old_contract;
    *mesh_summary = old_mesh_summary;
    free(old_vertices);
    free(target_positions);
    return false;
}
