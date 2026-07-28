#include "procedural_solid_graph_internal.h"
#include "procedural/procedural_solid_source_accel.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct SolidEvalValue {
    double distance;
    char contributor[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char secondary_contributor[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    ProceduralSolidRegionKind region_kind;
    double blend_weight;
    size_t source_triangle_tests;
    size_t source_query_count;
    size_t accelerated_source_query_count;
} SolidEvalValue;

typedef struct SolidEvalContext {
    const ProceduralSolidGraphV1 *graph;
    const ProceduralSolidSourceSet *sources;
    uint32_t evaluations;
    ProceduralSolidGraphReport *report;
} SolidEvalContext;

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static double vec_length(CoreObjectVec3 value) {
    return sqrt(vec_dot(value, value));
}

static double point_triangle_distance(CoreObjectVec3 p,
                                      CoreObjectVec3 a,
                                      CoreObjectVec3 b,
                                      CoreObjectVec3 c) {
    const CoreObjectVec3 ab = vec_sub(b, a);
    const CoreObjectVec3 ac = vec_sub(c, a);
    const CoreObjectVec3 ap = vec_sub(p, a);
    const double d1 = vec_dot(ab, ap);
    const double d2 = vec_dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return vec_length(ap);
    {
        const CoreObjectVec3 bp = vec_sub(p, b);
        const double d3 = vec_dot(ab, bp);
        const double d4 = vec_dot(ac, bp);
        if (d3 >= 0.0 && d4 <= d3) return vec_length(bp);
        {
            const double vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
                const double v = d1 / (d1 - d3);
                return vec_length(vec_sub(
                    p, (CoreObjectVec3){
                        a.x + v * ab.x, a.y + v * ab.y, a.z + v * ab.z}));
            }
        }
        {
            const CoreObjectVec3 cp = vec_sub(p, c);
            const double d5 = vec_dot(ab, cp);
            const double d6 = vec_dot(ac, cp);
            if (d6 >= 0.0 && d5 <= d6) return vec_length(cp);
            {
                const double vb = d5 * d2 - d1 * d6;
                if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
                    const double w = d2 / (d2 - d6);
                    return vec_length(vec_sub(
                        p, (CoreObjectVec3){
                            a.x + w * ac.x, a.y + w * ac.y,
                            a.z + w * ac.z}));
                }
            }
            {
                const double va = d3 * d6 - d5 * d4;
                if (va <= 0.0 && (d4 - d3) >= 0.0 &&
                    (d5 - d6) >= 0.0) {
                    const CoreObjectVec3 bc = vec_sub(c, b);
                    const double w =
                        (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    return vec_length(vec_sub(
                        p, (CoreObjectVec3){
                            b.x + w * bc.x, b.y + w * bc.y,
                            b.z + w * bc.z}));
                }
            }
        }
    }
    {
        const CoreObjectVec3 normal = vec_cross(ab, ac);
        const double normal_length = vec_length(normal);
        if (normal_length <= DBL_EPSILON) return HUGE_VAL;
        return fabs(vec_dot(ap, normal)) / normal_length;
    }
}

static double triangle_solid_angle(CoreObjectVec3 p,
                                   CoreObjectVec3 a,
                                   CoreObjectVec3 b,
                                   CoreObjectVec3 c) {
    const CoreObjectVec3 pa = vec_sub(a, p);
    const CoreObjectVec3 pb = vec_sub(b, p);
    const CoreObjectVec3 pc = vec_sub(c, p);
    const double la = vec_length(pa);
    const double lb = vec_length(pb);
    const double lc = vec_length(pc);
    const double numerator = vec_dot(pa, vec_cross(pb, pc));
    const double denominator =
        la * lb * lc + vec_dot(pa, pb) * lc +
        vec_dot(pb, pc) * la + vec_dot(pc, pa) * lb;
    if (la <= 1.0e-15 || lb <= 1.0e-15 || lc <= 1.0e-15) return 0.0;
    return 2.0 * atan2(numerator, denominator);
}

static const CoreMeshAssetRuntimeDocument *find_source(
    const ProceduralSolidSourceSet *sources,
    const char *source_id) {
    const CoreMeshAssetRuntimeDocument *found = NULL;
    if (!sources || !source_id ||
        sources->source_count > PROCEDURAL_SOLID_GRAPH_MAX_SOURCES) {
        return NULL;
    }
    for (size_t i = 0u; i < sources->source_count; ++i) {
        if (strcmp(sources->sources[i].source_id, source_id) == 0) {
            if (found) return NULL;
            found = sources->sources[i].mesh;
        }
    }
    return found;
}

