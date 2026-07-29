#include "procedural/procedural_solid_material_graph.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralSolidMaterialGraphReport *report,
    ProceduralSolidMaterialGraphStatus status,
    const char *field, const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool stable_id(const char *text, size_t capacity) {
    size_t n;
    if (!text || !text[0] || (n = strlen(text)) >= capacity) return false;
    for (size_t i = 0u; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

const char *ProceduralSolidMaterialNodeKind_Name(
    ProceduralSolidMaterialNodeKind kind) {
    static const char *names[] = {
        "constant", "height", "slope", "curvature", "cavity",
        "authored_region", "region", "boundary_distance", "noise", "add",
        "multiply", "invert", "smoothstep", "bands"};
    if ((unsigned)kind >= sizeof(names) / sizeof(names[0])) return "invalid";
    return names[(unsigned)kind];
}

bool ProceduralSolidMaterialNodeKind_FromName(
    const char *name, ProceduralSolidMaterialNodeKind *out_kind) {
    if (!name || !out_kind) return false;
    for (int i = PROCEDURAL_SOLID_MATERIAL_NODE_CONSTANT;
         i <= PROCEDURAL_SOLID_MATERIAL_NODE_BANDS; ++i) {
        if (strcmp(name, ProceduralSolidMaterialNodeKind_Name(
                             (ProceduralSolidMaterialNodeKind)i)) == 0) {
            *out_kind = (ProceduralSolidMaterialNodeKind)i;
            return true;
        }
    }
    return false;
}

void ProceduralSolidMaterialGraphV1_Init(
    ProceduralSolidMaterialGraphV1 *graph) {
    if (!graph) return;
    memset(graph, 0, sizeof(*graph));
    graph->schema_version = PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA_VERSION;
}

static ProceduralSolidMaterialNodeV1 *append_node(
    ProceduralSolidMaterialGraphV1 *graph, const char *id,
    ProceduralSolidMaterialNodeKind kind, const char *a, const char *b) {
    ProceduralSolidMaterialNodeV1 *node;
    if (!graph || graph->node_count >= PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES)
        return NULL;
    node = &graph->nodes[graph->node_count++];
    memset(node, 0, sizeof(*node));
    snprintf(node->node_id, sizeof(node->node_id), "%s", id);
    node->kind = kind;
    snprintf(node->input_a, sizeof(node->input_a), "%s", a ? a : "");
    snprintf(node->input_b, sizeof(node->input_b), "%s", b ? b : "");
    node->maximum = 1.0;
    node->scale = 1.0;
    node->seed = 1;
    return node;
}

static void append_layer(
    ProceduralSolidMaterialGraphV1 *graph, const char *material_id,
    const char *weight_node_id) {
    ProceduralSolidMaterialLayerV1 *layer =
        &graph->layers[graph->layer_count++];
    snprintf(layer->material_id, sizeof(layer->material_id), "%s",
             material_id);
    snprintf(layer->weight_node_id, sizeof(layer->weight_node_id), "%s",
             weight_node_id);
}

bool ProceduralSolidMaterialGraphV1_FromTemplate(
    const char *template_id, const char *graph_id,
    const char *authored_binding_id, const char *authored_binding_digest,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialGraphV1 graph;
    ProceduralSolidMaterialNodeV1 *node;
    if (!template_id || !graph_id || !authored_binding_id ||
        !authored_binding_digest || !out_graph) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_ARGUMENT,
                   "template", "template and binding identity are required");
        return false;
    }
    ProceduralSolidMaterialGraphV1_Init(&graph);
    snprintf(graph.graph_id, sizeof(graph.graph_id), "%s", graph_id);
    snprintf(graph.authored_binding_id, sizeof(graph.authored_binding_id),
             "%s", authored_binding_id);
    snprintf(graph.authored_binding_digest_sha256,
             sizeof(graph.authored_binding_digest_sha256), "%s",
             authored_binding_digest);
    node = append_node(&graph, "base", PROCEDURAL_SOLID_MATERIAL_NODE_CONSTANT,
                       NULL, NULL);
    if (node) node->value = 1.0;
    append_layer(&graph, "base_material", "base");
    if (strcmp(template_id, "snow_accumulation") == 0) {
        append_node(&graph, "height", PROCEDURAL_SOLID_MATERIAL_NODE_HEIGHT,
                    NULL, NULL);
        append_node(&graph, "slope", PROCEDURAL_SOLID_MATERIAL_NODE_SLOPE,
                    NULL, NULL);
        node = append_node(&graph, "snowline",
                           PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP,
                           "height", NULL);
        node->minimum = 0.52; node->maximum = 0.72;
        append_node(&graph, "snow_mask",
                    PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY,
                    "snowline", "slope");
        append_layer(&graph, "snow_material", "snow_mask");
    } else if (strcmp(template_id, "concrete_pores") == 0) {
        node = append_node(&graph, "pore_noise",
                           PROCEDURAL_SOLID_MATERIAL_NODE_NOISE, NULL, NULL);
        node->scale = 28.0; node->seed = 104729;
        node = append_node(&graph, "pore_threshold",
                           PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP,
                           "pore_noise", NULL);
        node->minimum = 0.78; node->maximum = 0.91;
        append_node(&graph, "cavity",
                    PROCEDURAL_SOLID_MATERIAL_NODE_CAVITY, NULL, NULL);
        node = append_node(&graph, "cavity_bias",
                           PROCEDURAL_SOLID_MATERIAL_NODE_ADD,
                           "pore_threshold", "cavity");
        (void)node;
        append_layer(&graph, "pore_material", "cavity_bias");
    } else if (strcmp(template_id, "sediment") == 0) {
        append_node(&graph, "cavity",
                    PROCEDURAL_SOLID_MATERIAL_NODE_CAVITY, NULL, NULL);
        append_node(&graph, "height",
                    PROCEDURAL_SOLID_MATERIAL_NODE_HEIGHT, NULL, NULL);
        append_node(&graph, "low",
                    PROCEDURAL_SOLID_MATERIAL_NODE_INVERT, "height", NULL);
        append_node(&graph, "settle_raw",
                    PROCEDURAL_SOLID_MATERIAL_NODE_ADD,
                    "cavity", "low");
        node = append_node(&graph, "settle",
                           PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP,
                           "settle_raw", NULL);
        node->minimum = 0.45; node->maximum = 0.75;
        append_layer(&graph, "sediment_material", "settle");
    } else if (strcmp(template_id, "dune_bands") == 0) {
        node = append_node(&graph, "bands",
                           PROCEDURAL_SOLID_MATERIAL_NODE_BANDS, NULL, NULL);
        node->scale = 8.0; node->offset = 0.0;
        append_node(&graph, "slope",
                    PROCEDURAL_SOLID_MATERIAL_NODE_SLOPE, NULL, NULL);
        append_node(&graph, "dune_mask",
                    PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY,
                    "bands", "slope");
        append_layer(&graph, "dune_band_material", "dune_mask");
    } else if (strcmp(template_id, "layered_rock") == 0) {
        node = append_node(&graph, "strata",
                           PROCEDURAL_SOLID_MATERIAL_NODE_BANDS, NULL, NULL);
        node->scale = 12.0; node->offset = 0.12;
        append_layer(&graph, "strata_material", "strata");
    } else {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_NODE,
                   "template_id", "unsupported material graph template");
        return false;
    }
    if (!ProceduralSolidMaterialGraphV1_Validate(&graph, report)) return false;
    *out_graph = graph;
    return true;
}

