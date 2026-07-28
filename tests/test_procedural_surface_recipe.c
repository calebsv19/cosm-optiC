#include "procedural/procedural_surface_recipe.h"
#include "procedural/procedural_surface_topology_contract.h"

#include <json-c/json.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIXTURE_ROOT "tests/fixtures/procedural_surface_rock_prism_psg0"

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

static struct json_object *load_json(const char *path) {
    struct json_object *root = json_object_from_file(path);
    if (!root) {
        fprintf(stderr, "FAIL: unable to load fixture %s\n", path);
        failures += 1;
    }
    return root;
}

static struct json_object *object_field(struct json_object *parent,
                                        const char *key) {
    struct json_object *value = NULL;
    if (!parent || !json_object_object_get_ex(parent, key, &value)) {
        fprintf(stderr, "FAIL: missing fixture field %s\n", key);
        failures += 1;
        return NULL;
    }
    return value;
}

static uint64_t object_u64(struct json_object *parent, const char *key) {
    struct json_object *value = object_field(parent, key);
    return value ? json_object_get_uint64(value) : 0u;
}

static bool object_bool(struct json_object *parent, const char *key) {
    struct json_object *value = object_field(parent, key);
    return value && json_object_get_boolean(value);
}

static const char *object_string(struct json_object *parent, const char *key) {
    struct json_object *value = object_field(parent, key);
    return value ? json_object_get_string(value) : NULL;
}

static double array_double(struct json_object *array, size_t index) {
    struct json_object *value = NULL;
    if (!array || !json_object_is_type(array, json_type_array) ||
        index >= json_object_array_length(array)) {
        fprintf(stderr, "FAIL: invalid fixture array access at %zu\n", index);
        failures += 1;
        return 0.0;
    }
    value = json_object_array_get_idx(array, index);
    return value ? json_object_get_double(value) : 0.0;
}

static uint64_t array_u64(struct json_object *array, size_t index) {
    struct json_object *value = NULL;
    if (!array || !json_object_is_type(array, json_type_array) ||
        index >= json_object_array_length(array)) {
        fprintf(stderr, "FAIL: invalid fixture array access at %zu\n", index);
        failures += 1;
        return 0u;
    }
    value = json_object_array_get_idx(array, index);
    return value ? json_object_get_uint64(value) : 0u;
}

static void test_recipe_contract(char out_digest[65]) {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeV1 round_trip;
    ProceduralSurfaceRecipeV1 invalid;
    ProceduralSurfaceRecipeV1 unchanged;
    ProceduralSurfaceRecipeReport report;
    char canonical_a[PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY];
    char canonical_b[PROCEDURAL_SURFACE_RECIPE_CANONICAL_CAPACITY];
    char temporary_path[] = "/tmp/ray_tracing_psg0_recipe_XXXXXX";
    char invalid_path[] = "/tmp/ray_tracing_psg0_invalid_XXXXXX";
    int descriptor = -1;
    FILE *invalid_file = NULL;

    expect_true(
        ProceduralSurfaceRecipeV1_LoadJsonFile(
            FIXTURE_ROOT "/recipe.json", &recipe, &report),
        report.message);
    expect_true(recipe.schema_version == 1u, "schema version is frozen at v1");
    expect_true(
        strcmp(recipe.recipe_id, "procedural_surface_rock_prism_psg0_v1") == 0,
        "fixture recipe id is stable");
    expect_true(recipe.seed == 104729u, "fixture seed is stable");
    expect_true(recipe.coordinate_space ==
                    PROCEDURAL_SURFACE_COORDINATE_SPACE_OBJECT,
                "PSG-0 coordinate space is object");
    expect_true(recipe.base_feature_size_units == 0.75,
                "base feature size retains object-unit meaning");
    expect_true(recipe.micro_feature_size_units == 0.125,
                "micro feature size retains object-unit meaning");
    expect_true(recipe.target_edge_length_units == 0.25,
                "target triangle edge length retains object-unit meaning");

    expect_true(
        ProceduralSurfaceRecipeV1_CanonicalJson(
            &recipe, canonical_a, sizeof(canonical_a), &report),
        report.message);
    expect_true(
        ProceduralSurfaceRecipeV1_CanonicalJson(
            &recipe, canonical_b, sizeof(canonical_b), &report),
        report.message);
    expect_true(strcmp(canonical_a, canonical_b) == 0,
                "canonical recipe JSON is exactly repeatable");
    expect_true(
        ProceduralSurfaceRecipeV1_Digest(&recipe, out_digest, &report),
        report.message);
    expect_true(strlen(out_digest) == 64u,
                "canonical recipe digest is lowercase SHA-256");

    descriptor = mkstemp(temporary_path);
    expect_true(descriptor >= 0, "round-trip temporary file can be created");
    if (descriptor >= 0) close(descriptor);
    expect_true(
        ProceduralSurfaceRecipeV1_SaveJsonFile(
            temporary_path, &recipe, &report),
        report.message);
    expect_true(
        ProceduralSurfaceRecipeV1_LoadJsonFile(
            temporary_path, &round_trip, &report),
        report.message);
    expect_true(
        ProceduralSurfaceRecipeV1_CanonicalJson(
            &round_trip, canonical_b, sizeof(canonical_b), &report),
        report.message);
    expect_true(strcmp(canonical_a, canonical_b) == 0,
                "JSON save/load round trip retains canonical identity");
    unlink(temporary_path);

    invalid = recipe;
    invalid.displacement_amplitude_units = 1.0e6 + 0.001;
    expect_true(!ProceduralSurfaceRecipeV1_Validate(&invalid, &report),
                "out-of-schema displacement amplitude is rejected");
    expect_true(report.status == PROCEDURAL_SURFACE_RECIPE_STATUS_RANGE,
                "out-of-schema amplitude reports range status");

    invalid = recipe;
    invalid.base_feature_size_units = NAN;
    expect_true(!ProceduralSurfaceRecipeV1_Validate(&invalid, &report),
                "non-finite field scale is rejected");

    invalid = recipe;
    invalid.coordinate_space = (ProceduralSurfaceCoordinateSpace)99;
    expect_true(!ProceduralSurfaceRecipeV1_Validate(&invalid, &report),
                "unsupported coordinate space is rejected");

    descriptor = mkstemp(invalid_path);
    expect_true(descriptor >= 0, "invalid-input temporary file can be created");
    if (descriptor >= 0) {
        invalid_file = fdopen(descriptor, "wb");
        expect_true(invalid_file != NULL,
                    "invalid-input temporary stream can be opened");
    }
    if (invalid_file) {
        fputs("{\"schema\":\"ray_tracing.procedural_surface_recipe\","
              "\"schema_version\":1,\"unexpected\":true}\n",
              invalid_file);
        fclose(invalid_file);
    }
    unchanged = recipe;
    expect_true(
        !ProceduralSurfaceRecipeV1_LoadJsonFile(
            invalid_path, &unchanged, &report),
        "unknown or missing JSON fields are rejected");
    expect_true(
        memcmp(&unchanged, &recipe, sizeof(recipe)) == 0,
        "failed JSON load leaves caller output unchanged");
    unlink(invalid_path);
}

