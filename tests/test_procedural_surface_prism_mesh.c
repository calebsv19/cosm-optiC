#include "procedural/procedural_surface_mesh_asset_adapter.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <json-c/json.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PROCEDURAL_SURFACE_FIXTURE_ROOT
#define PROCEDURAL_SURFACE_FIXTURE_ROOT \
    "tests/fixtures/procedural_surface_rock_prism_psg0"
#endif

typedef struct GeneratedPrism {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceCageContract cage;
    ProceduralSurfacePrismVertex *vertices;
    ProceduralSurfacePrismTriangle *triangles;
    ProceduralSurfacePrismMesh mesh;
    ProceduralSurfacePrismMeshSummary summary;
} GeneratedPrism;

static void fail_report(const char *operation,
                        const ProceduralSurfacePrismMeshReport *report) {
    fprintf(stderr, "%s failed: status=%s field=%s message=%s\n",
            operation,
            ProceduralSurfacePrismMeshStatus_Name(report->status),
            report->field,
            report->message);
    abort();
}

static void generated_prism_free(GeneratedPrism *generated) {
    free(generated->vertices);
    free(generated->triangles);
    memset(generated, 0, sizeof(*generated));
}

static bool constant_field_evaluator(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    (void)context;
    (void)point;
    assert(budget && out_field && report);
    assert(budget->evaluations < budget->max_evaluations);
    ++budget->evaluations;
    *out_field = (ProceduralSurfaceFieldOutput){
        .height = 0.5,
        .macro_variation = 0.0,
        .micro_variation = 0.0,
        .rock_mask = 0.5,
        .roughness = 0.7,
        .snow_precursor = 0.0};
    *report = (ProceduralSurfaceFieldReport){
        .status = PROCEDURAL_SURFACE_FIELD_STATUS_OK};
    return true;
}

static bool world_up_direction(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D *out_direction) {
    (void)context;
    (void)point;
    (void)source_normal;
    assert(out_direction);
    *out_direction = (ProceduralSurfaceFieldPoint3D){0.0, 0.0, 1.0};
    return true;
}

static GeneratedPrism generate_fixture(void) {
    GeneratedPrism generated = {0};
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfacePrismMeshReport report;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfaceFieldBudget budget;

    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &generated.recipe, &recipe_report));
    generated.cage = (ProceduralSurfaceCageContract){
        .kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM,
        .width_units = 4.0,
        .height_units = 3.0,
        .depth_units = 2.0,
        .target_edge_length_units =
            generated.recipe.target_edge_length_units};
    if (!ProceduralSurfacePrismMesh_DeriveRequirements(
            &generated.cage, &generated.recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &requirements, &report)) {
        fail_report("derive requirements", &report);
    }
    assert(requirements.subdivisions_x == 16u);
    assert(requirements.subdivisions_y == 12u);
    assert(requirements.subdivisions_z == 8u);
    assert(requirements.vertex_count == 834u);
    assert(requirements.triangle_count == 1664u);
    generated.vertices = calloc(
        (size_t)requirements.vertex_count, sizeof(*generated.vertices));
    generated.triangles = calloc(
        (size_t)requirements.triangle_count, sizeof(*generated.triangles));
    assert(generated.vertices && generated.triangles);
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = generated.vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = generated.triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = generated.recipe.quality.max_field_evaluations};
    if (!ProceduralSurfacePrismMesh_Generate(
            &generated.cage, &generated.recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &budget, &buffers, &generated.summary, &report)) {
        fail_report("generate", &report);
    }
    assert(budget.evaluations == 834u);
    generated.mesh = (ProceduralSurfacePrismMesh){
        generated.vertices, buffers.vertex_count,
        generated.triangles, buffers.triangle_count};
    return generated;
}

