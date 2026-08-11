#include "procedural/procedural_imported_surface_inset_internal.h"

#include <stdlib.h>
#include <string.h>

static bool triangle_selected(
    const RefinedMesh *mesh,
    const InsetTriangle *triangle,
    double threshold) {
    const double weight =
        (mesh->weights[triangle->a] +
         mesh->weights[triangle->b] +
         mesh->weights[triangle->c]) / 3.0;
    return weight >= threshold;
}

static bool triangle_straddles(
    const RefinedMesh *mesh,
    const InsetTriangle *triangle,
    double threshold) {
    const double wa = mesh->weights[triangle->a];
    const double wb = mesh->weights[triangle->b];
    const double wc = mesh->weights[triangle->c];
    const double minimum = fmin(wa, fmin(wb, wc));
    const double maximum = fmax(wa, fmax(wb, wc));
    return minimum < threshold && maximum >= threshold;
}

static size_t count_transition_triangles(
    const RefinedMesh *mesh,
    double threshold) {
    size_t count = 0u;
    for (size_t i = 0u; i < mesh->triangle_count; ++i)
        if (triangle_straddles(mesh, &mesh->triangles[i], threshold))
            ++count;
    return count;
}

static bool measure_boundary_edge(
    const RefinedMesh *mesh,
    double threshold,
    double *out_maximum) {
    InsetEdge *edges = NULL;
    size_t edge_count = 0u;
    double maximum = 0.0;
    if (!mesh || !out_maximum ||
        !inset_collect_edges(
            mesh->triangles, mesh->triangle_count, &edges, &edge_count)) {
        return false;
    }
    for (size_t i = 0u; i < edge_count; ++i) {
        const InsetEdge *edge = &edges[i];
        double length;
        if (edge->incidence != 2u ||
            edge->second_triangle == SIZE_MAX) {
            free(edges);
            return false;
        }
        if (triangle_selected(
                mesh, &mesh->triangles[edge->first_triangle], threshold) ==
            triangle_selected(
                mesh, &mesh->triangles[edge->second_triangle], threshold)) {
            continue;
        }
        length = vec_length(vec_sub(
            mesh->vertices[edge->lo].position,
            mesh->vertices[edge->hi].position));
        if (length > maximum) maximum = length;
    }
    free(edges);
    if (!(maximum > 0.0)) return false;
    *out_maximum = maximum;
    return true;
}

static bool initialize_source_mesh(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    RefinedMesh *out) {
    RefinedMesh result = {0};
    if (!source || !region || !out) return false;
    result.vertex_count = source->vertex_count;
    result.triangle_count = source->triangle_count;
    result.vertices = calloc(result.vertex_count, sizeof(*result.vertices));
    result.weights = calloc(result.vertex_count, sizeof(*result.weights));
    result.triangles = calloc(
        result.triangle_count, sizeof(*result.triangles));
    if (!result.vertices || !result.weights || !result.triangles) goto fail;
    memcpy(result.vertices, source->vertices,
           result.vertex_count * sizeof(*result.vertices));
    memcpy(result.weights, region->vertex_weights,
           result.vertex_count * sizeof(*result.weights));
    for (size_t i = 0u; i < result.triangle_count; ++i) {
        result.triangles[i] = (InsetTriangle){
            source->triangles[i].a,
            source->triangles[i].b,
            source->triangles[i].c,
            i};
    }
    /*
     * Runtime JSON may intentionally omit vertex normals. Reconstruct one
     * deterministic smooth normal field before any midpoint interpolation.
     */
    for (size_t i = 0u; i < result.vertex_count; ++i)
        result.vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    for (size_t i = 0u; i < result.triangle_count; ++i) {
        const InsetTriangle *triangle = &result.triangles[i];
        const CoreObjectVec3 a = result.vertices[triangle->a].position;
        const CoreObjectVec3 b = result.vertices[triangle->b].position;
        const CoreObjectVec3 c = result.vertices[triangle->c].position;
        const CoreObjectVec3 normal =
            vec_cross(vec_sub(b, a), vec_sub(c, a));
        result.vertices[triangle->a].normal =
            vec_add(result.vertices[triangle->a].normal, normal);
        result.vertices[triangle->b].normal =
            vec_add(result.vertices[triangle->b].normal, normal);
        result.vertices[triangle->c].normal =
            vec_add(result.vertices[triangle->c].normal, normal);
    }
    for (size_t i = 0u; i < result.vertex_count; ++i)
        if (!vec_normalize(
                result.vertices[i].normal,
                &result.vertices[i].normal)) goto fail;
    *out = result;
    return true;
fail:
    refined_free(&result);
    return false;
}

