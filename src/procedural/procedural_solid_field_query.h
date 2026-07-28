#ifndef PROCEDURAL_SOLID_FIELD_QUERY_H
#define PROCEDURAL_SOLID_FIELD_QUERY_H

#include "procedural/procedural_solid_graph.h"

bool procedural_solid_field_distance(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    double *out_distance);

bool procedural_solid_field_gradient(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    double step,
    CoreObjectVec3 *out_gradient);

#endif
