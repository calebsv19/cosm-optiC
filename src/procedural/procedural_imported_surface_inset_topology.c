#include "procedural/procedural_imported_surface_inset_internal.h"

#include <stdlib.h>
#include <string.h>

static int edge_compare(const void *left_value, const void *right_value) {
    const InsetEdge *left = left_value;
    const InsetEdge *right = right_value;
    if (left->lo < right->lo) return -1;
    if (left->lo > right->lo) return 1;
    if (left->hi < right->hi) return -1;
    if (left->hi > right->hi) return 1;
    return 0;
}

InsetEdge *inset_find_edge(
    InsetEdge *edges,
    size_t edge_count,
    size_t a,
    size_t b) {
    InsetEdge key = {
        a < b ? a : b, a < b ? b : a, 0u, 0u, 0u, 0u, false};
    return bsearch(&key, edges, edge_count, sizeof(*edges), edge_compare);
}

bool inset_collect_edges(
    const InsetTriangle *triangles,
    size_t triangle_count,
    InsetEdge **out_edges,
    size_t *out_edge_count) {
    InsetEdge *raw;
    size_t unique_count = 0u;
    if (!triangles || !out_edges || !out_edge_count ||
        triangle_count > SIZE_MAX / 3u) {
        return false;
    }
    raw = calloc(triangle_count * 3u, sizeof(*raw));
    if (!raw) return false;
    for (size_t i = 0u; i < triangle_count; ++i) {
        const size_t ids[3] = {
            triangles[i].a, triangles[i].b, triangles[i].c};
        for (size_t side = 0u; side < 3u; ++side) {
            const size_t a = ids[side];
            const size_t b = ids[(side + 1u) % 3u];
            InsetEdge *edge = &raw[i * 3u + side];
            edge->lo = a < b ? a : b;
            edge->hi = a < b ? b : a;
            edge->first_triangle = i;
            edge->second_triangle = SIZE_MAX;
            edge->incidence = 1u;
            edge->midpoint = SIZE_MAX;
        }
    }
    qsort(raw, triangle_count * 3u, sizeof(*raw), edge_compare);
    for (size_t i = 0u; i < triangle_count * 3u; ++i) {
        if (unique_count > 0u &&
            raw[unique_count - 1u].lo == raw[i].lo &&
            raw[unique_count - 1u].hi == raw[i].hi) {
            InsetEdge *edge = &raw[unique_count - 1u];
            if (edge->incidence == 1u)
                edge->second_triangle = raw[i].first_triangle;
            ++edge->incidence;
        } else {
            raw[unique_count++] = raw[i];
        }
    }
    *out_edges = raw;
    *out_edge_count = unique_count;
    return true;
}

void refined_free(RefinedMesh *mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->weights);
    free(mesh->triangles);
    memset(mesh, 0, sizeof(*mesh));
}

static size_t union_root(size_t *parents, size_t value) {
    while (parents[value] != value) {
        parents[value] = parents[parents[value]];
        value = parents[value];
    }
    return value;
}

static void union_join(size_t *parents, size_t a, size_t b) {
    a = union_root(parents, a);
    b = union_root(parents, b);
    if (a == b) return;
    if (a < b) parents[b] = a;
    else parents[a] = b;
}

