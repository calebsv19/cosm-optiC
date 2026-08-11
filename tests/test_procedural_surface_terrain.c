#include "procedural/procedural_surface_prism_binding.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROCEDURAL_SURFACE_FIELD_PRESET_ROOT
#define PROCEDURAL_SURFACE_FIELD_PRESET_ROOT \
    "tests/fixtures/procedural_surface_field_presets"
#endif

#ifndef PROCEDURAL_SURFACE_FIXTURE_ROOT
#define PROCEDURAL_SURFACE_FIXTURE_ROOT \
    "tests/fixtures/procedural_surface_rock_prism_psg0"
#endif

typedef struct TerrainMesh {
    ProceduralSurfacePrismVertex *vertices;
    ProceduralSurfacePrismTriangle *triangles;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfacePrismMeshSummary summary;
} TerrainMesh;

static void preset_path(char *out, size_t capacity, const char *name) {
    snprintf(out, capacity, "%s/%s",
             PROCEDURAL_SURFACE_FIELD_PRESET_ROOT, name);
}

static void terrain_mesh_free(TerrainMesh *mesh) {
    free(mesh->vertices);
    free(mesh->triangles);
    memset(mesh, 0, sizeof(*mesh));
}

static TerrainMesh generate_mountain(
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding) {
    TerrainMesh result = {0};
    ProceduralSurfacePrismBindingContext context;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceFieldBudget budget;

    assert(ProceduralSurfacePrismBindingContext_Init(
        &context, cage, binding, graph, &binding_report));
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        cage, recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
        &requirements, &mesh_report));
    result.vertices = calloc(
        (size_t)requirements.vertex_count, sizeof(*result.vertices));
    result.triangles = calloc(
        (size_t)requirements.triangle_count, sizeof(*result.triangles));
    assert(result.vertices && result.triangles);
    result.buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = result.vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = result.triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = recipe->quality.max_field_evaluations};
    assert(ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
        cage, recipe, PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
        ProceduralSurfacePrismBinding_EvaluateLegacy, &context,
        ProceduralSurfacePrismBinding_ResolveDisplacementDirection, &context,
        &budget, &result.buffers, &result.summary, &mesh_report));
    return result;
}

static unsigned boundary_axis_count(
    ProceduralSurfaceFieldPoint3D point,
    const ProceduralSurfaceCageContract *cage) {
    const double half[3] = {
        cage->width_units * 0.5,
        cage->height_units * 0.5,
        cage->depth_units * 0.5};
    const double value[3] = {point.x, point.y, point.z};
    unsigned count = 0u;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        if (fabs(fabs(value[axis]) - half[axis]) <= 1.0e-9) ++count;
    }
    return count;
}

static void test_mountain_is_a_top_bound_terrain_body(void) {
    char graph_path[1024];
    char binding_path[1024];
    ProceduralSurfaceFieldGraphV1 graph;
    ProceduralSurfaceBindingV1 binding;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfaceRecipeReport recipe_report;
    const ProceduralSurfaceCageContract cage = {
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 8.0,
        .height_units = 8.0,
        .depth_units = 0.8,
        .target_edge_length_units = 0.25};
    TerrainMesh first;
    TerrainMesh second;
    double top_max = 0.0;
    double bottom_max = 0.0;
    double side_max = 0.0;
    size_t top_count = 0u;
    size_t bottom_count = 0u;
    size_t side_count = 0u;

    preset_path(graph_path, sizeof(graph_path), "central_mountain_peak.json");
    preset_path(binding_path, sizeof(binding_path),
                "central_mountain_peak.terrain.binding.json");
    assert(ProceduralSurfaceFieldGraphV1_LoadJsonFile(
        graph_path, &graph, &graph_report));
    assert(ProceduralSurfaceBindingV1_LoadJsonFile(
        binding_path, &binding, &binding_report));
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &recipe, &recipe_report));
    snprintf(recipe.recipe_id, sizeof(recipe.recipe_id),
             "central_mountain_peak_terrain_test");
    recipe.target_edge_length_units = cage.target_edge_length_units;
    recipe.displacement_amplitude_units = 2.2;
    recipe.edge_lock_width_units = 0.36;
    assert(ProceduralSurfaceRecipeV1_Validate(&recipe, &recipe_report));

    first = generate_mountain(&cage, &recipe, &graph, &binding);
    second = generate_mountain(&cage, &recipe, &graph, &binding);
    assert(strcmp(first.summary.mesh_digest_sha256,
                  second.summary.mesh_digest_sha256) == 0);

    for (size_t i = 0u; i < first.buffers.vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *vertex = &first.vertices[i];
        const ProceduralSurfaceFieldPoint3D delta = {
            vertex->position.x - vertex->cage_position.x,
            vertex->position.y - vertex->cage_position.y,
            vertex->position.z - vertex->cage_position.z};
        const double displacement =
            sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (boundary_axis_count(vertex->cage_position, &cage) != 1u) {
            assert(displacement == 0.0);
            continue;
        }
        if (fabs(vertex->cage_position.z - 0.4) <= 1.0e-9) {
            top_max = fmax(top_max, displacement);
            ++top_count;
            assert(fabs(delta.x) <= 1.0e-12);
            assert(fabs(delta.y) <= 1.0e-12);
            assert(delta.z >= 0.0);
        } else if (fabs(vertex->cage_position.z + 0.4) <= 1.0e-9) {
            bottom_max = fmax(bottom_max, displacement);
            ++bottom_count;
        } else {
            side_max = fmax(side_max, displacement);
            ++side_count;
        }
    }

    assert(top_count > 0u && bottom_count > 0u && side_count > 0u);
    assert(top_max > 1.5);
    assert(bottom_max == 0.0);
    assert(side_max == 0.0);
    assert(fabs(first.summary.bounds_min.x + 4.0) <= 1.0e-12);
    assert(fabs(first.summary.bounds_max.x - 4.0) <= 1.0e-12);
    assert(fabs(first.summary.bounds_min.y + 4.0) <= 1.0e-12);
    assert(fabs(first.summary.bounds_max.y - 4.0) <= 1.0e-12);
    assert(fabs(first.summary.bounds_min.z + 0.4) <= 1.0e-12);
    assert(first.summary.bounds_max.z > 1.9);
    assert(first.summary.boundary_edge_count == 0u);
    assert(first.summary.connected_component_count == 1u);
    assert(first.summary.surface_group_count == 6u);
    assert(first.summary.euler_characteristic == 2);
    assert(first.summary.maximum_edge_absolute_displacement_units == 0.0);
    assert(first.summary.signed_volume_units3 > 0.0);
    assert(first.summary.minimum_outward_winding_dot > 0.0);

    printf(
        "PSG-8.5 terrain body passed: top_max=%.9f bottom_max=%.9f "
        "side_max=%.9f bounds_z=[%.9f,%.9f] digest=%s\n",
        top_max, bottom_max, side_max,
        first.summary.bounds_min.z, first.summary.bounds_max.z,
        first.summary.mesh_digest_sha256);
    terrain_mesh_free(&first);
    terrain_mesh_free(&second);
}

int main(void) {
    test_mountain_is_a_top_bound_terrain_body();
    return 0;
}
