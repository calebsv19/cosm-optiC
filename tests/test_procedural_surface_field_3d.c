#include "procedural/procedural_surface_field_3d.h"
#include "procedural/procedural_surface_recipe.h"

#include <json-c/json.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_ROOT "tests/fixtures/procedural_surface_rock_prism_psg0"
#define SAMPLE_COUNT 11u

typedef struct FixtureSample {
    char sample_id[64];
    ProceduralSurfaceFieldPoint3D point;
    ProceduralSurfaceFieldOutput expected;
} FixtureSample;

static int failures = 0;

static void expect_true(bool condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", message);
    failures += 1;
}

static bool output_exactly_equal(const ProceduralSurfaceFieldOutput *a,
                                 const ProceduralSurfaceFieldOutput *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static struct json_object *required_field(struct json_object *object,
                                          const char *key) {
    struct json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value)) {
        fprintf(stderr, "FAIL: missing fixture field %s\n", key);
        failures += 1;
        return NULL;
    }
    return value;
}

static double output_field(struct json_object *object, const char *key) {
    struct json_object *value = required_field(object, key);
    return value ? json_object_get_double(value) : 0.0;
}

static bool load_fixture(FixtureSample samples[SAMPLE_COUNT],
                         bool *out_values_frozen,
                         char out_expected_digest[65]) {
    struct json_object *points_root =
        json_object_from_file(FIXTURE_ROOT "/sample_points.json");
    struct json_object *expected_root =
        json_object_from_file(FIXTURE_ROOT "/expected_field_summary.json");
    struct json_object *points;
    struct json_object *expected_samples = NULL;
    struct json_object *status_json;
    struct json_object *digest_json = NULL;
    const char *status;

    if (!points_root || !expected_root) {
        fprintf(stderr, "FAIL: unable to load PSG field fixtures\n");
        failures += 1;
        if (points_root) json_object_put(points_root);
        if (expected_root) json_object_put(expected_root);
        return false;
    }
    points = required_field(points_root, "points");
    status_json = required_field(expected_root, "numeric_values_status");
    status = status_json ? json_object_get_string(status_json) : NULL;
    *out_values_frozen = status && strcmp(status, "frozen") == 0;
    if (*out_values_frozen) {
        expected_samples = required_field(expected_root, "samples");
        digest_json = required_field(
            expected_root, "canonical_field_summary_digest_sha256");
    }
    expect_true(points &&
                    json_object_array_length(points) == SAMPLE_COUNT,
                "fixture has eleven canonical sample points");
    if (*out_values_frozen) {
        expect_true(expected_samples &&
                        json_object_array_length(expected_samples) ==
                            SAMPLE_COUNT,
                    "frozen fixture has eleven expected outputs");
    }

    for (size_t i = 0u; points && i < SAMPLE_COUNT; ++i) {
        struct json_object *point_json =
            json_object_array_get_idx(points, i);
        struct json_object *position =
            required_field(point_json, "position");
        const char *sample_id = json_object_get_string(
            required_field(point_json, "sample_id"));
        snprintf(samples[i].sample_id,
                 sizeof(samples[i].sample_id),
                 "%s",
                 sample_id ? sample_id : "");
        samples[i].point.x = json_object_get_double(
            json_object_array_get_idx(position, 0u));
        samples[i].point.y = json_object_get_double(
            json_object_array_get_idx(position, 1u));
        samples[i].point.z = json_object_get_double(
            json_object_array_get_idx(position, 2u));
        if (*out_values_frozen && expected_samples) {
            struct json_object *expected =
                json_object_array_get_idx(expected_samples, i);
            const char *expected_id = json_object_get_string(
                required_field(expected, "sample_id"));
            expect_true(expected_id &&
                            strcmp(expected_id, samples[i].sample_id) == 0,
                        "expected field outputs retain sample order");
            samples[i].expected.height =
                output_field(expected, "height");
            samples[i].expected.macro_variation =
                output_field(expected, "macro_variation");
            samples[i].expected.micro_variation =
                output_field(expected, "micro_variation");
            samples[i].expected.rock_mask =
                output_field(expected, "rock_mask");
            samples[i].expected.roughness =
                output_field(expected, "roughness");
            samples[i].expected.snow_precursor =
                output_field(expected, "snow_precursor");
        }
    }
    snprintf(out_expected_digest,
             65u,
             "%s",
             digest_json ? json_object_get_string(digest_json) : "");
    json_object_put(points_root);
    json_object_put(expected_root);
    return true;
}