static void compare_topology(
    const ProceduralSurfaceTopologyExpectation *actual,
    struct json_object *expected,
    const char *cage_id) {
    struct json_object *subdivisions = object_field(expected, "subdivisions");
    char message[160];

#define EXPECT_TOPOLOGY_U64(member, key)                                      \
    do {                                                                       \
        snprintf(message, sizeof(message), "%s %s", cage_id, key);            \
        expect_u64(actual->member, object_u64(expected, key), message);         \
    } while (0)

    snprintf(message, sizeof(message), "%s subdivisions x", cage_id);
    expect_u64(actual->subdivisions_x, array_u64(subdivisions, 0u), message);
    snprintf(message, sizeof(message), "%s subdivisions y", cage_id);
    expect_u64(actual->subdivisions_y, array_u64(subdivisions, 1u), message);
    snprintf(message, sizeof(message), "%s subdivisions z", cage_id);
    expect_u64(actual->subdivisions_z, array_u64(subdivisions, 2u), message);
    EXPECT_TOPOLOGY_U64(vertex_count, "vertex_count");
    EXPECT_TOPOLOGY_U64(triangle_count, "triangle_count");
    EXPECT_TOPOLOGY_U64(unique_edge_count, "unique_edge_count");
    EXPECT_TOPOLOGY_U64(boundary_edge_count, "boundary_edge_count");
    EXPECT_TOPOLOGY_U64(connected_component_count,
                        "connected_component_count");
    EXPECT_TOPOLOGY_U64(surface_group_count, "surface_group_count");
    EXPECT_TOPOLOGY_U64(euler_characteristic, "euler_characteristic");

    snprintf(message, sizeof(message), "%s outward winding requirement", cage_id);
    expect_true(actual->requires_outward_winding ==
                    object_bool(expected, "requires_outward_winding"),
                message);
    snprintf(message, sizeof(message), "%s positive volume requirement", cage_id);
    expect_true(actual->requires_positive_signed_volume ==
                    object_bool(expected, "requires_positive_signed_volume"),
                message);
    snprintf(message,
             sizeof(message),
             "%s two-incident-triangles requirement",
             cage_id);
    expect_true(actual->requires_two_incident_triangles_per_edge ==
                    object_bool(
                        expected,
                        "requires_two_incident_triangles_per_edge"),
                message);
#undef EXPECT_TOPOLOGY_U64
}

