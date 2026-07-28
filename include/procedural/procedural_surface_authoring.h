#ifndef PROCEDURAL_SURFACE_AUTHORING_H
#define PROCEDURAL_SURFACE_AUTHORING_H

#include "procedural/procedural_surface_field_graph.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA \
    "ray_tracing.procedural_surface_parameter_manifest"
#define PROCEDURAL_SURFACE_PARAMETER_MANIFEST_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT 64u
#define PROCEDURAL_SURFACE_PARAMETER_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_PARAMETER_LABEL_CAPACITY 96u
#define PROCEDURAL_SURFACE_PARAMETER_UNIT_CAPACITY 32u

typedef enum ProceduralSurfaceParameterTarget {
    PROCEDURAL_SURFACE_PARAMETER_TARGET_INVALID = 0,
    PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_VALUE,
    PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_SEED,
    PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_OCTAVES,
    PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_LACUNARITY,
    PROCEDURAL_SURFACE_PARAMETER_TARGET_NODE_PERSISTENCE
} ProceduralSurfaceParameterTarget;

typedef struct ProceduralSurfaceParameter {
    char id[PROCEDURAL_SURFACE_PARAMETER_ID_CAPACITY];
    char label[PROCEDURAL_SURFACE_PARAMETER_LABEL_CAPACITY];
    char unit[PROCEDURAL_SURFACE_PARAMETER_UNIT_CAPACITY];
    char node_id[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    ProceduralSurfaceParameterTarget target;
    double minimum;
    double maximum;
    double default_value;
} ProceduralSurfaceParameter;

typedef struct ProceduralSurfaceParameterManifestV1 {
    uint32_t schema_version;
    char manifest_id[PROCEDURAL_SURFACE_PARAMETER_ID_CAPACITY];
    char graph_program_id[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    size_t parameter_count;
    ProceduralSurfaceParameter
        parameters[PROCEDURAL_SURFACE_PARAMETER_MAX_COUNT];
} ProceduralSurfaceParameterManifestV1;

typedef enum ProceduralSurfaceAuthoringStatus {
    PROCEDURAL_SURFACE_AUTHORING_STATUS_OK = 0,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_IO,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_JSON,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_PARAMETER,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_RANGE,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_TARGET,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_BASE_DIGEST,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_GRAPH,
    PROCEDURAL_SURFACE_AUTHORING_STATUS_SAVE
} ProceduralSurfaceAuthoringStatus;

typedef struct ProceduralSurfaceAuthoringReport {
    ProceduralSurfaceAuthoringStatus status;
    char field[96];
    char message[256];
    char base_graph_digest_sha256[
        PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
    char result_graph_digest_sha256[
        PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
} ProceduralSurfaceAuthoringReport;

void ProceduralSurfaceParameterManifestV1_Init(
    ProceduralSurfaceParameterManifestV1 *manifest);

bool ProceduralSurfaceParameterManifestV1_Validate(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceAuthoringReport *report);

bool ProceduralSurfaceParameterManifestV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceParameterManifestV1 *out_manifest,
    ProceduralSurfaceAuthoringReport *report);

bool ProceduralSurfaceParameterManifestV1_CanonicalJson(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceAuthoringReport *report);

bool ProceduralSurfaceParameterManifestV1_Digest(
    const ProceduralSurfaceParameterManifestV1 *manifest,
    char out_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceAuthoringReport *report);

bool ProceduralSurfaceAuthoring_ApplyParameter(
    const ProceduralSurfaceFieldGraphV1 *base_graph,
    const ProceduralSurfaceParameterManifestV1 *manifest,
    const char *expected_base_digest,
    const char *parameter_id,
    double value,
    ProceduralSurfaceFieldGraphV1 *out_graph,
    ProceduralSurfaceAuthoringReport *report);

bool ProceduralSurfaceAuthoring_SaveGraphAtomic(
    const char *path,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceAuthoringReport *report);

const char *ProceduralSurfaceParameterTarget_Name(
    ProceduralSurfaceParameterTarget target);
const char *ProceduralSurfaceAuthoringStatus_Name(
    ProceduralSurfaceAuthoringStatus status);

#endif
