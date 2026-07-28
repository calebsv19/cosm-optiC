#ifndef PROCEDURAL_SURFACE_MATERIAL_H
#define PROCEDURAL_SURFACE_MATERIAL_H

#include "procedural/procedural_surface_field_3d.h"

#include <stdbool.h>
#include <stddef.h>

#define PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY 65u
#define PROCEDURAL_SURFACE_MATERIAL_SUMMARY_CAPACITY 1048576u

typedef struct ProceduralSurfaceMaterialSample {
    double stone_color_r;
    double stone_color_g;
    double stone_color_b;
    double stone_roughness;
    double elevation_units;
    double upward_slope;
    double snow_elevation_weight;
    double snow_slope_weight;
    double snow_breakup_weight;
    double snow_likelihood;
    double final_color_r;
    double final_color_g;
    double final_color_b;
    double final_roughness;
} ProceduralSurfaceMaterialSample;

typedef enum ProceduralSurfaceMaterialStatus {
    PROCEDURAL_SURFACE_MATERIAL_STATUS_OK = 0,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_RECIPE,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_FIELD,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_POSITION,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_NORMAL,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_SAMPLE,
    PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY
} ProceduralSurfaceMaterialStatus;

typedef struct ProceduralSurfaceMaterialReport {
    ProceduralSurfaceMaterialStatus status;
    char field[64];
    char message[192];
} ProceduralSurfaceMaterialReport;

bool ProceduralSurfaceMaterial_Evaluate(
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldOutput *retained_field,
    ProceduralSurfaceFieldPoint3D displaced_position,
    ProceduralSurfaceFieldPoint3D geometry_normal,
    ProceduralSurfaceMaterialSample *out_sample,
    ProceduralSurfaceMaterialReport *report);

bool ProceduralSurfaceMaterial_CanonicalSummary(
    const char *recipe_digest,
    const char *shell_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceMaterialSample *samples,
    size_t sample_count,
    char *out_summary,
    size_t out_capacity,
    ProceduralSurfaceMaterialReport *report);

bool ProceduralSurfaceMaterial_SummaryDigest(
    const char *recipe_digest,
    const char *shell_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceMaterialSample *samples,
    size_t sample_count,
    char out_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY],
    ProceduralSurfaceMaterialReport *report);

const char *ProceduralSurfaceMaterialStatus_Name(
    ProceduralSurfaceMaterialStatus status);

#endif
