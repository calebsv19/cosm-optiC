#ifndef PROCEDURAL_SOLID_CREASE_H
#define PROCEDURAL_SOLID_CREASE_H

#include "procedural/procedural_solid_mesh.h"

typedef struct ProceduralSolidCreaseConfig {
    double crease_angle_degrees;
    double maximum_position_delta_units;
    double regularization;
    double minimum_qef_improvement_ratio;
    double maximum_relative_volume_delta;
} ProceduralSolidCreaseConfig;

typedef struct ProceduralSolidCreaseSummary {
    size_t candidate_vertex_count;
    size_t optimized_vertex_count;
    size_t constraint_count;
    double qef_rms_before;
    double qef_rms_after;
    double qef_improvement_ratio;
    double maximum_position_delta_units;
    double relative_volume_delta;
    bool topology_preserved;
    bool measurable_improvement;
} ProceduralSolidCreaseSummary;

void ProceduralSolidCreaseConfig_Init(ProceduralSolidCreaseConfig *config);

bool ProceduralSolidCrease_Optimize(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidCreaseConfig *config,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidCreaseSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
