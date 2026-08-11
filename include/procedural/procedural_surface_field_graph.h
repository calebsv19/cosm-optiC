#ifndef PROCEDURAL_SURFACE_FIELD_GRAPH_H
#define PROCEDURAL_SURFACE_FIELD_GRAPH_H

#include "procedural/procedural_surface_field_3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA \
    "ray_tracing.procedural_surface_field_graph"
#define PROCEDURAL_SURFACE_FIELD_GRAPH_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES 192u
#define PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_INPUTS 4u
#define PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY 131072u
#define PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY 65u

typedef enum ProceduralSurfaceFieldNodeOp {
    PROCEDURAL_SURFACE_FIELD_NODE_INVALID = 0,
    PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT,
    PROCEDURAL_SURFACE_FIELD_NODE_POSITION_X,
    PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Y,
    PROCEDURAL_SURFACE_FIELD_NODE_POSITION_Z,
    PROCEDURAL_SURFACE_FIELD_NODE_ADD,
    PROCEDURAL_SURFACE_FIELD_NODE_SUBTRACT,
    PROCEDURAL_SURFACE_FIELD_NODE_MULTIPLY,
    PROCEDURAL_SURFACE_FIELD_NODE_DIVIDE,
    PROCEDURAL_SURFACE_FIELD_NODE_MINIMUM,
    PROCEDURAL_SURFACE_FIELD_NODE_MAXIMUM,
    PROCEDURAL_SURFACE_FIELD_NODE_ABSOLUTE,
    PROCEDURAL_SURFACE_FIELD_NODE_NEGATE,
    PROCEDURAL_SURFACE_FIELD_NODE_SINE,
    PROCEDURAL_SURFACE_FIELD_NODE_COSINE,
    PROCEDURAL_SURFACE_FIELD_NODE_CLAMP01,
    PROCEDURAL_SURFACE_FIELD_NODE_SMOOTHSTEP,
    PROCEDURAL_SURFACE_FIELD_NODE_MIX,
    PROCEDURAL_SURFACE_FIELD_NODE_POWER,
    PROCEDURAL_SURFACE_FIELD_NODE_LENGTH2,
    PROCEDURAL_SURFACE_FIELD_NODE_VALUE_NOISE_3D,
    PROCEDURAL_SURFACE_FIELD_NODE_FBM_3D,
    PROCEDURAL_SURFACE_FIELD_NODE_RIDGED_FBM_3D,
    PROCEDURAL_SURFACE_FIELD_NODE_CELLULAR_F1_3D
} ProceduralSurfaceFieldNodeOp;

typedef struct ProceduralSurfaceFieldGraphNode {
    char id[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    ProceduralSurfaceFieldNodeOp op;
    size_t input_count;
    char inputs[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_INPUTS]
               [PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    double value;
    uint64_t seed;
    uint32_t octaves;
    double lacunarity;
    double persistence;
} ProceduralSurfaceFieldGraphNode;

typedef struct ProceduralSurfaceFieldGraphOutputs {
    char height[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char macro[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char micro[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char cavity[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char mask[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char color_r[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char color_g[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char color_b[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    char roughness[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
} ProceduralSurfaceFieldGraphOutputs;

typedef struct ProceduralSurfaceFieldGraphV1 {
    uint32_t schema_version;
    char program_id[PROCEDURAL_SURFACE_FIELD_GRAPH_ID_CAPACITY];
    uint32_t max_node_evaluations;
    size_t node_count;
    ProceduralSurfaceFieldGraphNode
        nodes[PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES];
    ProceduralSurfaceFieldGraphOutputs outputs;
} ProceduralSurfaceFieldGraphV1;

typedef struct ProceduralSurfaceFieldGraphSample {
    double height;
    double macro_variation;
    double micro_variation;
    double cavity;
    double mask;
    double color_r;
    double color_g;
    double color_b;
    double roughness;
} ProceduralSurfaceFieldGraphSample;

typedef enum ProceduralSurfaceFieldGraphStatus {
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OK = 0,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IO,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_JSON,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_NODE,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_INPUT,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CYCLE,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_DISCONNECTED,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_EVALUATION,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_OUTPUT,
    PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CANONICALIZATION
} ProceduralSurfaceFieldGraphStatus;

typedef struct ProceduralSurfaceFieldGraphReport {
    ProceduralSurfaceFieldGraphStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceFieldGraphReport;

void ProceduralSurfaceFieldGraphV1_Init(
    ProceduralSurfaceFieldGraphV1 *graph);

bool ProceduralSurfaceFieldGraphV1_Validate(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldGraphReport *report);

bool ProceduralSurfaceFieldGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceFieldGraphV1 *out_graph,
    ProceduralSurfaceFieldGraphReport *report);

bool ProceduralSurfaceFieldGraphV1_CanonicalJson(
    const ProceduralSurfaceFieldGraphV1 *graph,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceFieldGraphReport *report);

bool ProceduralSurfaceFieldGraphV1_Digest(
    const ProceduralSurfaceFieldGraphV1 *graph,
    char out_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceFieldGraphReport *report);

bool ProceduralSurfaceFieldGraphV1_Evaluate(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceFieldGraphSample *out_sample,
    ProceduralSurfaceFieldGraphReport *report);

bool ProceduralSurfaceFieldGraphV1_EvaluateLegacy(
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *sample_budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report);

const char *ProceduralSurfaceFieldNodeOp_Name(
    ProceduralSurfaceFieldNodeOp op);
const char *ProceduralSurfaceFieldGraphStatus_Name(
    ProceduralSurfaceFieldGraphStatus status);

#endif
