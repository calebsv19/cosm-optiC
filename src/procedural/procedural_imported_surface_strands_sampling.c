#include "procedural_imported_surface_strands_internal.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

typedef struct StrandCandidate {
    SurfaceStrandRoot root;
} StrandCandidate;

static int candidate_compare(const void *left_value, const void *right_value) {
    const StrandCandidate *left = left_value;
    const StrandCandidate *right = right_value;
    if (left->root.carrier_weight > right->root.carrier_weight) return -1;
    if (left->root.carrier_weight < right->root.carrier_weight) return 1;
    if (left->root.source_triangle_index <
        right->root.source_triangle_index) return -1;
    return left->root.source_triangle_index >
        right->root.source_triangle_index ? 1 : 0;
}

static bool make_basis(
    CoreObjectVec3 normal,
    CoreObjectVec3 *out_tangent,
    CoreObjectVec3 *out_bitangent) {
    const CoreObjectVec3 reference =
        fabs(normal.z) < 0.85
            ? (CoreObjectVec3){0.0, 0.0, 1.0}
            : (CoreObjectVec3){0.0, 1.0, 0.0};
    CoreObjectVec3 tangent;
    if (!strand_vec_normalize(
            strand_vec_cross(reference, normal), &tangent)) return false;
    *out_tangent = tangent;
    *out_bitangent = strand_vec_cross(normal, tangent);
    return true;
}

void surface_strand_selection_free(SurfaceStrandSelection *selection) {
    if (!selection) return;
    free(selection->roots);
    memset(selection, 0, sizeof(*selection));
}

