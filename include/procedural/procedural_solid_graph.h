#ifndef PROCEDURAL_SOLID_GRAPH_H
#define PROCEDURAL_SOLID_GRAPH_H

#include "core_mesh_asset.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_GRAPH_SCHEMA "ray_tracing.procedural_solid_graph"
#define PROCEDURAL_SOLID_GRAPH_SCHEMA_VERSION 1u
#define PROCEDURAL_SOLID_GRAPH_ID_CAPACITY 64u
#define PROCEDURAL_SOLID_GRAPH_MAX_NODES 128u
#define PROCEDURAL_SOLID_GRAPH_MAX_INPUTS 2u
#define PROCEDURAL_SOLID_GRAPH_MAX_SOURCES 8u
#define PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY 65u

typedef struct ProceduralSolidSourceAccel ProceduralSolidSourceAccel;

typedef enum ProceduralSolidNodeOp {
    PROCEDURAL_SOLID_NODE_INVALID = 0,
    PROCEDURAL_SOLID_NODE_SPHERE,
    PROCEDURAL_SOLID_NODE_BOX,
    PROCEDURAL_SOLID_NODE_CYLINDER_Z,
    PROCEDURAL_SOLID_NODE_SOURCE_MESH,
    PROCEDURAL_SOLID_NODE_TRANSFORM,
    PROCEDURAL_SOLID_NODE_TWIST_Z,
    PROCEDURAL_SOLID_NODE_TAPER_Z,
    PROCEDURAL_SOLID_NODE_ROUND,
    PROCEDURAL_SOLID_NODE_UNION,
    PROCEDURAL_SOLID_NODE_INTERSECTION,
    PROCEDURAL_SOLID_NODE_DIFFERENCE,
    PROCEDURAL_SOLID_NODE_SMOOTH_UNION
} ProceduralSolidNodeOp;

typedef struct ProceduralSolidGraphNode {
    char id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    ProceduralSolidNodeOp op;
    size_t input_count;
    char inputs[PROCEDURAL_SOLID_GRAPH_MAX_INPUTS]
               [PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char source_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    CoreObjectVec3 vector_a;
    CoreObjectVec3 vector_b;
    CoreObjectVec3 vector_c;
    double scalar_a;
    double scalar_b;
} ProceduralSolidGraphNode;

typedef struct ProceduralSolidGraphV1 {
    uint32_t schema_version;
    char graph_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char semantic_source_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    uint32_t max_node_evaluations;
    size_t node_count;
    ProceduralSolidGraphNode nodes[PROCEDURAL_SOLID_GRAPH_MAX_NODES];
    char output[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
} ProceduralSolidGraphV1;

typedef struct ProceduralSolidSource {
    char source_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    const CoreMeshAssetRuntimeDocument *mesh;
    const ProceduralSolidSourceAccel *accel;
} ProceduralSolidSource;

typedef struct ProceduralSolidSourceSet {
    size_t source_count;
    ProceduralSolidSource sources[PROCEDURAL_SOLID_GRAPH_MAX_SOURCES];
} ProceduralSolidSourceSet;

typedef enum ProceduralSolidRegionKind {
    PROCEDURAL_SOLID_REGION_RETAINED = 0,
    PROCEDURAL_SOLID_REGION_CUT = 1,
    PROCEDURAL_SOLID_REGION_BLEND = 2
} ProceduralSolidRegionKind;

typedef struct ProceduralSolidSample {
    double signed_distance;
    char contributing_node_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char secondary_contributing_node_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    ProceduralSolidRegionKind region_kind;
    double blend_weight;
    uint32_t evaluations_used;
    size_t source_triangle_tests;
    size_t source_query_count;
    size_t accelerated_source_query_count;
} ProceduralSolidSample;

typedef enum ProceduralSolidGraphStatus {
    PROCEDURAL_SOLID_GRAPH_STATUS_OK = 0,
    PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SOLID_GRAPH_STATUS_IO,
    PROCEDURAL_SOLID_GRAPH_STATUS_JSON,
    PROCEDURAL_SOLID_GRAPH_STATUS_SCHEMA,
    PROCEDURAL_SOLID_GRAPH_STATUS_IDENTITY,
    PROCEDURAL_SOLID_GRAPH_STATUS_CAPACITY,
    PROCEDURAL_SOLID_GRAPH_STATUS_NODE,
    PROCEDURAL_SOLID_GRAPH_STATUS_INPUT,
    PROCEDURAL_SOLID_GRAPH_STATUS_CYCLE,
    PROCEDURAL_SOLID_GRAPH_STATUS_DISCONNECTED,
    PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE,
    PROCEDURAL_SOLID_GRAPH_STATUS_BUDGET,
    PROCEDURAL_SOLID_GRAPH_STATUS_EVALUATION,
    PROCEDURAL_SOLID_GRAPH_STATUS_CANONICALIZATION
} ProceduralSolidGraphStatus;

typedef struct ProceduralSolidGraphReport {
    ProceduralSolidGraphStatus status;
    char field[96];
    char message[256];
} ProceduralSolidGraphReport;

void ProceduralSolidGraphV1_Init(ProceduralSolidGraphV1 *graph);

bool ProceduralSolidGraphV1_Validate(
    const ProceduralSolidGraphV1 *graph,
    ProceduralSolidGraphReport *report);

bool ProceduralSolidGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSolidGraphV1 *out_graph,
    ProceduralSolidGraphReport *report);

bool ProceduralSolidGraphV1_CanonicalJson(
    const ProceduralSolidGraphV1 *graph,
    char **out_json,
    size_t *out_length,
    ProceduralSolidGraphReport *report);

bool ProceduralSolidGraphV1_Digest(
    const ProceduralSolidGraphV1 *graph,
    char out_digest[PROCEDURAL_SOLID_GRAPH_DIGEST_CAPACITY],
    ProceduralSolidGraphReport *report);

bool ProceduralSolidGraphV1_Evaluate(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    ProceduralSolidSample *out_sample,
    ProceduralSolidGraphReport *report);

const char *ProceduralSolidNodeOp_Name(ProceduralSolidNodeOp op);
const char *ProceduralSolidGraphStatus_Name(ProceduralSolidGraphStatus status);
const char *ProceduralSolidRegionKind_Name(ProceduralSolidRegionKind kind);

#endif
