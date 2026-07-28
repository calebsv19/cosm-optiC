#ifndef PROCEDURAL_SOLID_SOURCE_ACCEL_H
#define PROCEDURAL_SOLID_SOURCE_ACCEL_H

#include "core_mesh_asset.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ProceduralSolidSourceAccelNode {
    CoreObjectVec3 bounds_min;
    CoreObjectVec3 bounds_max;
    size_t start;
    size_t count;
    size_t left;
    size_t right;
    bool leaf;
} ProceduralSolidSourceAccelNode;

typedef struct ProceduralSolidSourceAccelTriangle {
    size_t triangle_index;
    CoreObjectVec3 bounds_min;
    CoreObjectVec3 bounds_max;
    CoreObjectVec3 centroid;
} ProceduralSolidSourceAccelTriangle;

typedef struct ProceduralSolidSourceAccel {
    const CoreMeshAssetRuntimeDocument *mesh;
    ProceduralSolidSourceAccelNode *nodes;
    ProceduralSolidSourceAccelTriangle *triangles;
    size_t node_count;
    size_t node_capacity;
    size_t triangle_count;
    size_t leaf_size;
    size_t maximum_depth;
} ProceduralSolidSourceAccel;

typedef struct ProceduralSolidSourceQuery {
    double signed_distance;
    size_t distance_triangle_tests;
    size_t sign_triangle_tests;
    size_t nodes_visited;
} ProceduralSolidSourceQuery;

void ProceduralSolidSourceAccel_Init(ProceduralSolidSourceAccel *accel);
void ProceduralSolidSourceAccel_Free(ProceduralSolidSourceAccel *accel);

bool ProceduralSolidSourceAccel_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    size_t leaf_size,
    ProceduralSolidSourceAccel *out_accel);

bool ProceduralSolidSourceAccel_Query(
    const ProceduralSolidSourceAccel *accel,
    CoreObjectVec3 point,
    ProceduralSolidSourceQuery *out_query);

#endif
