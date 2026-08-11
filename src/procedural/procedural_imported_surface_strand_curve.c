#include "procedural/procedural_imported_surface_strand_curve.h"

#include <stdlib.h>

bool ProceduralImportedSurfaceStrands_BuildCurveAsset(
    const ProceduralImportedSurfaceStrandAsset *strands,
    RuntimeCurveAsset3D *out_curve_asset) {
    Vec3 *points = NULL;
    bool ok = false;
    size_t point_count = 0u;

    if (!strands || !out_curve_asset || !strands->points || !strands->radii ||
        strands->strand_count == 0u || strands->points_per_strand < 2u) {
        return false;
    }
    point_count = strands->strand_count * strands->points_per_strand;
    if (strands->points_per_strand != 0u &&
        point_count / strands->points_per_strand != strands->strand_count) {
        return false;
    }
    points = calloc(point_count, sizeof(*points));
    if (!points) return false;
    for (size_t i = 0u; i < point_count; ++i) {
        points[i] = vec3(strands->points[i].x,
                         strands->points[i].y,
                         strands->points[i].z);
    }
    ok = RuntimeCurveAsset3D_BuildFromPolylineStrands(
        out_curve_asset,
        points,
        strands->radii,
        strands->strand_count,
        strands->points_per_strand);
    free(points);
    return ok;
}
