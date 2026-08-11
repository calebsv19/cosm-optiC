#ifndef PROCEDURAL_IMPORTED_SURFACE_INSET_H
#define PROCEDURAL_IMPORTED_SURFACE_INSET_H

#include "procedural/procedural_imported_surface_region.h"
#include "procedural/procedural_solid_mesh.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_IMPORTED_SURFACE_INSET_SCHEMA \
    "ray_tracing.procedural_imported_surface_inset_receipt"
#define PROCEDURAL_IMPORTED_SURFACE_INSET_SCHEMA_VERSION 3u
#define PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY 65u

/*
 * Dense imported meshes need room for the conservative four-way temporary
 * refinement allocation used by PSG-20/21. These remain finite defaults, not
 * an unbounded allocation promise.
 */
#define PROCEDURAL_IMPORTED_SURFACE_INSET_DEFAULT_MAX_VERTICES 2000000u
#define PROCEDURAL_IMPORTED_SURFACE_INSET_DEFAULT_MAX_TRIANGLES 8000000u

typedef enum ProceduralImportedSurfaceInsetRole {
    PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_RETAINED_SURFACE = 0,
    PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_TRANSITION_WALL = 1,
    PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_INSET_FLOOR = 2
} ProceduralImportedSurfaceInsetRole;

typedef struct ProceduralImportedSurfaceInsetConfig {
    double selection_threshold;
    double depth_units;
    double depth_variation;
    double maximum_depth_to_bounds_diagonal_ratio;
    double minimum_triangle_area2;
    size_t max_vertices;
    size_t max_triangles;
    size_t max_adaptive_refinement_passes;
    size_t minimum_selected_component_triangles;
    double target_boundary_edge_length_units;
    bool refine_transition_band;
    bool adaptive_refinement_enabled;
} ProceduralImportedSurfaceInsetConfig;

typedef struct ProceduralImportedSurfaceInsetProvenance {
    size_t triangle_count;
    size_t *source_triangle_indices;
    ProceduralImportedSurfaceInsetRole *roles;
} ProceduralImportedSurfaceInsetProvenance;

typedef struct ProceduralImportedSurfaceInsetReceipt {
    uint32_t schema_version;
    char source_asset_id[64];
    char semantic_source_id[64];
    char derived_asset_id[64];
    char region_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char source_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char carrier_value_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char carrier_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char config_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char derived_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    char provenance_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY];
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t refined_vertex_count;
    size_t refined_triangle_count;
    size_t transition_source_triangle_count;
    size_t adaptive_refinement_pass_count;
    size_t discarded_candidate_triangle_count;
    size_t selected_component_count;
    size_t selected_refined_triangle_count;
    size_t retained_triangle_count;
    size_t transition_wall_triangle_count;
    size_t inset_floor_triangle_count;
    size_t boundary_loop_count;
    size_t boundary_ring_edge_count;
    size_t derived_vertex_count;
    size_t derived_triangle_count;
    size_t unique_edge_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t connected_component_count;
    int euler_characteristic;
    int source_euler_characteristic;
    double signed_volume_units3;
    double minimum_inset_depth_units;
    double maximum_inset_depth_units;
    double target_boundary_edge_length_units;
    double initial_max_boundary_edge_length_units;
    double final_max_boundary_edge_length_units;
    bool transition_refinement_active;
    bool adaptive_refinement_active;
    bool adaptive_refinement_converged;
    bool source_mesh_immutable;
    bool exact_source_and_carrier_binding;
    bool source_triangle_mapping_retained;
    bool explicit_region_transition_topology;
    bool replaceable_derived_geometry;
    bool closed_valid_shell;
} ProceduralImportedSurfaceInsetReceipt;

typedef struct ProceduralImportedSurfaceInsetReport {
    bool ok;
    char field[96];
    char message[256];
} ProceduralImportedSurfaceInsetReport;

void ProceduralImportedSurfaceInsetConfig_Init(
    ProceduralImportedSurfaceInsetConfig *config);
void ProceduralImportedSurfaceInsetProvenance_Init(
    ProceduralImportedSurfaceInsetProvenance *provenance);
void ProceduralImportedSurfaceInsetProvenance_Free(
    ProceduralImportedSurfaceInsetProvenance *provenance);

bool ProceduralImportedSurfaceInset_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceInsetConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceInsetProvenance *out_provenance,
    ProceduralImportedSurfaceInsetReceipt *out_receipt,
    ProceduralImportedSurfaceInsetReport *report);

const char *ProceduralImportedSurfaceInsetRole_Name(
    ProceduralImportedSurfaceInsetRole role);

#endif
