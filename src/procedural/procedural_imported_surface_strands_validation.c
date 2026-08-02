#include "procedural_imported_surface_strands_internal.h"

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double segment_distance(
    CoreObjectVec3 p0,
    CoreObjectVec3 p1,
    CoreObjectVec3 q0,
    CoreObjectVec3 q1) {
    const CoreObjectVec3 u = strand_vec_sub(p1, p0);
    const CoreObjectVec3 v = strand_vec_sub(q1, q0);
    const CoreObjectVec3 w = strand_vec_sub(p0, q0);
    const double a = strand_vec_dot(u, u);
    const double b = strand_vec_dot(u, v);
    const double c = strand_vec_dot(v, v);
    const double d = strand_vec_dot(u, w);
    const double e = strand_vec_dot(v, w);
    const double denominator = a * c - b * b;
    double s = 0.0;
    double t = 0.0;
    if (!(a > 1.0e-18) || !(c > 1.0e-18)) return 0.0;
    if (denominator > 1.0e-18) s = clamp01((b * e - c * d) / denominator);
    t = clamp01((b * s + e) / c);
    s = clamp01((b * t - d) / a);
    return strand_vec_length(strand_vec_sub(
        strand_vec_add(p0, strand_vec_scale(u, s)),
        strand_vec_add(q0, strand_vec_scale(v, t))));
}

static double segment_radius(
    const ProceduralImportedSurfaceStrandAsset *asset,
    size_t base,
    size_t segment) {
    return fmax(
        asset->radii[base + segment],
        asset->radii[base + segment + 1u]);
}

bool surface_strand_validate(
    const ProceduralImportedSurfaceStrandAsset *asset,
    const ProceduralImportedSurfaceStrandConfig *config,
    size_t *out_overlap_pairs,
    size_t *out_self_intersection_pairs) {
    size_t overlap_pairs = 0u;
    size_t self_pairs = 0u;
    if (!asset || !config || !out_overlap_pairs ||
        !out_self_intersection_pairs || asset->strand_count == 0u ||
        asset->points_per_strand < 2u) return false;
    for (size_t strand = 0u; strand < asset->strand_count; ++strand) {
        const size_t base = strand * asset->points_per_strand;
        for (size_t point = 0u;
             point < asset->points_per_strand; ++point) {
            const CoreObjectVec3 p = asset->points[base + point];
            const double radius = asset->radii[base + point];
            if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) ||
                !isfinite(radius) || !(radius > 0.0)) return false;
            if (point + 1u < asset->points_per_strand &&
                !(strand_vec_length(strand_vec_sub(
                    asset->points[base + point + 1u],
                    asset->points[base + point])) >
                  radius * 0.25)) return false;
        }
        for (size_t left_segment = 0u;
             left_segment + 1u < asset->points_per_strand; ++left_segment) {
            for (size_t right_segment = left_segment + 2u;
                 right_segment + 1u < asset->points_per_strand;
                 ++right_segment) {
                const double distance = segment_distance(
                    asset->points[base + left_segment],
                    asset->points[base + left_segment + 1u],
                    asset->points[base + right_segment],
                    asset->points[base + right_segment + 1u]);
                const double required =
                    segment_radius(asset, base, left_segment) +
                    segment_radius(asset, base, right_segment);
                if (distance < required) ++self_pairs;
            }
        }
    }
    for (size_t left = 0u; left < asset->strand_count; ++left) {
        const size_t left_base = left * asset->points_per_strand;
        for (size_t right = left + 1u;
             right < asset->strand_count; ++right) {
            const size_t right_base = right * asset->points_per_strand;
            bool overlaps = false;
            for (size_t a = 0u;
                 a + 1u < asset->points_per_strand && !overlaps; ++a) {
                for (size_t b = 0u;
                     b + 1u < asset->points_per_strand; ++b) {
                    const double required =
                        config->clearance_factor *
                        (segment_radius(asset, left_base, a) +
                         segment_radius(asset, right_base, b));
                    if (segment_distance(
                            asset->points[left_base + a],
                            asset->points[left_base + a + 1u],
                            asset->points[right_base + b],
                            asset->points[right_base + b + 1u]) <
                        required) {
                        overlaps = true;
                        break;
                    }
                }
            }
            if (overlaps) ++overlap_pairs;
        }
    }
    *out_overlap_pairs = overlap_pairs;
    *out_self_intersection_pairs = self_pairs;
    return overlap_pairs == 0u && self_pairs == 0u;
}
