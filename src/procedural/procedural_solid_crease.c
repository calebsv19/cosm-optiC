#include "procedural/procedural_solid_crease.h"

#include "procedural_solid_field_query.h"
#include "procedural_solid_geometry_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CreasePlane {
    CoreObjectVec3 normal;
    double rhs;
} CreasePlane;

static void crease_report(
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

void ProceduralSolidCreaseConfig_Init(ProceduralSolidCreaseConfig *config) {
    if (!config) return;
    *config = (ProceduralSolidCreaseConfig){
        .crease_angle_degrees = 38.0,
        .maximum_position_delta_units = 0.08,
        .regularization = 0.02,
        .minimum_qef_improvement_ratio = 0.10,
        .maximum_relative_volume_delta = 0.02};
}

static bool crease_config_valid(
    const ProceduralSolidCreaseConfig *config) {
    return config &&
        isfinite(config->crease_angle_degrees) &&
        config->crease_angle_degrees > 0.0 &&
        config->crease_angle_degrees < 180.0 &&
        isfinite(config->maximum_position_delta_units) &&
        config->maximum_position_delta_units > 0.0 &&
        isfinite(config->regularization) &&
        config->regularization > 0.0 &&
        config->regularization <= 1.0 &&
        isfinite(config->minimum_qef_improvement_ratio) &&
        config->minimum_qef_improvement_ratio >= 0.0 &&
        config->minimum_qef_improvement_ratio < 1.0 &&
        isfinite(config->maximum_relative_volume_delta) &&
        config->maximum_relative_volume_delta >= 0.0 &&
        config->maximum_relative_volume_delta < 1.0;
}

static bool solve_3x3(
    double a[3][3],
    double b[3],
    CoreObjectVec3 *out) {
    double augmented[3][4];
    for (size_t row = 0u; row < 3u; ++row) {
        for (size_t column = 0u; column < 3u; ++column) {
            augmented[row][column] = a[row][column];
        }
        augmented[row][3] = b[row];
    }
    for (size_t column = 0u; column < 3u; ++column) {
        size_t pivot = column;
        for (size_t row = column + 1u; row < 3u; ++row) {
            if (fabs(augmented[row][column]) >
                fabs(augmented[pivot][column])) {
                pivot = row;
            }
        }
        if (fabs(augmented[pivot][column]) <= 1.0e-14) return false;
        if (pivot != column) {
            for (size_t k = column; k < 4u; ++k) {
                const double swap = augmented[column][k];
                augmented[column][k] = augmented[pivot][k];
                augmented[pivot][k] = swap;
            }
        }
        {
            const double divisor = augmented[column][column];
            for (size_t k = column; k < 4u; ++k) {
                augmented[column][k] /= divisor;
            }
        }
        for (size_t row = 0u; row < 3u; ++row) {
            double factor;
            if (row == column) continue;
            factor = augmented[row][column];
            for (size_t k = column; k < 4u; ++k) {
                augmented[row][k] -= factor * augmented[column][k];
            }
        }
    }
    *out = (CoreObjectVec3){
        augmented[0][3], augmented[1][3], augmented[2][3]};
    return isfinite(out->x) && isfinite(out->y) && isfinite(out->z);
}

static double plane_error(
    const CreasePlane *planes,
    size_t count,
    CoreObjectVec3 point) {
    double error = 0.0;
    for (size_t i = 0u; i < count; ++i) {
        const double residual =
            psg_vec_dot(planes[i].normal, point) - planes[i].rhs;
        error += residual * residual;
    }
    return error;
}

bool ProceduralSolidCrease_Optimize(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidCreaseConfig *config,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidCreaseSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidCreaseSummary summary;
    const size_t vertex_count = document ? document->vertex_count : 0u;
    const size_t triangle_count = document ? document->triangle_count : 0u;
    CoreMeshAssetRuntimeVertex *old_vertices = NULL;
    CoreObjectVec3 *target_positions = NULL;
    CoreMeshAssetRuntimeContract old_contract;
    ProceduralSolidMeshSummary old_mesh_summary;
    CoreObjectVec3 *face_normals = NULL;
    CoreObjectVec3 *face_centroids = NULL;
    size_t *incident_counts = NULL;
    size_t *incident_offsets = NULL;
    size_t *incident_cursor = NULL;
    size_t *incident_triangles = NULL;
    const double cosine_threshold = config
        ? cos(config->crease_angle_degrees * M_PI / 180.0) : 0.0;
    double qef_sum_before = 0.0;
    double qef_linear = 0.0;
    double qef_quadratic = 0.0;
    if (!graph || !crease_config_valid(config) || !mesh_config ||
        !document || !mesh_summary || !out_summary ||
        vertex_count == 0u || !document->vertices ||
        triangle_count == 0u || !document->triangles ||
        triangle_count > SIZE_MAX / 3u) {
        crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                      "crease_config",
                      "crease optimizer inputs are invalid");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    old_vertices = malloc(vertex_count * sizeof(*old_vertices));
    target_positions = calloc(vertex_count, sizeof(*target_positions));
    face_normals = calloc(triangle_count, sizeof(*face_normals));
    face_centroids = calloc(triangle_count, sizeof(*face_centroids));
    incident_counts = calloc(vertex_count, sizeof(*incident_counts));
    incident_offsets = calloc(vertex_count + 1u, sizeof(*incident_offsets));
    incident_cursor = calloc(vertex_count, sizeof(*incident_cursor));
    incident_triangles =
        malloc(triangle_count * 3u * sizeof(*incident_triangles));
    if (!old_vertices || !target_positions ||
        !face_normals || !face_centroids ||
        !incident_counts || !incident_offsets || !incident_cursor ||
        !incident_triangles) {
        crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                      "crease_alloc", "crease optimizer allocation failed");
        goto fail;
    }
    memcpy(old_vertices, document->vertices,
           vertex_count * sizeof(*old_vertices));
    for (size_t vertex = 0u; vertex < vertex_count; ++vertex) {
        target_positions[vertex] = document->vertices[vertex].position;
    }
    old_contract = document->contract;
    old_mesh_summary = *mesh_summary;
    for (size_t triangle = 0u; triangle < triangle_count; ++triangle) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[triangle];
        const size_t ids[3] = {t->a, t->b, t->c};
        CoreObjectVec3 a;
        CoreObjectVec3 b;
        CoreObjectVec3 c;
        if (t->a >= vertex_count || t->b >= vertex_count ||
            t->c >= vertex_count) {
            crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                          "crease_triangle",
                          "crease optimizer found an invalid triangle");
            goto fail;
        }
        a = document->vertices[t->a].position;
        b = document->vertices[t->b].position;
        c = document->vertices[t->c].position;
        if (!psg_vec_normalize(
                psg_vec_cross(psg_vec_sub(b, a), psg_vec_sub(c, a)),
                &face_normals[triangle])) {
            crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                          "crease_triangle",
                          "crease optimizer found a degenerate triangle");
            goto fail;
        }
        face_centroids[triangle] = psg_vec_scale(
            psg_vec_add(psg_vec_add(a, b), c), 1.0 / 3.0);
        for (size_t corner = 0u; corner < 3u; ++corner) {
            ++incident_counts[ids[corner]];
        }
    }
    for (size_t vertex = 0u; vertex < vertex_count; ++vertex) {
        incident_offsets[vertex + 1u] =
            incident_offsets[vertex] + incident_counts[vertex];
    }
    memcpy(incident_cursor, incident_offsets,
           vertex_count * sizeof(*incident_cursor));
    for (size_t triangle = 0u; triangle < triangle_count; ++triangle) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[triangle];
        const size_t ids[3] = {t->a, t->b, t->c};
        for (size_t corner = 0u; corner < 3u; ++corner) {
            incident_triangles[incident_cursor[ids[corner]]++] = triangle;
        }
    }
    for (size_t vertex = 0u; vertex < vertex_count; ++vertex) {
        const size_t begin = incident_offsets[vertex];
        const size_t count = incident_offsets[vertex + 1u] - begin;
        bool crease = false;
        CreasePlane *planes = NULL;
        double matrix[3][3] = {{0.0}};
        double rhs[3] = {0.0, 0.0, 0.0};
        CoreObjectVec3 candidate;
        const CoreObjectVec3 original = document->vertices[vertex].position;
        for (size_t a = 0u; a < count && !crease; ++a) {
            for (size_t b = a + 1u; b < count; ++b) {
                if (psg_vec_dot(
                        face_normals[incident_triangles[begin + a]],
                        face_normals[incident_triangles[begin + b]]) <
                    cosine_threshold) {
                    crease = true;
                    break;
                }
            }
        }
        if (!crease) continue;
        ++summary.candidate_vertex_count;
        planes = calloc(count, sizeof(*planes));
        if (!planes) {
            crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                          "crease_planes",
                          "crease constraint allocation failed");
            goto fail;
        }
        for (size_t local = 0u; local < count; ++local) {
            const size_t triangle = incident_triangles[begin + local];
            CoreObjectVec3 gradient;
            double distance;
            if (!procedural_solid_field_gradient(
                    graph, sources, face_centroids[triangle],
                    mesh_config->gradient_step_units, &gradient) ||
                !procedural_solid_field_distance(
                    graph, sources, face_centroids[triangle], &distance)) {
                free(planes);
                crease_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                              "crease_field",
                              "crease constraint field query failed");
                goto fail;
            }
            planes[local].normal = gradient;
            planes[local].rhs =
                psg_vec_dot(gradient, face_centroids[triangle]) - distance;
            matrix[0][0] += gradient.x * gradient.x;
            matrix[0][1] += gradient.x * gradient.y;
            matrix[0][2] += gradient.x * gradient.z;
            matrix[1][0] += gradient.y * gradient.x;
            matrix[1][1] += gradient.y * gradient.y;
            matrix[1][2] += gradient.y * gradient.z;
            matrix[2][0] += gradient.z * gradient.x;
            matrix[2][1] += gradient.z * gradient.y;
            matrix[2][2] += gradient.z * gradient.z;
            rhs[0] += gradient.x * planes[local].rhs;
            rhs[1] += gradient.y * planes[local].rhs;
            rhs[2] += gradient.z * planes[local].rhs;
        }
        {
            const double regularization =
                config->regularization * (double)count;
            matrix[0][0] += regularization;
            matrix[1][1] += regularization;
            matrix[2][2] += regularization;
            rhs[0] += regularization * original.x;
            rhs[1] += regularization * original.y;
            rhs[2] += regularization * original.z;
        }
        if (solve_3x3(matrix, rhs, &candidate)) {
            CoreObjectVec3 delta = psg_vec_sub(candidate, original);
            double delta_length = psg_vec_length(delta);
            const double before = plane_error(planes, count, original);
            if (delta_length > config->maximum_position_delta_units) {
                delta = psg_vec_scale(
                    delta,
                    config->maximum_position_delta_units / delta_length);
                candidate = psg_vec_add(original, delta);
                delta_length = config->maximum_position_delta_units;
            }
            target_positions[vertex] = candidate;
            qef_sum_before += before;
            for (size_t local = 0u; local < count; ++local) {
                const double residual =
                    psg_vec_dot(planes[local].normal, original) -
                    planes[local].rhs;
                const double delta_projection =
                    psg_vec_dot(planes[local].normal, delta);
                qef_linear += 2.0 * residual * delta_projection;
                qef_quadratic += delta_projection * delta_projection;
            }
            summary.constraint_count += count;
            summary.maximum_position_delta_units = fmax(
                summary.maximum_position_delta_units, delta_length);
            if (delta_length > 1.0e-12) ++summary.optimized_vertex_count;
        }
        free(planes);
    }
    summary.qef_rms_before = summary.constraint_count > 0u
        ? sqrt(qef_sum_before / (double)summary.constraint_count)
        : 0.0;
    {
        bool accepted = false;
        double scale = 1.0;
        for (size_t attempt = 0u; attempt < 8u; ++attempt, scale *= 0.5) {
            double qef_sum_after;
            memcpy(document->vertices, old_vertices,
                   vertex_count * sizeof(*old_vertices));
            document->contract = old_contract;
            *mesh_summary = old_mesh_summary;
            for (size_t vertex = 0u; vertex < vertex_count; ++vertex) {
                const CoreObjectVec3 delta = psg_vec_sub(
                    target_positions[vertex],
                    old_vertices[vertex].position);
                document->vertices[vertex].position = psg_vec_add(
                    old_vertices[vertex].position,
                    psg_vec_scale(delta, scale));
                if (!procedural_solid_field_gradient(
                        graph, sources,
                        document->vertices[vertex].position,
                        mesh_config->gradient_step_units,
                        &document->vertices[vertex].normal)) {
                    crease_report(
                        report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                        "crease_normal",
                        "crease optimizer normal query failed");
                    goto rollback;
                }
            }
            qef_sum_after = fmax(
                0.0,
                qef_sum_before + scale * qef_linear +
                    scale * scale * qef_quadratic);
            summary.qef_rms_after = summary.constraint_count > 0u
                ? sqrt(qef_sum_after /
                       (double)summary.constraint_count)
                : 0.0;
            summary.qef_improvement_ratio =
                summary.qef_rms_before > 1.0e-15
                    ? 1.0 - summary.qef_rms_after /
                        summary.qef_rms_before
                    : 1.0;
            summary.maximum_position_delta_units *= scale;
            if (!ProceduralSolidMesh_Reanalyze(
                    mesh_config, document, mesh_summary, report)) {
                summary.maximum_position_delta_units /= scale;
                continue;
            }
            summary.relative_volume_delta =
                fabs(mesh_summary->signed_volume_units3 -
                     old_mesh_summary.signed_volume_units3) /
                fmax(fabs(old_mesh_summary.signed_volume_units3), 1.0e-12);
            summary.topology_preserved =
                mesh_summary->boundary_edge_count ==
                    old_mesh_summary.boundary_edge_count &&
                mesh_summary->nonmanifold_edge_count ==
                    old_mesh_summary.nonmanifold_edge_count &&
                mesh_summary->connected_component_count ==
                    old_mesh_summary.connected_component_count &&
                mesh_summary->euler_characteristic ==
                    old_mesh_summary.euler_characteristic &&
                summary.relative_volume_delta <=
                    config->maximum_relative_volume_delta;
            summary.measurable_improvement =
                summary.constraint_count == 0u ||
                (summary.qef_rms_after <= summary.qef_rms_before &&
                 summary.qef_improvement_ratio >=
                    config->minimum_qef_improvement_ratio);
            if (summary.topology_preserved &&
                summary.measurable_improvement) {
                accepted = true;
                break;
            }
            summary.maximum_position_delta_units /= scale;
        }
        if (!accepted) {
            crease_report(
                report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                "crease_line_search",
                "no QEF step preserved shell validity and improvement");
            goto rollback;
        }
    }
    *out_summary = summary;
    free(old_vertices);
    free(target_positions);
    free(face_normals);
    free(face_centroids);
    free(incident_counts);
    free(incident_offsets);
    free(incident_cursor);
    free(incident_triangles);
    return true;

rollback:
    memcpy(document->vertices, old_vertices,
           vertex_count * sizeof(*old_vertices));
    document->contract = old_contract;
    *mesh_summary = old_mesh_summary;
fail:
    free(old_vertices);
    free(target_positions);
    free(face_normals);
    free(face_centroids);
    free(incident_counts);
    free(incident_offsets);
    free(incident_cursor);
    free(incident_triangles);
    return false;
}