static bool source_mesh_distance(
    SolidEvalContext *context,
    const ProceduralSolidGraphNode *node,
    CoreObjectVec3 point,
    SolidEvalValue *out) {
    const CoreMeshAssetRuntimeDocument *mesh =
        find_source(context->sources, node->source_id);
    const ProceduralSolidSourceAccel *accel = NULL;
    double minimum = HUGE_VAL;
    double angle = 0.0;
    if (!mesh || mesh->triangle_count == 0u || !mesh->vertices ||
        !mesh->triangles ||
        !mesh->contract.topology_closed_volume ||
        !mesh->contract.topology_manifold_expected ||
        core_mesh_asset_runtime_document_validate(mesh).code != CORE_OK) {
        procedural_solid_graph_report_set(
            context->report, PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE,
            node->source_id,
            "source_mesh node requires a valid registered runtime mesh");
        return false;
    }
    if (context->sources) {
        for (size_t i = 0u; i < context->sources->source_count; ++i) {
            if (strcmp(
                    context->sources->sources[i].source_id,
                    node->source_id) == 0) {
                accel = context->sources->sources[i].accel;
                break;
            }
        }
    }
    memset(out, 0, sizeof(*out));
    if (accel) {
        ProceduralSolidSourceQuery query;
        if (accel->mesh != mesh ||
            !ProceduralSolidSourceAccel_Query(accel, point, &query)) {
            procedural_solid_graph_report_set(
                context->report, PROCEDURAL_SOLID_GRAPH_STATUS_SOURCE,
                node->source_id,
                "source acceleration does not match its registered mesh");
            return false;
        }
        out->distance = query.signed_distance;
        out->source_triangle_tests =
            query.distance_triangle_tests + query.sign_triangle_tests;
        out->source_query_count = 1u;
        out->accelerated_source_query_count = 1u;
        snprintf(out->contributor, sizeof(out->contributor), "%s", node->id);
        return true;
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &mesh->triangles[i];
        const CoreObjectVec3 a = mesh->vertices[triangle->a].position;
        const CoreObjectVec3 b = mesh->vertices[triangle->b].position;
        const CoreObjectVec3 c = mesh->vertices[triangle->c].position;
        const double distance = point_triangle_distance(point, a, b, c);
        if (distance < minimum) minimum = distance;
        angle += triangle_solid_angle(point, a, b, c);
    }
    if (!isfinite(minimum)) {
        procedural_solid_graph_report_set(
            context->report, PROCEDURAL_SOLID_GRAPH_STATUS_EVALUATION,
            node->id, "source mesh distance evaluation failed");
        return false;
    }
    out->distance = fabs(angle) > (2.0 * M_PI) ? -minimum : minimum;
    out->source_triangle_tests = mesh->triangle_count * 2u;
    out->source_query_count = 1u;
    snprintf(out->contributor, sizeof(out->contributor), "%s", node->id);
    return true;
}

static void eval_value_init_leaf(
    SolidEvalValue *value,
    double distance,
    const char *contributor) {
    memset(value, 0, sizeof(*value));
    value->distance = distance;
    value->region_kind = PROCEDURAL_SOLID_REGION_RETAINED;
    snprintf(
        value->contributor, sizeof(value->contributor), "%s", contributor);
}

static void eval_value_accumulate_sources(
    SolidEvalValue *value,
    const SolidEvalValue *a,
    const SolidEvalValue *b) {
    value->source_triangle_tests =
        a->source_triangle_tests + b->source_triangle_tests;
    value->source_query_count =
        a->source_query_count + b->source_query_count;
    value->accelerated_source_query_count =
        a->accelerated_source_query_count +
        b->accelerated_source_query_count;
}

static CoreObjectVec3 inverse_rotate_xyz(CoreObjectVec3 p,
                                         CoreObjectVec3 rotation) {
    double c = cos(-rotation.z);
    double s = sin(-rotation.z);
    CoreObjectVec3 q = {c * p.x - s * p.y, s * p.x + c * p.y, p.z};
    c = cos(-rotation.y);
    s = sin(-rotation.y);
    p = (CoreObjectVec3){c * q.x + s * q.z, q.y, -s * q.x + c * q.z};
    c = cos(-rotation.x);
    s = sin(-rotation.x);
    return (CoreObjectVec3){
        p.x, c * p.y - s * p.z, s * p.y + c * p.z};
}

