#ifndef PROCEDURAL_SURFACE_PRISM_MESH_H
#define PROCEDURAL_SURFACE_PRISM_MESH_H

#include "procedural/procedural_surface_field_3d.h"
#include "procedural/procedural_surface_plane_mesh.h"
#include "procedural/procedural_surface_recipe.h"
#include "procedural/procedural_surface_topology_contract.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_PRISM_MESH_DIGEST_CAPACITY 65u

typedef enum ProceduralSurfacePrismFace {
    PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_X = 0,
    PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_X = 1,
    PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Y = 2,
    PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Y = 3,
    PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_Z = 4,
    PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_Z = 5,
    PROCEDURAL_SURFACE_PRISM_FACE_COUNT = 6
} ProceduralSurfacePrismFace;

typedef struct ProceduralSurfacePrismVertex {
    ProceduralSurfaceFieldPoint3D cage_position;
    ProceduralSurfaceFieldPoint3D position;
    ProceduralSurfaceFieldPoint3D normal;
    ProceduralSurfaceFieldPoint3D displacement_direction;
    ProceduralSurfaceFieldOutput field;
    double edge_lock_weight;
    double displacement_units;
    uint32_t lattice_x;
    uint32_t lattice_y;
    uint32_t lattice_z;
} ProceduralSurfacePrismVertex;

typedef struct ProceduralSurfacePrismTriangle {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    ProceduralSurfacePrismFace surface_group;
} ProceduralSurfacePrismTriangle;

typedef struct ProceduralSurfacePrismMesh {
    ProceduralSurfacePrismVertex *vertices;
    size_t vertex_count;
    ProceduralSurfacePrismTriangle *triangles;
    size_t triangle_count;
} ProceduralSurfacePrismMesh;

typedef struct ProceduralSurfacePrismMeshBuffers {
    ProceduralSurfacePrismVertex *vertices;
    size_t vertex_capacity;
    ProceduralSurfacePrismTriangle *triangles;
    size_t triangle_capacity;
    size_t vertex_count;
    size_t triangle_count;
} ProceduralSurfacePrismMeshBuffers;

typedef struct ProceduralSurfacePrismMeshRequirements {
    uint64_t subdivisions_x;
    uint64_t subdivisions_y;
    uint64_t subdivisions_z;
    uint64_t vertex_count;
    uint64_t triangle_count;
    uint64_t field_evaluation_count;
    uint64_t selected_triangle_budget;
} ProceduralSurfacePrismMeshRequirements;

typedef struct ProceduralSurfacePrismMeshSummary {
    uint64_t subdivisions_x;
    uint64_t subdivisions_y;
    uint64_t subdivisions_z;
    uint64_t vertex_count;
    uint64_t triangle_count;
    uint64_t unique_edge_count;
    uint64_t boundary_edge_count;
    uint64_t field_evaluation_count;
    uint32_t connected_component_count;
    uint32_t surface_group_count;
    int32_t euler_characteristic;
    ProceduralSurfaceFieldPoint3D bounds_min;
    ProceduralSurfaceFieldPoint3D bounds_max;
    double maximum_absolute_displacement_units;
    double maximum_edge_absolute_displacement_units;
    double minimum_twice_triangle_area_units2;
    double total_surface_area_units2;
    double signed_volume_units3;
    double minimum_outward_winding_dot;
    char mesh_digest_sha256[PROCEDURAL_SURFACE_PRISM_MESH_DIGEST_CAPACITY];
} ProceduralSurfacePrismMeshSummary;

typedef enum ProceduralSurfacePrismMeshStatus {
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_OK = 0,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_CAGE,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_RECIPE,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_QUALITY,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_ALLOCATION,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_FIELD,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TRIANGLE,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NORMAL,
    PROCEDURAL_SURFACE_PRISM_MESH_STATUS_SUMMARY
} ProceduralSurfacePrismMeshStatus;

typedef struct ProceduralSurfacePrismMeshReport {
    ProceduralSurfacePrismMeshStatus status;
    char field[64];
    char message[192];
} ProceduralSurfacePrismMeshReport;

typedef bool (*ProceduralSurfacePrismFieldEvaluator)(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report);

typedef bool (*ProceduralSurfacePrismDisplacementDirectionResolver)(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D *out_direction);

bool ProceduralSurfacePrismMesh_DeriveRequirements(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshReport *report);

bool ProceduralSurfacePrismMesh_Generate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report);

bool ProceduralSurfacePrismMesh_GenerateWithEvaluator(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report);

bool ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfacePrismDisplacementDirectionResolver direction_resolver,
    const void *direction_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report);

bool ProceduralSurfacePrismMesh_Validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePrismMesh *mesh,
    uint64_t field_evaluation_count,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfacePrismMeshReport *report);

const char *ProceduralSurfacePrismFace_Name(ProceduralSurfacePrismFace face);
const char *ProceduralSurfacePrismMeshStatus_Name(
    ProceduralSurfacePrismMeshStatus status);

#endif
