#include "core_mesh_asset.h"
#include "procedural/procedural_solid_authored_material.h"
#include "procedural/procedural_solid_material_graph.h"
#include "procedural/procedural_solid_material_runtime_program.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum DebugChannel {
    DEBUG_CHANNEL_HEIGHT = 0,
    DEBUG_CHANNEL_SIGNED_UP_SLOPE,
    DEBUG_CHANNEL_LAYER_WEIGHT
} DebugChannel;

typedef struct DebugEdge {
    size_t vertex_a;
    size_t vertex_b;
    size_t triangle_index;
    size_t local_edge;
} DebugEdge;

static int compare_edges(const void *left, const void *right) {
    const DebugEdge *a = left;
    const DebugEdge *b = right;
    if (a->vertex_a < b->vertex_a) return -1;
    if (a->vertex_a > b->vertex_a) return 1;
    if (a->vertex_b < b->vertex_b) return -1;
    if (a->vertex_b > b->vertex_b) return 1;
    return 0;
}

static const char *arg_value(
    int argc, char **argv, const char *name, const char *fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return fallback;
}

static const char *region_kind(const char *surface_group_id) {
    if (surface_group_id && strstr(surface_group_id, "cut")) return "cut";
    if (surface_group_id && strstr(surface_group_id, "blend")) return "blend";
    return "retained";
}

static bool parse_channel(const char *name, DebugChannel *out_channel) {
    if (!name || !out_channel) return false;
    if (strcmp(name, "height") == 0) {
        *out_channel = DEBUG_CHANNEL_HEIGHT;
        return true;
    }
    if (strcmp(name, "signed_up_slope") == 0) {
        *out_channel = DEBUG_CHANNEL_SIGNED_UP_SLOPE;
        return true;
    }
    if (strcmp(name, "layer_weight") == 0) {
        *out_channel = DEBUG_CHANNEL_LAYER_WEIGHT;
        return true;
    }
    return false;
}

static bool point_barycentric(
    double px, double py,
    const CoreMeshAssetRuntimeVertex *a,
    const CoreMeshAssetRuntimeVertex *b,
    const CoreMeshAssetRuntimeVertex *c,
    double *out_u, double *out_v, double *out_w) {
    double x0 = a->position.x;
    double y0 = a->position.y;
    double x1 = b->position.x;
    double y1 = b->position.y;
    double x2 = c->position.x;
    double y2 = c->position.y;
    double denominator =
        ((y1 - y2) * (x0 - x2)) + ((x2 - x1) * (y0 - y2));
    double u;
    double v;
    double w;
    if (fabs(denominator) <= 1e-15) return false;
    u = (((y1 - y2) * (px - x2)) +
         ((x2 - x1) * (py - y2))) / denominator;
    v = (((y2 - y0) * (px - x2)) +
         ((x0 - x2) * (py - y2))) / denominator;
    w = 1.0 - u - v;
    if (u < -1e-9 || v < -1e-9 || w < -1e-9) return false;
    *out_u = u;
    *out_v = v;
    *out_w = w;
    return true;
}

static bool load_materials(
    const ProceduralSolidMaterialGraphV1 *graph,
    ProceduralSolidAuthoredMaterialV1 *materials) {
    for (size_t i = 0u; i < graph->layer_count; ++i) {
        ProceduralSolidAuthoredMaterialReport report;
        if (!ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                graph->layers[i].material_path, &materials[i], &report) ||
            strcmp(materials[i].material_id,
                   graph->layers[i].material_id) != 0) {
            return false;
        }
    }
    return true;
}

static double sample_channel(
    DebugChannel channel,
    const ProceduralSolidMaterialRuntimeSampleV1 *sample,
    size_t layer_index) {
    if (channel == DEBUG_CHANNEL_HEIGHT) return sample->geometry.height;
    if (channel == DEBUG_CHANNEL_SIGNED_UP_SLOPE)
        return sample->geometry.slope;
    return layer_index < sample->layer_count
               ? sample->layer_weights[layer_index]
               : 0.0;
}

static bool sample_edge_midpoint(
    const ProceduralSolidMaterialRuntimeProgramV1 *program,
    const DebugEdge *edge,
    DebugChannel channel,
    size_t layer_index,
    double *out_value,
    ProceduralSolidMaterialGraphReport *report) {
    static const double barycentrics[3][3] = {
        {0.5, 0.5, 0.0},
        {0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5},
    };
    ProceduralSolidMaterialRuntimeSampleV1 sample;
    const double *bary;
    if (!program || !edge || edge->local_edge >= 3u || !out_value)
        return false;
    bary = barycentrics[edge->local_edge];
    if (!ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
            program, edge->triangle_index,
            bary[0], bary[1], bary[2], &sample, report)) {
        return false;
    }
    *out_value = sample_channel(channel, &sample, layer_index);
    return true;
}

