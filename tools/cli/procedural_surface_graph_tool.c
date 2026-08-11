#include "procedural/procedural_surface_graph.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr, "usage: %s --graph PATH\n", program);
}

int main(int argc, char **argv) {
    const char *graph_path = NULL;
    ProceduralSurfaceGraphV1 graph;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceGraphCompilePlan plan;
    ProceduralSurfaceGraphReport graph_report;
    ProceduralSurfaceRecipeReport recipe_report;
    char plan_json[2048];
    char recipe_json[PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY];

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--graph") == 0 && i + 1 < argc) {
            graph_path = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (!graph_path) {
        usage(argv[0]);
        return 2;
    }
    if (!ProceduralSurfaceGraphV1_LoadJsonFile(
            graph_path, &graph, &graph_report) ||
        !ProceduralSurfaceGraphV1_CompileRecipe(
            &graph, &recipe, &plan, &graph_report) ||
        !ProceduralSurfaceGraphCompilePlan_CanonicalJson(
            &plan, plan_json, sizeof(plan_json), &graph_report) ||
        !ProceduralSurfaceRecipeV1_CanonicalJson(
            &recipe, recipe_json, sizeof(recipe_json), &recipe_report)) {
        fprintf(stderr, "procedural graph compile failed: %s (%s)\n",
                graph_report.message[0] ?
                    graph_report.message : recipe_report.message,
                graph_report.field[0] ?
                    graph_report.field : recipe_report.field);
        return 1;
    }
    printf("{\"compile_plan\":%s,\"compiled_recipe\":%s}\n",
           plan_json, recipe_json);
    return 0;
}
