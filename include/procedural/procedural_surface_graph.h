#ifndef PROCEDURAL_SURFACE_GRAPH_H
#define PROCEDURAL_SURFACE_GRAPH_H

#include "procedural/procedural_surface_recipe.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_GRAPH_SCHEMA "ray_tracing.procedural_surface_graph"
#define PROCEDURAL_SURFACE_GRAPH_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_GRAPH_SOCKET_CAPACITY 64u
#define PROCEDURAL_SURFACE_GRAPH_MAX_NODES 64u
#define PROCEDURAL_SURFACE_GRAPH_MAX_LINKS 128u
#define PROCEDURAL_SURFACE_GRAPH_CANONICAL_CAPACITY 32768u
#define PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY 65u

typedef enum ProceduralSurfaceGraphValueType {
    PROCEDURAL_SURFACE_GRAPH_VALUE_INVALID = 0,
    PROCEDURAL_SURFACE_GRAPH_VALUE_F64,
    PROCEDURAL_SURFACE_GRAPH_VALUE_U32,
    PROCEDURAL_SURFACE_GRAPH_VALUE_U64,
    PROCEDURAL_SURFACE_GRAPH_VALUE_STRING,
    PROCEDURAL_SURFACE_GRAPH_VALUE_COORDINATE_SPACE,
    PROCEDURAL_SURFACE_GRAPH_VALUE_OUTPUT_CLAMP
} ProceduralSurfaceGraphValueType;

typedef enum ProceduralSurfaceGraphNodeKind {
    PROCEDURAL_SURFACE_GRAPH_NODE_INVALID = 0,
    PROCEDURAL_SURFACE_GRAPH_NODE_CONSTANT,
    PROCEDURAL_SURFACE_GRAPH_NODE_F64_ADD,
    PROCEDURAL_SURFACE_GRAPH_NODE_RECIPE_OUTPUT
} ProceduralSurfaceGraphNodeKind;

typedef enum ProceduralSurfaceGraphOutputDomain {
    PROCEDURAL_SURFACE_GRAPH_DOMAIN_FIELD_IR = 1u << 0,
    PROCEDURAL_SURFACE_GRAPH_DOMAIN_GEOMETRY = 1u << 1,
    PROCEDURAL_SURFACE_GRAPH_DOMAIN_MATERIAL = 1u << 2
} ProceduralSurfaceGraphOutputDomain;

typedef struct ProceduralSurfaceGraphValue {
    ProceduralSurfaceGraphValueType type;
    double f64;
    uint32_t u32;
    uint64_t u64;
    char string_value[PROCEDURAL_SURFACE_RECIPE_ID_CAPACITY];
} ProceduralSurfaceGraphValue;

typedef struct ProceduralSurfaceGraphNode {
    char id[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    ProceduralSurfaceGraphNodeKind kind;
    ProceduralSurfaceGraphValue constant;
    uint32_t output_domains;
} ProceduralSurfaceGraphNode;

typedef struct ProceduralSurfaceGraphLink {
    char from_node[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    char from_socket[PROCEDURAL_SURFACE_GRAPH_SOCKET_CAPACITY];
    char to_node[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    char to_socket[PROCEDURAL_SURFACE_GRAPH_SOCKET_CAPACITY];
} ProceduralSurfaceGraphLink;

typedef struct ProceduralSurfaceGraphV1 {
    uint32_t schema_version;
    char graph_id[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    uint32_t max_node_evaluations;
    size_t node_count;
    ProceduralSurfaceGraphNode nodes[PROCEDURAL_SURFACE_GRAPH_MAX_NODES];
    size_t link_count;
    ProceduralSurfaceGraphLink links[PROCEDURAL_SURFACE_GRAPH_MAX_LINKS];
} ProceduralSurfaceGraphV1;

typedef enum ProceduralSurfaceGraphStatus {
    PROCEDURAL_SURFACE_GRAPH_STATUS_OK = 0,
    PROCEDURAL_SURFACE_GRAPH_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_GRAPH_STATUS_IO,
    PROCEDURAL_SURFACE_GRAPH_STATUS_JSON,
    PROCEDURAL_SURFACE_GRAPH_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_GRAPH_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_GRAPH_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_GRAPH_STATUS_NODE,
    PROCEDURAL_SURFACE_GRAPH_STATUS_SOCKET,
    PROCEDURAL_SURFACE_GRAPH_STATUS_TYPE,
    PROCEDURAL_SURFACE_GRAPH_STATUS_LINK,
    PROCEDURAL_SURFACE_GRAPH_STATUS_CYCLE,
    PROCEDURAL_SURFACE_GRAPH_STATUS_DISCONNECTED,
    PROCEDURAL_SURFACE_GRAPH_STATUS_BUDGET,
    PROCEDURAL_SURFACE_GRAPH_STATUS_RECIPE,
    PROCEDURAL_SURFACE_GRAPH_STATUS_CANONICALIZATION
} ProceduralSurfaceGraphStatus;

typedef struct ProceduralSurfaceGraphReport {
    ProceduralSurfaceGraphStatus status;
    char field[64];
    char message[192];
} ProceduralSurfaceGraphReport;

typedef struct ProceduralSurfaceGraphCompilePlan {
    bool valid;
    char graph_id[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    char output_node_id[PROCEDURAL_SURFACE_GRAPH_ID_CAPACITY];
    char graph_digest_sha256[PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY];
    char recipe_digest_sha256[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY];
    uint32_t node_count;
    uint32_t link_count;
    uint32_t evaluated_node_count;
    uint32_t max_node_evaluations;
    bool field_ir_output;
    bool geometry_output;
    bool material_output;
} ProceduralSurfaceGraphCompilePlan;

void ProceduralSurfaceGraphV1_Init(ProceduralSurfaceGraphV1 *graph);

bool ProceduralSurfaceGraphV1_Validate(
    const ProceduralSurfaceGraphV1 *graph,
    ProceduralSurfaceGraphReport *report);
bool ProceduralSurfaceGraphV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceGraphV1 *out_graph,
    ProceduralSurfaceGraphReport *report);
bool ProceduralSurfaceGraphV1_CanonicalJson(
    const ProceduralSurfaceGraphV1 *graph,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceGraphReport *report);
bool ProceduralSurfaceGraphV1_Digest(
    const ProceduralSurfaceGraphV1 *graph,
    char out_digest[PROCEDURAL_SURFACE_GRAPH_DIGEST_CAPACITY],
    ProceduralSurfaceGraphReport *report);
bool ProceduralSurfaceGraphV1_CompileRecipe(
    const ProceduralSurfaceGraphV1 *graph,
    ProceduralSurfaceRecipeV1 *out_recipe,
    ProceduralSurfaceGraphCompilePlan *out_plan,
    ProceduralSurfaceGraphReport *report);
bool ProceduralSurfaceGraphCompilePlan_CanonicalJson(
    const ProceduralSurfaceGraphCompilePlan *plan,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceGraphReport *report);

const char *ProceduralSurfaceGraphValueType_Name(
    ProceduralSurfaceGraphValueType type);
const char *ProceduralSurfaceGraphNodeKind_Name(
    ProceduralSurfaceGraphNodeKind kind);
const char *ProceduralSurfaceGraphStatus_Name(
    ProceduralSurfaceGraphStatus status);

#endif