static bool write_pgm(
    const char *path, const unsigned char *pixels, int width, int height) {
    FILE *file = fopen(path, "wb");
    bool ok;
    if (!file) return false;
    ok = fprintf(file, "P5\n%d %d\n255\n", width, height) > 0 &&
         fwrite(pixels, 1u, (size_t)width * (size_t)height, file) ==
             (size_t)width * (size_t)height;
    if (fclose(file) != 0) ok = false;
    return ok;
}

int main(int argc, char **argv) {
    const char *graph_path = arg_value(argc, argv, "--graph", NULL);
    const char *mesh_path = arg_value(argc, argv, "--mesh", NULL);
    const char *output_path = arg_value(argc, argv, "--output", NULL);
    const char *channel_name =
        arg_value(argc, argv, "--channel", "layer_weight");
    int width = atoi(arg_value(argc, argv, "--width", "256"));
    int height = atoi(arg_value(argc, argv, "--height", "256"));
    size_t layer_index =
        (size_t)strtoul(arg_value(argc, argv, "--layer", "1"), NULL, 10);
    CoreMeshAssetRuntimeDocument mesh;
    ProceduralSolidMaterialGraphV1 graph;
    ProceduralSolidMaterialGraphReport report = {0};
    ProceduralSolidAuthoredMaterialV1
        materials[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    ProceduralSolidMaterialRuntimeProgramV1 program;
    const char **region_kinds = NULL;
    double *depth = NULL;
    double *values = NULL;
    unsigned char *pixels = NULL;
    DebugEdge *edges = NULL;
    size_t covered = 0u;
    size_t compared_internal_edges = 0u;
    size_t pixel_count;
    size_t edge_count;
    double maximum_internal_edge_jump = 0.0;
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    DebugChannel channel;
    int exit_code = 1;
    core_mesh_asset_runtime_document_init(&mesh);
    ProceduralSolidMaterialRuntimeProgramV1_Init(&program);
    memset(materials, 0, sizeof(materials));
    if (!graph_path || !mesh_path || !output_path ||
        width < 8 || height < 8 || width > 4096 || height > 4096 ||
        !parse_channel(channel_name, &channel) ||
        core_mesh_asset_runtime_document_load_file(mesh_path, &mesh).code !=
            CORE_OK ||
        !ProceduralSolidMaterialGraphV1_LoadJsonFile(
            graph_path, &graph, &report) ||
        !load_materials(&graph, materials) ||
        (channel == DEBUG_CHANNEL_LAYER_WEIGHT &&
         layer_index >= graph.layer_count)) {
        fprintf(stderr, "invalid debug request\n");
        goto cleanup;
    }
    if (mesh.triangle_count > SIZE_MAX / 3u) {
        fprintf(stderr, "debug mesh edge count exceeds capacity\n");
        goto cleanup;
    }
    pixel_count = (size_t)width * (size_t)height;
    edge_count = mesh.triangle_count * 3u;
    region_kinds = calloc(mesh.triangle_count, sizeof(*region_kinds));
    depth = malloc(pixel_count * sizeof(*depth));
    values = calloc(pixel_count, sizeof(*values));
    pixels = calloc(pixel_count, sizeof(*pixels));
    edges = calloc(edge_count, sizeof(*edges));
    if ((!region_kinds && mesh.triangle_count > 0u) ||
        !depth || !values || !pixels ||
        (!edges && mesh.triangle_count > 0u)) {
        goto cleanup;
    }
    for (size_t i = 0u; i < mesh.triangle_count; ++i)
        region_kinds[i] = region_kind(mesh.triangles[i].surface_group_id);
    if (!ProceduralSolidMaterialRuntimeProgramV1_Build(
            &graph, materials, graph.layer_count, &mesh, region_kinds,
            &program, &report)) {
        fprintf(stderr, "%s\n", report.message);
        goto cleanup;
    }
    for (size_t i = 0u; i < mesh.triangle_count; ++i) {
        size_t vertices[3] = {
            mesh.triangles[i].a,
            mesh.triangles[i].b,
            mesh.triangles[i].c,
        };
        for (size_t edge = 0u; edge < 3u; ++edge) {
            size_t a = vertices[edge];
            size_t b = vertices[(edge + 1u) % 3u];
            DebugEdge *record = &edges[(i * 3u) + edge];
            record->vertex_a = a < b ? a : b;
            record->vertex_b = a < b ? b : a;
            record->triangle_index = i;
            record->local_edge = edge;
        }
    }
    qsort(edges, edge_count, sizeof(*edges), compare_edges);
    for (size_t i = 0u; i + 1u < edge_count;) {
        size_t end = i + 1u;
        while (end < edge_count &&
               edges[end].vertex_a == edges[i].vertex_a &&
               edges[end].vertex_b == edges[i].vertex_b) {
            ++end;
        }
        if (end - i == 2u &&
            strcmp(region_kinds[edges[i].triangle_index],
                   region_kinds[edges[i + 1u].triangle_index]) == 0) {
            double left;
            double right;
            double jump;
            if (!sample_edge_midpoint(
                    &program, &edges[i], channel, layer_index,
                    &left, &report) ||
                !sample_edge_midpoint(
                    &program, &edges[i + 1u], channel, layer_index,
                    &right, &report)) {
                goto cleanup;
            }
            jump = fabs(left - right);
            if (jump > maximum_internal_edge_jump)
                maximum_internal_edge_jump = jump;
            compared_internal_edges += 1u;
        }
        i = end;
    }
    for (size_t i = 0u; i < pixel_count; ++i)
        depth[i] = -DBL_MAX;
    for (size_t triangle_index = 0u;
         triangle_index < mesh.triangle_count; ++triangle_index) {
        const CoreMeshAssetRuntimeTriangle *triangle =
            &mesh.triangles[triangle_index];
        const CoreMeshAssetRuntimeVertex *a = &mesh.vertices[triangle->a];
        const CoreMeshAssetRuntimeVertex *b = &mesh.vertices[triangle->b];
        const CoreMeshAssetRuntimeVertex *c = &mesh.vertices[triangle->c];
        double min_x = fmin(a->position.x,
                            fmin(b->position.x, c->position.x));
        double max_x = fmax(a->position.x,
                            fmax(b->position.x, c->position.x));
        double min_y = fmin(a->position.y,
                            fmin(b->position.y, c->position.y));
        double max_y = fmax(a->position.y,
                            fmax(b->position.y, c->position.y));
        int x0 = (int)floor(
            (min_x - mesh.contract.local_bounds.min.x) /
            (mesh.contract.local_bounds.max.x -
             mesh.contract.local_bounds.min.x) * width);
        int x1 = (int)ceil(
            (max_x - mesh.contract.local_bounds.min.x) /
            (mesh.contract.local_bounds.max.x -
             mesh.contract.local_bounds.min.x) * width);
        int y0 = (int)floor(
            (min_y - mesh.contract.local_bounds.min.y) /
            (mesh.contract.local_bounds.max.y -
             mesh.contract.local_bounds.min.y) * height);
        int y1 = (int)ceil(
            (max_y - mesh.contract.local_bounds.min.y) /
            (mesh.contract.local_bounds.max.y -
             mesh.contract.local_bounds.min.y) * height);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= width) x1 = width - 1;
        if (y1 >= height) y1 = height - 1;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                double px = mesh.contract.local_bounds.min.x +
                    (((double)x + 0.5) / width) *
                    (mesh.contract.local_bounds.max.x -
                     mesh.contract.local_bounds.min.x);
                double py = mesh.contract.local_bounds.min.y +
                    (((double)y + 0.5) / height) *
                    (mesh.contract.local_bounds.max.y -
                     mesh.contract.local_bounds.min.y);
                double u;
                double v;
                double w;
                double z;
                size_t pixel;
                ProceduralSolidMaterialRuntimeSampleV1 sample;
                if (!point_barycentric(px, py, a, b, c, &u, &v, &w))
                    continue;
                z = (a->position.z * u) +
                    (b->position.z * v) +
                    (c->position.z * w);
                pixel = (size_t)(height - 1 - y) * (size_t)width +
                        (size_t)x;
                if (z <= depth[pixel]) continue;
                if (!ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
                        &program, triangle_index, u, v, w,
                        &sample, &report)) {
                    goto cleanup;
                }
                depth[pixel] = z;
                values[pixel] =
                    sample_channel(channel, &sample, layer_index);
            }
        }
    }
    for (size_t i = 0u; i < pixel_count; ++i) {
        double value;
        if (depth[i] == -DBL_MAX) continue;
        value = values[i];
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
        pixels[i] = (unsigned char)lround(value * 255.0);
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        covered += 1u;
    }
    if (covered == 0u || !write_pgm(output_path, pixels, width, height))
        goto cleanup;
    printf("{\"status\":\"ok\",\"channel\":\"%s\",\"layer\":%zu,"
           "\"width\":%d,\"height\":%d,\"covered_pixels\":%zu,"
           "\"minimum\":%.17g,\"maximum\":%.17g,"
           "\"compared_internal_edges\":%zu,"
           "\"maximum_internal_edge_jump\":%.17g,"
           "\"continuous_inputs\":[\"height\",\"signed_up_slope\","
           "\"object_position\"],\"compatibility_inputs\":["
           "\"curvature\",\"cavity\",\"boundary_distance\",\"region\"]}\n",
           channel_name, layer_index, width, height, covered,
           minimum, maximum, compared_internal_edges,
           maximum_internal_edge_jump);
    exit_code = 0;
cleanup:
    free(region_kinds);
    free(depth);
    free(values);
    free(pixels);
    free(edges);
    ProceduralSolidMaterialRuntimeProgramV1_Free(&program);
    core_mesh_asset_runtime_document_free(&mesh);
    return exit_code;
}
