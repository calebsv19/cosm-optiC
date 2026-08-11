#include "procedural/procedural_surface_plane_mesh.h"

#include <json-c/json.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_ROOT "tests/fixtures/procedural_surface_rock_prism_psg0"
#define PLANE_VERTEX_COUNT 221u
#define PLANE_TRIANGLE_COUNT 384u

static int failures = 0;

static void expect_true(bool condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    failures += 1;
}

static void expect_u64(uint64_t actual,
                       uint64_t expected,
                       const char *message) {
    if (actual == expected) return;
    fprintf(stderr,
            "FAIL: %s (actual=%llu expected=%llu)\n",
            message,
            (unsigned long long)actual,
            (unsigned long long)expected);
    failures += 1;
}

static bool load_recipe_and_cage(ProceduralSurfaceRecipeV1 *recipe,
                                 ProceduralSurfaceCageContract *cage) {
    ProceduralSurfaceRecipeReport recipe_report;
    struct json_object *root =
        json_object_from_file(FIXTURE_ROOT "/cages.json");
    struct json_object *cages = NULL;
    struct json_object *plane = NULL;
    struct json_object *dimensions = NULL;
    struct json_object *edge = NULL;
    bool ok = true;

    if (!ProceduralSurfaceRecipeV1_LoadJsonFile(
            FIXTURE_ROOT "/recipe.json", recipe, &recipe_report)) {
        fprintf(stderr, "FAIL: %s\n", recipe_report.message);
        failures += 1;
        ok = false;
    }
    if (!root ||
        !json_object_object_get_ex(root, "cages", &cages) ||
        json_object_array_length(cages) == 0u) {
        fprintf(stderr, "FAIL: unable to load plane cage fixture\n");
        failures += 1;
        if (root) json_object_put(root);
        return false;
    }
    plane = json_object_array_get_idx(cages, 0u);
    if (!json_object_object_get_ex(plane, "dimensions", &dimensions) ||
        !json_object_object_get_ex(
            plane, "target_edge_length_units", &edge)) {
        fprintf(stderr, "FAIL: plane cage fixture is incomplete\n");
        failures += 1;
        json_object_put(root);
        return false;
    }
    memset(cage, 0, sizeof(*cage));
    cage->kind = PROCEDURAL_SURFACE_CAGE_PLANE;
    cage->width_units =
        json_object_get_double(json_object_array_get_idx(dimensions, 0u));
    cage->height_units =
        json_object_get_double(json_object_array_get_idx(dimensions, 1u));
    cage->depth_units =
        json_object_get_double(json_object_array_get_idx(dimensions, 2u));
    cage->target_edge_length_units = json_object_get_double(edge);
    json_object_put(root);
    return ok;
}

static void test_quality_scaling(
    const ProceduralSurfaceRecipeV1 *base_recipe,
    const ProceduralSurfaceCageContract *base_cage) {
    ProceduralSurfaceRecipeV1 recipe = *base_recipe;
    ProceduralSurfaceCageContract cage = *base_cage;
    ProceduralSurfacePlaneMeshRequirements requirements;
    ProceduralSurfacePlaneMeshRequirements unchanged;
    ProceduralSurfacePlaneMeshReport report;

    expect_true(
        ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &requirements,
            &report),
        report.message);
    expect_u64(requirements.subdivisions_x, 16u, "preview subdivisions x");
    expect_u64(requirements.subdivisions_y, 12u, "preview subdivisions y");
    expect_u64(requirements.vertex_count, PLANE_VERTEX_COUNT,
               "preview vertex count");
    expect_u64(requirements.triangle_count, PLANE_TRIANGLE_COUNT,
               "preview triangle count");

    recipe.target_edge_length_units = 0.03125;
    recipe.displacement_amplitude_units = 0.015;
    recipe.edge_lock_width_units = 0.03125;
    cage.target_edge_length_units = 0.03125;
    memset(&requirements, 0x5a, sizeof(requirements));
    unchanged = requirements;
    expect_true(
        !ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &requirements,
            &report),
        "inspection-scale plane is rejected by preview budget");
    expect_true(memcmp(&requirements, &unchanged, sizeof(requirements)) == 0,
                "failed requirement derivation leaves output unchanged");
    expect_true(
        ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION,
            &requirements,
            &report),
        report.message);
    expect_u64(requirements.vertex_count, 12513u,
               "inspection-scale vertex count");
    expect_u64(requirements.triangle_count, 24576u,
               "inspection-scale triangle count");

    recipe.target_edge_length_units = 0.0078125;
    recipe.displacement_amplitude_units = 0.003;
    recipe.edge_lock_width_units = 0.0078125;
    cage.target_edge_length_units = 0.0078125;
    expect_true(
        !ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_INSPECTION,
            &requirements,
            &report),
        "final-scale plane is rejected by inspection budget");
    expect_true(
        ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
            &requirements,
            &report),
        report.message);
    expect_u64(requirements.vertex_count, 197505u,
               "final-scale vertex count");
    expect_u64(requirements.triangle_count, 393216u,
               "final-scale triangle count");

    recipe.target_edge_length_units = 0.00390625;
    recipe.displacement_amplitude_units = 0.0015;
    recipe.edge_lock_width_units = 0.00390625;
    cage.target_edge_length_units = 0.00390625;
    expect_true(
        !ProceduralSurfacePlaneMesh_DeriveRequirements(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_FINAL,
            &requirements,
            &report),
        "over-million-triangle plane is rejected by final budget");
}

