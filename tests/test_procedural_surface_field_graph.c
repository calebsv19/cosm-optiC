#include "procedural/procedural_surface_field_graph.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIELD_PRESET_ROOT
#define PROCEDURAL_SURFACE_FIELD_PRESET_ROOT \
    "tests/fixtures/procedural_surface_field_presets"
#endif

static void path_join(
    char *out,
    size_t capacity,
    const char *name) {
    snprintf(out, capacity, "%s/%s", PROCEDURAL_SURFACE_FIELD_PRESET_ROOT, name);
}

typedef struct PresetStats {
    double min_height;
    double max_height;
    double mean_height;
    double max_cavity;
    double mean_cavity;
    double min_roughness;
    double max_roughness;
    double mean_color;
    char digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY];
} PresetStats;

static PresetStats evaluate_preset(const char *filename) {
    char path[512];
    char canonical[PROCEDURAL_SURFACE_FIELD_GRAPH_CANONICAL_CAPACITY];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphReport report;
    ProceduralSurfaceFieldBudget budget = {.max_evaluations = 10000u};
    PresetStats stats = {
        .min_height = INFINITY,
        .max_height = -INFINITY,
        .min_roughness = INFINITY,
        .max_roughness = -INFINITY
    };
    size_t sample_count = 0u;
    path_join(path, sizeof(path), filename);
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(path, &graph, &report));
    assert(ProceduralSurfaceFieldGraphV1_CanonicalJson(
        &graph, canonical, sizeof(canonical), &report));
    assert(ProceduralSurfaceFieldGraphV1_Digest(
        &graph, stats.digest, &report));
    for (int y = -32; y <= 32; ++y) {
        for (int x = -32; x <= 32; ++x) {
            const ProceduralSurfaceFieldPoint3D point = {
                .x = (double)x * 0.125,
                .y = (double)y * 0.125,
                .z = 0.0
            };
            ProceduralSurfaceFieldGraphSample sample;
            assert(ProceduralSurfaceFieldGraphV1_Evaluate(
                &graph, point, &budget, &sample, &report));
            stats.min_height = fmin(stats.min_height, sample.height);
            stats.max_height = fmax(stats.max_height, sample.height);
            stats.mean_height += sample.height;
            stats.max_cavity = fmax(stats.max_cavity, sample.cavity);
            stats.mean_cavity += sample.cavity;
            stats.min_roughness =
                fmin(stats.min_roughness, sample.roughness);
            stats.max_roughness =
                fmax(stats.max_roughness, sample.roughness);
            stats.mean_color +=
                (sample.color_r + sample.color_g + sample.color_b) / 3.0;
            ++sample_count;
        }
    }
    assert(budget.evaluations == sample_count);
    stats.mean_height /= (double)sample_count;
    stats.mean_cavity /= (double)sample_count;
    stats.mean_color /= (double)sample_count;
    printf(
        "%s digest=%s height=[%.6f,%.6f] mean=%.6f "
        "cavity=[0,%.6f] mean=%.6f rough=[%.6f,%.6f] color=%.6f\n",
        filename, stats.digest, stats.min_height, stats.max_height,
        stats.mean_height, stats.max_cavity, stats.mean_cavity,
        stats.min_roughness, stats.max_roughness, stats.mean_color);
    return stats;
}

static size_t find_node(
    const ProceduralSurfaceFieldGraphV1 *graph,
    const char *id) {
    for (size_t i = 0u; i < graph->node_count; ++i) {
        if (strcmp(graph->nodes[i].id, id) == 0) return i;
    }
    assert(!"required field node is missing");
    return 0u;
}

static void test_rejections_are_transactional(void) {
    char path[512];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceFieldGraphReport report;
    ProceduralSurfaceFieldGraphSample sample = {
        .height = 7.0,
        .macro_variation = 7.0,
        .micro_variation = 7.0,
        .cavity = 7.0,
        .mask = 7.0,
        .color_r = 7.0,
        .color_g = 7.0,
        .color_b = 7.0,
        .roughness = 7.0
    };
    const ProceduralSurfaceFieldGraphSample original_sample = sample;
    ProceduralSurfaceFieldBudget budget = {
        .max_evaluations = 0u,
        .evaluations = 0u
    };
    path_join(path, sizeof(path), "pitted_concrete.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        path, &graph, &report));
    assert(!ProceduralSurfaceFieldGraphV1_Evaluate(
        &graph, (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 0.0},
        &budget, &sample, &report));
    assert(report.status == PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_BUDGET);
    assert(budget.evaluations == 0u);
    assert(memcmp(&sample, &original_sample, sizeof(sample)) == 0);

    ProceduralSurfaceFieldGraphV1 cycle = graph;
    const size_t height_index = find_node(&cycle, "height");
    assert(cycle.nodes[height_index].input_count > 0u);
    snprintf(cycle.nodes[height_index].inputs[0],
             sizeof(cycle.nodes[height_index].inputs[0]), "height");
    assert(!ProceduralSurfaceFieldGraphV1_Validate(&cycle, &report));
    assert(report.status == PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_CYCLE);

    ProceduralSurfaceFieldGraphV1 disconnected = graph;
    assert(disconnected.node_count <
           PROCEDURAL_SURFACE_FIELD_GRAPH_MAX_NODES);
    ProceduralSurfaceFieldGraphNode *orphan =
        &disconnected.nodes[disconnected.node_count++];
    memset(orphan, 0, sizeof(*orphan));
    snprintf(orphan->id, sizeof(orphan->id), "orphan");
    orphan->op = PROCEDURAL_SURFACE_FIELD_NODE_CONSTANT;
    orphan->value = 0.5;
    assert(!ProceduralSurfaceFieldGraphV1_Validate(
        &disconnected, &report));
    assert(
        report.status ==
        PROCEDURAL_SURFACE_FIELD_GRAPH_STATUS_DISCONNECTED);
}

int main(void) {
    const PresetStats concrete = evaluate_preset("pitted_concrete.json");
    const PresetStats sand = evaluate_preset("wind_shaped_sand.json");
    const PresetStats rock = evaluate_preset("rocky_terrain.json");
    const PresetStats mountain =
        evaluate_preset("central_mountain_peak.json");
    assert(concrete.min_height < -0.35 && concrete.max_cavity > 0.6);
    assert(sand.min_height < -0.55 && sand.max_height > 0.55);
    assert(rock.min_height < -0.3 && rock.max_height > 0.25);
    assert(mountain.min_height <= 0.02 && mountain.max_height > 0.75);
    assert(fabs(concrete.mean_height - sand.mean_height) > 0.02);
    assert(fabs(sand.mean_color - rock.mean_color) > 0.10);
    assert(fabs(rock.mean_color - mountain.mean_color) > 0.05);
    assert(strcmp(concrete.digest, sand.digest) != 0);
    assert(strcmp(sand.digest, rock.digest) != 0);
    assert(strcmp(rock.digest, mountain.digest) != 0);
    test_rejections_are_transactional();
    return 0;
}