bool surface_strand_select(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceStrandConfig *config,
    SurfaceStrandSelection *out) {
    StrandCandidate *candidates = NULL;
    SurfaceStrandSelection result = {0};
    size_t candidate_count = 0u;
    if (!source || !region || !config || !out ||
        !source->vertices || !source->triangles ||
        !region->vertex_weights) return false;
    candidates = calloc(source->triangle_count, sizeof(*candidates));
    result.roots = calloc(config->max_strands, sizeof(*result.roots));
    if (!candidates || !result.roots) goto fail;
    for (size_t i = 0u; i < source->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &source->triangles[i];
        const size_t ids[3] = {triangle->a, triangle->b, triangle->c};
        const double weight =
            (region->vertex_weights[ids[0]] +
             region->vertex_weights[ids[1]] +
             region->vertex_weights[ids[2]]) / 3.0;
        const CoreObjectVec3 a = source->vertices[ids[0]].position;
        const CoreObjectVec3 b = source->vertices[ids[1]].position;
        const CoreObjectVec3 c = source->vertices[ids[2]].position;
        CoreObjectVec3 normal;
        SurfaceStrandRoot root = {0};
        double normalized;
        double variation;
        if (weight < config->selection_threshold ||
            !strand_vec_normalize(
                strand_vec_cross(
                    strand_vec_sub(b, a), strand_vec_sub(c, a)),
                &normal)) continue;
        root.source_triangle_index = i;
        root.anchor = strand_vec_scale(
            strand_vec_add(strand_vec_add(a, b), c), 1.0 / 3.0);
        root.normal = normal;
        root.barycentrics = (CoreObjectVec3){
            1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
        root.carrier_weight = weight;
        normalized = (weight - config->selection_threshold) /
            (1.0 - config->selection_threshold);
        variation =
            (double)((i * 1664525u + 1013904223u) & 0xffffu) / 65535.0;
        root.length = config->strand_length_units *
            (1.0 - config->length_variation +
             config->length_variation *
                 (0.65 * normalized + 0.35 * variation));
        root.phase = variation * 6.2831853071795864769;
        if (!make_basis(
                normal, &root.tangent, &root.bitangent)) continue;
        candidates[candidate_count++].root = root;
    }
    if (candidate_count == 0u) goto fail;
    qsort(candidates, candidate_count, sizeof(*candidates),
          candidate_compare);
    result.minimum_clearance_units = DBL_MAX;
    for (size_t i = 0u;
         i < candidate_count && result.count < config->max_strands; ++i) {
        const SurfaceStrandRoot *candidate = &candidates[i].root;
        bool accepted = true;
        for (size_t j = 0u; j < result.count; ++j) {
            const double distance = strand_vec_length(strand_vec_sub(
                candidate->anchor, result.roots[j].anchor));
            const double required = fmax(
                config->clearance_factor *
                    2.0 * config->root_radius_units,
                config->strand_length_units * 0.35);
            if (distance < required) {
                accepted = false;
                break;
            }
        }
        if (accepted) result.roots[result.count++] = *candidate;
        else ++result.rejected_clearance_count;
    }
    result.candidate_count = candidate_count;
    if (result.count == 0u) goto fail;
    if (result.count < 2u) {
        result.minimum_clearance_units = 0.0;
    } else {
        result.minimum_clearance_units = DBL_MAX;
        for (size_t i = 0u; i < result.count; ++i) {
            for (size_t j = i + 1u; j < result.count; ++j) {
                const double clearance = strand_vec_length(strand_vec_sub(
                    result.roots[i].anchor, result.roots[j].anchor)) -
                    2.0 * config->root_radius_units;
                if (clearance < result.minimum_clearance_units)
                    result.minimum_clearance_units = clearance;
            }
        }
    }
    free(candidates);
    *out = result;
    return true;
fail:
    free(candidates);
    surface_strand_selection_free(&result);
    return false;
}

bool surface_strand_build_asset(
    const SurfaceStrandSelection *selection,
    const ProceduralImportedSurfaceStrandConfig *config,
    ProceduralImportedSurfaceStrandAsset *out_asset) {
    ProceduralImportedSurfaceStrandAsset result = {0};
    const size_t points_per_strand = config->curve_segment_count + 1u;
    size_t point_count;
    if (!selection || !config || !out_asset || selection->count == 0u ||
        selection->count > SIZE_MAX / points_per_strand) return false;
    point_count = selection->count * points_per_strand;
    result.points = calloc(point_count, sizeof(*result.points));
    result.radii = calloc(point_count, sizeof(*result.radii));
    result.source_triangle_indices = calloc(
        selection->count, sizeof(*result.source_triangle_indices));
    result.root_barycentrics = calloc(
        selection->count, sizeof(*result.root_barycentrics));
    result.root_normals = calloc(
        selection->count, sizeof(*result.root_normals));
    result.root_tangents = calloc(
        selection->count, sizeof(*result.root_tangents));
    if (!result.points || !result.radii ||
        !result.source_triangle_indices || !result.root_barycentrics ||
        !result.root_normals || !result.root_tangents) goto fail;
    result.strand_count = selection->count;
    result.points_per_strand = points_per_strand;
    for (size_t strand = 0u; strand < selection->count; ++strand) {
        const SurfaceStrandRoot *root = &selection->roots[strand];
        result.source_triangle_indices[strand] =
            root->source_triangle_index;
        result.root_barycentrics[strand] = root->barycentrics;
        result.root_normals[strand] = root->normal;
        result.root_tangents[strand] = root->tangent;
        for (size_t point = 0u; point < points_per_strand; ++point) {
            const double t =
                (double)point / (double)config->curve_segment_count;
            const double axial =
                -config->root_penetration_units +
                (root->length + config->root_penetration_units) * t;
            const double bend =
                config->bend_strength * root->length * t * t;
            const double curl =
                config->curl_strength * root->length *
                sin(3.14159265358979323846 * t) *
                sin(root->phase + 3.14159265358979323846 * t);
            CoreObjectVec3 position = root->anchor;
            position = strand_vec_add(
                position, strand_vec_scale(root->normal, axial));
            position = strand_vec_add(
                position, strand_vec_scale(root->tangent, bend));
            position = strand_vec_add(
                position, strand_vec_scale(root->bitangent, curl));
            result.points[strand * points_per_strand + point] = position;
            result.radii[strand * points_per_strand + point] =
                config->root_radius_units +
                (config->tip_radius_units -
                 config->root_radius_units) * t;
        }
    }
    *out_asset = result;
    return true;
fail:
    ProceduralImportedSurfaceStrandAsset_Free(&result);
    return false;
}