static bool load_expected_summary(
    ProceduralSurfacePlaneMeshSummary *expected,
    bool *out_frozen) {
    struct json_object *root = json_object_from_file(
        FIXTURE_ROOT "/expected_plane_mesh_summary.json");
    struct json_object *value = NULL;
    struct json_object *bounds = NULL;
    struct json_object *subdivisions = NULL;
    if (!root) {
        fprintf(stderr, "FAIL: unable to load plane mesh summary fixture\n");
        failures += 1;
        return false;
    }
#define GET_FIELD(key)                                                        \
    (json_object_object_get_ex(root, (key), &value) ? value : NULL)
    value = GET_FIELD("numeric_values_status");
    *out_frozen =
        value && strcmp(json_object_get_string(value), "frozen") == 0;
    value = GET_FIELD("subdivisions");
    subdivisions = value;
    expected->subdivisions_x = (uint64_t)json_object_get_int64(
        json_object_array_get_idx(subdivisions, 0u));
    expected->subdivisions_y = (uint64_t)json_object_get_int64(
        json_object_array_get_idx(subdivisions, 1u));
    value = GET_FIELD("vertex_count");
    expected->vertex_count = json_object_get_uint64(value);
    value = GET_FIELD("triangle_count");
    expected->triangle_count = json_object_get_uint64(value);
    value = GET_FIELD("field_evaluation_count");
    expected->field_evaluation_count = json_object_get_uint64(value);
    if (*out_frozen) {
        value = GET_FIELD("bounds_min");
        bounds = value;
        expected->bounds_min.x =
            json_object_get_double(json_object_array_get_idx(bounds, 0u));
        expected->bounds_min.y =
            json_object_get_double(json_object_array_get_idx(bounds, 1u));
        expected->bounds_min.z =
            json_object_get_double(json_object_array_get_idx(bounds, 2u));
        value = GET_FIELD("bounds_max");
        bounds = value;
        expected->bounds_max.x =
            json_object_get_double(json_object_array_get_idx(bounds, 0u));
        expected->bounds_max.y =
            json_object_get_double(json_object_array_get_idx(bounds, 1u));
        expected->bounds_max.z =
            json_object_get_double(json_object_array_get_idx(bounds, 2u));
        value = GET_FIELD("maximum_absolute_displacement_units");
        expected->maximum_absolute_displacement_units =
            json_object_get_double(value);
        value = GET_FIELD("maximum_boundary_absolute_displacement_units");
        expected->maximum_boundary_absolute_displacement_units =
            json_object_get_double(value);
        value = GET_FIELD("minimum_twice_triangle_area_units2");
        expected->minimum_twice_triangle_area_units2 =
            json_object_get_double(value);
        value = GET_FIELD("total_surface_area_units2");
        expected->total_surface_area_units2 =
            json_object_get_double(value);
        value = GET_FIELD("minimum_normal_z");
        expected->minimum_normal_z = json_object_get_double(value);
        value = GET_FIELD("mesh_digest_sha256");
        snprintf(expected->mesh_digest_sha256,
                 sizeof(expected->mesh_digest_sha256),
                 "%s",
                 value ? json_object_get_string(value) : "");
    }
#undef GET_FIELD
    json_object_put(root);
    return true;
}

