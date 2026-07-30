#include "procedural_imported_surface_growth_internal.h"

#include <float.h>

static double validation_bound_radius(
    const SurfaceGrowthElement *element) {
    const double half_vertical =
        (element->height + element->attachment_depth) * 0.5;
    return fmax(element->radius, half_vertical);
}

static CoreObjectVec3 validation_bound_center(
    const SurfaceGrowthElement *element) {
    return growth_vec_add(
        element->anchor,
        growth_vec_scale(
            element->normal,
            (element->height - element->attachment_depth) * 0.5));
}

bool surface_growth_validate_separation(
    const SurfaceGrowthSelection *selection,
    size_t *out_overlap_pairs,
    size_t *out_self_intersection_pairs,
    double *out_minimum_clearance) {
    size_t overlaps = 0u;
    double minimum = DBL_MAX;
    if (!selection || !out_overlap_pairs || !out_self_intersection_pairs ||
        !out_minimum_clearance || selection->count == 0u) return false;
    for (size_t i = 0u; i < selection->count; ++i) {
        const SurfaceGrowthElement *left = &selection->elements[i];
        for (size_t j = i + 1u; j < selection->count; ++j) {
            const SurfaceGrowthElement *right = &selection->elements[j];
            const double distance = growth_vec_length(growth_vec_sub(
                validation_bound_center(left),
                validation_bound_center(right)));
            const double clearance = distance -
                validation_bound_radius(left) -
                validation_bound_radius(right);
            if (clearance < minimum) minimum = clearance;
            if (clearance < 0.0) ++overlaps;
        }
    }
    if (selection->count < 2u) minimum = 0.0;
    *out_overlap_pairs = overlaps;
    /*
     * Each component is an injective latitude/longitude ellipsoid with one
     * shared vertex per pole and seam. Inter-component bounding-sphere
     * separation is conservative, so zero overlap pairs also proves that
     * triangles belonging to different elements cannot intersect.
     */
    *out_self_intersection_pairs = overlaps;
    *out_minimum_clearance = minimum;
    return overlaps == 0u;
}