bool classify_patch(
    const RefinedMesh *mesh,
    double threshold,
    bool **out_selected,
    InsetEdge **out_edges,
    size_t *out_edge_count,
    size_t *out_selected_count,
    size_t *out_discarded_candidate_count,
    size_t *out_selected_component_count,
    size_t *out_boundary_edge_count,
    size_t *out_boundary_loop_count,
    double *out_max_boundary_edge_length_units,
    size_t minimum_component_triangles) {
    bool *selected = NULL;
    InsetEdge *edges = NULL;
    size_t edge_count = 0u;
    size_t selected_count = 0u;
    size_t boundary_count = 0u;
    size_t boundary_vertices = 0u;
    size_t component_count = 0u;
    size_t loop_count = 0u;
    double maximum_boundary_edge = 0.0;
    size_t *parents = NULL;
    size_t *boundary_parents = NULL;
    size_t *component_sizes = NULL;
    unsigned int *degrees = NULL;
    bool *boundary_used = NULL;
    bool *boundary_roots_seen = NULL;
    if (!mesh || !out_selected || !out_edges || !out_edge_count ||
        !out_selected_count || !out_discarded_candidate_count ||
        !out_selected_component_count || !out_boundary_edge_count ||
        !out_boundary_loop_count || !out_max_boundary_edge_length_units ||
        minimum_component_triangles == 0u) {
        return false;
    }
    selected = calloc(mesh->triangle_count, sizeof(*selected));
    if (!selected) return false;
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const InsetTriangle *triangle = &mesh->triangles[i];
        const double weight =
            (mesh->weights[triangle->a] +
             mesh->weights[triangle->b] +
             mesh->weights[triangle->c]) / 3.0;
        selected[i] = weight >= threshold;
        if (selected[i]) ++selected_count;
    }
    if (selected_count == 0u || selected_count == mesh->triangle_count ||
        !inset_collect_edges(mesh->triangles, mesh->triangle_count,
                       &edges, &edge_count)) {
        free(selected);
        return false;
    }
    parents = malloc(mesh->triangle_count * sizeof(*parents));
    component_sizes = calloc(mesh->triangle_count, sizeof(*component_sizes));
    boundary_parents = malloc(mesh->vertex_count * sizeof(*boundary_parents));
    degrees = calloc(mesh->vertex_count, sizeof(*degrees));
    boundary_used = calloc(mesh->vertex_count, sizeof(*boundary_used));
    boundary_roots_seen = calloc(
        mesh->vertex_count, sizeof(*boundary_roots_seen));
    if (!parents || !component_sizes || !boundary_parents || !degrees ||
        !boundary_used || !boundary_roots_seen) goto fail;
    for (size_t i = 0u; i < mesh->triangle_count; ++i) parents[i] = i;
    for (size_t i = 0u; i < edge_count; ++i) {
        InsetEdge *edge = &edges[i];
        if (edge->incidence != 2u ||
            edge->second_triangle == SIZE_MAX) {
            goto fail;
        }
        if (selected[edge->first_triangle] ==
            selected[edge->second_triangle]) {
            if (selected[edge->first_triangle])
                union_join(parents, edge->first_triangle,
                           edge->second_triangle);
        }
    }
    {
        const size_t original_selected_count = selected_count;
        for (size_t i = 0u; i < mesh->triangle_count; ++i)
            if (selected[i])
                ++component_sizes[union_root(parents, i)];
        selected_count = 0u;
        for (size_t i = 0u; i < mesh->triangle_count; ++i) {
            if (!selected[i]) continue;
            if (component_sizes[union_root(parents, i)] <
                minimum_component_triangles) {
                selected[i] = false;
            } else {
                ++selected_count;
            }
        }
        for (size_t i = 0u; i < mesh->triangle_count; ++i)
            if (component_sizes[i] >= minimum_component_triangles)
                ++component_count;
        *out_discarded_candidate_count =
            original_selected_count - selected_count;
    }
    if (selected_count == 0u || selected_count == mesh->triangle_count ||
        component_count == 0u) goto fail;
    for (size_t i = 0u; i < mesh->vertex_count; ++i)
        boundary_parents[i] = i;
    for (size_t i = 0u; i < edge_count; ++i) {
        const InsetEdge *edge = &edges[i];
        if (selected[edge->first_triangle] ==
            selected[edge->second_triangle]) continue;
        {
            const double length = vec_length(vec_sub(
                mesh->vertices[edge->lo].position,
                mesh->vertices[edge->hi].position));
            if (length > maximum_boundary_edge)
                maximum_boundary_edge = length;
        }
        ++boundary_count;
        ++degrees[edge->lo];
        ++degrees[edge->hi];
        boundary_used[edge->lo] = true;
        boundary_used[edge->hi] = true;
        union_join(boundary_parents, edge->lo, edge->hi);
    }
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        size_t root;
        if (!boundary_used[i]) continue;
        ++boundary_vertices;
        if (degrees[i] != 2u) goto fail;
        root = union_root(boundary_parents, i);
        if (!boundary_roots_seen[root]) {
            boundary_roots_seen[root] = true;
            ++loop_count;
        }
    }
    if (boundary_vertices != boundary_count || boundary_count < 3u ||
        loop_count == 0u || !(maximum_boundary_edge > 0.0))
        goto fail;
    free(parents);
    free(component_sizes);
    free(boundary_parents);
    free(degrees);
    free(boundary_used);
    free(boundary_roots_seen);
    *out_selected = selected;
    *out_edges = edges;
    *out_edge_count = edge_count;
    *out_selected_count = selected_count;
    *out_selected_component_count = component_count;
    *out_boundary_edge_count = boundary_count;
    *out_boundary_loop_count = loop_count;
    *out_max_boundary_edge_length_units = maximum_boundary_edge;
    return true;
fail:
    free(parents);
    free(component_sizes);
    free(boundary_parents);
    free(degrees);
    free(boundary_used);
    free(boundary_roots_seen);
    free(edges);
    free(selected);
    return false;
}

bool selected_directed_edge(
    const InsetTriangle *triangle,
    size_t lo,
    size_t hi,
    size_t *out_a,
    size_t *out_b) {
    const size_t ids[3] = {triangle->a, triangle->b, triangle->c};
    for (size_t side = 0u; side < 3u; ++side) {
        const size_t a = ids[side];
        const size_t b = ids[(side + 1u) % 3u];
        if ((a == lo && b == hi) || (a == hi && b == lo)) {
            *out_a = a;
            *out_b = b;
            return true;
        }
    }
    return false;
}
