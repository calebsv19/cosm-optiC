#ifndef PROCEDURAL_IMPORTED_SURFACE_GROWTH_INTERNAL_H
#define PROCEDURAL_IMPORTED_SURFACE_GROWTH_INTERNAL_H

#include "procedural/procedural_imported_surface_growth.h"

#include <math.h>

typedef struct SurfaceGrowthElement {
    size_t source_triangle_index;
    CoreObjectVec3 anchor;
    CoreObjectVec3 normal;
    CoreObjectVec3 tangent;
    CoreObjectVec3 bitangent;
    double radius;
    double aspect;
    double height;
    double attachment_depth;
    double carrier_weight;
} SurfaceGrowthElement;

typedef struct SurfaceGrowthSelection {
    SurfaceGrowthElement *elements;
    size_t count;
    size_t candidate_count;
    size_t rejected_clearance_count;
    double minimum_clearance_units;
} SurfaceGrowthSelection;

static inline CoreObjectVec3 growth_vec_add(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline CoreObjectVec3 growth_vec_sub(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline CoreObjectVec3 growth_vec_scale(
    CoreObjectVec3 value, double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static inline double growth_vec_dot(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline CoreObjectVec3 growth_vec_cross(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static inline double growth_vec_length(CoreObjectVec3 value) {
    return sqrt(growth_vec_dot(value, value));
}

static inline bool growth_vec_normalize(
    CoreObjectVec3 value, CoreObjectVec3 *out) {
    const double length = growth_vec_length(value);
    if (!out || !isfinite(length) || !(length > 1.0e-14)) return false;
    *out = growth_vec_scale(value, 1.0 / length);
    return true;
}

void surface_growth_selection_free(SurfaceGrowthSelection *selection);

bool surface_growth_select(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceGrowthConfig *config,
    SurfaceGrowthSelection *out);

bool surface_growth_build_geometry(
    const CoreMeshAssetRuntimeDocument *source,
    const SurfaceGrowthSelection *selection,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *receipt);

bool surface_growth_validate_separation(
    const SurfaceGrowthSelection *selection,
    size_t *out_overlap_pairs,
    size_t *out_self_intersection_pairs,
    double *out_minimum_clearance);

bool surface_growth_compile_selection(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const SurfaceGrowthSelection *selection,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *out_receipt,
    ProceduralImportedSurfaceGrowthReport *report);

#endif