static int find_node(
    const ProceduralSolidMaterialGraphV1 *graph, const char *node_id) {
    if (!graph || !node_id || !node_id[0]) return -1;
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].node_id, node_id) == 0) return (int)i;
    }
    return -1;
}

static bool node_needs_a(ProceduralSolidMaterialNodeKind kind) {
    return kind == PROCEDURAL_SOLID_MATERIAL_NODE_ADD ||
           kind == PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY ||
           kind == PROCEDURAL_SOLID_MATERIAL_NODE_INVERT ||
           kind == PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP;
}

static bool node_needs_b(ProceduralSolidMaterialNodeKind kind) {
    return kind == PROCEDURAL_SOLID_MATERIAL_NODE_ADD ||
           kind == PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY;
}

static bool visit_node(
    const ProceduralSolidMaterialGraphV1 *graph, int index,
    unsigned char *state) {
    const ProceduralSolidMaterialNodeV1 *node;
    int input;
    if (state[index] == 1u) return false;
    if (state[index] == 2u) return true;
    state[index] = 1u;
    node = &graph->nodes[index];
    if (node_needs_a(node->kind)) {
        input = find_node(graph, node->input_a);
        if (input < 0 || !visit_node(graph, input, state)) return false;
    }
    if (node_needs_b(node->kind)) {
        input = find_node(graph, node->input_b);
        if (input < 0 || !visit_node(graph, input, state)) return false;
    }
    state[index] = 2u;
    return true;
}

