#ifndef PROCEDURAL_IMPORTED_SURFACE_STRANDS_INTERNAL_H
#define PROCEDURAL_IMPORTED_SURFACE_STRANDS_INTERNAL_H

#include "procedural/procedural_imported_surface_strands.h"

#include <math.h>

typedef struct SurfaceStrandRoot {
    size_t source_triangle_index;
    CoreObjectVec3 anchor;
    CoreObjectVec3 normal;
    CoreObjectVec3 tangent;
    CoreObjectVec3 bitangent;
    CoreObjectVec3 barycentrics;
    double carrier_weight;
    double length;
    double phase;
} SurfaceStrandRoot;

typedef struct SurfaceStrandSelection {
    SurfaceStrandRoot *roots;
    size_t count;
    size_t candidate_count;
    size_t rejected_clearance_count;
    double minimum_clearance_units;
} SurfaceStrandSelection;

static inline CoreObjectVec3 strand_vec_add(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline CoreObjectVec3 strand_vec_sub(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline CoreObjectVec3 strand_vec_scale(
    CoreObjectVec3 value, double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static inline double strand_vec_dot(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline CoreObjectVec3 strand_vec_cross(
    CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static inline double strand_vec_length(CoreObjectVec3 value) {
    return sqrt(strand_vec_dot(value, value));
}

static inline bool strand_vec_normalize(
    CoreObjectVec3 value, CoreObjectVec3 *out) {
    const double length = strand_vec_length(value);
    if (!out || !(length > 1.0e-12) || !isfinite(length)) return false;
    *out = strand_vec_scale(value, 1.0 / length);
    return true;
}

void surface_strand_selection_free(SurfaceStrandSelection *selection);
bool surface_strand_select(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceStrandConfig *config,
    SurfaceStrandSelection *out);
bool surface_strand_build_asset(
    const SurfaceStrandSelection *selection,
    const ProceduralImportedSurfaceStrandConfig *config,
    ProceduralImportedSurfaceStrandAsset *out_asset);
bool surface_strand_build_tubes(
    const CoreMeshAssetRuntimeDocument *source,
    const SurfaceStrandSelection *selection,
    const ProceduralImportedSurfaceStrandConfig *config,
    const char *strand_asset_id,
    const ProceduralImportedSurfaceStrandAsset *asset,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceStrandProvenance *out_provenance,
    ProceduralImportedSurfaceStrandReceipt *receipt);
bool surface_strand_validate(
    const ProceduralImportedSurfaceStrandAsset *asset,
    const ProceduralImportedSurfaceStrandConfig *config,
    size_t *out_overlap_pairs,
    size_t *out_self_intersection_pairs);

#endif
