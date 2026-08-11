#ifndef PROCEDURAL_SOLID_SHADING_H
#define PROCEDURAL_SOLID_SHADING_H

#include "procedural/procedural_solid_mesh.h"

typedef struct ProceduralSolidShadingConfig {
    double crease_angle_degrees;
    size_t maximum_output_vertices;
    double minimum_hard_corner_improvement_ratio;
} ProceduralSolidShadingConfig;

typedef struct ProceduralSolidShadingSummary {
    size_t source_vertex_count;
    size_t output_vertex_count;
    size_t split_vertex_count;
    size_t hard_vertex_count;
    size_t hard_corner_count;
    size_t normal_island_count;
    double hard_corner_rms_degrees_before;
    double hard_corner_rms_degrees_after;
    double hard_corner_improvement_ratio;
    bool geometric_topology_preserved;
    bool measurable_improvement;
} ProceduralSolidShadingSummary;

void ProceduralSolidShadingConfig_Init(
    ProceduralSolidShadingConfig *config);

bool ProceduralSolidShading_SplitCreases(
    const ProceduralSolidShadingConfig *config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *geometric_summary,
    ProceduralSolidShadingSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