static bool evaluate_node(SolidEvalContext *context,
                          size_t index,
                          CoreObjectVec3 point,
                          SolidEvalValue *out) {
    const ProceduralSolidGraphNode *node = &context->graph->nodes[index];
    SolidEvalValue a;
    SolidEvalValue b;
    int child;
    if (++context->evaluations >
        context->graph->max_node_evaluations) {
        procedural_solid_graph_report_set(
            context->report, PROCEDURAL_SOLID_GRAPH_STATUS_BUDGET,
            node->id, "solid graph evaluation budget exhausted");
        return false;
    }
    switch (node->op) {
        case PROCEDURAL_SOLID_NODE_SPHERE:
            eval_value_init_leaf(
                out, vec_length(point) - node->scalar_a, node->id);
            return true;
        case PROCEDURAL_SOLID_NODE_BOX: {
            const CoreObjectVec3 q = {
                fabs(point.x) - node->vector_a.x,
                fabs(point.y) - node->vector_a.y,
                fabs(point.z) - node->vector_a.z};
            const CoreObjectVec3 outside = {
                fmax(q.x, 0.0), fmax(q.y, 0.0), fmax(q.z, 0.0)};
            eval_value_init_leaf(
                out,
                vec_length(outside) +
                    fmin(fmax(q.x, fmax(q.y, q.z)), 0.0) -
                    node->scalar_a,
                node->id);
            return true;
        }
        case PROCEDURAL_SOLID_NODE_CYLINDER_Z: {
            const double radial = hypot(point.x, point.y) - node->scalar_a;
            const double axial = fabs(point.z) - node->scalar_b;
            eval_value_init_leaf(
                out,
                hypot(fmax(radial, 0.0), fmax(axial, 0.0)) +
                    fmin(fmax(radial, axial), 0.0),
                node->id);
            return true;
        }
        case PROCEDURAL_SOLID_NODE_SOURCE_MESH:
            return source_mesh_distance(context, node, point, out);
        case PROCEDURAL_SOLID_NODE_TRANSFORM: {
            const double min_scale = fmin(
                fabs(node->vector_c.x),
                fmin(fabs(node->vector_c.y), fabs(node->vector_c.z)));
            point = vec_sub(point, node->vector_a);
            point = inverse_rotate_xyz(point, node->vector_b);
            point.x /= node->vector_c.x;
            point.y /= node->vector_c.y;
            point.z /= node->vector_c.z;
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[0]);
            if (child < 0 || !evaluate_node(
                    context, (size_t)child, point, out)) {
                return false;
            }
            out->distance *= min_scale;
            return true;
        }
        case PROCEDURAL_SOLID_NODE_TWIST_Z: {
            const double angle = -node->scalar_a * point.z;
            const double c = cos(angle);
            const double s = sin(angle);
            const CoreObjectVec3 q = {
                c * point.x - s * point.y,
                s * point.x + c * point.y,
                point.z};
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[0]);
            return child >= 0 && evaluate_node(
                context, (size_t)child, q, out);
        }
        case PROCEDURAL_SOLID_NODE_TAPER_Z: {
            const double scale = 1.0 + node->scalar_a * point.z;
            CoreObjectVec3 q = point;
            if (scale <= 0.05) {
                eval_value_init_leaf(
                    out, 1.0e6 + fabs(scale), node->id);
                return true;
            }
            q.x /= scale;
            q.y /= scale;
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[0]);
            if (child < 0 || !evaluate_node(
                    context, (size_t)child, q, out)) {
                return false;
            }
            out->distance *= fmin(scale, 1.0);
            return true;
        }
        case PROCEDURAL_SOLID_NODE_ROUND:
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[0]);
            if (child < 0 || !evaluate_node(
                    context, (size_t)child, point, out)) {
                return false;
            }
            out->distance -= node->scalar_a;
            return true;
        case PROCEDURAL_SOLID_NODE_UNION:
        case PROCEDURAL_SOLID_NODE_INTERSECTION:
        case PROCEDURAL_SOLID_NODE_DIFFERENCE:
        case PROCEDURAL_SOLID_NODE_SMOOTH_UNION:
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[0]);
            if (child < 0 || !evaluate_node(
                    context, (size_t)child, point, &a)) {
                return false;
            }
            child = procedural_solid_graph_find_node(
                context->graph, node->inputs[1]);
            if (child < 0 || !evaluate_node(
                    context, (size_t)child, point, &b)) {
                return false;
            }
            if (node->op == PROCEDURAL_SOLID_NODE_UNION) {
                *out = a.distance <= b.distance ? a : b;
            } else if (node->op == PROCEDURAL_SOLID_NODE_INTERSECTION) {
                *out = a.distance >= b.distance ? a : b;
            } else if (node->op == PROCEDURAL_SOLID_NODE_DIFFERENCE) {
                if (a.distance >= -b.distance) {
                    *out = a;
                } else {
                    memset(out, 0, sizeof(*out));
                    out->distance = -b.distance;
                    out->region_kind = PROCEDURAL_SOLID_REGION_CUT;
                    snprintf(
                        out->contributor, sizeof(out->contributor),
                        "%s", b.contributor);
                    snprintf(
                        out->secondary_contributor,
                        sizeof(out->secondary_contributor),
                        "%s", a.contributor);
                    eval_value_accumulate_sources(out, &a, &b);
                }
            } else {
                const double k = node->scalar_a;
                const double h =
                    fmax(0.0, fmin(1.0, 0.5 + 0.5 *
                                  (b.distance - a.distance) / k));
                out->distance =
                    (b.distance * (1.0 - h) + a.distance * h) -
                    k * h * (1.0 - h);
                if (h > 0.02 && h < 0.98) {
                    out->region_kind = PROCEDURAL_SOLID_REGION_BLEND;
                    snprintf(
                        out->contributor, sizeof(out->contributor),
                        "%s", a.contributor);
                    snprintf(
                        out->secondary_contributor,
                        sizeof(out->secondary_contributor),
                        "%s", b.contributor);
                    out->blend_weight = h;
                } else {
                    const SolidEvalValue *selected =
                        h >= 0.5 ? &a : &b;
                    out->region_kind = selected->region_kind;
                    snprintf(
                        out->contributor, sizeof(out->contributor),
                        "%s", selected->contributor);
                    snprintf(
                        out->secondary_contributor,
                        sizeof(out->secondary_contributor),
                        "%s", selected->secondary_contributor);
                    out->blend_weight = selected->blend_weight;
                }
                eval_value_accumulate_sources(out, &a, &b);
            }
            if (node->op != PROCEDURAL_SOLID_NODE_SMOOTH_UNION &&
                !(node->op == PROCEDURAL_SOLID_NODE_DIFFERENCE &&
                  a.distance < -b.distance)) {
                const SolidEvalValue selected = *out;
                eval_value_accumulate_sources(out, &a, &b);
                out->region_kind = selected.region_kind;
                out->blend_weight = selected.blend_weight;
                snprintf(
                    out->contributor, sizeof(out->contributor),
                    "%s", selected.contributor);
                snprintf(
                    out->secondary_contributor,
                    sizeof(out->secondary_contributor),
                    "%s", selected.secondary_contributor);
            }
            return isfinite(out->distance);
        case PROCEDURAL_SOLID_NODE_INVALID:
            break;
    }
    procedural_solid_graph_report_set(
        context->report, PROCEDURAL_SOLID_GRAPH_STATUS_EVALUATION,
        node->id, "solid node operation is not evaluable");
    return false;
}

