#ifndef PROCEDURAL_SOLID_MATERIAL_GRAPH_H
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_H

#include "core_mesh_asset.h"
#include "procedural/procedural_solid_authored_material.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA \
    "ray_tracing.procedural_solid_material_composition_graph"
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA_VERSION 1u
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES 64u
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS 8u
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY 64u
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_PATH_CAPACITY 512u
#define PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY 65u

typedef enum ProceduralSolidMaterialNodeKind {
    PROCEDURAL_SOLID_MATERIAL_NODE_CONSTANT = 0,
    PROCEDURAL_SOLID_MATERIAL_NODE_HEIGHT,
    PROCEDURAL_SOLID_MATERIAL_NODE_SLOPE,
    PROCEDURAL_SOLID_MATERIAL_NODE_CURVATURE,
    PROCEDURAL_SOLID_MATERIAL_NODE_CAVITY,
    PROCEDURAL_SOLID_MATERIAL_NODE_REGION,
    PROCEDURAL_SOLID_MATERIAL_NODE_BOUNDARY_DISTANCE,
    PROCEDURAL_SOLID_MATERIAL_NODE_NOISE,
    PROCEDURAL_SOLID_MATERIAL_NODE_ADD,
    PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY,
    PROCEDURAL_SOLID_MATERIAL_NODE_INVERT,
    PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP,
    PROCEDURAL_SOLID_MATERIAL_NODE_BANDS
} ProceduralSolidMaterialNodeKind;

typedef struct ProceduralSolidMaterialNodeV1 {
    char node_id[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
    ProceduralSolidMaterialNodeKind kind;
    char input_a[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
    char input_b[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
    char region_kind[16];
    double value;
    double minimum;
    double maximum;
    double scale;
    double offset;
    int seed;
} ProceduralSolidMaterialNodeV1;

typedef struct ProceduralSolidMaterialLayerV1 {
    char material_id[PROCEDURAL_SOLID_AUTHORED_MATERIAL_ID_CAPACITY];
    char material_path[PROCEDURAL_SOLID_MATERIAL_GRAPH_PATH_CAPACITY];
    char material_digest_sha256[
        PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    char weight_node_id[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
} ProceduralSolidMaterialLayerV1;

typedef struct ProceduralSolidMaterialGraphV1 {
    uint32_t schema_version;
    char graph_id[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
    char authored_binding_id[PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY];
    char authored_binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY];
    size_t node_count;
    ProceduralSolidMaterialNodeV1
        nodes[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES];
    size_t layer_count;
    ProceduralSolidMaterialLayerV1
        layers[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
} ProceduralSolidMaterialGraphV1;

typedef struct ProceduralSolidMaterialGeometryInputs {
    double height;
    double slope;
    double curvature;
    double cavity;
    double boundary_distance;
    double region_retained;
    double region_cut;
    double region_blend;
    double object_x;
    double object_y;
    double object_z;
} ProceduralSolidMaterialGeometryInputs;

typedef enum ProceduralSolidMaterialGraphStatus {
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_OK = 0,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_ARGUMENT,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_IO,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_JSON,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_SCHEMA,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_IDENTITY,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_NODE,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_CYCLE,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_MATERIAL,
    PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_STALE_BASE
} ProceduralSolidMaterialGraphStatus;

typedef struct ProceduralSolidMaterialGraphReport {
    ProceduralSolidMaterialGraphStatus status;
    char field[96];
    char message[256];
    char graph_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY];
} ProceduralSolidMaterialGraphReport;

void ProceduralSolidMaterialGraphV1_Init(ProceduralSolidMaterialGraphV1 *graph);
bool ProceduralSolidMaterialGraphV1_FromTemplate(
    const char *template_id, const char *graph_id,
    const char *authored_binding_id, const char *authored_binding_digest,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_Validate(
    const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_LoadJsonFile(
    const char *path, ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_SaveJsonFileAtomic(
    const char *path, const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_Digest(
    const ProceduralSolidMaterialGraphV1 *graph,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY],
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_SetParameter(
    const ProceduralSolidMaterialGraphV1 *base,
    const char *expected_base_digest, const char *node_id,
    const char *parameter_id, const char *value,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_Connect(
    const ProceduralSolidMaterialGraphV1 *base,
    const char *expected_base_digest, const char *node_id,
    const char *input_id, const char *source_node_id,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_Evaluate(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidMaterialGeometryInputs *inputs,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    ProceduralSolidAuthoredMaterialSurfaceV1 *out_surface,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGraphV1_EvaluateWithReadback(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidMaterialGeometryInputs *inputs,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    ProceduralSolidAuthoredMaterialSurfaceV1 *out_surface,
    double out_layer_weights[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS],
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGeometryInputs_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialGeometryInputs *out_inputs,
    size_t input_count,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialGeometryCornerInputs_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialGeometryInputs *out_inputs,
    size_t input_count,
    ProceduralSolidMaterialGraphReport *report);

const char *ProceduralSolidMaterialNodeKind_Name(
    ProceduralSolidMaterialNodeKind kind);
bool ProceduralSolidMaterialNodeKind_FromName(
    const char *name, ProceduralSolidMaterialNodeKind *out_kind);

#endif