bool ProceduralSolidMaterialGraphV1_Validate(
    const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidMaterialGraphReport *report) {
    unsigned char state[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES] = {0};
    report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_OK, "", "ok");
    if (!graph ||
        graph->schema_version != PROCEDURAL_SOLID_MATERIAL_GRAPH_SCHEMA_VERSION ||
        !stable_id(graph->graph_id, sizeof(graph->graph_id)) ||
        !stable_id(graph->authored_binding_id,
                   sizeof(graph->authored_binding_id)) ||
        strlen(graph->authored_binding_digest_sha256) != 64u ||
        graph->node_count == 0u ||
        graph->node_count > PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES ||
        graph->layer_count == 0u ||
        graph->layer_count > PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_SCHEMA,
                   "graph", "graph identity or bounded counts are invalid");
        return false;
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        const ProceduralSolidMaterialNodeV1 *node = &graph->nodes[i];
        if (!stable_id(node->node_id, sizeof(node->node_id)) ||
            find_node(graph, node->node_id) != (int)i ||
            (unsigned)node->kind >
                (unsigned)PROCEDURAL_SOLID_MATERIAL_NODE_BANDS ||
            !isfinite(node->value) || !isfinite(node->minimum) ||
            !isfinite(node->maximum) || !isfinite(node->scale) ||
            !isfinite(node->offset) ||
            (node->kind == PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP &&
             node->minimum >= node->maximum) ||
            (node->kind == PROCEDURAL_SOLID_MATERIAL_NODE_REGION &&
             strcmp(node->region_kind, "retained") != 0 &&
             strcmp(node->region_kind, "cut") != 0 &&
             strcmp(node->region_kind, "blend") != 0)) {
            report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_NODE,
                       "nodes", "node identity, kind, or parameter is invalid");
            return false;
        }
    }
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (!visit_node(graph, (int)i, state)) {
            report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_CYCLE,
                       "nodes", "node inputs are missing or cyclic");
            return false;
        }
    }
    for (size_t i = 0u; i < graph->layer_count; ++i) {
        const ProceduralSolidMaterialLayerV1 *layer = &graph->layers[i];
        if (!stable_id(layer->material_id, sizeof(layer->material_id)) ||
            find_node(graph, layer->weight_node_id) < 0) {
            report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_MATERIAL,
                       "layers", "layer material or weight node is invalid");
            return false;
        }
    }
    return true;
}

static bool parse_double(const char *text, double *out) {
    char *end = NULL;
    double value;
    errno = 0;
    value = text ? strtod(text, &end) : 0.0;
    if (!text || errno || end == text || *end || !isfinite(value)) return false;
    *out = value;
    return true;
}

