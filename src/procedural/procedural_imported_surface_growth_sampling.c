#include "procedural_imported_surface_growth_internal.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

typedef struct GrowthCandidate {
    SurfaceGrowthElement element;
} GrowthCandidate;

static int candidate_compare(const void *left_value, const void *right_value) {
    const GrowthCandidate *left = left_value;
    const GrowthCandidate *right = right_value;
    if (left->element.carrier_weight > right->element.carrier_weight)
        return -1;
    if (left->element.carrier_weight < right->element.carrier_weight)
        return 1;
    if (left->element.source_triangle_index <
        right->element.source_triangle_index) return -1;
    return left->element.source_triangle_index >
        right->element.source_triangle_index ? 1 : 0;
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
    if (!growth_vec_normalize(
            growth_vec_cross(reference, normal), &tangent)) return false;
    *out_tangent = tangent;
    *out_bitangent = growth_vec_cross(normal, tangent);
    return true;
}

static double element_bound_radius(const SurfaceGrowthElement *element) {
    const double half_vertical =
        (element->height + element->attachment_depth) * 0.5;
    return fmax(element->radius * fmax(1.0, element->aspect), half_vertical);
}

static CoreObjectVec3 element_bound_center(
    const SurfaceGrowthElement *element) {
    return growth_vec_add(
        element->anchor,
        growth_vec_scale(
            element->normal,
            (element->height - element->attachment_depth) * 0.5));
}

void surface_growth_selection_free(SurfaceGrowthSelection *selection) {
    if (!selection) return;
    free(selection->elements);
    memset(selection, 0, sizeof(*selection));
}

bool surface_growth_select(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceGrowthConfig *config,
    SurfaceGrowthSelection *out) {
    GrowthCandidate *candidates = NULL;
    SurfaceGrowthSelection result = {0};
    size_t candidate_count = 0u;
    if (!source || !region || !config || !out ||
        !source->vertices || !source->triangles ||
        !region->vertex_weights) return false;
    candidates = calloc(source->triangle_count, sizeof(*candidates));
    result.elements = calloc(
        config->max_growth_elements, sizeof(*result.elements));
    if (!candidates || !result.elements) goto fail;
    for (size_t i = 0u; i < source->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &source->triangles[i];
        const size_t ids[3] = {triangle->a, triangle->b, triangle->c};
        const double weight =
            (region->vertex_weights[ids[0]] +
             region->vertex_weights[ids[1]] +
             region->vertex_weights[ids[2]]) / 3.0;
        CoreObjectVec3 a;
        CoreObjectVec3 b;
        CoreObjectVec3 c;
        CoreObjectVec3 normal;
        SurfaceGrowthElement element;
        double normalized;
        double variation_key;
        if (weight < config->selection_threshold) continue;
        a = source->vertices[ids[0]].position;
        b = source->vertices[ids[1]].position;
        c = source->vertices[ids[2]].position;
        if (!growth_vec_normalize(
                growth_vec_cross(
                    growth_vec_sub(b, a), growth_vec_sub(c, a)),
                &normal)) continue;
        memset(&element, 0, sizeof(element));
        element.source_triangle_index = i;
        element.anchor = growth_vec_scale(
            growth_vec_add(growth_vec_add(a, b), c), 1.0 / 3.0);
        element.normal = normal;
        element.carrier_weight = weight;
        normalized = (weight - config->selection_threshold) /
            (1.0 - config->selection_threshold);
        variation_key =
            (double)((i * 1103515245u + 12345u) & 0xffffu) / 65535.0;
        element.radius = config->mound_radius_units *
            (1.0 - config->radius_variation +
             config->radius_variation * (0.5 * normalized +
                                          0.5 * variation_key));
        element.aspect = 1.0;
        element.height = config->mound_height_units *
            (1.0 - config->height_variation +
             config->height_variation * (0.65 * normalized +
                                          0.35 * (1.0 - variation_key)));
        element.attachment_depth = config->attachment_depth_units;
        if (!make_basis(
                normal, &element.tangent, &element.bitangent)) continue;
        candidates[candidate_count++].element = element;
    }
    if (candidate_count == 0u) goto fail;
    qsort(candidates, candidate_count, sizeof(*candidates),
          candidate_compare);
    result.minimum_clearance_units = DBL_MAX;
    for (size_t i = 0u;
         i < candidate_count &&
         result.count < config->max_growth_elements; ++i) {
        const SurfaceGrowthElement *candidate = &candidates[i].element;
        const CoreObjectVec3 candidate_center =
            element_bound_center(candidate);
        const double candidate_radius = element_bound_radius(candidate);
        bool accepted = true;
        for (size_t j = 0u; j < result.count; ++j) {
            const SurfaceGrowthElement *existing = &result.elements[j];
            const double distance = growth_vec_length(growth_vec_sub(
                candidate_center, element_bound_center(existing)));
            const double required = config->clearance_factor *
                (candidate_radius + element_bound_radius(existing));
            const double clearance = distance - candidate_radius -
                element_bound_radius(existing);
            if (clearance < result.minimum_clearance_units)
                result.minimum_clearance_units = clearance;
            if (!(distance >= required)) {
                accepted = false;
                break;
            }
        }
        if (accepted) result.elements[result.count++] = *candidate;
        else ++result.rejected_clearance_count;
    }
    result.candidate_count = candidate_count;
    if (result.count == 0u) goto fail;
    if (result.count < 2u) result.minimum_clearance_units = 0.0;
    free(candidates);
    *out = result;
    return true;
fail:
    free(candidates);
    surface_growth_selection_free(&result);
    return false;
}
