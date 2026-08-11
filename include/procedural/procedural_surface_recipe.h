#ifndef PROCEDURAL_SURFACE_RECIPE_H
#define PROCEDURAL_SURFACE_RECIPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_RECIPE_SCHEMA "ray_tracing.procedural_surface_recipe"
#define PROCEDURAL_SURFACE_RECIPE_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_RECIPE_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY 2048u
#define PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY 65u

typedef enum ProceduralSurfaceCoordinateSpace {
    PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT = 1
} ProceduralSurfaceCoordinateSpace;

typedef enum ProceduralSurfaceOutputClamp {
    PROCEDURAL_SURFACE_OUTPUT_CLAMP_SIGNED_UNIT = 1
} ProceduralSurfaceOutputClamp;

typedef enum ProceduralSurfaceRecipeStatus {
    PROCEDURAL_SURFACE_RECIPE_STATUS_OK = 0,
    PROCEDURAL_SURFACE_RECIPE_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_RECIPE_STATUS_IO,
    PROCEDURAL_SURFACE_RECIPE_STATUS_JSON,
    PROCEDURAL_SURFACE_RECIPE_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_RECIPE_STATUS_RECIPE_ID,
    PROCEDURAL_SURFACE_RECIPE_STATUS_COORDINATE_SPACE,
    PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
    PROCEDURAL_SURFACE_RECIPE_STATUS_QUALITY_BUDGET,
    PROCEDURAL_SURFACE_RECIPE_STATUS_CANONICALIZATION
} ProceduralSurfaceRecipeStatus;

typedef struct ProceduralSurfaceQualityBudgets {
    uint32_t preview_max_triangles;
    uint32_t inspection_max_triangles;
    uint32_t final_max_triangles;
    uint32_t max_field_evaluations;
} ProceduralSurfaceQualityBudgets;

typedef struct ProceduralSurfaceRecipeV1 {
    uint32_t schema_version;
    char recipe_id[PROCEDURAL_SURFACE_RECIPE_ID_CAPACITY];
    uint64_t seed;
    ProceduralSurfaceCoordinateSpace coordinate_space;

    double base_feature_size_units;
    double micro_feature_size_units;
    uint32_t octave_count;
    double lacunarity;
    double persistence;
    double ridge_valley_blend;
    double macro_micro_mix;

    double target_edge_length_units;
    double displacement_amplitude_units;
    double edge_lock_width_units;
    ProceduralSurfaceOutputClamp output_clamp;

    double snow_elevation_threshold_units;
    double snow_slope_threshold;

    ProceduralSurfaceQualityBudgets quality;
} ProceduralSurfaceRecipeV1;

typedef struct ProceduralSurfaceRecipeReport {
    ProceduralSurfaceRecipeStatus status;
    char field[64];
    char message[192];
} ProceduralSurfaceRecipeReport;

void ProceduralSurfaceRecipeV1_InitDefaults(ProceduralSurfaceRecipeV1 *recipe);

bool ProceduralSurfaceRecipeV1_Validate(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceRecipeReport *report);

bool ProceduralSurfaceRecipeV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceRecipeV1 *out_recipe,
    ProceduralSurfaceRecipeReport *report);

bool ProceduralSurfaceRecipeV1_SaveJsonFile(
    const char *path,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceRecipeReport *report);

bool ProceduralSurfaceRecipeV1_CanonicalJson(
    const ProceduralSurfaceRecipeV1 *recipe,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceRecipeReport *report);

bool ProceduralSurfaceRecipeV1_Digest(
    const ProceduralSurfaceRecipeV1 *recipe,
    char out_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY],
    ProceduralSurfaceRecipeReport *report);

const char *ProceduralSurfaceRecipeStatus_Name(
    ProceduralSurfaceRecipeStatus status);

#endif
