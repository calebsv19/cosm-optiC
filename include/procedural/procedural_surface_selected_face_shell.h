#ifndef PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_H
#define PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_H

#include "procedural/procedural_surface_field_graph.h"
#include "procedural/procedural_surface_prism_binding.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_SCHEMA \
    "ray_tracing.procedural_surface_selected_face_shell_receipt"
#define PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_SELECTED_FACE_ASSET_ID_CAPACITY 128u

typedef struct ProceduralSurfaceSelectedFaceShellRequest {
    const char *source_asset_id;
    const char *derived_asset_id;
    ProceduralSurfacePrismFace selected_face;
    const ProceduralSurfaceCageContract *cage;
    const ProceduralSurfaceRecipeV1 *recipe;
    const ProceduralSurfaceFieldGraphV1 *graph;
    const ProceduralSurfaceBindingV1 *binding;
    ProceduralSurfacePlaneQuality quality;
} ProceduralSurfaceSelectedFaceShellRequest;

typedef struct ProceduralSurfaceSelectedFaceShellReceipt {
    uint32_t schema_version;
    char source_asset_id[PROCEDURAL_SURFACE_SELECTED_FACE_ASSET_ID_CAPACITY];
    char derived_asset_id[PROCEDURAL_SURFACE_SELECTED_FACE_ASSET_ID_CAPACITY];
    ProceduralSurfacePrismFace selected_face;
    uint64_t source_triangle_count;
    uint64_t source_selected_face_triangle_count;
    uint64_t derived_selected_face_triangle_count;
    uint64_t closure_support_triangle_count;
    uint64_t derived_vertex_count;
    uint64_t derived_triangle_count;
    double maximum_selected_face_absolute_displacement_units;
    double maximum_unselected_face_absolute_displacement_units;
    bool geometry_displacement_active;
    bool source_semantic_identity_retained;
    bool replaceable_derived_geometry;
    char recipe_digest_sha256[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    char field_graph_digest_sha256[
        PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char binding_digest_sha256[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY];
    char mesh_digest_sha256[PROCEDURAL_SURFACE_PRISM_MESH_DIGEST_CAPACITY];
} ProceduralSurfaceSelectedFaceShellReceipt;

typedef enum ProceduralSurfaceSelectedFaceShellStatus {
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_OK = 0,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_SELECTION,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_BINDING,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_REQUIREMENTS,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_GENERATION,
    PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_RECEIPT
} ProceduralSurfaceSelectedFaceShellStatus;

typedef struct ProceduralSurfaceSelectedFaceShellReport {
    ProceduralSurfaceSelectedFaceShellStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceSelectedFaceShellReport;

bool ProceduralSurfaceSelectedFaceShell_Compile(
    const ProceduralSurfaceSelectedFaceShellRequest *request,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceSelectedFaceShellReceipt *out_receipt,
    ProceduralSurfaceSelectedFaceShellReport *report);

bool ProceduralSurfaceSelectedFaceShell_CompileWithEvaluator(
    const ProceduralSurfaceSelectedFaceShellRequest *request,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfacePrismDisplacementDirectionResolver direction_resolver,
    const void *direction_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceSelectedFaceShellReceipt *out_receipt,
    ProceduralSurfaceSelectedFaceShellReport *report);

bool ProceduralSurfacePrismFace_Parse(
    const char *name,
    ProceduralSurfacePrismFace *out_face);

const char *ProceduralSurfaceSelectedFaceShellStatus_Name(
    ProceduralSurfaceSelectedFaceShellStatus status);

#endif
