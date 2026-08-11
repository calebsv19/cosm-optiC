#ifndef PROCEDURAL_SOLID_MESH_H
#define PROCEDURAL_SOLID_MESH_H

#include "procedural/procedural_solid_graph.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY 65u

typedef enum ProceduralSolidCollisionAuthority {
    PROCEDURAL_SOLID_COLLISION_AUTHORITY_SEMANTIC_SOURCE = 0,
    PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL = 1
} ProceduralSolidCollisionAuthority;

typedef struct ProceduralSolidMeshConfig {
    CoreObjectVec3 bounds_min;
    CoreObjectVec3 bounds_max;
    uint32_t cells_x;
    uint32_t cells_y;
    uint32_t cells_z;
    size_t max_samples;
    size_t max_vertices;
    size_t max_triangles;
    size_t min_components;
    size_t max_components;
    double gradient_step_units;
    double minimum_triangle_area2;
    bool require_closed_manifold;
    bool require_positive_volume;
    ProceduralSolidCollisionAuthority collision_authority;
    const uint8_t *active_cell_mask;
    size_t active_cell_mask_count;
} ProceduralSolidMeshConfig;

typedef struct ProceduralSolidMeshSummary {
    uint32_t samples_x;
    uint32_t samples_y;
    uint32_t samples_z;
    size_t sample_count;
    size_t evaluated_sample_count;
    size_t inside_sample_count;
    size_t total_cell_count;
    size_t active_cell_count;
    size_t source_query_count;
    size_t accelerated_source_query_count;
    size_t source_triangle_tests;
    size_t vertex_count;
    size_t triangle_count;
    size_t unique_edge_count;
    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t connected_component_count;
    int euler_characteristic;
    double signed_volume_units3;
    double minimum_triangle_area2;
    double minimum_edge_length_units;
    double maximum_edge_length_units;
    double maximum_cell_size_units;
    double thin_feature_floor_units;
    double boundary_min_signed_distance;
    double boundary_max_signed_distance;
    bool conforming_cell_self_intersection_free;
    CoreObjectVec3 bounds_min;
    CoreObjectVec3 bounds_max;
    ProceduralSolidCollisionAuthority collision_authority;
    char graph_digest_sha256[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char mesh_digest_sha256[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
} ProceduralSolidMeshSummary;

typedef enum ProceduralSolidMeshStatus {
    PROCEDURAL_SOLID_MESH_STATUS_OK = 0,
    PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
    PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
    PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
    PROCEDURAL_SOLID_MESH_STATUS_FIELD,
    PROCEDURAL_SOLID_MESH_STATUS_DOMAIN_CLIPPED,
    PROCEDURAL_SOLID_MESH_STATUS_EMPTY,
    PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
    PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
    PROCEDURAL_SOLID_MESH_STATUS_COMPONENT_POLICY,
    PROCEDURAL_SOLID_MESH_STATUS_CORE_MESH,
    PROCEDURAL_SOLID_MESH_STATUS_IDENTITY
} ProceduralSolidMeshStatus;

typedef struct ProceduralSolidMeshReport {
    ProceduralSolidMeshStatus status;
    char field[96];
    char message[256];
} ProceduralSolidMeshReport;

void ProceduralSolidMeshConfig_Init(ProceduralSolidMeshConfig *config);

bool ProceduralSolidMesh_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidMeshSummary *out_summary,
    ProceduralSolidMeshReport *report);

bool ProceduralSolidMesh_Reanalyze(
    const ProceduralSolidMeshConfig *config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report);

bool ProceduralSolidMesh_RefreshIdentity(
    const CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report);

bool ProceduralSolidMesh_Digest(
    const CoreMeshAssetRuntimeDocument *document,
    char out_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY]);

const char *ProceduralSolidMeshStatus_Name(ProceduralSolidMeshStatus status);
const char *ProceduralSolidCollisionAuthority_Name(
    ProceduralSolidCollisionAuthority authority);

#endif
