#ifndef PROCEDURAL_IMPORTED_SURFACE_GROWTH_H
#define PROCEDURAL_IMPORTED_SURFACE_GROWTH_H

#include "procedural/procedural_imported_surface_region.h"
#include "procedural/procedural_solid_mesh.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_IMPORTED_SURFACE_GROWTH_SCHEMA \
    "ray_tracing.procedural_imported_surface_growth_receipt"
#define PROCEDURAL_IMPORTED_SURFACE_GROWTH_SCHEMA_VERSION 1u
#define PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY 65u

typedef enum ProceduralImportedSurfaceGrowthRole {
    PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH = 0,
    PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE = 1
} ProceduralImportedSurfaceGrowthRole;

typedef struct ProceduralImportedSurfaceGrowthConfig {
    double selection_threshold;
    double mound_radius_units;
    double mound_height_units;
    double attachment_depth_units;
    double radius_variation;
    double height_variation;
    double clearance_factor;
    double maximum_radius_to_bounds_diagonal_ratio;
    double minimum_triangle_area2;
    size_t max_growth_elements;
    size_t radial_segments;
    size_t latitude_segments;
    size_t max_vertices;
    size_t max_triangles;
} ProceduralImportedSurfaceGrowthConfig;

typedef struct ProceduralImportedSurfaceGrowthExplicitRoot {
    size_t source_triangle_index;
    double barycentric[3];
    CoreObjectVec3 normal;
    CoreObjectVec3 tangent;
    CoreObjectVec3 bitangent;
    double aspect;
    double rotation_radians;
} ProceduralImportedSurfaceGrowthExplicitRoot;

typedef struct ProceduralImportedSurfaceGrowthProvenance {
    size_t triangle_count;
    size_t *source_triangle_indices;
    size_t *growth_element_indices;
    ProceduralImportedSurfaceGrowthRole *roles;
} ProceduralImportedSurfaceGrowthProvenance;

typedef struct ProceduralImportedSurfaceGrowthReceipt {
    uint32_t schema_version;
    char source_asset_id[64];
    char semantic_source_id[64];
    char growth_asset_id[64];
    char region_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char source_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char carrier_value_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char carrier_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char config_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char growth_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    char provenance_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY];
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t candidate_triangle_count;
    size_t growth_element_count;
    size_t rejected_clearance_candidate_count;
    size_t exposed_growth_triangle_count;
    size_t attachment_base_triangle_count;
    size_t growth_vertex_count;
    size_t growth_triangle_count;
    size_t unique_edge_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t connected_component_count;
    size_t inter_element_overlap_pair_count;
    size_t self_intersection_pair_count;
    int euler_characteristic;
    double signed_volume_units3;
    double minimum_attachment_depth_units;
    double maximum_growth_height_units;
    double minimum_inter_element_clearance_units;
    bool source_mesh_immutable;
    bool exact_source_and_carrier_binding;
    bool source_triangle_mapping_retained;
    bool attachment_penetration_verified;
    bool overlap_gate_passed;
    bool self_intersection_gate_passed;
    bool closed_valid_growth_shells;
    bool replaceable_attached_geometry;
} ProceduralImportedSurfaceGrowthReceipt;

typedef struct ProceduralImportedSurfaceGrowthReport {
    bool ok;
    char field[96];
    char message[256];
} ProceduralImportedSurfaceGrowthReport;

void ProceduralImportedSurfaceGrowthConfig_Init(
    ProceduralImportedSurfaceGrowthConfig *config);
void ProceduralImportedSurfaceGrowthProvenance_Init(
    ProceduralImportedSurfaceGrowthProvenance *provenance);
void ProceduralImportedSurfaceGrowthProvenance_Free(
    ProceduralImportedSurfaceGrowthProvenance *provenance);

bool ProceduralImportedSurfaceGrowth_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *out_receipt,
    ProceduralImportedSurfaceGrowthReport *report);

bool ProceduralImportedSurfaceGrowth_CompileExplicitRoot(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const ProceduralImportedSurfaceGrowthExplicitRoot *root,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *out_receipt,
    ProceduralImportedSurfaceGrowthReport *report);

const char *ProceduralImportedSurfaceGrowthRole_Name(
    ProceduralImportedSurfaceGrowthRole role);

#endif
