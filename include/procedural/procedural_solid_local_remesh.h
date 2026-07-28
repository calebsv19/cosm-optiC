#ifndef PROCEDURAL_SOLID_LOCAL_REMESH_H
#define PROCEDURAL_SOLID_LOCAL_REMESH_H

#include "procedural/procedural_solid_feature.h"
#include "procedural/procedural_solid_regions.h"

#define PROCEDURAL_SOLID_LOCAL_REMESH_MAX_PASSES 4u

typedef struct ProceduralSolidLocalRemeshConfig {
    ProceduralSolidMeshConfig mesh;
    ProceduralSolidFeatureConfig feature;
    uint32_t base_cells;
    uint32_t maximum_cells;
    size_t maximum_passes;
    uint32_t refinement_factor;
    double surface_band_cells;
    double maximum_active_cell_ratio;
    double maximum_relative_volume_delta;
    double maximum_bounds_delta_units;
    bool require_topology_convergence;
} ProceduralSolidLocalRemeshConfig;

typedef struct ProceduralSolidLocalRemeshPass {
    uint32_t coarse_cells;
    uint32_t fine_cells;
    size_t coarse_sample_count;
    size_t total_fine_cell_count;
    size_t active_fine_cell_count;
    double active_cell_ratio;
    size_t inactive_interface_face_count;
    size_t transition_surface_crossing_count;
    double relative_volume_delta;
    double bounds_delta_units;
    bool topology_matches_previous;
    bool converged;
    ProceduralSolidMeshSummary mesh;
    ProceduralSolidFeatureSummary feature;
    ProceduralSolidRegionSummary regions;
} ProceduralSolidLocalRemeshPass;

typedef struct ProceduralSolidLocalRemeshSummary {
    size_t pass_count;
    size_t selected_pass;
    bool converged;
    ProceduralSolidLocalRemeshPass
        passes[PROCEDURAL_SOLID_LOCAL_REMESH_MAX_PASSES];
    ProceduralSolidMeshSummary selected_mesh;
    ProceduralSolidFeatureSummary selected_feature;
    ProceduralSolidRegionSummary selected_regions;
} ProceduralSolidLocalRemeshSummary;

void ProceduralSolidLocalRemeshConfig_Init(
    ProceduralSolidLocalRemeshConfig *config);

bool ProceduralSolidLocalRemesh_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidLocalRemeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidLocalRemeshSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