static bool append_triangle(
    RefinedMesh *mesh,
    size_t capacity,
    size_t a,
    size_t b,
    size_t c,
    size_t source_triangle) {
    if (!mesh || mesh->triangle_count >= capacity ||
        a == b || b == c || c == a) {
        return false;
    }
    mesh->triangles[mesh->triangle_count++] =
        (InsetTriangle){a, b, c, source_triangle};
    return true;
}

static bool refine_once(
    const RefinedMesh *input,
    double threshold,
    const ProceduralImportedSurfaceInsetConfig *config,
    RefinedMesh *out) {
    RefinedMesh result = {0};
    InsetEdge *edges = NULL;
    size_t edge_count = 0u;
    size_t split_count = 0u;
    size_t triangle_capacity;
    if (!input || !config || !out ||
        !inset_collect_edges(
            input->triangles, input->triangle_count, &edges, &edge_count)) {
        return false;
    }
    for (size_t i = 0u; i < edge_count; ++i)
        if (edges[i].incidence != 2u) goto fail;
    for (size_t i = 0u; i < input->triangle_count; ++i) {
        const InsetTriangle *triangle = &input->triangles[i];
        InsetEdge *ab;
        InsetEdge *bc;
        InsetEdge *ca;
        if (!triangle_straddles(input, triangle, threshold)) continue;
        ab = inset_find_edge(edges, edge_count, triangle->a, triangle->b);
        bc = inset_find_edge(edges, edge_count, triangle->b, triangle->c);
        ca = inset_find_edge(edges, edge_count, triangle->c, triangle->a);
        if (!ab || !bc || !ca) goto fail;
        ab->split = true;
        bc->split = true;
        ca->split = true;
    }
    for (size_t i = 0u; i < edge_count; ++i)
        if (edges[i].split) ++split_count;
    if (split_count == 0u ||
        split_count > config->max_vertices ||
        input->vertex_count > config->max_vertices - split_count ||
        input->triangle_count > SIZE_MAX / 4u) goto fail;
    result.vertex_count = input->vertex_count + split_count;
    triangle_capacity = input->triangle_count * 4u;
    if (result.vertex_count > config->max_vertices ||
        triangle_capacity > config->max_triangles) goto fail;
    result.vertices = calloc(result.vertex_count, sizeof(*result.vertices));
    result.weights = calloc(result.vertex_count, sizeof(*result.weights));
    result.triangles = calloc(triangle_capacity, sizeof(*result.triangles));
    if (!result.vertices || !result.weights || !result.triangles) goto fail;
    memcpy(result.vertices, input->vertices,
           input->vertex_count * sizeof(*result.vertices));
    memcpy(result.weights, input->weights,
           input->vertex_count * sizeof(*result.weights));
    {
        size_t next_vertex = input->vertex_count;
        for (size_t i = 0u; i < edge_count; ++i) {
            if (!edges[i].split) continue;
            edges[i].midpoint = next_vertex++;
            result.vertices[edges[i].midpoint].position = vec_scale(
                vec_add(input->vertices[edges[i].lo].position,
                        input->vertices[edges[i].hi].position), 0.5);
            if (!vec_normalize(
                    vec_add(input->vertices[edges[i].lo].normal,
                            input->vertices[edges[i].hi].normal),
                    &result.vertices[edges[i].midpoint].normal)) goto fail;
            result.weights[edges[i].midpoint] =
                (input->weights[edges[i].lo] +
                 input->weights[edges[i].hi]) * 0.5;
        }
    }
    for (size_t i = 0u; i < input->triangle_count; ++i) {
        const InsetTriangle *triangle = &input->triangles[i];
        InsetEdge *ab = inset_find_edge(
            edges, edge_count, triangle->a, triangle->b);
        InsetEdge *bc = inset_find_edge(
            edges, edge_count, triangle->b, triangle->c);
        InsetEdge *ca = inset_find_edge(
            edges, edge_count, triangle->c, triangle->a);
        const unsigned int mask =
            (ab && ab->split ? 1u : 0u) |
            (bc && bc->split ? 2u : 0u) |
            (ca && ca->split ? 4u : 0u);
        const size_t a = triangle->a;
        const size_t b = triangle->b;
        const size_t c = triangle->c;
        const size_t mab = ab ? ab->midpoint : SIZE_MAX;
        const size_t mbc = bc ? bc->midpoint : SIZE_MAX;
        const size_t mca = ca ? ca->midpoint : SIZE_MAX;
#define ADD(a_, b_, c_) \
    do { \
        if (!append_triangle( \
                &result, triangle_capacity, (a_), (b_), (c_), \
                triangle->source_triangle)) goto fail; \
    } while (0)
        switch (mask) {
            case 0u: ADD(a, b, c); break;
            case 1u: ADD(a, mab, c); ADD(mab, b, c); break;
            case 2u: ADD(b, mbc, a); ADD(mbc, c, a); break;
            case 4u: ADD(c, mca, b); ADD(mca, a, b); break;
            case 3u:
                ADD(b, mbc, mab); ADD(a, mab, mbc); ADD(a, mbc, c);
                break;
            case 5u:
                ADD(a, mab, mca); ADD(b, c, mca); ADD(b, mca, mab);
                break;
            case 6u:
                ADD(c, mca, mbc); ADD(a, b, mbc); ADD(a, mbc, mca);
                break;
            case 7u:
                ADD(a, mab, mca); ADD(mab, b, mbc);
                ADD(mca, mbc, c); ADD(mab, mbc, mca);
                break;
            default: goto fail;
        }
#undef ADD
    }
    free(edges);
    *out = result;
    return true;
fail:
    free(edges);
    refined_free(&result);
    return false;
}

