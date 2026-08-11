#ifndef PROCEDURAL_SURFACE_FIELD_GRAPH_INTERNAL_H
#define PROCEDURAL_SURFACE_FIELD_GRAPH_INTERNAL_H

#include "procedural/procedural_surface_field_graph.h"

#include <stdarg.h>

struct json_object;

void procedural_surface_field_graph_report_set(
    ProceduralSurfaceFieldGraphReport *report,
    ProceduralSurfaceFieldGraphStatus status,
    const char *field,
    const char *message);

bool procedural_surface_field_graph_exact_keys(
    struct json_object *object,
    const char *const *keys,
    size_t key_count);

int procedural_surface_field_graph_find_node(
    const ProceduralSurfaceFieldGraphV1 *graph,
    const char *id);

uint32_t procedural_surface_field_graph_expected_inputs(
    ProceduralSurfaceFieldNodeOp op);

bool procedural_surface_field_graph_noise_value(
    uint64_t seed,
    double x,
    double y,
    double z,
    double *out_value);

bool procedural_surface_field_graph_noise_fbm(
    uint64_t seed,
    double x,
    double y,
    double z,
    double feature_size,
    uint32_t octaves,
    double lacunarity,
    double persistence,
    bool ridged,
    double *out_value);

bool procedural_surface_field_graph_noise_cellular_f1(
    uint64_t seed,
    double x,
    double y,
    double z,
    double feature_size,
    double *out_value);

#endif