bool ProceduralSolidMaterialGraphV1_SetParameter(
    const ProceduralSolidMaterialGraphV1 *base,
    const char *expected_base_digest, const char *node_id,
    const char *parameter_id, const char *value,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialGraphV1 graph;
    char digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    double number;
    int index;
    if (!base || !expected_base_digest || !node_id || !parameter_id ||
        !value || !out_graph ||
        !ProceduralSolidMaterialGraphV1_Digest(base, digest, report))
        return false;
    if (strcmp(digest, expected_base_digest) != 0) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_STALE_BASE,
                   "expected_base_digest", "material graph changed");
        return false;
    }
    graph = *base;
    index = find_node(&graph, node_id);
    if (index < 0 || !parse_double(value, &number)) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_NODE,
                   "parameter", "node or numeric parameter is invalid");
        return false;
    }
    if (strcmp(parameter_id, "value") == 0) graph.nodes[index].value = number;
    else if (strcmp(parameter_id, "minimum") == 0)
        graph.nodes[index].minimum = number;
    else if (strcmp(parameter_id, "maximum") == 0)
        graph.nodes[index].maximum = number;
    else if (strcmp(parameter_id, "scale") == 0)
        graph.nodes[index].scale = number;
    else if (strcmp(parameter_id, "offset") == 0)
        graph.nodes[index].offset = number;
    else {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_NODE,
                   "parameter_id", "unsupported typed node parameter");
        return false;
    }
    if (!ProceduralSolidMaterialGraphV1_Validate(&graph, report)) return false;
    *out_graph = graph;
    return true;
}

bool ProceduralSolidMaterialGraphV1_Connect(
    const ProceduralSolidMaterialGraphV1 *base,
    const char *expected_base_digest, const char *node_id,
    const char *input_id, const char *source_node_id,
    ProceduralSolidMaterialGraphV1 *out_graph,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialGraphV1 graph;
    char digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    int index;
    if (!base || !expected_base_digest || !node_id || !input_id ||
        !source_node_id || !out_graph ||
        !ProceduralSolidMaterialGraphV1_Digest(base, digest, report))
        return false;
    if (strcmp(digest, expected_base_digest) != 0) {
        report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_STALE_BASE,
                   "expected_base_digest", "material graph changed");
        return false;
    }
    graph = *base;
    index = find_node(&graph, node_id);
    if (index < 0 || find_node(&graph, source_node_id) < 0) return false;
    if (strcmp(input_id, "a") == 0)
        snprintf(graph.nodes[index].input_a,
                 sizeof(graph.nodes[index].input_a), "%s", source_node_id);
    else if (strcmp(input_id, "b") == 0)
        snprintf(graph.nodes[index].input_b,
                 sizeof(graph.nodes[index].input_b), "%s", source_node_id);
    else return false;
    if (!ProceduralSolidMaterialGraphV1_Validate(&graph, report)) return false;
    *out_graph = graph;
    return true;
}

static double hash_noise(double x, double y, double z, int seed) {
    double n = sin(x * 12.9898 + y * 78.233 + z * 37.719 +
                   (double)seed * 0.12345) * 43758.5453;
    return n - floor(n);
}

