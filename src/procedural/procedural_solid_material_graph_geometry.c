#include "procedural/procedural_solid_material_graph.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct V3 {
    double x, y, z;
} V3;

static V3 sub(V3 a, V3 b) {
    V3 v = {a.x - b.x, a.y - b.y, a.z - b.z};
    return v;
}

static V3 cross(V3 a, V3 b) {
    V3 v = {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
    return v;
}

static double dot(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static double length(V3 a) {
    return sqrt(dot(a, a));
}

static V3 normalize(V3 a) {
    double n = length(a);
    V3 zero = {0.0, 0.0, 0.0};
    if (n <= 1e-12) return zero;
    a.x /= n; a.y /= n; a.z /= n;
    return a;
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static V3 position(
    const CoreMeshAssetRuntimeDocument *mesh, size_t vertex) {
    V3 p = {mesh->vertices[vertex].position.x,
            mesh->vertices[vertex].position.y,
            mesh->vertices[vertex].position.z};
    return p;
}

static V3 normal(
    const CoreMeshAssetRuntimeDocument *mesh, size_t vertex) {
    V3 n = {mesh->vertices[vertex].normal.x,
            mesh->vertices[vertex].normal.y,
            mesh->vertices[vertex].normal.z};
    return normalize(n);
}

bool ProceduralSolidMaterialGeometryInputs_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialGeometryInputs *out_inputs,
    size_t input_count,
    ProceduralSolidMaterialGraphReport *report) {
    double min_z = DBL_MAX, max_z = -DBL_MAX, diagonal;
    double *vertex_distance = NULL;
    bool *vertex_boundary = NULL;
    const char **vertex_group = NULL;
    if (!mesh || !out_inputs || input_count != mesh->triangle_count ||
        !mesh->vertices || !mesh->triangles) return false;
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        double z = mesh->vertices[i].position.z;
        if (z < min_z) min_z = z;
        if (z > max_z) max_z = z;
    }
    diagonal = sqrt(
        pow(mesh->contract.local_bounds.max.x -
                mesh->contract.local_bounds.min.x, 2.0) +
        pow(mesh->contract.local_bounds.max.y -
                mesh->contract.local_bounds.min.y, 2.0) +
        pow(mesh->contract.local_bounds.max.z -
                mesh->contract.local_bounds.min.z, 2.0));
    if (diagonal <= 1e-9) diagonal = 1.0;
    vertex_distance = malloc(sizeof(*vertex_distance) * mesh->vertex_count);
    vertex_boundary = calloc(mesh->vertex_count, sizeof(*vertex_boundary));
    vertex_group = calloc(mesh->vertex_count, sizeof(*vertex_group));
    if (!vertex_distance || !vertex_boundary || !vertex_group) goto fail;
    for (size_t v = 0u; v < mesh->vertex_count; ++v)
        vertex_distance[v] = DBL_MAX;

    /*
     * A vertex is a semantic boundary when incident triangles disagree on
     * surface group. This keeps the mask attached to the retained/cut/blend
     * shell semantics without changing mesh identity.
     */
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &mesh->triangles[i];
        size_t vertices[3] = {triangle->a, triangle->b, triangle->c};
        for (size_t j = 0u; j < 3u; ++j) {
            size_t vertex = vertices[j];
            if (!vertex_group[vertex])
                vertex_group[vertex] = triangle->surface_group_id;
            else if (strcmp(vertex_group[vertex],
                            triangle->surface_group_id) != 0)
                vertex_boundary[vertex] = true;
        }
    }
    for (size_t v = 0u; v < mesh->vertex_count; ++v)
        if (vertex_boundary[v]) vertex_distance[v] = 0.0;

    /* Deterministic bounded graph-distance relaxation over triangle edges. */
    for (size_t pass = 0u; pass < 32u; ++pass) {
        bool changed = false;
        for (size_t i = 0u; i < mesh->triangle_count; ++i) {
            const CoreMeshAssetRuntimeTriangle *t = &mesh->triangles[i];
            size_t v[3] = {t->a, t->b, t->c};
            for (size_t edge = 0u; edge < 3u; ++edge) {
                size_t a = v[edge], b = v[(edge + 1u) % 3u];
                double d = length(sub(position(mesh, a), position(mesh, b)));
                if (vertex_distance[a] + d < vertex_distance[b]) {
                    vertex_distance[b] = vertex_distance[a] + d;
                    changed = true;
                }
                if (vertex_distance[b] + d < vertex_distance[a]) {
                    vertex_distance[a] = vertex_distance[b] + d;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *t = &mesh->triangles[i];
        V3 p0 = position(mesh, t->a), p1 = position(mesh, t->b);
        V3 p2 = position(mesh, t->c);
        V3 face = normalize(cross(sub(p1, p0), sub(p2, p0)));
        V3 smooth = normalize((V3){
            normal(mesh, t->a).x + normal(mesh, t->b).x +
                normal(mesh, t->c).x,
            normal(mesh, t->a).y + normal(mesh, t->b).y +
                normal(mesh, t->c).y,
            normal(mesh, t->a).z + normal(mesh, t->b).z +
                normal(mesh, t->c).z});
        double centroid_z = (p0.z + p1.z + p2.z) / 3.0;
        double curvature = clamp01((1.0 - dot(face, smooth)) * 5.0);
        double boundary = fmin(
            vertex_distance[t->a],
            fmin(vertex_distance[t->b], vertex_distance[t->c]));
        const char *kind = region_kinds ? region_kinds[i] : "retained";
        ProceduralSolidMaterialGeometryInputs *out = &out_inputs[i];
        memset(out, 0, sizeof(*out));
        out->height = max_z > min_z
                          ? clamp01((centroid_z - min_z) / (max_z - min_z))
                          : 0.5;
        /* Signed upward exposure prevents snow on bottoms and vertical walls. */
        out->slope = clamp01(face.z);
        out->curvature = curvature;
        out->cavity = clamp01(
            curvature * (0.35 + 0.65 * (1.0 - out->slope)));
        out->boundary_distance =
            boundary == DBL_MAX ? 1.0 : clamp01(boundary / diagonal);
        out->region_cut = kind && strcmp(kind, "cut") == 0;
        out->region_blend = kind && strcmp(kind, "blend") == 0;
        out->region_retained = !out->region_cut && !out->region_blend;
        out->object_x = (p0.x + p1.x + p2.x) / 3.0;
        out->object_y = (p0.y + p1.y + p2.y) / 3.0;
        out->object_z = centroid_z;
    }
    free(vertex_distance);
    free(vertex_boundary);
    free(vertex_group);
    if (report) {
        report->status = PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_OK;
        snprintf(report->message, sizeof(report->message), "ok");
    }
    return true;
fail:
    free(vertex_distance);
    free(vertex_boundary);
    free(vertex_group);
    if (report) {
        report->status = PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_IO;
        snprintf(report->field, sizeof(report->field), "geometry_inputs");
        snprintf(report->message, sizeof(report->message),
                 "unable to allocate geometry feature buffers");
    }
    return false;
}
