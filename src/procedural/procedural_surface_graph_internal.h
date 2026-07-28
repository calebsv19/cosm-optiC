#ifndef PROCEDURAL_SURFACE_GRAPH_INTERNAL_H
#define PROCEDURAL_SURFACE_GRAPH_INTERNAL_H

#include "procedural/procedural_surface_graph.h"

typedef struct ProceduralSurfaceGraphRecipeInputSpec {
    const char *socket;
    ProceduralSurfaceGraphValueType type;
} ProceduralSurfaceGraphRecipeInputSpec;

extern const ProceduralSurfaceGraphRecipeInputSpec
    g_procedural_surface_graph_recipe_inputs[];
extern const size_t g_procedural_surface_graph_recipe_input_count;

bool procedural_surface_graph_fail(
    ProceduralSurfaceGraphReport *report,
    ProceduralSurfaceGraphStatus status,
    const char *field,
    const char *message);
bool procedural_surface_graph_id_valid(const char *value);
int procedural_surface_graph_find_node(
    const ProceduralSurfaceGraphV1 *graph,
    const char *id);
const ProceduralSurfaceGraphRecipeInputSpec *
procedural_surface_graph_recipe_input(const char *socket);
bool procedural_surface_graph_output_type(
    const ProceduralSurfaceGraphNode *node,
    const char *socket,
    ProceduralSurfaceGraphValueType *out_type);
bool procedural_surface_graph_input_type(
    const ProceduralSurfaceGraphNode *node,
    const char *socket,
    ProceduralSurfaceGraphValueType *out_type);

#endif
