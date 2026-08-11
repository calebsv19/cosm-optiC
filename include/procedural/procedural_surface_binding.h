#ifndef PROCEDURAL_SURFACE_BINDING_H
#define PROCEDURAL_SURFACE_BINDING_H

#include "procedural/procedural_surface_field_graph.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_BINDING_SCHEMA \
    "ray_tracing.procedural_surface_binding"
#define PROCEDURAL_SURFACE_BINDING_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_BINDING_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_BINDING_GROUP_CAPACITY 64u
#define PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY 65u

typedef enum ProceduralSurfaceSelectorKind {
    PROCEDURAL_SURFACE_SELECTOR_INVALID = 0,
    PROCEDURAL_SURFACE_SELECTOR_ALL,
    PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP,
    PROCEDURAL_SURFACE_SELECTOR_UPWARD_FACING
} ProceduralSurfaceSelectorKind;

typedef enum ProceduralSurfaceProjectionKind {
    PROCEDURAL_SURFACE_PROJECTION_INVALID = 0,
    PROCEDURAL_SURFACE_PROJECTION_OBJECT_3D,
    PROCEDURAL_SURFACE_PROJECTION_PLANAR_XY,
    PROCEDURAL_SURFACE_PROJECTION_PLANAR_XZ,
    PROCEDURAL_SURFACE_PROJECTION_PLANAR_YZ
} ProceduralSurfaceProjectionKind;

typedef enum ProceduralSurfaceDisplacementDirection {
    PROCEDURAL_SURFACE_DISPLACEMENT_INVALID = 0,
    PROCEDURAL_SURFACE_DISPLACEMENT_SOURCE_NORMAL,
    PROCEDURAL_SURFACE_DISPLACEMENT_SMOOTH_NORMAL,
    PROCEDURAL_SURFACE_DISPLACEMENT_WORLD_UP
} ProceduralSurfaceDisplacementDirection;

typedef struct ProceduralSurfaceBindingV1 {
    uint32_t schema_version;
    char binding_id[PROCEDURAL_SURFACE_BINDING_ID_CAPACITY];
    char graph_program_id[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    ProceduralSurfaceSelectorKind selector;
    char surface_group_id[PROCEDURAL_SURFACE_BINDING_GROUP_CAPACITY];
    ProceduralSurfaceFieldPoint3D up_axis;
    double selector_min_dot;
    double selector_feather;
    ProceduralSurfaceProjectionKind projection;
    double projection_scale;
    ProceduralSurfaceDisplacementDirection displacement_direction;
    double displacement_scale;
    double fallback_color_r;
    double fallback_color_g;
    double fallback_color_b;
    double fallback_roughness;
} ProceduralSurfaceBindingV1;

typedef struct ProceduralSurfaceBoundSample {
    ProceduralSurfaceFieldGraphSample graph_sample;
    ProceduralSurfaceFieldOutput legacy_field;
    ProceduralSurfaceFieldPoint3D evaluation_point;
    double application_weight;
} ProceduralSurfaceBoundSample;

typedef enum ProceduralSurfaceBindingStatus {
    PROCEDURAL_SURFACE_BINDING_STATUS_OK = 0,
    PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_BINDING_STATUS_IO,
    PROCEDURAL_SURFACE_BINDING_STATUS_JSON,
    PROCEDURAL_SURFACE_BINDING_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_BINDING_STATUS_SELECTOR,
    PROCEDURAL_SURFACE_BINDING_STATUS_PROJECTION,
    PROCEDURAL_SURFACE_BINDING_STATUS_GRAPH,
    PROCEDURAL_SURFACE_BINDING_STATUS_EVALUATION,
    PROCEDURAL_SURFACE_BINDING_STATUS_CANONICALIZATION
} ProceduralSurfaceBindingStatus;

typedef struct ProceduralSurfaceBindingReport {
    ProceduralSurfaceBindingStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceBindingReport;

void ProceduralSurfaceBindingV1_Init(ProceduralSurfaceBindingV1 *binding);

bool ProceduralSurfaceBindingV1_Validate(
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfaceBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceBindingV1 *out_binding,
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfaceBindingV1_CanonicalJson(
    const ProceduralSurfaceBindingV1 *binding,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfaceBindingV1_Digest(
    const ProceduralSurfaceBindingV1 *binding,
    char out_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY],
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfaceBinding_Evaluate(
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D source_position,
    ProceduralSurfaceFieldPoint3D source_normal,
    const char *surface_group_id,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceBoundSample *out_sample,
    ProceduralSurfaceBindingReport *report);

ProceduralSurfaceFieldPoint3D ProceduralSurfaceBinding_DisplacementDirection(
    const ProceduralSurfaceBindingV1 *binding,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D smooth_normal);

const char *ProceduralSurfaceSelectorKind_Name(
    ProceduralSurfaceSelectorKind selector);
const char *ProceduralSurfaceProjectionKind_Name(
    ProceduralSurfaceProjectionKind projection);
const char *ProceduralSurfaceDisplacementDirection_Name(
    ProceduralSurfaceDisplacementDirection direction);
const char *ProceduralSurfaceBindingStatus_Name(
    ProceduralSurfaceBindingStatus status);

#endif