static void test_frozen_topology_and_determinism(void) {
    GeneratedPrism first = generate_fixture();
    GeneratedPrism second = generate_fixture();
    assert(first.summary.unique_edge_count == 2496u);
    assert(first.summary.boundary_edge_count == 0u);
    assert(first.summary.connected_component_count == 1u);
    assert(first.summary.surface_group_count == 6u);
    assert(first.summary.euler_characteristic == 2);
    assert(first.summary.maximum_edge_absolute_displacement_units == 0.0);
    assert(first.summary.signed_volume_units3 > 0.0);
    assert(first.summary.minimum_outward_winding_dot > 0.0);
    assert(strcmp(first.summary.mesh_digest_sha256,
                  second.summary.mesh_digest_sha256) == 0);
    assert(memcmp(first.vertices, second.vertices,
                  first.mesh.vertex_count * sizeof(*first.vertices)) == 0);
    assert(memcmp(first.triangles, second.triangles,
                  first.mesh.triangle_count * sizeof(*first.triangles)) == 0);
    printf("PSG-3 summary: bounds=[%.17g,%.17g,%.17g]-"
           "[%.17g,%.17g,%.17g] max_displacement=%.17g "
           "min_area2=%.17g area=%.17g volume=%.17g "
           "min_winding=%.17g digest=%s\n",
           first.summary.bounds_min.x, first.summary.bounds_min.y,
           first.summary.bounds_min.z, first.summary.bounds_max.x,
           first.summary.bounds_max.y, first.summary.bounds_max.z,
           first.summary.maximum_absolute_displacement_units,
           first.summary.minimum_twice_triangle_area_units2,
           first.summary.total_surface_area_units2,
           first.summary.signed_volume_units3,
           first.summary.minimum_outward_winding_dot,
           first.summary.mesh_digest_sha256);
    generated_prism_free(&first);
    generated_prism_free(&second);
}