static void test_transactional_capacity_failure(
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceCageContract *cage) {
    ProceduralSurfacePlaneVertex vertices[PLANE_VERTEX_COUNT];
    ProceduralSurfacePlaneTriangle triangles[PLANE_TRIANGLE_COUNT];
    ProceduralSurfacePlaneMeshBuffers buffers;
    ProceduralSurfacePlaneMeshBuffers unchanged_buffers;
    ProceduralSurfacePlaneMeshSummary summary;
    ProceduralSurfacePlaneMeshSummary unchanged_summary;
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfaceFieldBudget unchanged_budget;
    ProceduralSurfacePlaneMeshReport report;

    memset(vertices, 0x5a, sizeof(vertices));
    memset(triangles, 0x5a, sizeof(triangles));
    memset(&summary, 0x5a, sizeof(summary));
    unchanged_summary = summary;
    buffers = (ProceduralSurfacePlaneMeshBuffers){
        .vertices = vertices,
        .vertex_capacity = PLANE_VERTEX_COUNT - 1u,
        .triangles = triangles,
        .triangle_capacity = PLANE_TRIANGLE_COUNT,
        .vertex_count = 7u,
        .triangle_count = 9u
    };
    unchanged_buffers = buffers;
    ProceduralSurfaceFieldBudget_Init(recipe, &budget);
    unchanged_budget = budget;
    expect_true(
        !ProceduralSurfacePlaneMesh_Generate(
            cage,
            recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &budget,
            &buffers,
            &summary,
            &report),
        "insufficient caller capacity is rejected");
    expect_true(report.status ==
                    PROCEDURAL_SURFACE_PLANE_MESH_STATUS_CAPACITY,
                "capacity failure has typed status");
    expect_true(memcmp(&buffers, &unchanged_buffers, sizeof(buffers)) == 0,
                "capacity failure leaves buffer metadata unchanged");
    expect_true(memcmp(&summary, &unchanged_summary, sizeof(summary)) == 0,
                "capacity failure leaves summary unchanged");
    expect_true(memcmp(&budget, &unchanged_budget, sizeof(budget)) == 0,
                "capacity failure leaves field budget unchanged");
    for (size_t i = 0u; i < sizeof(vertices); ++i) {
        if (((const unsigned char *)vertices)[i] != 0x5a) {
            expect_true(false, "capacity failure leaves vertices unchanged");
            break;
        }
    }
    for (size_t i = 0u; i < sizeof(triangles); ++i) {
        if (((const unsigned char *)triangles)[i] != 0x5a) {
            expect_true(false, "capacity failure leaves triangles unchanged");
            break;
        }
    }
}

