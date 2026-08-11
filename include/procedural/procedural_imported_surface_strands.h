#ifndef PROCEDURAL_IMPORTED_SURFACE_STRANDS_H
#define PROCEDURAL_IMPORTED_SURFACE_STRANDS_H

#include "procedural/procedural_imported_surface_region.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_IMPORTED_SURFACE_STRANDS_SCHEMA \
    "ray_tracing.procedural_imported_surface_strands_receipt"
#define PROCEDURAL_IMPORTED_SURFACE_STRANDS_SCHEMA_VERSION 1u
#define PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY 65u

typedef enum ProceduralImportedSurfaceStrandRole {
    PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_ROOT_CAP = 0,
    PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_SHAFT = 1,
    PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_TIP_CAP = 2
} ProceduralImportedSurfaceStrandRole;

typedef struct ProceduralImportedSurfaceStrandConfig {
    double selection_threshold;
    double strand_length_units;
    double root_radius_units;
    double tip_radius_units;
    double root_penetration_units;
    double length_variation;
    double bend_strength;
    double curl_strength;
    double clearance_factor;
    double maximum_length_to_bounds_diagonal_ratio;
    double minimum_triangle_area2;
    size_t max_strands;
    size_t curve_segment_count;
    size_t radial_segments;
    size_t max_vertices;
    size_t max_triangles;
} ProceduralImportedSurfaceStrandConfig;

typedef struct ProceduralImportedSurfaceStrandAsset {
    size_t strand_count;
    size_t points_per_strand;
    CoreObjectVec3 *points;
    double *radii;
    size_t *source_triangle_indices;
    CoreObjectVec3 *root_barycentrics;
    CoreObjectVec3 *root_normals;
    CoreObjectVec3 *root_tangents;
} ProceduralImportedSurfaceStrandAsset;

typedef struct ProceduralImportedSurfaceStrandProvenance {
    size_t triangle_count;
    size_t *source_triangle_indices;
    size_t *strand_indices;
    size_t *segment_indices;
    ProceduralImportedSurfaceStrandRole *roles;
} ProceduralImportedSurfaceStrandProvenance;

typedef struct ProceduralImportedSurfaceStrandReceipt {
    uint32_t schema_version;
    char source_asset_id[64];
    char semantic_source_id[64];
    char strand_asset_id[64];
    char region_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char source_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char carrier_value_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char carrier_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char config_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char strand_data_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char tube_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    char provenance_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY];
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t candidate_triangle_count;
    size_t rejected_clearance_candidate_count;
    size_t strand_count;
    size_t control_point_count;
    size_t root_cap_triangle_count;
    size_t shaft_triangle_count;
    size_t tip_cap_triangle_count;
    size_t tube_vertex_count;
    size_t tube_triangle_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t connected_component_count;
    size_t inter_strand_overlap_pair_count;
    size_t strand_self_intersection_pair_count;
    int euler_characteristic;
    double signed_volume_units3;
    double minimum_root_penetration_units;
    double minimum_root_clearance_units;
    double minimum_strand_length_units;
    double maximum_strand_length_units;
    bool source_mesh_immutable;
    bool exact_source_and_carrier_binding;
    bool root_triangle_mapping_retained;
    bool root_barycentrics_valid;
    bool root_attachment_verified;
    bool finite_continuous_strands;
    bool overlap_gate_passed;
    bool self_intersection_gate_passed;
    bool closed_valid_tube_shells;
    bool replaceable_strand_asset;
    bool triangle_tube_proof_backend;
} ProceduralImportedSurfaceStrandReceipt;

typedef struct ProceduralImportedSurfaceStrandReport {
    bool ok;
    char field[96];
    char message[256];
} ProceduralImportedSurfaceStrandReport;

void ProceduralImportedSurfaceStrandConfig_Init(
    ProceduralImportedSurfaceStrandConfig *config);
void ProceduralImportedSurfaceStrandAsset_Init(
    ProceduralImportedSurfaceStrandAsset *asset);
void ProceduralImportedSurfaceStrandAsset_Free(
    ProceduralImportedSurfaceStrandAsset *asset);
void ProceduralImportedSurfaceStrandProvenance_Init(
    ProceduralImportedSurfaceStrandProvenance *provenance);
void ProceduralImportedSurfaceStrandProvenance_Free(
    ProceduralImportedSurfaceStrandProvenance *provenance);

bool ProceduralImportedSurfaceStrands_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceStrandConfig *config,
    const char *strand_asset_id,
    ProceduralImportedSurfaceStrandAsset *out_strands,
    CoreMeshAssetRuntimeDocument *out_tube_document,
    ProceduralImportedSurfaceStrandProvenance *out_provenance,
    ProceduralImportedSurfaceStrandReceipt *out_receipt,
    ProceduralImportedSurfaceStrandReport *report);

const char *ProceduralImportedSurfaceStrandRole_Name(
    ProceduralImportedSurfaceStrandRole role);

#endif
