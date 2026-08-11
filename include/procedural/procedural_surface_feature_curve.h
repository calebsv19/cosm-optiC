#ifndef PROCEDURAL_SURFACE_FEATURE_CURVE_H
#define PROCEDURAL_SURFACE_FEATURE_CURVE_H

#include "procedural/procedural_surface_feature_field.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS 256u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION 32u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY 32u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_CANONICAL_CAPACITY 1048576u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY 65u

typedef struct ProceduralSurfaceFeatureCurveSegmentV1 {
    uint32_t curve_id;
    uint32_t segment_id;
    uint32_t parent_curve_id;
    uint32_t source_triangle;
    double barycentric_start[3];
    double barycentric_end[3];
    ProceduralSurfaceFeatureVec3 start;
    ProceduralSurfaceFeatureVec3 end;
    ProceduralSurfaceFeatureVec3 normal_start;
    ProceduralSurfaceFeatureVec3 normal_end;
    ProceduralSurfaceFeatureVec3 tangent;
    double width_start;
    double width_end;
    double depth_start;
    double depth_end;
    double edge_softness;
    double rim_width;
} ProceduralSurfaceFeatureCurveSegmentV1;

typedef struct ProceduralSurfaceFeatureCurveFieldV1 {
    char source_mesh_digest_sha256[
        PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY];
    char authoring_digest_sha256[
        PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY];
    uint64_t seed;
    double normal_compatibility_cosine;
    size_t segment_count;
    ProceduralSurfaceFeatureCurveSegmentV1
        segments[PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS];
    /* Compiled deterministic object-space index; not serialized authoring. */
    double grid_min_x;
    double grid_min_y;
    double grid_max_x;
    double grid_max_y;
    uint16_t grid_counts[
        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION *
        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION];
    uint16_t grid_indices[
        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION *
        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION *
        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY];
    size_t grid_index_count;
} ProceduralSurfaceFeatureCurveFieldV1;

typedef struct ProceduralSurfaceFeatureCurveSampleV1 {
    double coverage;
    double interior;
    double rim;
    double signed_depth;
    double signed_lateral_distance;
    double depth_slope;
    uint32_t curve_id;
    uint32_t segment_id;
    ProceduralSurfaceFeatureVec3 direction;
    size_t candidates_considered;
} ProceduralSurfaceFeatureCurveSampleV1;

bool ProceduralSurfaceFeatureCurveFieldV1_Validate(
    const ProceduralSurfaceFeatureCurveFieldV1 *field);
bool ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(
    ProceduralSurfaceFeatureCurveFieldV1 *field);
bool ProceduralSurfaceFeatureCurveFieldV1_CanonicalJson(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    char *out_json, size_t out_capacity);
bool ProceduralSurfaceFeatureCurveFieldV1_Digest(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    char out_digest[PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY]);
bool ProceduralSurfaceFeatureCurveFieldV1_LoadJsonFile(
    const char *path, ProceduralSurfaceFeatureCurveFieldV1 *out_field);
bool ProceduralSurfaceFeatureCurveFieldV1_Sample(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    ProceduralSurfaceFeatureVec3 position,
    ProceduralSurfaceFeatureVec3 normal,
    ProceduralSurfaceFeatureCurveSampleV1 *out_sample);
bool ProceduralSurfaceFeatureCurveSampleV1_ApplyShadingNormal(
    const ProceduralSurfaceFeatureCurveSampleV1 *sample,
    ProceduralSurfaceFeatureVec3 geometric_normal,
    double strength,
    ProceduralSurfaceFeatureVec3 *out_shading_normal);

#endif