int main(void) {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceCageContract cage;
    ProceduralSurfacePlaneVertex vertices_a[PLANE_VERTEX_COUNT];
    ProceduralSurfacePlaneVertex vertices_b[PLANE_VERTEX_COUNT];
    ProceduralSurfacePlaneTriangle triangles_a[PLANE_TRIANGLE_COUNT];
    ProceduralSurfacePlaneTriangle triangles_b[PLANE_TRIANGLE_COUNT];
    ProceduralSurfacePlaneMeshBuffers buffers_a = {
        .vertices = vertices_a,
        .vertex_capacity = PLANE_VERTEX_COUNT,
        .triangles = triangles_a,
        .triangle_capacity = PLANE_TRIANGLE_COUNT
    };
    ProceduralSurfacePlaneMeshBuffers buffers_b = {
        .vertices = vertices_b,
        .vertex_capacity = PLANE_VERTEX_COUNT,
        .triangles = triangles_b,
        .triangle_capacity = PLANE_TRIANGLE_COUNT
    };
    ProceduralSurfacePlaneMeshSummary summary_a;
    ProceduralSurfacePlaneMeshSummary summary_b;
    ProceduralSurfacePlaneMeshSummary expected = {0};
    ProceduralSurfaceFieldBudget budget_a;
    ProceduralSurfaceFieldBudget budget_b;
    ProceduralSurfacePlaneMeshReport report;
    bool expected_frozen = false;

    expect_true(load_recipe_and_cage(&recipe, &cage),
                "recipe and plane cage load");
    test_quality_scaling(&recipe, &cage);
    ProceduralSurfaceFieldBudget_Init(&recipe, &budget_a);
    ProceduralSurfaceFieldBudget_Init(&recipe, &budget_b);
    expect_true(
        ProceduralSurfacePlaneMesh_Generate(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &budget_a,
            &buffers_a,
            &summary_a,
            &report),
        report.message);
    expect_true(
        ProceduralSurfacePlaneMesh_Generate(
            &cage,
            &recipe,
            PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW,
            &budget_b,
            &buffers_b,
            &summary_b,
            &report),
        report.message);
    expect_u64(buffers_a.vertex_count, PLANE_VERTEX_COUNT,
               "generated plane vertex count");
    expect_u64(buffers_a.triangle_count, PLANE_TRIANGLE_COUNT,
               "generated plane triangle count");
    expect_u64(budget_a.evaluations, PLANE_VERTEX_COUNT,
               "generated plane field evaluation count");
    expect_true(memcmp(vertices_a, vertices_b, sizeof(vertices_a)) == 0,
                "plane vertices are exactly repeatable");
    expect_true(memcmp(triangles_a, triangles_b, sizeof(triangles_a)) == 0,
                "plane triangles are exactly repeatable");
    expect_true(memcmp(&summary_a, &summary_b, sizeof(summary_a)) == 0,
                "plane summary is exactly repeatable");
    expect_true(summary_a.bounds_min.x == -2.0 &&
                    summary_a.bounds_min.y == -1.5 &&
                    summary_a.bounds_max.x == 2.0 &&
                    summary_a.bounds_max.y == 1.5,
                "plane retains semantic cage XY bounds");
    expect_true(
        summary_a.maximum_absolute_displacement_units > 0.0 &&
            summary_a.maximum_absolute_displacement_units <=
                recipe.displacement_amplitude_units,
        "plane displacement is nonzero and amplitude bounded");
    expect_true(
        summary_a.maximum_boundary_absolute_displacement_units == 0.0,
        "plane boundary displacement is exactly zero");
    expect_true(summary_a.minimum_twice_triangle_area_units2 >= 0.0625,
                "all fixed-diagonal triangles retain positive projected area");
    expect_true(summary_a.total_surface_area_units2 > 12.0,
                "displacement increases plane surface area");
    expect_true(summary_a.minimum_normal_z > 0.0,
                "all recomputed normals retain upward orientation");

    expect_true(load_expected_summary(&expected, &expected_frozen),
                "plane summary fixture loads");
    expect_u64(summary_a.subdivisions_x, expected.subdivisions_x,
               "summary subdivisions x");
    expect_u64(summary_a.subdivisions_y, expected.subdivisions_y,
               "summary subdivisions y");
    expect_u64(summary_a.vertex_count, expected.vertex_count,
               "summary vertex count");
    expect_u64(summary_a.triangle_count, expected.triangle_count,
               "summary triangle count");
    expect_u64(summary_a.field_evaluation_count,
               expected.field_evaluation_count,
               "summary evaluation count");
    if (expected_frozen) {
        expect_true(memcmp(&summary_a.bounds_min,
                           &expected.bounds_min,
                           sizeof(summary_a.bounds_min)) == 0 &&
                        memcmp(&summary_a.bounds_max,
                               &expected.bounds_max,
                               sizeof(summary_a.bounds_max)) == 0,
                    "plane bounds match frozen fixture");
        expect_true(
            summary_a.maximum_absolute_displacement_units ==
                    expected.maximum_absolute_displacement_units &&
                summary_a.maximum_boundary_absolute_displacement_units ==
                    expected.maximum_boundary_absolute_displacement_units &&
                summary_a.minimum_twice_triangle_area_units2 ==
                    expected.minimum_twice_triangle_area_units2 &&
                summary_a.total_surface_area_units2 ==
                    expected.total_surface_area_units2 &&
                summary_a.minimum_normal_z == expected.minimum_normal_z,
            "plane metrics match frozen fixture");
        expect_true(
            strcmp(summary_a.mesh_digest_sha256,
                   expected.mesh_digest_sha256) == 0,
            "plane mesh digest matches frozen fixture");
    } else {
        printf(
            "PSG-2 summary to freeze:\n"
            "bounds_min %.17g %.17g %.17g\n"
            "bounds_max %.17g %.17g %.17g\n"
            "maximum_absolute_displacement_units %.17g\n"
            "maximum_boundary_absolute_displacement_units %.17g\n"
            "minimum_twice_triangle_area_units2 %.17g\n"
            "total_surface_area_units2 %.17g\n"
            "minimum_normal_z %.17g\n"
            "mesh_digest_sha256 %s\n",
            summary_a.bounds_min.x,
            summary_a.bounds_min.y,
            summary_a.bounds_min.z,
            summary_a.bounds_max.x,
            summary_a.bounds_max.y,
            summary_a.bounds_max.z,
            summary_a.maximum_absolute_displacement_units,
            summary_a.maximum_boundary_absolute_displacement_units,
            summary_a.minimum_twice_triangle_area_units2,
            summary_a.total_surface_area_units2,
            summary_a.minimum_normal_z,
            summary_a.mesh_digest_sha256);
    }

    {
        ProceduralSurfacePlaneMesh mesh = {
            .vertices = vertices_a,
            .vertex_count = PLANE_VERTEX_COUNT,
            .triangles = triangles_a,
            .triangle_count = PLANE_TRIANGLE_COUNT
        };
        ProceduralSurfacePlaneMeshSummary validation_summary;
        const ProceduralSurfacePlaneTriangle saved = triangles_a[0];
        triangles_a[0].b = triangles_a[0].c;
        expect_true(
            !ProceduralSurfacePlaneMesh_Validate(
                &cage,
                &recipe,
                &mesh,
                PLANE_VERTEX_COUNT,
                &validation_summary,
                &report),
            "tampered fixed-diagonal triangle is rejected");
        triangles_a[0] = saved;

        {
            const ProceduralSurfacePlaneVertex saved_vertex = vertices_a[0];
            vertices_a[0].displacement_units = 0.01;
            vertices_a[0].position.z = 0.01;
            expect_true(
                !ProceduralSurfacePlaneMesh_Validate(
                    &cage,
                    &recipe,
                    &mesh,
                    PLANE_VERTEX_COUNT,
                    &validation_summary,
                    &report),
                "tampered boundary displacement is rejected");
            vertices_a[0] = saved_vertex;
        }
        {
            const size_t center = 6u * 17u + 8u;
            const ProceduralSurfacePlaneVertex saved_vertex =
                vertices_a[center];
            vertices_a[center].normal =
                (ProceduralSurfaceFieldPoint3D){.x = 0.0, .y = 0.0, .z = 1.0};
            expect_true(
                !ProceduralSurfacePlaneMesh_Validate(
                    &cage,
                    &recipe,
                    &mesh,
                    PLANE_VERTEX_COUNT,
                    &validation_summary,
                    &report),
                "normal inconsistent with geometry is rejected");
            vertices_a[center] = saved_vertex;
        }
        {
            const size_t center = 6u * 17u + 8u;
            const ProceduralSurfacePlaneVertex saved_vertex =
                vertices_a[center];
            vertices_a[center].field.roughness = NAN;
            expect_true(
                !ProceduralSurfacePlaneMesh_Validate(
                    &cage,
                    &recipe,
                    &mesh,
                    PLANE_VERTEX_COUNT,
                    &validation_summary,
                    &report),
                "non-finite retained field output is rejected");
            vertices_a[center] = saved_vertex;
        }
    }

    test_transactional_capacity_failure(&recipe, &cage);

    if (failures != 0) {
        fprintf(stderr,
                "procedural surface PSG-2 plane tests failed: %d\n",
                failures);
        return EXIT_FAILURE;
    }
    printf("procedural surface PSG-2 plane tests passed "
           "(mesh_sha256=%s)\n",
           summary_a.mesh_digest_sha256);
    return EXIT_SUCCESS;
}
