#include "core_authored_surface_graph.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Values computed outside the C evaluator using 60-digit Decimal interpolation
 * of the specified wrapped lattice/hash. No runtime evaluator is the oracle. */
static void check_noise_channels(const CoreSurfaceGraph *g, const CoreSurfaceGraphQuery *q,
                                 double expected) {
    CoreSurfaceGraphResult r;
    assert(core_surface_graph_evaluate(g, q, &r));
    const double low[] = {.125, .25, .75}, high[] = {.875, .625, .0625};
    assert(r.has_roughness && fabs(r.roughness - expected) < 2e-14);
    for (int c = 0; c < 3; ++c)
        assert(fabs(r.color[c] - (low[c] + (high[c] - low[c]) * expected)) < 2e-14);
}

static double noise_value(const CoreSurfaceGraph *g, const CoreSurfaceGraphQuery *q) {
    CoreSurfaceGraphResult r;
    assert(core_surface_graph_evaluate(g, q, &r));
    return r.roughness;
}

static void noise_periodic_oracle(void) {
    static const uint32_t seeds[] = {0, 123, UINT32_MAX};
    static const double points[][3] = {{.25, .375, .5}, {-.25, .375, .5}, {-.25, -.375, -.5}};
    static const double expected[][3] = {
        {.3353265627317597387, .3303225477543398379, .3413663585982894541},
        {.4329870291883822472, .4527022522058757613, .7132464867652326131},
        {.2882491568596716003, .3031183682626869189, .5040375066927446226}};
    CoreSurfaceGraph g = {0};
    g.count = 5;
    g.color_output = 4;
    g.roughness_output = 1;
    g.nodes[0] = (CoreSurfaceGraphNode){.kind = CORE_SG_COORDINATE, .scale_m = 1};
    g.nodes[1] = (CoreSurfaceGraphNode){.kind = CORE_SG_NOISE3D, .inputs = {0}};
    g.nodes[2] = (CoreSurfaceGraphNode){.kind = CORE_SG_COLOR, .value = {.125, .25, .75}};
    g.nodes[3] = (CoreSurfaceGraphNode){.kind = CORE_SG_COLOR, .value = {.875, .625, .0625}};
    g.nodes[4] = (CoreSurfaceGraphNode){.kind = CORE_SG_MIX, .inputs = {2, 3, 1}};
    for (size_t seed = 0; seed < 3; ++seed) {
        g.nodes[1].seed = seeds[seed];
        assert(core_surface_graph_prepare(&g, NULL, 0));
        for (size_t point = 0; point < 3; ++point) {
            CoreSurfaceGraphQuery q = {0};
            memcpy(q.position[0], points[point], sizeof(points[point]));
            check_noise_channels(&g, &q, expected[seed][point]);
            for (int axis = 0; axis < 3; ++axis)
                for (int shift = -2; shift <= 2; ++shift) {
                    CoreSurfaceGraphQuery shifted = q;
                    shifted.position[0][axis] += shift * 1048576.0;
                    check_noise_channels(&g, &shifted, expected[seed][point]);
                }
            q.dx[0][1] = .125;
            check_noise_channels(&g, &q, .5 + (expected[seed][point] - .5) * .875);
            q.unbounded = true;
            check_noise_channels(&g, &q, .5);
        }
        /* Continuity is checked on all axes at negative, zero and positive seams,
         * both at meter scale and at the minimum supported physical cell size. */
        for (int tiny = 0; tiny < 2; ++tiny) {
            double scale = tiny ? 1e-6 : 1;
            g.nodes[0].scale_m = scale;
            assert(core_surface_graph_prepare(&g, NULL, 0));
            for (int axis = 0; axis < 3; ++axis)
                for (int period = -1; period <= 1; ++period) {
                    double errors[2];
                    for (int level = 0; level < 2; ++level) {
                        double eps = level ? 1e-4 : 1e-3;
                        CoreSurfaceGraphQuery a = {0}, b = {0};
                        for (int k = 0; k < 3; ++k)
                            a.position[0][k] = b.position[0][k] = points[0][k] * scale;
                        a.position[0][axis] = (period * 1048576.0 - eps) * scale;
                        b.position[0][axis] = (period * 1048576.0 + eps) * scale;
                        errors[level] = fabs(noise_value(&g, &a) - noise_value(&g, &b));
                    }
                    assert(errors[1] < 1e-6);
                    assert(errors[1] <= errors[0] * .02 + 1e-12);
                }
        }
        g.nodes[0].scale_m = 1;
    }
}

int main(void) {
    noise_periodic_oracle();
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
         "checks passed; periodic noise oracle, full RGB and variable roughness passed");
    return 0;
}