bool ProceduralSolidGraphV1_Evaluate(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    ProceduralSolidSample *out_sample,
    ProceduralSolidGraphReport *report) {
    SolidEvalContext context;
    SolidEvalValue value;
    int output;
    if (!out_sample || !isfinite(point.x) || !isfinite(point.y) ||
        !isfinite(point.z) ||
        !ProceduralSolidGraphV1_Validate(graph, report)) {
        if (!out_sample) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_NULL_ARGUMENT,
                "sample", "solid sample output is required");
        }
        return false;
    }
    context = (SolidEvalContext){
        .graph = graph,
        .sources = sources,
        .evaluations = 0u,
        .report = report};
    output = procedural_solid_graph_find_node(graph, graph->output);
    if (output < 0 || !evaluate_node(
            &context, (size_t)output, point, &value) ||
        !isfinite(value.distance) || !value.contributor[0]) {
        if (report && report->status == PROCEDURAL_SOLID_GRAPH_STATUS_OK) {
            procedural_solid_graph_report_set(
                report, PROCEDURAL_SOLID_GRAPH_STATUS_EVALUATION,
                graph->output, "solid graph evaluation failed");
        }
        return false;
    }
    memset(out_sample, 0, sizeof(*out_sample));
    out_sample->signed_distance = value.distance;
    out_sample->evaluations_used = context.evaluations;
    out_sample->region_kind = value.region_kind;
    out_sample->blend_weight = value.blend_weight;
    out_sample->source_triangle_tests = value.source_triangle_tests;
    out_sample->source_query_count = value.source_query_count;
    out_sample->accelerated_source_query_count =
        value.accelerated_source_query_count;
    snprintf(out_sample->contributing_node_id,
             sizeof(out_sample->contributing_node_id),
             "%s", value.contributor);
    snprintf(
        out_sample->secondary_contributing_node_id,
        sizeof(out_sample->secondary_contributing_node_id),
        "%s", value.secondary_contributor);
    return true;
}
