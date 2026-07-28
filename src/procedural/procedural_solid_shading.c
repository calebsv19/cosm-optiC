#include "procedural/procedural_solid_shading.h"

#include "procedural_solid_geometry_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shading_report(
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

void ProceduralSolidShadingConfig_Init(
    ProceduralSolidShadingConfig *config) {
    if (!config) return;
    *config = (ProceduralSolidShadingConfig){
        .crease_angle_degrees = 38.0,
        .maximum_output_vertices = 1000000u,
        .minimum_hard_corner_improvement_ratio = 0.25};
}

static size_t root_of(size_t *parents, size_t value) {
    while (parents[value] != value) {
        parents[value] = parents[parents[value]];
        value = parents[value];
    }
    return value;
}

static void join(size_t *parents, size_t a, size_t b) {
    a = root_of(parents, a);
    b = root_of(parents, b);
    if (a == b) return;
    if (a < b) parents[b] = a;
    else parents[a] = b;
}

static bool shading_config_valid(
    const ProceduralSolidShadingConfig *config) {
    return config &&
        isfinite(config->crease_angle_degrees) &&
        config->crease_angle_degrees > 0.0 &&
        config->crease_angle_degrees < 180.0 &&
        config->maximum_output_vertices > 0u &&
        isfinite(config->minimum_hard_corner_improvement_ratio) &&
        config->minimum_hard_corner_improvement_ratio >= 0.0 &&
        config->minimum_hard_corner_improvement_ratio < 1.0;
}

static double angle_degrees(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 na;
    CoreObjectVec3 nb;
    if (!psg_vec_normalize(a, &na) || !psg_vec_normalize(b, &nb)) {
        return 180.0;
    }
    return acos(psg_clamp_unit(psg_vec_dot(na, nb))) *
        180.0 / M_PI;
}

