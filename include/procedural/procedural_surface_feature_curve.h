#ifndef PROCEDURAL_SURFACE_FEATURE_CURVE_H
#define PROCEDURAL_SURFACE_FEATURE_CURVE_H

#include "procedural/procedural_surface_feature_field.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS 256u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION 32u
#define PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY 32u

typedef struct ProceduralSurfaceFeatureCurveSegmentV1 {
    uint32_t curve_id, segment_id, source_triangle;
    double barycentric_root[3];
    ProceduralSurfaceFeatureVec3 start, end, normal, tangent;
    double width, depth, edge_softness;
} ProceduralSurfaceFeatureCurveSegmentV1;

typedef struct ProceduralSurfaceFeatureCurveFieldV1 {
    double normal_compatibility_cosine;
    size_t segment_count;
    ProceduralSurfaceFeatureCurveSegmentV1 segments[PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS];
    double grid_min_x, grid_min_y, grid_max_x, grid_max_y;
    uint16_t grid_counts[PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION];
    uint16_t grid_indices[PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY];
    size_t grid_index_count;
} ProceduralSurfaceFeatureCurveFieldV1;

typedef struct ProceduralSurfaceFeatureCurveSampleV1 {
    double coverage, interior, rim, signed_depth;
    uint32_t curve_id, segment_id;
    ProceduralSurfaceFeatureVec3 direction;
    size_t candidates_considered;
} ProceduralSurfaceFeatureCurveSampleV1;

bool ProceduralSurfaceFeatureCurveFieldV1_Validate(const ProceduralSurfaceFeatureCurveFieldV1 *field);
bool ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(
    ProceduralSurfaceFeatureCurveFieldV1 *field);
bool ProceduralSurfaceFeatureCurveFieldV1_Sample(const ProceduralSurfaceFeatureCurveFieldV1 *field,
    ProceduralSurfaceFeatureVec3 position, ProceduralSurfaceFeatureVec3 normal,
    ProceduralSurfaceFeatureCurveSampleV1 *out_sample);
bool ProceduralSurfaceFeatureCurveSampleV1_ApplyShadingNormal(
    const ProceduralSurfaceFeatureCurveSampleV1 *sample,
    ProceduralSurfaceFeatureVec3 geometric_normal, double strength,
    ProceduralSurfaceFeatureVec3 *out_shading_normal);

#endif