static void test_expected_summary_fixture(void) {
    GeneratedPrism generated = generate_fixture();
    struct json_object *root = json_object_from_file(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/expected_prism_mesh_summary.json");
    struct json_object *value = NULL;
    assert(root);
#define ASSERT_JSON_U64(key, actual) \
    do { \
        assert(json_object_object_get_ex(root, key, &value)); \
        assert((uint64_t)json_object_get_int64(value) == (uint64_t)(actual)); \
    } while (0)
#define ASSERT_JSON_DOUBLE(key, actual) \
    do { \
        assert(json_object_object_get_ex(root, key, &value)); \
        assert(fabs(json_object_get_double(value) - (actual)) <= 1.0e-12); \
    } while (0)
    ASSERT_JSON_U64("vertex_count", generated.summary.vertex_count);
    ASSERT_JSON_U64("triangle_count", generated.summary.triangle_count);
    ASSERT_JSON_U64("unique_edge_count", generated.summary.unique_edge_count);
    ASSERT_JSON_U64("boundary_edge_count", generated.summary.boundary_edge_count);
    ASSERT_JSON_U64("connected_component_count",
                    generated.summary.connected_component_count);
    ASSERT_JSON_U64("surface_group_count", generated.summary.surface_group_count);
    ASSERT_JSON_U64("euler_characteristic",
                    generated.summary.euler_characteristic);
    ASSERT_JSON_U64("field_evaluation_count",
                    generated.summary.field_evaluation_count);
    ASSERT_JSON_DOUBLE("maximum_absolute_displacement_units",
                       generated.summary.maximum_absolute_displacement_units);
    ASSERT_JSON_DOUBLE("maximum_edge_absolute_displacement_units",
                       generated.summary.maximum_edge_absolute_displacement_units);
    ASSERT_JSON_DOUBLE("minimum_twice_triangle_area_units2",
                       generated.summary.minimum_twice_triangle_area_units2);
    ASSERT_JSON_DOUBLE("total_surface_area_units2",
                       generated.summary.total_surface_area_units2);
    ASSERT_JSON_DOUBLE("signed_volume_units3",
                       generated.summary.signed_volume_units3);
    ASSERT_JSON_DOUBLE("minimum_outward_winding_dot",
                       generated.summary.minimum_outward_winding_dot);
    assert(json_object_object_get_ex(root, "mesh_digest_sha256", &value));
    assert(strcmp(json_object_get_string(value),
                  generated.summary.mesh_digest_sha256) == 0);
#undef ASSERT_JSON_U64
#undef ASSERT_JSON_DOUBLE
    json_object_put(root);
    generated_prism_free(&generated);
}

static void test_tamper_rejection(void) {
    GeneratedPrism generated = generate_fixture();
    ProceduralSurfacePrismMeshReport report;
    ProceduralSurfacePrismMeshSummary summary;
    const ProceduralSurfacePrismTriangle saved_triangle =
        generated.triangles[0];
    generated.triangles[0].b = generated.triangles[0].c;
    assert(!ProceduralSurfacePrismMesh_Validate(
        &generated.cage, &generated.recipe, &generated.mesh, 834u,
        &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TRIANGLE);
    generated.triangles[0] = saved_triangle;

    generated.triangles[0].surface_group =
        PROCEDURAL_SURFACE_PRISM_FACE_POSITIVE_X;
    assert(!ProceduralSurfacePrismMesh_Validate(
        &generated.cage, &generated.recipe, &generated.mesh, 834u,
        &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_TOPOLOGY);
    generated.triangles[0] = saved_triangle;

    size_t edge_vertex = 0u;
    while (edge_vertex < generated.mesh.vertex_count &&
           generated.vertices[edge_vertex].edge_lock_weight != 0.0) {
        ++edge_vertex;
    }
    assert(edge_vertex < generated.mesh.vertex_count);
    generated.vertices[edge_vertex].displacement_units = 0.01;
    assert(!ProceduralSurfacePrismMesh_Validate(
        &generated.cage, &generated.recipe, &generated.mesh, 834u,
        &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX);
    generated.vertices[edge_vertex].displacement_units = 0.0;
    generated.vertices[0].normal.x += 0.1;
    assert(!ProceduralSurfacePrismMesh_Validate(
        &generated.cage, &generated.recipe, &generated.mesh, 834u,
        &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX ||
           report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_NORMAL);
    generated.vertices[0].normal.x -= 0.1;
    generated.vertices[0].field.height = NAN;
    assert(!ProceduralSurfacePrismMesh_Validate(
        &generated.cage, &generated.recipe, &generated.mesh, 834u,
        &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_VERTEX);
    generated_prism_free(&generated);
}

static void test_runtime_document_round_trip(void) {
    GeneratedPrism generated = generate_fixture();
    CoreMeshAssetRuntimeDocument document;
    CoreMeshAssetRuntimeDocument loaded;
    CoreResult result;
    char path[] = "/tmp/optic_psg3_runtime_XXXXXX";
    int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    core_mesh_asset_runtime_document_init(&document);
    core_mesh_asset_runtime_document_init(&loaded);
    result = ProceduralSurfaceMeshAsset_FromPrism(
        &generated.mesh, &generated.summary,
        "procedural_rock_prism_runtime",
        "procedural_rock_prism_cage",
        &document);
    assert(result.code == CORE_OK);
    assert(document.vertex_count == 834u);
    assert(document.triangle_count == 1664u);
    assert(document.surface_group_count == 6u);
    assert(document.contract.topology_closed_volume);
    assert(document.contract.topology_manifold_expected);
    result = core_mesh_asset_runtime_document_save_file(&document, path);
    assert(result.code == CORE_OK);
    result = core_mesh_asset_runtime_document_load_file(path, &loaded);
    assert(result.code == CORE_OK);
    assert(loaded.vertex_count == document.vertex_count);
    assert(loaded.triangle_count == document.triangle_count);
    assert(loaded.surface_group_count == document.surface_group_count);
    for (size_t i = 0u; i < document.surface_group_count; ++i) {
        assert(strcmp(loaded.surface_groups[i].group_id,
                      document.surface_groups[i].group_id) == 0);
        assert(loaded.surface_groups[i].triangle_start ==
               document.surface_groups[i].triangle_start);
        assert(loaded.surface_groups[i].triangle_count ==
               document.surface_groups[i].triangle_count);
    }
    unlink(path);
    core_mesh_asset_runtime_document_free(&loaded);
    core_mesh_asset_runtime_document_free(&document);
    generated_prism_free(&generated);
}

static void test_transactional_capacity_failure(void) {
    GeneratedPrism generated = {0};
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfacePrismMeshReport report;
    ProceduralSurfacePrismVertex vertex = {0};
    ProceduralSurfacePrismTriangle triangle = {0};
    ProceduralSurfacePrismMeshBuffers buffers = {
        .vertices = &vertex, .vertex_capacity = 1u,
        .triangles = &triangle, .triangle_capacity = 1u,
        .vertex_count = 77u, .triangle_count = 88u};
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceFieldBudget budget;
    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &generated.recipe, &recipe_report));
    generated.cage = (ProceduralSurfaceCageContract){
        PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM, 4.0, 3.0, 2.0,
        generated.recipe.target_edge_length_units};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = generated.recipe.quality.max_field_evaluations};
    memset(&summary, 0x5a, sizeof(summary));
    assert(!ProceduralSurfacePrismMesh_Generate(
        &generated.cage, &generated.recipe,
        PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &budget, &buffers, &summary, &report));
    assert(report.status == PROCEDURAL_SURFACE_PRISM_MESH_STATUS_CAPACITY);
    assert(buffers.vertex_count == 77u && buffers.triangle_count == 88u);
    assert(budget.evaluations == 0u);
}

static void test_custom_displacement_direction_is_honored(void) {
    GeneratedPrism generated = {0};
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfacePrismMeshReport report;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshBuffers buffers;
    ProceduralSurfaceFieldBudget budget;
    bool found_positive_x_interior = false;

    assert(ProceduralSurfaceRecipeV1_LoadJsonFile(
        PROCEDURAL_SURFACE_FIXTURE_ROOT "/recipe.json",
        &generated.recipe, &recipe_report));
    generated.recipe.displacement_amplitude_units = 0.05;
    generated.cage = (ProceduralSurfaceCageContract){
        PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM, 4.0, 3.0, 2.0,
        generated.recipe.target_edge_length_units};
    assert(ProceduralSurfacePrismMesh_DeriveRequirements(
        &generated.cage, &generated.recipe,
        PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
        &requirements, &report));
    generated.vertices = calloc(
        (size_t)requirements.vertex_count, sizeof(*generated.vertices));
    generated.triangles = calloc(
        (size_t)requirements.triangle_count, sizeof(*generated.triangles));
    assert(generated.vertices && generated.triangles);
    buffers = (ProceduralSurfacePrismMeshBuffers){
        .vertices = generated.vertices,
        .vertex_capacity = (size_t)requirements.vertex_count,
        .triangles = generated.triangles,
        .triangle_capacity = (size_t)requirements.triangle_count};
    budget = (ProceduralSurfaceFieldBudget){
        .max_evaluations = generated.recipe.quality.max_field_evaluations};
    if (!ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
            &generated.cage, &generated.recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            constant_field_evaluator, NULL,
            world_up_direction, NULL,
            &budget, &buffers, &generated.summary, &report)) {
        fail_report("custom displacement direction", &report);
    }
    for (size_t i = 0u; i < buffers.vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *vertex = &generated.vertices[i];
        if (vertex->lattice_x == requirements.subdivisions_x &&
            vertex->lattice_y > 0u &&
            vertex->lattice_y < requirements.subdivisions_y &&
            vertex->lattice_z > 0u &&
            vertex->lattice_z < requirements.subdivisions_z) {
            const double expected =
                0.5 * generated.recipe.displacement_amplitude_units *
                vertex->edge_lock_weight;
            assert(fabs(vertex->position.x - vertex->cage_position.x) <=
                   1.0e-12);
            assert(fabs(vertex->position.y - vertex->cage_position.y) <=
                   1.0e-12);
            assert(fabs(
                (vertex->position.z - vertex->cage_position.z) -
                expected) <= 1.0e-12);
            assert(expected > 0.0);
            found_positive_x_interior = true;
            break;
        }
    }
    assert(found_positive_x_interior);
    assert(generated.summary.boundary_edge_count == 0u);
    assert(generated.summary.connected_component_count == 1u);
    assert(generated.summary.signed_volume_units3 > 0.0);
    generated_prism_free(&generated);
}

int main(void) {
    test_frozen_topology_and_determinism();
    test_expected_summary_fixture();
    test_tamper_rejection();
    test_runtime_document_round_trip();
    test_transactional_capacity_failure();
    test_custom_displacement_direction_is_honored();
    puts("procedural surface prism mesh contract passed");
    return 0;
}
