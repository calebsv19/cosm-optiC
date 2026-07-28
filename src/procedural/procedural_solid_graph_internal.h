#ifndef PROCEDURAL_SOLID_GRAPH_INTERNAL_H
#define PROCEDURAL_SOLID_GRAPH_INTERNAL_H

#include "procedural/procedural_solid_graph.h"

struct json_object;

void procedural_solid_graph_report_set(
    ProceduralSolidGraphReport *report,
    ProceduralSolidGraphStatus status,
    const char *field,
    const char *message);

int procedural_solid_graph_find_node(
    const ProceduralSolidGraphV1 *graph,
    const char *id);

uint32_t procedural_solid_graph_expected_inputs(ProceduralSolidNodeOp op);

bool procedural_solid_graph_exact_keys(
    struct json_object *object,
    const char *const *keys,
    size_t key_count);

#endif
