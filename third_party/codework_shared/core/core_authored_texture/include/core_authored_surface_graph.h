#ifndef CORE_AUTHORED_SURFACE_GRAPH_H
#define CORE_AUTHORED_SURFACE_GRAPH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CORE_SURFACE_GRAPH_MAX_NODES 32
/* Pure, bounded program. Inputs refer to node indices; storage order is arbitrary. */
typedef enum {
    CORE_SG_SCALAR,
    CORE_SG_COLOR,
    CORE_SG_COORDINATE,
    CORE_SG_NOISE3D,
    CORE_SG_TRIPLANAR,
    CORE_SG_MULTIPLY,
    CORE_SG_MIX,
    CORE_SG_IMAGE_COLOR,
    CORE_SG_IMAGE_SCALAR,
    CORE_SG_ROUGHNESS_MIX
} CoreSurfaceGraphKind;
typedef enum {
    CORE_SG_RESOURCE_BASE_COLOR,
    CORE_SG_RESOURCE_ROUGHNESS,
    CORE_SG_RESOURCE_HEIGHT,
    CORE_SG_RESOURCE_BASE_COLOR_ALPHA
} CoreSurfaceGraphResource;
typedef struct {
    CoreSurfaceGraphKind kind;
    int inputs[3];
    double value[3], scale_m, sharpness;
    uint32_t seed;
    bool world_space;
    CoreSurfaceGraphResource resource;
} CoreSurfaceGraphNode;
typedef struct {
    size_t count;
    CoreSurfaceGraphNode nodes[CORE_SURFACE_GRAPH_MAX_NODES];
    int color_output, roughness_output;
    unsigned char order[CORE_SURFACE_GRAPH_MAX_NODES];
    bool valid;
} CoreSurfaceGraph;
/* Host supplies meter-space rest/world positions, normals and pixel derivatives. */
typedef struct {
    double position[2][3], normal[2][3], dx[2][3], dy[2][3];
    bool unbounded;
} CoreSurfaceGraphQuery;
typedef struct {
    double color[3], roughness;
    bool has_roughness;
} CoreSurfaceGraphResult;
/* Host samples immutable resources; every image node requires a matching typed slot. */
typedef struct {
    CoreSurfaceGraphKind kind;
    bool valid;
    double value[3];
} CoreSurfaceGraphInput;
typedef struct {
    CoreSurfaceGraphInput nodes[CORE_SURFACE_GRAPH_MAX_NODES];
} CoreSurfaceGraphInputs;
bool core_surface_graph_evaluate_with_inputs(const CoreSurfaceGraph *graph,
                                             const CoreSurfaceGraphQuery *query,
                                             const CoreSurfaceGraphInputs *inputs,
                                             CoreSurfaceGraphResult *result);
bool core_surface_graph_prepare(CoreSurfaceGraph *graph, char *diagnostic, size_t size);
bool core_surface_graph_evaluate(const CoreSurfaceGraph *graph, const CoreSurfaceGraphQuery *query,
                                 CoreSurfaceGraphResult *result);
#endif
