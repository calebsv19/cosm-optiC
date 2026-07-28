#ifndef PROCEDURAL_SURFACE_SHELL_H
#define PROCEDURAL_SURFACE_SHELL_H

#include "core_mesh_asset.h"
#include "procedural/procedural_surface_binding.h"
#include "procedural/procedural_surface_material.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ProceduralSurfaceShellConfig {
    double target_edge_length_units;
    double max_displacement_to_source_edge_ratio;
    size_t max_vertices;
    size_t max_triangles;
    unsigned int max_refinement_levels;
    bool require_closed_manifold;
    bool require_positive_volume;
} ProceduralSurfaceShellConfig;

typedef struct ProceduralSurfaceShellSummary {
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t vertex_count;
    size_t triangle_count;
    size_t unique_edge_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t connected_component_count;
    int euler_characteristic;
    unsigned int refinement_levels;
    double source_max_edge_length_units;
    double final_max_edge_length_units;
    double source_min_edge_length_units;
    double max_abs_displacement_units;
    double signed_volume_units3;
} ProceduralSurfaceShellSummary;

typedef enum ProceduralSurfaceShellStatus {
    PROCEDURAL_SURFACE_SHELL_STATUS_OK = 0,
    PROCEDURAL_SURFACE_SHELL_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_SHELL_STATUS_CONFIG,
    PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_INVALID,
    PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_OPEN,
    PROCEDURAL_SURFACE_SHELL_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_SHELL_STATUS_ALLOCATION,
    PROCEDURAL_SURFACE_SHELL_STATUS_FIELD,
    PROCEDURAL_SURFACE_SHELL_STATUS_DISPLACEMENT_LIMIT,
    PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID
} ProceduralSurfaceShellStatus;

typedef struct ProceduralSurfaceShellReport {
    ProceduralSurfaceShellStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceShellReport;

void ProceduralSurfaceShellConfig_Init(ProceduralSurfaceShellConfig *config);

bool ProceduralSurfaceShell_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceShellConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSurfaceMaterialSample **out_vertex_materials,
    ProceduralSurfaceShellSummary *out_summary,
    ProceduralSurfaceShellReport *report);

const char *ProceduralSurfaceShellStatus_Name(
    ProceduralSurfaceShellStatus status);

#endif