bool ProceduralSolidShading_SplitCreases(
    const ProceduralSolidShadingConfig *config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *geometric_summary,
    ProceduralSolidShadingSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidShadingSummary summary;
    const size_t old_vertex_count = document ? document->vertex_count : 0u;
    const size_t triangle_count = document ? document->triangle_count : 0u;
    const size_t corner_count =
        triangle_count <= SIZE_MAX / 3u ? triangle_count * 3u : 0u;
    CoreObjectVec3 *face_vectors = NULL;
    CoreObjectVec3 *face_normals = NULL;
    size_t *incident_counts = NULL;
    size_t *incident_offsets = NULL;
    size_t *incident_cursor = NULL;
    size_t *incident_corners = NULL;
    CoreMeshAssetRuntimeVertex *new_vertices = NULL;
    CoreMeshAssetRuntimeTriangle *new_triangles = NULL;
    size_t new_vertex_count = 0u;
    const double cosine_threshold = config
        ? cos(config->crease_angle_degrees * M_PI / 180.0) : 0.0;
    if (!shading_config_valid(config) || !document ||
        !geometric_summary || !out_summary ||
        old_vertex_count == 0u || !document->vertices ||
        triangle_count == 0u || !document->triangles ||
        corner_count == 0u) {
        shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                       "shading_config",
                       "crease shading inputs are invalid");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    summary.source_vertex_count = old_vertex_count;
    face_vectors = calloc(triangle_count, sizeof(*face_vectors));
    face_normals = calloc(triangle_count, sizeof(*face_normals));
    incident_counts = calloc(old_vertex_count, sizeof(*incident_counts));
    incident_offsets = calloc(old_vertex_count + 1u, sizeof(*incident_offsets));
    incident_cursor = calloc(old_vertex_count, sizeof(*incident_cursor));
    incident_corners = malloc(corner_count * sizeof(*incident_corners));
    new_triangles = malloc(triangle_count * sizeof(*new_triangles));
    if (!face_vectors || !face_normals || !incident_counts ||
        !incident_offsets || !incident_cursor || !incident_corners ||
        !new_triangles) {
        shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                       "shading_alloc",
                       "crease shading allocation failed");
        goto fail;
    }
    memcpy(new_triangles, document->triangles,
           triangle_count * sizeof(*new_triangles));
    for (size_t triangle = 0u; triangle < triangle_count; ++triangle) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[triangle];
        const size_t ids[3] = {t->a, t->b, t->c};
        if (t->a >= old_vertex_count || t->b >= old_vertex_count ||
            t->c >= old_vertex_count) {
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                           "shading_triangle",
                           "crease shading found an invalid triangle index");
            goto fail;
        }
        face_vectors[triangle] = psg_vec_cross(
            psg_vec_sub(document->vertices[t->b].position,
                        document->vertices[t->a].position),
            psg_vec_sub(document->vertices[t->c].position,
                        document->vertices[t->a].position));
        if (!psg_vec_normalize(
                face_vectors[triangle], &face_normals[triangle])) {
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                           "shading_triangle",
                           "crease shading found a degenerate triangle");
            goto fail;
        }
        for (size_t corner = 0u; corner < 3u; ++corner) {
            ++incident_counts[ids[corner]];
        }
    }
    for (size_t vertex = 0u; vertex < old_vertex_count; ++vertex) {
        incident_offsets[vertex + 1u] =
            incident_offsets[vertex] + incident_counts[vertex];
    }
    memcpy(incident_cursor, incident_offsets,
           old_vertex_count * sizeof(*incident_cursor));
    for (size_t triangle = 0u; triangle < triangle_count; ++triangle) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[triangle];
        const size_t ids[3] = {t->a, t->b, t->c};
        for (size_t corner = 0u; corner < 3u; ++corner) {
            incident_corners[incident_cursor[ids[corner]]++] =
                triangle * 3u + corner;
        }
    }
    {
        const size_t maximum_possible =
            corner_count < config->maximum_output_vertices
                ? corner_count : config->maximum_output_vertices;
        new_vertices = calloc(maximum_possible, sizeof(*new_vertices));
        if (!new_vertices) {
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                           "shading_vertices",
                           "crease shading vertex allocation failed");
            goto fail;
        }
    }
    for (size_t vertex = 0u; vertex < old_vertex_count; ++vertex) {
        const size_t begin = incident_offsets[vertex];
        const size_t count = incident_offsets[vertex + 1u] - begin;
        size_t *parents = NULL;
        size_t *root_to_new = NULL;
        size_t component_count = 0u;
        if (count == 0u) {
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                           "shading_orphan",
                           "crease shading found an unreferenced vertex");
            goto fail;
        }
        parents = malloc(count * sizeof(*parents));
        root_to_new = malloc(count * sizeof(*root_to_new));
        if (!parents || !root_to_new) {
            free(parents);
            free(root_to_new);
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                           "shading_islands",
                           "crease normal-island allocation failed");
            goto fail;
        }
        for (size_t local = 0u; local < count; ++local) {
            parents[local] = local;
            root_to_new[local] = SIZE_MAX;
        }
        for (size_t a = 0u; a < count; ++a) {
            const size_t ta = incident_corners[begin + a] / 3u;
            for (size_t b = a + 1u; b < count; ++b) {
                const size_t tb = incident_corners[begin + b] / 3u;
                if (psg_vec_dot(face_normals[ta], face_normals[tb]) >=
                    cosine_threshold) {
                    join(parents, a, b);
                }
            }
        }
        for (size_t local = 0u; local < count; ++local) {
            const size_t root = root_of(parents, local);
            if (root_to_new[root] == SIZE_MAX) {
                CoreObjectVec3 normal_sum = {0.0, 0.0, 0.0};
                CoreObjectVec3 normal;
                if (new_vertex_count >= config->maximum_output_vertices) {
                    free(parents);
                    free(root_to_new);
                    shading_report(
                        report, PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
                        "shading_vertex_budget",
                        "crease split exceeded the output vertex budget");
                    goto fail;
                }
                for (size_t member = 0u; member < count; ++member) {
                    if (root_of(parents, member) == root) {
                        const size_t triangle =
                            incident_corners[begin + member] / 3u;
                        normal_sum = psg_vec_add(
                            normal_sum, face_vectors[triangle]);
                    }
                }
                if (!psg_vec_normalize(normal_sum, &normal)) {
                    free(parents);
                    free(root_to_new);
                    shading_report(
                        report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                        "shading_normal",
                        "crease normal island could not be normalized");
                    goto fail;
                }
                root_to_new[root] = new_vertex_count;
                new_vertices[new_vertex_count] = document->vertices[vertex];
                new_vertices[new_vertex_count].normal = normal;
                ++new_vertex_count;
                ++component_count;
            }
        }
        if (component_count > 1u) ++summary.hard_vertex_count;
        for (size_t local = 0u; local < count; ++local) {
            const size_t encoded = incident_corners[begin + local];
            const size_t triangle = encoded / 3u;
            const size_t corner = encoded % 3u;
            const size_t index = root_to_new[root_of(parents, local)];
            if (corner == 0u) new_triangles[triangle].a = index;
            else if (corner == 1u) new_triangles[triangle].b = index;
            else new_triangles[triangle].c = index;
            if (component_count > 1u) {
                const double before = angle_degrees(
                    document->vertices[vertex].normal,
                    face_normals[triangle]);
                const double after = angle_degrees(
                    new_vertices[index].normal, face_normals[triangle]);
                summary.hard_corner_rms_degrees_before += before * before;
                summary.hard_corner_rms_degrees_after += after * after;
                ++summary.hard_corner_count;
            }
        }
        free(parents);
        free(root_to_new);
    }
    if (new_vertex_count < old_vertex_count) {
        shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                       "shading_vertex_count",
                       "crease split unexpectedly removed vertices");
        goto fail;
    }
    if (summary.hard_corner_count > 0u) {
        summary.hard_corner_rms_degrees_before = sqrt(
            summary.hard_corner_rms_degrees_before /
            (double)summary.hard_corner_count);
        summary.hard_corner_rms_degrees_after = sqrt(
            summary.hard_corner_rms_degrees_after /
            (double)summary.hard_corner_count);
        if (summary.hard_corner_rms_degrees_before > 1.0e-12) {
            summary.hard_corner_improvement_ratio =
                1.0 - summary.hard_corner_rms_degrees_after /
                    summary.hard_corner_rms_degrees_before;
        }
    } else {
        summary.hard_corner_improvement_ratio = 1.0;
    }
    summary.measurable_improvement =
        summary.hard_corner_count == 0u ||
        (summary.hard_corner_rms_degrees_after <=
             summary.hard_corner_rms_degrees_before &&
         summary.hard_corner_improvement_ratio >=
             config->minimum_hard_corner_improvement_ratio);
    if (!summary.measurable_improvement) {
        shading_report(
            report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
            "shading_improvement",
            "crease split did not meet its normal-fit improvement floor");
        goto fail;
    }
    {
        CoreMeshAssetRuntimeDocument candidate = *document;
        CoreResult result;
        CoreMeshAssetRuntimeVertex *old_vertices = document->vertices;
        CoreMeshAssetRuntimeTriangle *old_triangles = document->triangles;
        const size_t old_document_vertex_count = document->vertex_count;
        const size_t old_normal_count = document->vertex_normal_count;
        const CoreMeshAssetRuntimeNormalProvenance old_provenance =
            document->normal_provenance;
        const size_t old_contract_vertex_count =
            document->contract.vertex_count;
        candidate.vertices = new_vertices;
        candidate.vertex_count = new_vertex_count;
        candidate.vertex_normal_count = new_vertex_count;
        candidate.triangles = new_triangles;
        candidate.normal_provenance =
            CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE;
        candidate.contract.vertex_count = new_vertex_count;
        result = core_mesh_asset_runtime_document_validate(&candidate);
        if (result.code != CORE_OK) {
            shading_report(report, PROCEDURAL_SOLID_MESH_STATUS_CORE_MESH,
                           "shading_document", result.message);
            goto fail;
        }
        document->vertices = new_vertices;
        document->vertex_count = new_vertex_count;
        document->vertex_normal_count = new_vertex_count;
        document->triangles = new_triangles;
        document->normal_provenance =
            CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE;
        document->contract.vertex_count = new_vertex_count;
        if (!ProceduralSolidMesh_RefreshIdentity(
                document, geometric_summary, report)) {
            document->vertices = old_vertices;
            document->vertex_count = old_document_vertex_count;
            document->vertex_normal_count = old_normal_count;
            document->triangles = old_triangles;
            document->normal_provenance = old_provenance;
            document->contract.vertex_count = old_contract_vertex_count;
            goto fail;
        }
        free(old_vertices);
        free(old_triangles);
        new_vertices = NULL;
        new_triangles = NULL;
    }
    summary.output_vertex_count = new_vertex_count;
    summary.split_vertex_count = new_vertex_count - old_vertex_count;
    summary.normal_island_count = new_vertex_count;
    summary.geometric_topology_preserved = true;
    *out_summary = summary;
    free(face_vectors);
    free(face_normals);
    free(incident_counts);
    free(incident_offsets);
    free(incident_cursor);
    free(incident_corners);
    return true;

fail:
    free(new_vertices);
    free(new_triangles);
    free(face_vectors);
    free(face_normals);
    free(incident_counts);
    free(incident_offsets);
    free(incident_cursor);
    free(incident_corners);
    return false;
}