static bool eval_node(
    const ProceduralSolidMaterialGraphV1 *graph, int index,
    const ProceduralSolidMaterialGeometryInputs *in,
    double *values, unsigned char *done) {
    const ProceduralSolidMaterialNodeV1 *node = &graph->nodes[index];
    double a = 0.0, b = 0.0, t;
    int ai = find_node(graph, node->input_a);
    int bi = find_node(graph, node->input_b);
    if (done[index]) return true;
    if (node_needs_a(node->kind) &&
        (ai < 0 || !eval_node(graph, ai, in, values, done))) return false;
    if (node_needs_b(node->kind) &&
        (bi < 0 || !eval_node(graph, bi, in, values, done))) return false;
    if (ai >= 0) a = values[ai];
    if (bi >= 0) b = values[bi];
    switch (node->kind) {
        case PROCEDURAL_SOLID_MATERIAL_NODE_CONSTANT: t = node->value; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_HEIGHT: t = in->height; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_SLOPE: t = in->slope; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_CURVATURE: t = in->curvature; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_CAVITY: t = in->cavity; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_AUTHORED_REGION:
            t = in->authored_region; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_BOUNDARY_DISTANCE:
            t = in->boundary_distance; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_REGION:
            t = strcmp(node->region_kind, "cut") == 0 ? in->region_cut :
                strcmp(node->region_kind, "blend") == 0 ? in->region_blend :
                in->region_retained;
            break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_NOISE:
            t = hash_noise(in->object_x * node->scale + node->offset,
                           in->object_y * node->scale,
                           in->object_z * node->scale, node->seed);
            break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_ADD: t = a + b; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_MULTIPLY: t = a * b; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_INVERT: t = 1.0 - a; break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_SMOOTHSTEP:
            t = clamp01((a - node->minimum) /
                        (node->maximum - node->minimum));
            t = t * t * (3.0 - 2.0 * t);
            break;
        case PROCEDURAL_SOLID_MATERIAL_NODE_BANDS:
            t = 0.5 + 0.5 * sin((in->object_z * node->scale +
                                 in->object_x * 0.35 * node->scale +
                                 node->offset) * 6.283185307179586);
            break;
    }
    values[index] = clamp01(t);
    done[index] = 1u;
    return true;
}

static void blend_surface(
    ProceduralSolidAuthoredMaterialSurfaceV1 *dst,
    const ProceduralSolidAuthoredMaterialSurfaceV1 *src, double w) {
#define BLEND(field) dst->field += (src->field - dst->field) * w
    BLEND(base_color_r); BLEND(base_color_g); BLEND(base_color_b);
    BLEND(roughness); BLEND(metallic); BLEND(reflectivity); BLEND(specular);
    BLEND(emission_color_r); BLEND(emission_color_g); BLEND(emission_color_b);
    BLEND(emission_strength); BLEND(transparency); BLEND(ior);
#undef BLEND
}

bool ProceduralSolidMaterialGraphV1_EvaluateWithReadback(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidMaterialGeometryInputs *inputs,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    ProceduralSolidAuthoredMaterialSurfaceV1 *out_surface,
    double out_layer_weights[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS],
    ProceduralSolidMaterialGraphReport *report) {
    double values[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES] = {0};
    unsigned char done[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_NODES] = {0};
    bool initialized = false;
    if (!inputs || !materials || !out_surface ||
        !ProceduralSolidMaterialGraphV1_Validate(graph, report)) return false;
    for (size_t layer = 0u; layer < graph->layer_count; ++layer) {
        const ProceduralSolidAuthoredMaterialV1 *material = NULL;
        int node = find_node(graph, graph->layers[layer].weight_node_id);
        for (size_t i = 0u; i < material_count; ++i) {
            if (strcmp(materials[i].material_id,
                       graph->layers[layer].material_id) == 0) {
                material = &materials[i];
                break;
            }
        }
        if (!material || node < 0 ||
            !eval_node(graph, node, inputs, values, done)) {
            report_set(report, PROCEDURAL_SOLID_MATERIAL_GRAPH_STATUS_MATERIAL,
                       "layers", "layer material could not be resolved");
            return false;
        }
        if (out_layer_weights) out_layer_weights[layer] = values[node];
        if (!initialized) {
            *out_surface = material->surface;
            initialized = true;
        } else {
            blend_surface(out_surface, &material->surface, values[node]);
        }
    }
    return initialized;
}

bool ProceduralSolidMaterialGraphV1_Evaluate(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidMaterialGeometryInputs *inputs,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    ProceduralSolidAuthoredMaterialSurfaceV1 *out_surface,
    ProceduralSolidMaterialGraphReport *report) {
    return ProceduralSolidMaterialGraphV1_EvaluateWithReadback(
        graph, inputs, materials, material_count, out_surface, NULL, report);
}
