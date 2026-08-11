#include "procedural_solid_field_query.h"

#include "procedural_solid_geometry_internal.h"

bool procedural_solid_field_distance(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    double *out_distance) {
    ProceduralSolidSample sample;
    ProceduralSolidGraphReport report;
    if (!graph || !out_distance ||
        !ProceduralSolidGraphV1_Evaluate(
            graph, sources, point, &sample, &report)) {
        return false;
    }
    *out_distance = sample.signed_distance;
    return true;
}

bool procedural_solid_field_gradient(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    double step,
    CoreObjectVec3 *out_gradient) {
    CoreObjectVec3 p = point;
    CoreObjectVec3 gradient;
    double plus;
    double minus;
    if (!graph || !out_gradient || !isfinite(step) || step <= 0.0) {
        return false;
    }
    p.x += step;
    if (!procedural_solid_field_distance(graph, sources, p, &plus)) {
        return false;
    }
    p.x -= 2.0 * step;
    if (!procedural_solid_field_distance(graph, sources, p, &minus)) {
        return false;
    }
    gradient.x = plus - minus;
    p = point;
    p.y += step;
    if (!procedural_solid_field_distance(graph, sources, p, &plus)) {
        return false;
    }
    p.y -= 2.0 * step;
    if (!procedural_solid_field_distance(graph, sources, p, &minus)) {
        return false;
    }
    gradient.y = plus - minus;
    p = point;
    p.z += step;
    if (!procedural_solid_field_distance(graph, sources, p, &plus)) {
        return false;
    }
    p.z -= 2.0 * step;
    if (!procedural_solid_field_distance(graph, sources, p, &minus)) {
        return false;
    }
    gradient.z = plus - minus;
    return psg_vec_normalize(gradient, out_gradient);
}
