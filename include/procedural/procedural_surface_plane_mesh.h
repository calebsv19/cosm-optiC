#ifndef PROCEDURAL_SURFACE_PLANE_MESH_H
#define PROCEDURAL_SURFACE_PLANE_MESH_H

#include "procedural/procedural_surface_field_3d.h"
#include "procedural/procedural_surface_recipe.h"
#include "procedural/procedural_surface_topology_contract.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_PLANE_MESH_DIGEST_CAPACITY 65u
#define PROCEDURAL_SURFACE_PLANE_MESH_CANONICAL_CAPACITY 131072u

typedef enum ProceduralSurfacePlaneQuality {
    PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW = 1,
    PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION = 2,
    PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL = 3
} ProceduralSurfacePlaneQuality;

typedef struct ProceduralSurfacePlaneVertex {
    ProceduralSurfaceFieldPoint3D position;
    ProceduralSurfaceFieldPoint3D normal;
    ProceduralSurfaceFieldOutput field;
    double edge_lock_weight;
    double displacement_units;
} ProceduralSurfacePlaneVertex;

typedef struct ProceduralSurfacePlaneTriangle {
    uint32_t a;
    uint32_t b;
    uint32_t c;
} ProceduralSurfacePlaneTriangle;

typedef struct ProceduralSurfacePlaneMesh {
    ProceduralSurfacePlaneVertex *vertices;
    size_t vertex_count;
    ProceduralSurfacePlaneTriangle *triangles;
    size_t triangle_count;
} ProceduralSurfacePlaneMesh;

typedef struct ProceduralSurfacePlaneMeshBuffers {
    ProceduralSurfacePlaneVertex *vertices;
    size_t vertex_capacity;
    ProceduralSurfacePlaneTriangle *triangles;
    size_t triangle_capacity;
    size_t vertex_count;
    size_t triangle_count;
} ProceduralSurfacePlaneMeshBuffers;

typedef struct ProceduralSurfacePlaneMeshRequirements {
    uint64_t subdivisions_x;
    uint64_t subdivisions_y;
    uint64_t vertex_count;
    uint64_t triangle_count;
    uint64_t field_evaluation_count;
    uint64_t selected_triangle_budget;
} ProceduralSurfacePlaneMeshRequirements;

typedef struct ProceduralSurfacePlaneMeshSummary {
    uint64_t subdivisions_x;
    uint64_t subdivisions_y;
    uint64_t vertex_count;
    uint64_t triangle_count;
    uint64_t field_evaluation_count;
    ProceduralSurfaceFieldPoint3D bounds_min;
    ProceduralSurfaceFieldPoint3D bounds_max;
    double maximum_absolute_displacement_units;
    double maximum_boundary_absolute_displacement_units;
    double minimum_twice_triangle_area_units2;
    double total_surface_area_units2;
    double minimum_normal_z;
    char mesh_digest_sha256[PROCEDURAL_SURFACE_PLANE_MESH_DIGEST_CAPACITY];
} ProceduralSurfacePlaneMeshSummary;

typedef enum ProceduralSurfacePlaneMeshStatus {
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_OK = 0,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAGE,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_RECIPE,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_QUALITY,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_ALLOCATION,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_FIELD,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_VERTEX,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_TRIANGLE,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_NORMAL,
    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_SUMMARY
} ProceduralSurfacePlaneMeshStatus;

typedef struct ProceduralSurfacePlaneMeshReport {
    ProceduralSurfacePlaneMeshStatus status;
    char field[64];
    char message[192];
} ProceduralSurfacePlaneMeshReport;

bool ProceduralSurfacePlaneMesh_DeriveRequirements(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfacePlaneMeshRequirements *out_requirements,
    ProceduralSurfacePlaneMeshReport *report);

bool ProceduralSurfacePlaneMesh_Generate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfacePlaneQuality quality,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePlaneMeshBuffers *buffers,
    ProceduralSurfacePlaneMeshSummary *out_summary,
    ProceduralSurfacePlaneMeshReport *report);

bool ProceduralSurfacePlaneMesh_Validate(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfacePlaneMesh *mesh,
    uint64_t field_evaluation_count,
    ProceduralSurfacePlaneMeshSummary *out_summary,
    ProceduralSurfacePlaneMeshReport *report);

const char *ProceduralSurfacePlaneMeshStatus_Name(
    ProceduralSurfacePlaneMeshStatus status);

#endif