static void evaluate_samples(
    const ProceduralSurfaceRecipeV1 *recipe,
    const FixtureSample samples[SAMPLE_COUNT],
    const size_t order[SAMPLE_COUNT],
    ProceduralSurfaceFieldOutput outputs[SAMPLE_COUNT]) {
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfaceFieldReport report;
    ProceduralSurfaceFieldBudget_Init(recipe, &budget);
    for (size_t i = 0u; i < SAMPLE_COUNT; ++i) {
        const size_t index = order[i];
        char message[160];
        snprintf(message,
                 sizeof(message),
                 "evaluate sample %s",
                 samples[index].sample_id);
        expect_true(
            ProceduralSurfaceField3D_Evaluate(
                recipe,
                samples[index].point,
                &budget,
                &outputs[index],
                &report),
            report.message[0] ? report.message : message);
    }
    expect_true(budget.evaluations == SAMPLE_COUNT,
                "caller-owned budget counts successful point evaluations");
}

static void test_transactional_failures(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D valid_point) {
    ProceduralSurfaceFieldBudget budget;
    ProceduralSurfaceFieldBudget unchanged_budget;
    ProceduralSurfaceFieldOutput output;
    ProceduralSurfaceFieldOutput unchanged_output;
    ProceduralSurfaceFieldReport report;

    memset(&output, 0x5a, sizeof(output));
    unchanged_output = output;
    ProceduralSurfaceFieldBudget_Init(recipe, &budget);
    unchanged_budget = budget;
    valid_point.x = NAN;
    expect_true(
        !ProceduralSurfaceField3D_Evaluate(
            recipe, valid_point, &budget, &output, &report),
        "non-finite point is rejected");
    expect_true(report.status ==
                    PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_POINT,
                "non-finite point has a typed status");
    expect_true(output_exactly_equal(&output, &unchanged_output),
                "non-finite failure leaves output unchanged");
    expect_true(memcmp(&budget, &unchanged_budget, sizeof(budget)) == 0,
                "non-finite failure leaves budget unchanged");

    valid_point.x = 1000000001.0;
    expect_true(
        !ProceduralSurfaceField3D_Evaluate(
            recipe, valid_point, &budget, &output, &report),
        "unsafe deterministic coordinate range is rejected");
    expect_true(report.status ==
                    PROCEDURAL_SURFACE_FIELD_STATUS_COORDINATE_RANGE,
                "unsafe coordinate has a typed status");
    expect_true(output_exactly_equal(&output, &unchanged_output),
                "coordinate-range failure leaves output unchanged");
    expect_true(memcmp(&budget, &unchanged_budget, sizeof(budget)) == 0,
                "coordinate-range failure leaves budget unchanged");

    valid_point.x = 0.0;
    budget.max_evaluations = 1u;
    unchanged_budget = budget;
    expect_true(
        !ProceduralSurfaceField3D_Evaluate(
            recipe, valid_point, &budget, &output, &report),
        "budget not derived from recipe is rejected");
    expect_true(report.status == PROCEDURAL_SURFACE_FIELD_STATUS_BUDGET,
                "invalid budget has a typed status");
    expect_true(output_exactly_equal(&output, &unchanged_output),
                "budget failure leaves output unchanged");
    expect_true(memcmp(&budget, &unchanged_budget, sizeof(budget)) == 0,
                "budget failure leaves budget unchanged");

    ProceduralSurfaceFieldBudget_Init(recipe, &budget);
    budget.evaluations = budget.max_evaluations;
    unchanged_budget = budget;
    expect_true(
        !ProceduralSurfaceField3D_Evaluate(
            recipe, valid_point, &budget, &output, &report),
        "exhausted budget is rejected");
    expect_true(output_exactly_equal(&output, &unchanged_output),
                "exhausted-budget failure leaves output unchanged");
    expect_true(memcmp(&budget, &unchanged_budget, sizeof(budget)) == 0,
                "exhausted-budget failure leaves budget unchanged");
}