bool refine_transition_band(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceInsetConfig *config,
    RefinedMesh *out,
    InsetRefinementSummary *out_summary) {
    RefinedMesh current = {0};
    InsetRefinementSummary summary = {0};
    size_t maximum_passes;
    if (!source || !region || !config || !out || !out_summary ||
        !initialize_source_mesh(source, region, &current) ||
        !measure_boundary_edge(
            &current, config->selection_threshold,
            &summary.initial_max_boundary_edge_length_units)) {
        refined_free(&current);
        return false;
    }
    summary.transition_source_triangle_count =
        count_transition_triangles(&current, config->selection_threshold);
    summary.target_boundary_edge_length_units =
        config->target_boundary_edge_length_units > 0.0
            ? config->target_boundary_edge_length_units
            : summary.initial_max_boundary_edge_length_units * 0.30;
    summary.final_max_boundary_edge_length_units =
        summary.initial_max_boundary_edge_length_units;
    maximum_passes = config->adaptive_refinement_enabled
        ? config->max_adaptive_refinement_passes : 1u;
    if (!config->refine_transition_band) maximum_passes = 0u;
    for (size_t pass = 0u;
         pass < maximum_passes &&
         summary.final_max_boundary_edge_length_units >
             summary.target_boundary_edge_length_units;
         ++pass) {
        RefinedMesh next = {0};
        if (!refine_once(
                &current, config->selection_threshold, config, &next)) {
            refined_free(&current);
            return false;
        }
        refined_free(&current);
        current = next;
        ++summary.pass_count;
        if (!measure_boundary_edge(
                &current, config->selection_threshold,
                &summary.final_max_boundary_edge_length_units)) {
            refined_free(&current);
            return false;
        }
    }
    summary.converged =
        summary.final_max_boundary_edge_length_units <=
        summary.target_boundary_edge_length_units;
    *out = current;
    *out_summary = summary;
    return true;
}
