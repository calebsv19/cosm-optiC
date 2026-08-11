#ifndef PROCEDURAL_SOLID_REMESH_H
#define PROCEDURAL_SOLID_REMESH_H

#include "procedural/procedural_solid_mesh.h"

#define PROCEDURAL_SOLID_REMESH_MAX_PASSES 8u

typedef struct ProceduralSolidRemeshConfig {
    ProceduralSolidMeshConfig mesh;
    uint32_t base_cells;
    uint32_t maximum_cells;
    size_t maximum_passes;
    double requested_feature_size_units;
    double maximum_relative_volume_delta;
    double maximum_bounds_delta_units;
    bool require_topology_convergence;
} ProceduralSolidRemeshConfig;

typedef struct ProceduralSolidRemeshPass {
    uint32_t cells;
    size_t vertex_count;
    size_t triangle_count;
    int euler_characteristic;
    size_t connected_component_count;
    double signed_volume_units3;
    double relative_volume_delta;
    double bounds_delta_units;
    double maximum_edge_length_units;
    double thin_feature_floor_units;
    bool topology_matches_previous;
    bool converged;
    char mesh_digest_sha256[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
} ProceduralSolidRemeshPass;

typedef struct ProceduralSolidRemeshSummary {
    size_t pass_count;
    size_t selected_pass;
    bool converged;
    ProceduralSolidRemeshPass passes[PROCEDURAL_SOLID_REMESH_MAX_PASSES];
    ProceduralSolidMeshSummary selected_mesh;
} ProceduralSolidRemeshSummary;

void ProceduralSolidRemeshConfig_Init(ProceduralSolidRemeshConfig *config);

bool ProceduralSolidRemesh_CompileAdaptive(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidRemeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidRemeshSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
