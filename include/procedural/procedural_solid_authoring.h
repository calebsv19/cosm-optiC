#ifndef PROCEDURAL_SOLID_AUTHORING_H
#define PROCEDURAL_SOLID_AUTHORING_H

#include "procedural/procedural_solid_graph.h"

#include <stdbool.h>
#include <stddef.h>

#define PROCEDURAL_SOLID_AUTHORING_MAX_PARAMETERS 512u
#define PROCEDURAL_SOLID_AUTHORING_PARAMETER_ID_CAPACITY 128u
#define PROCEDURAL_SOLID_AUTHORING_LABEL_CAPACITY 96u

typedef enum ProceduralSolidParameterTarget {
    PROCEDURAL_SOLID_PARAMETER_INVALID = 0,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_A_X,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Y,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_A_Z,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_B_X,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Y,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_B_Z,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_C_X,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Y,
    PROCEDURAL_SOLID_PARAMETER_VECTOR_C_Z,
    PROCEDURAL_SOLID_PARAMETER_SCALAR_A,
    PROCEDURAL_SOLID_PARAMETER_SCALAR_B
} ProceduralSolidParameterTarget;

typedef struct ProceduralSolidParameter {
    char id[PROCEDURAL_SOLID_AUTHORING_PARAMETER_ID_CAPACITY];
    char node_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char label[PROCEDURAL_SOLID_AUTHORING_LABEL_CAPACITY];
    char unit[24];
    ProceduralSolidParameterTarget target;
    double value;
    double minimum;
    double maximum;
} ProceduralSolidParameter;

typedef struct ProceduralSolidAuthoringView {
    char graph_digest_sha256[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    size_t node_count;
    size_t connection_count;
    size_t parameter_count;
    ProceduralSolidParameter
        parameters[PROCEDURAL_SOLID_AUTHORING_MAX_PARAMETERS];
} ProceduralSolidAuthoringView;

typedef enum ProceduralSolidAuthoringStatus {
    PROCEDURAL_SOLID_AUTHORING_STATUS_OK = 0,
    PROCEDURAL_SOLID_AUTHORING_STATUS_ARGUMENT,
    PROCEDURAL_SOLID_AUTHORING_STATUS_GRAPH,
    PROCEDURAL_SOLID_AUTHORING_STATUS_PARAMETER,
    PROCEDURAL_SOLID_AUTHORING_STATUS_RANGE,
    PROCEDURAL_SOLID_AUTHORING_STATUS_BASE_DIGEST,
    PROCEDURAL_SOLID_AUTHORING_STATUS_IO
} ProceduralSolidAuthoringStatus;

typedef struct ProceduralSolidAuthoringReport {
    ProceduralSolidAuthoringStatus status;
    char field[128];
    char message[256];
    char base_graph_digest_sha256[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
    char result_graph_digest_sha256[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY];
} ProceduralSolidAuthoringReport;

bool ProceduralSolidAuthoring_Inspect(
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidAuthoringView *out_view,
    ProceduralSolidAuthoringReport *report);

bool ProceduralSolidAuthoring_ApplyParameter(
    const ProceduralSolidGraphV1 *base_graph,
    const char *expected_base_digest,
    const char *parameter_id,
    double value,
    ProceduralSolidGraphV1 *out_graph,
    ProceduralSolidAuthoringReport *report);

bool ProceduralSolidAuthoring_SaveGraphAtomic(
    const char *path,
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidAuthoringReport *report);

const char *ProceduralSolidParameterTarget_Name(
    ProceduralSolidParameterTarget target);
const char *ProceduralSolidAuthoringStatus_Name(
    ProceduralSolidAuthoringStatus status);

#endif
