#include "test_runtime_timeline_property_registry.h"

#include <stdio.h>
#include <string.h>

#include "animation/timeline_property_registry.h"
#include "test_support.h"

static TimelineTrack registry_scalar_track(const char* track_id,
                                           const char* target_id,
                                           const char* property_id,
                                           TimelineUnit unit,
                                           double start_value,
                                           double end_value,
                                           TimelineInterpolation interpolation) {
    TimelineTrack track;
    memset(&track, 0, sizeof(track));
    assert_true("registry_scalar_track_init",
                TimelineTrackInit(&track, track_id, target_id, property_id,
                                  TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert_true("registry_scalar_track_unit",
                TimelineTrackSetUnit(&track, unit) == TIMELINE_STATUS_OK);
    assert_true("registry_scalar_track_start",
                TimelineTrackAddKey(&track, 0, TimelineValueScalar(start_value),
                                    interpolation) == TIMELINE_STATUS_OK);
    assert_true("registry_scalar_track_end",
                TimelineTrackAddKey(&track, 10, TimelineValueScalar(end_value),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    return track;
}

static TimelineTrack registry_position_track(void) {
    TimelineTrack track;
    memset(&track, 0, sizeof(track));
    assert_true("registry_position_track_init",
                TimelineTrackInit(&track, "track_position", "object/cube",
                                  "object/transform/position",
                                  TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
    assert_true("registry_position_track_unit",
                TimelineTrackSetUnit(&track, TIMELINE_UNIT_WORLD_DISTANCE) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_position_track_start",
                TimelineTrackAddKey(&track, 0, TimelineValueVec3(0.0, 1.0, 2.0),
                                    TIMELINE_INTERPOLATION_LINEAR) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_position_track_end",
                TimelineTrackAddKey(&track, 10,
                                    TimelineValueVec3(10.0, 11.0, 12.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    return track;
}

static TimelineEvaluationContext registry_context(void) {
    TimelineEvaluationContext context;
    TimelineRate rate = {20u, 1u};
    TimelineRange range = {0, 11u};
    TimelineSample sample = {5, 0u, 1u};
    memset(&context, 0, sizeof(context));
    assert_true("registry_context_build",
                TimelineEvaluationContextBuild(rate, range, sample, &context) ==
                    TIMELINE_STATUS_OK);
    return context;
}

static int test_registry_foundation_defaults(void) {
    TimelinePropertyRegistry registry;
    const TimelinePropertyDescriptor* descriptor = NULL;
    assert_true("registry_defaults_init",
                TimelinePropertyRegistryInitFoundationDefaults(&registry) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_defaults_count", registry.descriptor_count == 3u);
    assert_true("registry_find_position",
                TimelinePropertyRegistryFind(
                    &registry, "object/transform/position", &descriptor) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_position_type",
                descriptor->value_type == TIMELINE_VALUE_VEC3);
    assert_true("registry_position_unit",
                descriptor->unit == TIMELINE_UNIT_WORLD_DISTANCE);
    assert_true("registry_position_invalidation",
                descriptor->invalidation_domains ==
                    TIMELINE_INVALIDATION_RIGID_TRANSFORM);
    assert_true("registry_find_light",
                TimelinePropertyRegistryFind(&registry, "light/intensity",
                                             &descriptor) == TIMELINE_STATUS_OK);
    assert_true("registry_light_minimum", descriptor->has_minimum);
    assert_true("registry_find_roughness",
                TimelinePropertyRegistryFind(&registry, "material/roughness",
                                             &descriptor) == TIMELINE_STATUS_OK);
    assert_true("registry_roughness_bounds",
                descriptor->has_minimum && descriptor->has_maximum);
    assert_true("registry_target_label",
                strcmp(TimelinePropertyTargetKindLabel(
                           TIMELINE_PROPERTY_TARGET_MATERIAL),
                       "material") == 0);
    assert_true("registry_access_label",
                strcmp(TimelinePropertyAccessLabel(
                           TIMELINE_PROPERTY_ACCESS_SIMULATION_OWNED),
                       "simulation_owned") == 0);
    return 0;
}

static int test_registry_descriptor_validation_and_duplicates(void) {
    TimelinePropertyRegistry registry;
    TimelinePropertyDescriptor descriptor;
    TimelinePropertyDescriptor sentinel;
    TimelinePropertyDescriptor original;
    TimelineValue lower = TimelineValueScalar(2.0);
    TimelineValue upper = TimelineValueScalar(1.0);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    original = sentinel;
    assert_true("registry_invalid_descriptor_refused",
                TimelinePropertyDescriptorInit(
                    &sentinel, "", TIMELINE_PROPERTY_TARGET_OBJECT,
                    TIMELINE_VALUE_SCALAR, TIMELINE_UNIT_UNITLESS,
                    TIMELINE_PROPERTY_ACCESS_AUTHORABLE,
                    TIMELINE_INTERPOLATION_MASK_LINEAR,
                    TIMELINE_INVALIDATION_MATERIAL) ==
                    TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR);
    assert_true("registry_invalid_descriptor_nonmutation",
                memcmp(&sentinel, &original, sizeof(sentinel)) == 0);
    assert_true("registry_descriptor_init",
                TimelinePropertyDescriptorInit(
                    &descriptor, "material/custom", TIMELINE_PROPERTY_TARGET_MATERIAL,
                    TIMELINE_VALUE_SCALAR, TIMELINE_UNIT_UNITLESS,
                    TIMELINE_PROPERTY_ACCESS_AUTHORABLE,
                    TIMELINE_INTERPOLATION_MASK_STEP,
                    TIMELINE_INVALIDATION_MATERIAL) == TIMELINE_STATUS_OK);
    original = descriptor;
    assert_true("registry_reversed_bounds_refused",
                TimelinePropertyDescriptorSetBounds(&descriptor, &lower, &upper) ==
                    TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
    assert_true("registry_reversed_bounds_nonmutation",
                memcmp(&descriptor, &original, sizeof(descriptor)) == 0);
    assert_true("registry_empty_init",
                TimelinePropertyRegistryInit(&registry) == TIMELINE_STATUS_OK);
    assert_true("registry_descriptor_add",
                TimelinePropertyRegistryAdd(&registry, &descriptor) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_duplicate_descriptor",
                TimelinePropertyRegistryAdd(&registry, &descriptor) ==
                    TIMELINE_STATUS_DUPLICATE_ID);
    return 0;
}

static int test_registry_track_binding_refusals(void) {
    TimelinePropertyRegistry registry;
    TimelinePropertyDescriptor simulated;
    TimelinePropertyDescriptor step_only;
    TimelineTrack valid = registry_position_track();
    TimelineTrack altered;
    TimelineRange range = {0, 11u};
    assert_true("registry_binding_defaults",
                TimelinePropertyRegistryInitFoundationDefaults(&registry) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_binding_valid",
                TimelinePropertyRegistryValidateTrack(&registry, &valid, &range) ==
                    TIMELINE_STATUS_OK);

    altered = valid;
    snprintf(altered.property_id, sizeof(altered.property_id), "%s", "object/missing");
    assert_true("registry_unknown_property",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_UNKNOWN_PROPERTY);
    altered = valid;
    snprintf(altered.target_id, sizeof(altered.target_id), "%s", "light/key");
    assert_true("registry_target_mismatch",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_TARGET_KIND_MISMATCH);
    altered = valid;
    altered.value_type = TIMELINE_VALUE_SCALAR;
    assert_true("registry_type_mismatch",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_TYPE_MISMATCH);
    altered = valid;
    altered.unit = TIMELINE_UNIT_UNITLESS;
    assert_true("registry_unit_mismatch",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_UNIT_MISMATCH);

    assert_true("registry_sim_descriptor_init",
                TimelinePropertyDescriptorInit(
                    &simulated, "object/simulated/position",
                    TIMELINE_PROPERTY_TARGET_OBJECT, TIMELINE_VALUE_VEC3,
                    TIMELINE_UNIT_WORLD_DISTANCE,
                    TIMELINE_PROPERTY_ACCESS_SIMULATION_OWNED,
                    TIMELINE_INTERPOLATION_MASK_STEP,
                    TIMELINE_INVALIDATION_SIMULATION_CACHE |
                        TIMELINE_INVALIDATION_RIGID_TRANSFORM) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_sim_descriptor_add",
                TimelinePropertyRegistryAdd(&registry, &simulated) ==
                    TIMELINE_STATUS_OK);
    altered = valid;
    snprintf(altered.property_id, sizeof(altered.property_id), "%s",
             simulated.property_id);
    assert_true("registry_ownership_mismatch",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_OWNERSHIP_MISMATCH);

    assert_true("registry_step_descriptor_init",
                TimelinePropertyDescriptorInit(
                    &step_only, "object/step_only", TIMELINE_PROPERTY_TARGET_OBJECT,
                    TIMELINE_VALUE_VEC3, TIMELINE_UNIT_WORLD_DISTANCE,
                    TIMELINE_PROPERTY_ACCESS_AUTHORABLE,
                    TIMELINE_INTERPOLATION_MASK_STEP,
                    TIMELINE_INVALIDATION_RIGID_TRANSFORM) == TIMELINE_STATUS_OK);
    assert_true("registry_step_descriptor_add",
                TimelinePropertyRegistryAdd(&registry, &step_only) ==
                    TIMELINE_STATUS_OK);
    altered = valid;
    snprintf(altered.property_id, sizeof(altered.property_id), "%s",
             step_only.property_id);
    assert_true("registry_interpolation_mismatch",
                TimelinePropertyRegistryValidateTrack(&registry, &altered, &range) ==
                    TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION);
    return 0;
}

static int test_registry_document_evaluation_metadata(void) {
    TimelinePropertyRegistry registry;
    TimelineDocument document;
    TimelineTrack position = registry_position_track();
    TimelineTrack intensity = registry_scalar_track(
        "track_intensity", "light/key", "light/intensity",
        TIMELINE_UNIT_RELATIVE_INTENSITY, 0.0, 4.0,
        TIMELINE_INTERPOLATION_LINEAR);
    TimelineTrack roughness = registry_scalar_track(
        "track_roughness", "material/glass", "material/roughness",
        TIMELINE_UNIT_UNITLESS, 0.2, 0.8, TIMELINE_INTERPOLATION_LINEAR);
    TimelineEvaluationContext context = registry_context();
    TimelinePropertyEvaluationResult results[3];
    size_t result_count = 0u;
    TimelineRate rate = {20u, 1u};
    TimelineRange range = {0, 11u};
    assert_true("registry_eval_defaults",
                TimelinePropertyRegistryInitFoundationDefaults(&registry) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_eval_document_init",
                TimelineDocumentInit(&document, rate, range) == TIMELINE_STATUS_OK);
    assert_true("registry_eval_add_position",
                TimelineDocumentAddTrack(&document, &position) == TIMELINE_STATUS_OK);
    assert_true("registry_eval_add_intensity",
                TimelineDocumentAddTrack(&document, &intensity) == TIMELINE_STATUS_OK);
    assert_true("registry_eval_add_roughness",
                TimelineDocumentAddTrack(&document, &roughness) == TIMELINE_STATUS_OK);
    assert_true("registry_eval_document_validate",
                TimelinePropertyRegistryValidateDocument(&registry, &document) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_eval_document",
                TimelinePropertyRegistryEvaluateDocument(
                    &registry, &document, &context, results, 3u, &result_count) ==
                    TIMELINE_STATUS_OK);
    assert_true("registry_eval_result_count", result_count == 3u);
    assert_close("registry_eval_position_x", results[0].track.value.as.vec3.x,
                 5.0, 1e-12);
    assert_true("registry_eval_position_domain",
                results[0].invalidation_domains ==
                    TIMELINE_INVALIDATION_RIGID_TRANSFORM);
    assert_close("registry_eval_intensity", results[1].track.value.as.scalar,
                 2.0, 1e-12);
    assert_true("registry_eval_intensity_kind",
                results[1].target_kind == TIMELINE_PROPERTY_TARGET_LIGHT);
    assert_close("registry_eval_roughness", results[2].track.value.as.scalar,
                 0.5, 1e-12);
    assert_true("registry_eval_roughness_domain",
                results[2].invalidation_domains == TIMELINE_INVALIDATION_MATERIAL);
    return 0;
}

static int test_registry_duplicate_ownership(void) {
    TimelinePropertyRegistry registry;
    TimelineDocument document;
    TimelineTrack first = registry_scalar_track(
        "track_roughness_a", "material/glass", "material/roughness",
        TIMELINE_UNIT_UNITLESS, 0.2, 0.8, TIMELINE_INTERPOLATION_LINEAR);
    TimelineTrack second = registry_scalar_track(
        "track_roughness_b", "material/glass", "material/roughness",
        TIMELINE_UNIT_UNITLESS, 0.3, 0.7, TIMELINE_INTERPOLATION_LINEAR);
    TimelineRate rate = {20u, 1u};
    TimelineRange range = {0, 11u};
    TimelinePropertyRegistryInitFoundationDefaults(&registry);
    TimelineDocumentInit(&document, rate, range);
    TimelineDocumentAddTrack(&document, &first);
    TimelineDocumentAddTrack(&document, &second);
    assert_true("registry_duplicate_ownership",
                TimelinePropertyRegistryValidateDocument(&registry, &document) ==
                    TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    return 0;
}

static int test_registry_refusal_nonmutation(void) {
    TimelinePropertyRegistry registry;
    TimelineDocument document;
    TimelineTrack invalid = registry_scalar_track(
        "track_invalid_roughness", "material/glass", "material/roughness",
        TIMELINE_UNIT_UNITLESS, 0.2, 1.2, TIMELINE_INTERPOLATION_LINEAR);
    TimelineEvaluationContext context = registry_context();
    TimelinePropertyEvaluationResult result;
    TimelinePropertyEvaluationResult original;
    size_t result_count = 77u;
    TimelineRate rate = {20u, 1u};
    TimelineRange range = {0, 11u};
    memset(&result, 0x5a, sizeof(result));
    original = result;
    TimelinePropertyRegistryInitFoundationDefaults(&registry);
    TimelineDocumentInit(&document, rate, range);
    TimelineDocumentAddTrack(&document, &invalid);
    assert_true("registry_refusal_status",
                TimelinePropertyRegistryEvaluateDocument(
                    &registry, &document, &context, &result, 1u,
                    &result_count) == TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
    assert_true("registry_refusal_result_nonmutation",
                memcmp(&result, &original, sizeof(result)) == 0);
    assert_true("registry_refusal_count_nonmutation", result_count == 77u);
    return 0;
}

int run_test_runtime_timeline_property_registry_tests(void) {
    test_registry_foundation_defaults();
    test_registry_descriptor_validation_and_duplicates();
    test_registry_track_binding_refusals();
    test_registry_document_evaluation_metadata();
    test_registry_duplicate_ownership();
    test_registry_refusal_nonmutation();
    return test_support_failures();
}
