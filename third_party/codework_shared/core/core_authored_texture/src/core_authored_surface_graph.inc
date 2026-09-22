#include "core_authored_surface_graph.h"
#include <stdio.h>
/* Type checking and DFS happen once, never in the per-sample evaluator. */
static int sg_type(CoreSurfaceGraphKind k) {
    return k == CORE_SG_COORDINATE ? 2 : (k == CORE_SG_COLOR || k == CORE_SG_MIX ? 1 : 0);
}
static int sg_arity(CoreSurfaceGraphKind k) {
    return k == CORE_SG_MIX        ? 3
           : k == CORE_SG_MULTIPLY ? 2
                                   : (k == CORE_SG_NOISE3D || k == CORE_SG_TRIPLANAR ? 1 : 0);
}
static bool sg_visit(CoreSurfaceGraph *g, int i, unsigned char *state, size_t *written) {
    if (i < 0 || (size_t)i >= g->count || state[i] == 1)
        return false;
    if (state[i] == 2)
        return true;
    state[i] = 1;
    CoreSurfaceGraphNode *n = &g->nodes[i];
    for (int a = 0; a < sg_arity(n->kind); ++a) {
        int j = n->inputs[a];
        int expected = n->kind == CORE_SG_MIX        ? (a < 2 ? 1 : 0)
                       : n->kind == CORE_SG_MULTIPLY ? 0
                                                     : 2;
        if (j < 0 || (size_t)j >= g->count || sg_type(g->nodes[j].kind) != expected ||
            !sg_visit(g, j, state, written))
            return false;
    }
    state[i] = 2;
    g->order[(*written)++] = (unsigned char)i;
    return true;
}
bool core_surface_graph_prepare(CoreSurfaceGraph *g, char *diagnostic, size_t size) {
    if (!g)
        return false;
    g->valid = false;
    bool ok = g->count > 0 && g->count <= CORE_SURFACE_GRAPH_MAX_NODES;
    for (size_t i = 0; ok && i < g->count; ++i) {
        CoreSurfaceGraphNode *n = &g->nodes[i];
        ok = n->kind >= CORE_SG_SCALAR && n->kind <= CORE_SG_MIX;
        for (int a = 0; a < 3; ++a)
            ok = ok && isfinite(n->value[a]) && fabs(n->value[a]) <= 1e6;
        if (n->kind == CORE_SG_SCALAR || n->kind == CORE_SG_COLOR)
            for (int a = 0; a < (n->kind == CORE_SG_COLOR ? 3 : 1); ++a)
                ok = ok && n->value[a] >= 0 && n->value[a] <= 1;
        if (n->kind == CORE_SG_COORDINATE)
            ok = ok && isfinite(n->scale_m) && n->scale_m >= 1e-6 && n->scale_m <= 1e6;
        if (n->kind == CORE_SG_TRIPLANAR)
            ok = ok && isfinite(n->sharpness) && n->sharpness >= 1 && n->sharpness <= 16;
    }
    if (ok)
        ok = g->color_output >= 0 && (size_t)g->color_output < g->count &&
             sg_type(g->nodes[g->color_output].kind) == 1 &&
             (g->roughness_output == -1 ||
              (g->roughness_output >= 0 && (size_t)g->roughness_output < g->count &&
               sg_type(g->nodes[g->roughness_output].kind) == 0));
    unsigned char state[CORE_SURFACE_GRAPH_MAX_NODES] = {0};
    size_t written = 0;
    for (size_t i = 0; ok && i < g->count; ++i)
        ok = sg_visit(g, (int)i, state, &written);
    g->valid = ok;
    if (diagnostic && size)
        snprintf(diagnostic, size, "%s",
                 ok ? "typed graph prepared"
                    : "invalid graph type, parameter, output, reference or cycle");
    return ok;
}
static double sg_hash(int64_t x, int64_t y, int64_t z, uint32_t seed) {
    uint64_t v = (uint64_t)x * UINT64_C(0x9e3779b185ebca87) ^
                 (uint64_t)y * UINT64_C(0xc2b2ae3d27d4eb4f) ^
                 (uint64_t)z * UINT64_C(0x165667b19e3779f9) ^ seed;
    v ^= v >> 30;
    v *= UINT64_C(0xbf58476d1ce4e5b9);
    v ^= v >> 27;
    return (double)(v >> 11) / 9007199254740992.0;
}
/* Wrap both positions and adjacent lattice corners to keep the periodic field
 * continuous, including negative coordinates and cells straddling the seam. */
