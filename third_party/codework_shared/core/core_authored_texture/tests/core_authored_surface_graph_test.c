#include "core_authored_surface_graph.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    CoreSurfaceGraph g = {0};
    g.count = 7;
    g.color_output = 6;
    g.roughness_output = 3;
    g.nodes[0] = (CoreSurfaceGraphNode){.kind = CORE_SG_COORDINATE, .scale_m = 1};
    g.nodes[1] = (CoreSurfaceGraphNode){.kind = CORE_SG_COLOR, .value = {1, 0, 0}};
    g.nodes[2] = (CoreSurfaceGraphNode){.kind = CORE_SG_COLOR, .value = {0, 0, 1}};
    g.nodes[3] = (CoreSurfaceGraphNode){.kind = CORE_SG_TRIPLANAR, .inputs = {0}, .sharpness = 1};
    g.nodes[4] = (CoreSurfaceGraphNode){.kind = CORE_SG_SCALAR, .value = {.5}};
    g.nodes[5] = (CoreSurfaceGraphNode){.kind = CORE_SG_MULTIPLY, .inputs = {3, 4}};
    g.nodes[6] = (CoreSurfaceGraphNode){.kind = CORE_SG_MIX, .inputs = {1, 2, 5}};
    char diagnostic[128];
    assert(core_surface_graph_prepare(&g, diagnostic, sizeof(diagnostic)));
    CoreSurfaceGraphQuery q = {0};
    q.position[0][0] = .25;
    q.position[0][1] = 1.25;
    q.position[0][2] = 2.25;
    q.normal[0][0] = 1;
    q.normal[0][1] = 2;
    q.normal[0][2] = 3;
    CoreSurfaceGraphResult result;
    assert(core_surface_graph_evaluate(&g, &q, &result));
    /* YZ=1, ZX=0, XY=1; independent weighted projection oracle. */
    assert(fabs(result.roughness - 4.0 / 6) < 1e-12);
    assert(fabs(result.color[2] - 1.0 / 3) < 1e-12);
    q.dx[0][0] = q.dx[0][1] = q.dx[0][2] = 2;
    for (int i = 0; i < 100; ++i) {
        q.position[0][0] += .071;
        assert(core_surface_graph_evaluate(&g, &q, &result));
        assert(fabs(result.roughness - .5) < 1e-12);
    }
    q.unbounded = true;
    assert(core_surface_graph_evaluate(&g, &q, &result));
    assert(result.roughness == .5);
    CoreSurfaceGraph bad = g;
    bad.nodes[5].inputs[0] = 6;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    bad = g;
    bad.nodes[5].inputs[0] = 5;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    bad = g;
    bad.nodes[0].scale_m = 0;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    bad = g;
    bad.color_output = 3;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    bad = g;
    bad.nodes[3].inputs[0] = 99;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    bad = g;
    bad.count = 33;
    assert(!core_surface_graph_prepare(&bad, diagnostic, sizeof(diagnostic)));
    g.nodes[3].kind = CORE_SG_NOISE3D;
    g.nodes[3].seed = 123;
    assert(core_surface_graph_prepare(&g, diagnostic, sizeof(diagnostic)));
    q.unbounded = false;
    memset(q.dx, 0, sizeof(q.dx));
    assert(core_surface_graph_evaluate(&g, &q, &result));
    double first = result.roughness;
    g.nodes[3].seed = 124;
    assert(core_surface_graph_evaluate(&g, &q, &result));
    assert(fabs(first - result.roughness) > .00001);
    q.unbounded = true;
    assert(core_surface_graph_evaluate(&g, &q, &result));
    assert(result.roughness == .5);
    q.position[0][0] = NAN;
    assert(!core_surface_graph_evaluate(&g, &q, &result));
    puts("surface graph: typed DAG, cycle/type failures, projection oracle, filtering and seed "
         "checks passed");
    return 0;
}
