#ifndef PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_H
#define PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_H

#include "procedural/procedural_surface_feature_field.h"
#include "procedural/procedural_surface_selected_face_shell.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_SCHEMA \
    "ray_tracing.procedural_surface_feature_relief_shell_receipt"
#define PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_SCHEMA_VERSION 1u

typedef struct ProceduralSurfaceFeatureReliefShellRequest {
    ProceduralSurfaceSelectedFaceShellRequest selected_face_shell;
    const ProceduralSurfaceFeatureFieldV1 *feature_field;
    const char *expected_source_mesh_digest_sha256;
    double relief_scale;
} ProceduralSurfaceFeatureReliefShellRequest;

typedef struct ProceduralSurfaceFeatureReliefShellReceipt {
    uint32_t schema_version;
    ProceduralSurfaceSelectedFaceShellReceipt selected_face_shell;
    char source_mesh_digest_sha256[
        PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY];
    char feature_field_digest_sha256[
        PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY];
    uint64_t feature_count;
    uint64_t zero_height_feature_count;
    uint64_t negative_depth_feature_count;
    uint64_t positive_height_feature_count;
    uint64_t negatively_displaced_vertex_count;
    uint64_t positively_displaced_vertex_count;
    uint64_t maximum_candidates_considered_per_vertex;
    double minimum_authored_height_or_depth_units;
    double maximum_authored_height_or_depth_units;
    double minimum_emitted_displacement_units;
    double maximum_emitted_displacement_units;
    double relief_scale;
    bool feature_source_identity_bound;
    bool one_coherent_derived_shell;
} ProceduralSurfaceFeatureReliefShellReceipt;

typedef enum ProceduralSurfaceFeatureReliefShellStatus {
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_OK = 0,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_FIELD,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_SOURCE_IDENTITY,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RANGE,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_BINDING,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_GENERATION,
    PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RECEIPT
} ProceduralSurfaceFeatureReliefShellStatus;

typedef struct ProceduralSurfaceFeatureReliefShellReport {
    ProceduralSurfaceFeatureReliefShellStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceFeatureReliefShellReport;

bool ProceduralSurfaceFeatureReliefShell_Compile(
    const ProceduralSurfaceFeatureReliefShellRequest *request,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceFeatureReliefShellReceipt *out_receipt,
    ProceduralSurfaceFeatureReliefShellReport *report);

const char *ProceduralSurfaceFeatureReliefShellStatus_Name(
    ProceduralSurfaceFeatureReliefShellStatus status);

#endif