int main(void) {
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeV1 alternate_seed;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceFieldReport field_report;
    FixtureSample samples[SAMPLE_COUNT] = {0};
    ProceduralSurfaceFieldOutput forward[SAMPLE_COUNT] = {0};
    ProceduralSurfaceFieldOutput reverse[SAMPLE_COUNT] = {0};
    ProceduralSurfaceFieldOutput shuffled[SAMPLE_COUNT] = {0};
    ProceduralSurfaceFieldOutput alternate;
    ProceduralSurfaceFieldBudget budget;
    const size_t forward_order[SAMPLE_COUNT] =
        {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u};
    const size_t reverse_order[SAMPLE_COUNT] =
        {10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u};
    const size_t shuffled_order[SAMPLE_COUNT] =
        {4u, 1u, 9u, 0u, 7u, 3u, 10u, 2u, 8u, 5u, 6u};
    const char *sample_ids[SAMPLE_COUNT];
    char recipe_digest[65] = {0};
    char summary_a[PROCEDURAL_SURFACE_FIELD_SUMMARY_CAPACITY];
    char summary_b[PROCEDURAL_SURFACE_FIELD_SUMMARY_CAPACITY];
    char summary_digest[65] = {0};
    char expected_summary_digest[65] = {0};
    bool values_frozen = false;

    expect_true(
        ProceduralSurfaceRecipeV1_LoadJsonFile(
            FIXTURE_ROOT "/recipe.json", &recipe, &recipe_report),
        recipe_report.message);
    expect_true(
        ProceduralSurfaceRecipeV1_Digest(
            &recipe, recipe_digest, &recipe_report),
        recipe_report.message);
    expect_true(load_fixture(
                    samples, &values_frozen, expected_summary_digest),
                "field fixture loads");
    for (size_t i = 0u; i < SAMPLE_COUNT; ++i) {
        sample_ids[i] = samples[i].sample_id;
    }

    evaluate_samples(&recipe, samples, forward_order, forward);
    srand(773u);
    (void)rand();
    (void)rand();
    evaluate_samples(&recipe, samples, reverse_order, reverse);
    srand(991u);
    (void)rand();
    evaluate_samples(&recipe, samples, shuffled_order, shuffled);

    for (size_t i = 0u; i < SAMPLE_COUNT; ++i) {
        char message[160];
        snprintf(message,
                 sizeof(message),
                 "%s is reverse-order independent",
                 samples[i].sample_id);
        expect_true(output_exactly_equal(&forward[i], &reverse[i]), message);
        snprintf(message,
                 sizeof(message),
                 "%s is shuffled-order and global-RNG independent",
                 samples[i].sample_id);
        expect_true(output_exactly_equal(&forward[i], &shuffled[i]), message);
        expect_true(forward[i].height >= -1.0 &&
                        forward[i].height <= 1.0 &&
                        forward[i].macro_variation >= -1.0 &&
                        forward[i].macro_variation <= 1.0 &&
                        forward[i].micro_variation >= -1.0 &&
                        forward[i].micro_variation <= 1.0 &&
                        forward[i].rock_mask >= 0.0 &&
                        forward[i].rock_mask <= 1.0 &&
                        forward[i].roughness >= 0.0 &&
                        forward[i].roughness <= 1.0 &&
                        forward[i].snow_precursor >= 0.0 &&
                        forward[i].snow_precursor <= 1.0,
                    "field outputs honor signed/unit ranges");
        if (values_frozen) {
            snprintf(message,
                     sizeof(message),
                     "%s matches frozen numeric output",
                     samples[i].sample_id);
            expect_true(
                output_exactly_equal(&forward[i], &samples[i].expected),
                message);
        }
    }

    alternate_seed = recipe;
    alternate_seed.seed += 1u;
    ProceduralSurfaceFieldBudget_Init(&alternate_seed, &budget);
    expect_true(
        ProceduralSurfaceField3D_Evaluate(
            &alternate_seed,
            samples[1].point,
            &budget,
            &alternate,
            &field_report),
        field_report.message);
    expect_true(
        !output_exactly_equal(&alternate, &forward[1]),
        "different seed produces a distinct field output");

    test_transactional_failures(&recipe, samples[0].point);

    expect_true(
        ProceduralSurfaceField3D_CanonicalSummary(
            recipe_digest,
            sample_ids,
            forward,
            SAMPLE_COUNT,
            summary_a,
            sizeof(summary_a),
            &field_report),
        field_report.message);
    expect_true(
        ProceduralSurfaceField3D_CanonicalSummary(
            recipe_digest,
            sample_ids,
            shuffled,
            SAMPLE_COUNT,
            summary_b,
            sizeof(summary_b),
            &field_report),
        field_report.message);
    expect_true(strcmp(summary_a, summary_b) == 0,
                "canonical summary is independent of evaluation traversal");
    expect_true(
        ProceduralSurfaceField3D_SummaryDigest(
            recipe_digest,
            sample_ids,
            forward,
            SAMPLE_COUNT,
            summary_digest,
            &field_report),
        field_report.message);

    if (values_frozen) {
        expect_true(
            strcmp(summary_digest, expected_summary_digest) == 0,
            "field summary digest matches frozen fixture");
    } else {
        printf("PSG-1 fixture values to freeze:\n");
        for (size_t i = 0u; i < SAMPLE_COUNT; ++i) {
            printf(
                "%s %.17g %.17g %.17g %.17g %.17g %.17g\n",
                samples[i].sample_id,
                forward[i].height,
                forward[i].macro_variation,
                forward[i].micro_variation,
                forward[i].rock_mask,
                forward[i].roughness,
                forward[i].snow_precursor);
        }
        printf("field_summary_sha256=%s\n", summary_digest);
    }

    if (failures != 0) {
        fprintf(stderr,
                "procedural surface PSG-1 field tests failed: %d\n",
                failures);
        return EXIT_FAILURE;
    }
    printf("procedural surface PSG-1 field tests passed "
           "(field_summary_sha256=%s)\n",
           summary_digest);
    return EXIT_SUCCESS;
}
