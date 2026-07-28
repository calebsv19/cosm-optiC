#ifndef PROCEDURAL_SOLID_QUALITY_H
#define PROCEDURAL_SOLID_QUALITY_H

#include "procedural/procedural_solid_crease.h"
#include "procedural/procedural_solid_local_remesh.h"
#include "procedural/procedural_solid_shading.h"

typedef struct ProceduralSolidSurfaceErrorSummary {
    size_t sample_count;
    double signed_distance_rms_units;
    double signed_distance_max_units;
    double face_gradient_rms_degrees;
    double face_gradient_max_degrees;
    double composite_score;
} ProceduralSolidSurfaceErrorSummary;

typedef struct ProceduralSolidQualityConfig {
    ProceduralSolidLocalRemeshConfig local;
    ProceduralSolidCreaseConfig crease;
    ProceduralSolidShadingConfig shading;
    uint32_t baseline_maximum_cells;
    uint32_t quality_maximum_cells;
    double maximum_signed_distance_rms_units;
    double maximum_signed_distance_max_units;
    double maximum_face_gradient_rms_degrees;
    double minimum_refinement_improvement_ratio;
    bool enable_error_driven_refinement;
} ProceduralSolidQualityConfig;

typedef struct ProceduralSolidQualitySummary {
    bool refinement_triggered;
    bool refinement_selected;
    uint32_t baseline_cells;
    uint32_t selected_cells;
    double refinement_improvement_ratio;
    ProceduralSolidSurfaceErrorSummary baseline_error;
    ProceduralSolidSurfaceErrorSummary selected_error;
    ProceduralSolidCreaseSummary crease;
    ProceduralSolidShadingSummary shading;
    ProceduralSolidLocalRemeshSummary local;
} ProceduralSolidQualitySummary;

void ProceduralSolidQualityConfig_Init(ProceduralSolidQualityConfig *config);

bool ProceduralSolidQuality_Analyze(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *mesh_config,
    const CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidSurfaceErrorSummary *out_summary,
    ProceduralSolidMeshReport *report);

bool ProceduralSolidQuality_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidQualityConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidQualitySummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
