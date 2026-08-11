#include "procedural_imported_surface_growth_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void explicit_report_set(
    ProceduralImportedSurfaceGrowthReport *report,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    snprintf(report->field, sizeof(report->field), "%s", field);
    snprintf(report->message, sizeof(report->message), "%s", message);
}

static bool finite_vec(CoreObjectVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool validate_frame(
    CoreObjectVec3 normal,
    CoreObjectVec3 tangent,
    CoreObjectVec3 bitangent) {
    const double tolerance = 1.0e-5;
    const CoreObjectVec3 handed = growth_vec_cross(normal, tangent);
    return finite_vec(normal) && finite_vec(tangent) && finite_vec(bitangent) &&
        fabs(growth_vec_length(normal) - 1.0) <= tolerance &&
        fabs(growth_vec_length(tangent) - 1.0) <= tolerance &&
        fabs(growth_vec_length(bitangent) - 1.0) <= tolerance &&
        fabs(growth_vec_dot(normal, tangent)) <= tolerance &&
        fabs(growth_vec_dot(normal, bitangent)) <= tolerance &&
        fabs(growth_vec_dot(tangent, bitangent)) <= tolerance &&
        growth_vec_dot(handed, bitangent) >= 1.0 - tolerance;
}

bool ProceduralImportedSurfaceGrowth_CompileExplicitRoot(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const ProceduralImportedSurfaceGrowthExplicitRoot *root,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *out_receipt,
    ProceduralImportedSurfaceGrowthReport *report) {
    SurfaceGrowthSelection selection = {0};
    const CoreMeshAssetRuntimeTriangle *triangle;
    CoreObjectVec3 positions[3];
    CoreObjectVec3 geometric_normal;
    CoreObjectVec3 tangent;
    CoreObjectVec3 bitangent;
    CoreObjectVec3 extent;
    double diagonal;
    double barycentric_sum;
    double cosine;
    double sine;
    bool ok;
    explicit_report_set(
        report, "explicit_root", "valid explicit PSG-22 root is required");
    if (!source || !source->vertices || !source->triangles || !region ||
        !region->vertex_weights ||
        !config || !root || root->source_triangle_index >= source->triangle_count ||
        !isfinite(root->aspect) || !(root->aspect > 0.0) || root->aspect > 4.0 ||
        !isfinite(root->rotation_radians) ||
        !validate_frame(root->normal, root->tangent, root->bitangent)) return false;
    barycentric_sum = root->barycentric[0] + root->barycentric[1] +
        root->barycentric[2];
    if (!isfinite(root->barycentric[0]) || !isfinite(root->barycentric[1]) ||
        !isfinite(root->barycentric[2]) || root->barycentric[0] < 0.0 ||
        root->barycentric[1] < 0.0 || root->barycentric[2] < 0.0 ||
        fabs(barycentric_sum - 1.0) > 1.0e-6) return false;
    extent = growth_vec_sub(
        source->contract.local_bounds.max, source->contract.local_bounds.min);
    diagonal = growth_vec_length(extent);
    if (!(diagonal > 0.0) ||
        config->mound_radius_units * fmax(1.0, root->aspect) >
            diagonal * config->maximum_radius_to_bounds_diagonal_ratio) {
        explicit_report_set(
            report, "growth_scale",
            "explicit root aspect exceeds the bounded source-diagonal ratio");
        return false;
    }
    triangle = &source->triangles[root->source_triangle_index];
    positions[0] = source->vertices[triangle->a].position;
    positions[1] = source->vertices[triangle->b].position;
    positions[2] = source->vertices[triangle->c].position;
    if (!growth_vec_normalize(
            growth_vec_cross(
                growth_vec_sub(positions[1], positions[0]),
                growth_vec_sub(positions[2], positions[0])),
            &geometric_normal) ||
        growth_vec_dot(geometric_normal, root->normal) <= 0.0) {
        explicit_report_set(
            report, "normal_compatibility",
            "explicit root normal opposes its source triangle");
        return false;
    }
    selection.elements = calloc(1u, sizeof(*selection.elements));
    if (!selection.elements) return false;
    selection.count = 1u;
    selection.candidate_count = 1u;
    selection.minimum_clearance_units = 0.0;
    selection.elements[0].source_triangle_index = root->source_triangle_index;
    for (size_t i = 0u; i < 3u; ++i) {
        selection.elements[0].anchor = growth_vec_add(
            selection.elements[0].anchor,
            growth_vec_scale(positions[i], root->barycentric[i]));
    }
    cosine = cos(root->rotation_radians);
    sine = sin(root->rotation_radians);
    tangent = growth_vec_add(
        growth_vec_scale(root->tangent, cosine),
        growth_vec_scale(root->bitangent, sine));
    bitangent = growth_vec_add(
        growth_vec_scale(root->tangent, -sine),
        growth_vec_scale(root->bitangent, cosine));
    selection.elements[0].normal = root->normal;
    selection.elements[0].tangent = tangent;
    selection.elements[0].bitangent = bitangent;
    selection.elements[0].radius = config->mound_radius_units;
    selection.elements[0].aspect = root->aspect;
    selection.elements[0].height = config->mound_height_units;
    selection.elements[0].attachment_depth = config->attachment_depth_units;
    selection.elements[0].carrier_weight =
        root->barycentric[0] * region->vertex_weights[triangle->a] +
        root->barycentric[1] * region->vertex_weights[triangle->b] +
        root->barycentric[2] * region->vertex_weights[triangle->c];
    if (!(selection.elements[0].carrier_weight > 0.0)) {
        explicit_report_set(
            report, "carrier_agreement",
            "explicit root has no positive selected carrier support");
        surface_growth_selection_free(&selection);
        return false;
    }
    ok = surface_growth_compile_selection(
        source, source_runtime_path, region, region_path, config, &selection,
        growth_asset_id, out_document, out_provenance, out_receipt, report);
    surface_growth_selection_free(&selection);
    return ok;
}
