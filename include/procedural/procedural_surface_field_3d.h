#ifndef PROCEDURAL_SURFACE_FIELD_3D_H
#define PROCEDURAL_SURFACE_FIELD_3D_H

#include "procedural/procedural_surface_recipe.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FIELD_SUMMARY_CAPACITY 16384u
#define PROCEDURAL_SURFACE_FIELD_DIGEST_CAPACITY 65u

typedef struct ProceduralSurfaceFieldPoint3D {
    double x;
    double y;
    double z;
} ProceduralSurfaceFieldPoint3D;

typedef struct ProceduralSurfaceFieldOutput {
    double height;
    double macro_variation;
    double micro_variation;
    double rock_mask;
    double roughness;
    double snow_precursor;
} ProceduralSurfaceFieldOutput;

typedef struct ProceduralSurfaceFieldBudget {
    uint64_t evaluations;
    uint64_t max_evaluations;
} ProceduralSurfaceFieldBudget;

typedef enum ProceduralSurfaceFieldStatus {
    PROCEDURAL_SURFACE_FIELD_STATUS_OK = 0,
    PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_FIELD_STATUS_RECIPE,
    PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_POINT,
    PROCEDURAL_SURFACE_FIELD_STATUS_COORDINATE_RANGE,
    PROCEDURAL_SURFACE_FIELD_STATUS_BUDGET,
    PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT,
    PROCEDURAL_SURFACE_FIELD_STATUS_SAMPLE_ID,
    PROCEDURAL_SURFACE_FIELD_STATUS_SUMMARY
} ProceduralSurfaceFieldStatus;

typedef struct ProceduralSurfaceFieldReport {
    ProceduralSurfaceFieldStatus status;
    char field[64];
    char message[192];
} ProceduralSurfaceFieldReport;

void ProceduralSurfaceFieldBudget_Init(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldBudget *budget);

bool ProceduralSurfaceField3D_Evaluate(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_output,
    ProceduralSurfaceFieldReport *report);

bool ProceduralSurfaceField3D_CanonicalSummary(
    const char *recipe_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceFieldOutput *outputs,
    size_t sample_count,
    char *out_summary,
    size_t out_capacity,
    ProceduralSurfaceFieldReport *report);

bool ProceduralSurfaceField3D_SummaryDigest(
    const char *recipe_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceFieldOutput *outputs,
    size_t sample_count,
    char out_digest[PROCEDURAL_SURFACE_FIELD_DIGEST_CAPACITY],
    ProceduralSurfaceFieldReport *report);

const char *ProceduralSurfaceFieldStatus_Name(
    ProceduralSurfaceFieldStatus status);

#endif