static void test_topology_fixture(void) {
    struct json_object *cages_root = load_json(FIXTURE_ROOT "/cages.json");
    struct json_object *expected_root =
        load_json(FIXTURE_ROOT "/expected_topology_summary.json");
    struct json_object *cages = object_field(cages_root, "cages");
    struct json_object *expected_cages = object_field(expected_root, "cages");
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;

    expect_true(cages && json_object_array_length(cages) == 2u,
                "fixture contains one plane and one prism cage");
    expect_true(expected_cages && json_object_array_length(expected_cages) == 2u,
                "fixture contains two topology expectations");
    expect_true(
        ProceduralSurfaceRecipeV1_LoadJsonFile(
            FIXTURE_ROOT "/recipe.json", &recipe, &recipe_report),
        recipe_report.message);

    for (size_t i = 0u; cages && expected_cages && i < 2u; ++i) {
        struct json_object *cage_json = json_object_array_get_idx(cages, i);
        struct json_object *expected_json =
            json_object_array_get_idx(expected_cages, i);
        struct json_object *dimensions = object_field(cage_json, "dimensions");
        const char *kind = object_string(cage_json, "kind");
        const char *cage_id = object_string(cage_json, "cage_id");
        ProceduralSurfaceCageContract cage = {0};
        ProceduralSurfaceTopologyExpectation actual = {0};

        cage.kind = kind && strcmp(kind, "plane") == 0
                        ? PROCEDURAL_SURFACE_CAGE_PLANE
                        : PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM;
        cage.width_units = array_double(dimensions, 0u);
        cage.height_units = array_double(dimensions, 1u);
        cage.depth_units = array_double(dimensions, 2u);
        cage.target_edge_length_units =
            json_object_get_double(
                object_field(cage_json, "target_edge_length_units"));
        expect_true(
            ProceduralSurfaceTopologyContract_Derive(&cage, &actual),
            "topology expectation can be derived from object-unit cage dimensions");
        compare_topology(&actual, expected_json, cage_id ? cage_id : "cage");
        expect_true(
            actual.triangle_count <= recipe.quality.preview_max_triangles,
            "PSG-0 plane and prism expectations fit the preview triangle budget");
    }

    {
        ProceduralSurfaceCageContract invalid = {
            .kind = PROCEDURAL_SURFACE_CAGE_PLANE,
            .width_units = 4.0,
            .height_units = 3.0,
            .depth_units = 0.0,
            .target_edge_length_units = 0.0
        };
        ProceduralSurfaceTopologyExpectation sentinel;
        ProceduralSurfaceTopologyExpectation unchanged;
        memset(&sentinel, 0x5a, sizeof(sentinel));
        unchanged = sentinel;
        expect_true(
            !ProceduralSurfaceTopologyContract_Derive(&invalid, &sentinel),
            "zero target edge length is rejected");
        expect_true(memcmp(&sentinel, &unchanged, sizeof(sentinel)) == 0,
                    "failed topology derivation leaves output unchanged");
    }

    if (cages_root) json_object_put(cages_root);
    if (expected_root) json_object_put(expected_root);
}

static void test_field_contract(const char *recipe_digest) {
    struct json_object *samples =
        load_json(FIXTURE_ROOT "/sample_points.json");
    struct json_object *field =
        load_json(FIXTURE_ROOT "/expected_field_summary.json");
    struct json_object *points = object_field(samples, "points");
    struct json_object *outputs = object_field(field, "output_names");
    struct json_object *requirements =
        object_field(field, "determinism_requirements");
    const char *status = object_string(field, "numeric_values_status");
    const char *expected_digest =
        object_string(field, "canonical_recipe_digest_sha256");

    expect_true(status && strcmp(status, "frozen") == 0,
                "PSG-1 numeric field evaluation is frozen");
    expect_u64(points ? json_object_array_length(points) : 0u,
               object_u64(field, "sample_count"),
               "canonical field sample count matches fixture");
    expect_u64(outputs ? json_object_array_length(outputs) : 0u,
               6u,
               "field output vocabulary is frozen");
    expect_true(object_bool(requirements, "same_recipe_same_point_exact"),
                "repeat evaluation must be exact");
    expect_true(
        object_bool(requirements, "sample_traversal_order_independent"),
        "sample traversal order must not affect values");
    expect_true(
        object_bool(requirements, "non_finite_input_rejected_without_output_mutation"),
        "non-finite field input must fail transactionally");
    if (!expected_digest || strcmp(expected_digest, recipe_digest) != 0) {
        fprintf(stderr,
                "FAIL: fixture recipe digest mismatch "
                "(actual=%s expected=%s)\n",
                recipe_digest,
                expected_digest ? expected_digest : "(missing)");
        failures += 1;
    }

    if (samples) json_object_put(samples);
    if (field) json_object_put(field);
}

int main(void) {
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY] = {0};
    test_recipe_contract(recipe_digest);
    test_topology_fixture();
    test_field_contract(recipe_digest);
    if (failures != 0) {
        fprintf(stderr,
                "procedural surface PSG-0 contract tests failed: %d\n",
                failures);
        return EXIT_FAILURE;
    }
    printf("procedural surface PSG-0 contract tests passed "
           "(recipe_sha256=%s)\n",
           recipe_digest);
    return EXIT_SUCCESS;
}
