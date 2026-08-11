#ifndef PROCEDURAL_SOLID_FEATURE_H
#define PROCEDURAL_SOLID_FEATURE_H

#include "procedural/procedural_solid_mesh.h"

typedef struct ProceduralSolidFeatureConfig {
    size_t projection_iterations;
    double maximum_projection_step_units;
    double crease_angle_degrees;
    double minimum_improvement_ratio;
} ProceduralSolidFeatureConfig;

typedef struct ProceduralSolidFeatureSummary {
    size_t projected_vertex_count;
    size_t feature_vertex_count;
    double residual_rms_before;
    double residual_rms_after;
    double residual_max_before;
    double residual_max_after;
    double improvement_ratio;
    double maximum_position_delta_units;
    bool topology_preserved;
    bool measurable_improvement;
} ProceduralSolidFeatureSummary;

void ProceduralSolidFeatureConfig_Init(ProceduralSolidFeatureConfig *config);

bool ProceduralSolidFeature_Optimize(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidFeatureConfig *config,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidFeatureSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