static double sg_noise(const double *p, uint32_t seed) {
    int64_t cell[3];
    double f[3];
    for (int a = 0; a < 3; ++a) {
        double v = fmod(p[a], 1048576.0);
        if (v < 0)
            v += 1048576.0;
        cell[a] = (int64_t)floor(v);
        f[a] = v - floor(v);
        f[a] = f[a] * f[a] * (3 - 2 * f[a]);
    }
    double out = 0;
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                out += sg_hash((cell[0] + x) % 1048576, (cell[1] + y) % 1048576,
                               (cell[2] + z) % 1048576, seed) *
                       (x ? f[0] : 1 - f[0]) * (y ? f[1] : 1 - f[1]) * (z ? f[2] : 1 - f[2]);
    return out;
}
/* Integral of an alternating unit-cell wave. Box filtering is exact per axis. */
static double sg_wave_integral(double x) {
    double r = x - 2 * floor(x / 2);
    return r <= 1 ? r : 2 - r;
}
static double sg_wave(double x, double width) {
    if (width > 1e6)
        return 0;
    if (width < 1e-9)
        return fmod(floor(x), 2) == 0 ? 1 : -1;
    return (sg_wave_integral(x + width / 2) - sg_wave_integral(x - width / 2)) / width;
}
bool core_surface_graph_evaluate(const CoreSurfaceGraph *g, const CoreSurfaceGraphQuery *q,
                                 CoreSurfaceGraphResult *out) {
    if (!g || !g->valid || !q || !out)
        return false;
    typedef struct {
        double v[3], n[3], width[3];
    } Value;
    Value values[CORE_SURFACE_GRAPH_MAX_NODES] = {0};
    for (int d = 0; d < 2; ++d)
        for (int a = 0; a < 3; ++a)
            if (!isfinite(q->position[d][a]) || !isfinite(q->normal[d][a]) ||
                !isfinite(q->dx[d][a]) || !isfinite(q->dy[d][a]) ||
                fabs(q->position[d][a]) > 1e12 || fabs(q->normal[d][a]) > 1e12 ||
                fabs(q->dx[d][a]) > 1e12 || fabs(q->dy[d][a]) > 1e12)
                return false;
    for (size_t step = 0; step < g->count; ++step) {
        int i = g->order[step];
        const CoreSurfaceGraphNode *n = &g->nodes[i];
        Value *v = &values[i];
        if (n->kind == CORE_SG_SCALAR || n->kind == CORE_SG_COLOR)
            memcpy(v->v, n->value, sizeof(v->v));
        else if (n->kind == CORE_SG_COORDINATE) {
            int d = n->world_space ? 1 : 0;
            for (int a = 0; a < 3; ++a) {
                v->v[a] = q->position[d][a] / n->scale_m + n->value[a];
                v->n[a] = q->normal[d][a];
                v->width[a] = (fabs(q->dx[d][a]) + fabs(q->dy[d][a])) / n->scale_m;
            }
        } else if (n->kind == CORE_SG_NOISE3D) {
            const Value *p = &values[n->inputs[0]];
            double width = fmax(p->width[0], fmax(p->width[1], p->width[2]));
            double fade = q->unbounded ? 0 : fmax(0, 1 - width);
            v->v[0] = .5 + (sg_noise(p->v, n->seed) - .5) * fade;
        } else if (n->kind == CORE_SG_TRIPLANAR) {
            const Value *p = &values[n->inputs[0]];
            double sum = 0, weight = 0;
            for (int a = 0; a < 3; ++a) {
                int b = (a + 1) % 3, c = (a + 2) % 3;
                double w = pow(fabs(p->n[a]), n->sharpness);
                double sample = q->unbounded ? .5
                                             : .5 - .5 * sg_wave(p->v[b], p->width[b]) *
                                                        sg_wave(p->v[c], p->width[c]);
                sum += w * sample;
                weight += w;
            }
            v->v[0] = weight > 1e-30 ? sum / weight : .5;
        } else if (n->kind == CORE_SG_MULTIPLY)
            v->v[0] = values[n->inputs[0]].v[0] * values[n->inputs[1]].v[0];
        else if (n->kind == CORE_SG_MIX)
            for (int a = 0; a < 3; ++a) {
                double t = values[n->inputs[2]].v[0];
                v->v[a] = values[n->inputs[0]].v[a] * (1 - t) + values[n->inputs[1]].v[a] * t;
            }
    }
    memcpy(out->color, values[g->color_output].v, sizeof(out->color));
    out->has_roughness = g->roughness_output >= 0;
    out->roughness = out->has_roughness ? values[g->roughness_output].v[0] : 0;
    return true;
}
