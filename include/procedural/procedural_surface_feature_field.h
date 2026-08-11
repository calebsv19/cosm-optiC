#ifndef PROCEDURAL_SURFACE_FEATURE_FIELD_H
#define PROCEDURAL_SURFACE_FEATURE_FIELD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FEATURE_FIELD_MAX_FEATURES 512u
#define PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION 32u
#define PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY 64u
#define PROCEDURAL_SURFACE_FEATURE_FIELD_CANONICAL_CAPACITY 1048576u
#define PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY 65u

typedef struct ProceduralSurfaceFeatureVec3 { double x, y, z; } ProceduralSurfaceFeatureVec3;

typedef struct ProceduralSurfaceFeatureRootV1 {
    uint32_t source_triangle;
    double barycentric[3];
    ProceduralSurfaceFeatureVec3 position, normal, tangent, bitangent;
    double radius, aspect, rotation, edge_softness, rim_width;
    double height_or_depth;
    uint32_t population, feature_id;
} ProceduralSurfaceFeatureRootV1;

typedef struct ProceduralSurfaceFeatureFieldV1 {
    char source_mesh_digest_sha256[65];
    char authoring_digest_sha256[65];
    uint64_t seed;
    double normal_compatibility_cosine;
    size_t feature_count;
    ProceduralSurfaceFeatureRootV1 features[PROCEDURAL_SURFACE_FEATURE_FIELD_MAX_FEATURES];
    /* Compiled deterministic object-space index; not authoring payload. */
    double grid_min_x, grid_min_y, grid_max_x, grid_max_y;
    uint16_t grid_counts[PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION * PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION];
    uint16_t grid_indices[PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION *
                          PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION *
                          PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY];
    size_t grid_index_count;
} ProceduralSurfaceFeatureFieldV1;

typedef struct ProceduralSurfaceFeatureSampleV1 {
    double coverage, interior, rim, height_or_depth;
    uint32_t feature_id;
    ProceduralSurfaceFeatureVec3 direction;
    size_t candidates_considered;
} ProceduralSurfaceFeatureSampleV1;

bool ProceduralSurfaceFeatureFieldV1_Validate(const ProceduralSurfaceFeatureFieldV1 *field);
bool ProceduralSurfaceFeatureFieldV1_BuildIndex(ProceduralSurfaceFeatureFieldV1 *field);
bool ProceduralSurfaceFeatureFieldV1_CanonicalJson(const ProceduralSurfaceFeatureFieldV1 *field,
    char *out_json, size_t out_capacity);
bool ProceduralSurfaceFeatureFieldV1_Digest(const ProceduralSurfaceFeatureFieldV1 *field,
    char out_digest[PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY]);
bool ProceduralSurfaceFeatureFieldV1_SaveJsonFileAtomic(
    const char *path, const ProceduralSurfaceFeatureFieldV1 *field);
bool ProceduralSurfaceFeatureFieldV1_LoadJsonFile(
    const char *path, ProceduralSurfaceFeatureFieldV1 *out_field);
bool ProceduralSurfaceFeatureFieldV1_Sample(const ProceduralSurfaceFeatureFieldV1 *field,
    ProceduralSurfaceFeatureVec3 position, ProceduralSurfaceFeatureVec3 normal,
    ProceduralSurfaceFeatureSampleV1 *out_sample);

#endif
