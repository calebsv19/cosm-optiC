#include "test_runtime_timeline_frame_snapshot.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animation/timeline_frame_snapshot.h"
#include "test_support.h"

typedef struct SnapshotTestObject {
  char target_id[TIMELINE_ID_CAPACITY];
  TimelineVec3 position;
} SnapshotTestObject;

typedef struct SnapshotTestLight {
  char target_id[TIMELINE_ID_CAPACITY];
  double intensity;
} SnapshotTestLight;

typedef struct SnapshotTestMaterial {
  char target_id[TIMELINE_ID_CAPACITY];
  double roughness;
} SnapshotTestMaterial;

typedef struct SnapshotTestScene {
  SnapshotTestObject object;
  SnapshotTestLight light;
  SnapshotTestMaterial material;
} SnapshotTestScene;

typedef struct SnapshotTestAdapterState {
  size_t apply_calls;
  bool reject_validation;
} SnapshotTestAdapterState;

static TimelineEvaluationContext snapshot_context(void) {
  TimelineEvaluationContext context;
  TimelineRate rate = {20u, 1u};
  TimelineRange range = {0, 11u};
  TimelineSample sample = {5, 0u, 1u};
  memset(&context, 0, sizeof(context));
  assert_true("snapshot_context_build",
              TimelineEvaluationContextBuild(rate, range, sample, &context) ==
                  TIMELINE_STATUS_OK);
  return context;
}

static TimelineTrack snapshot_position_track(const char *target_id) {
  TimelineTrack track;
  memset(&track, 0, sizeof(track));
  assert_true("snapshot_position_init",
              TimelineTrackInit(&track, "track_position", target_id,
                                "object/transform/position",
                                TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
  assert_true("snapshot_position_unit",
              TimelineTrackSetUnit(&track, TIMELINE_UNIT_WORLD_DISTANCE) ==
                  TIMELINE_STATUS_OK);
  assert_true("snapshot_position_first",
              TimelineTrackAddKey(&track, 0, TimelineValueVec3(0.0, 1.0, 2.0),
                                  TIMELINE_INTERPOLATION_LINEAR) ==
                  TIMELINE_STATUS_OK);
  assert_true(
      "snapshot_position_last",
      TimelineTrackAddKey(&track, 10, TimelineValueVec3(10.0, 11.0, 12.0),
                          TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
  return track;
}

static TimelineTrack snapshot_scalar_track(const char *track_id,
                                           const char *target_id,
                                           const char *property_id,
                                           TimelineUnit unit, double first,
                                           double last) {
  TimelineTrack track;
  memset(&track, 0, sizeof(track));
  assert_true("snapshot_scalar_init",
              TimelineTrackInit(&track, track_id, target_id, property_id,
                                TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
  assert_true("snapshot_scalar_unit",
              TimelineTrackSetUnit(&track, unit) == TIMELINE_STATUS_OK);
  assert_true("snapshot_scalar_first",
              TimelineTrackAddKey(&track, 0, TimelineValueScalar(first),
                                  TIMELINE_INTERPOLATION_LINEAR) ==
                  TIMELINE_STATUS_OK);
  assert_true("snapshot_scalar_last",
              TimelineTrackAddKey(&track, 10, TimelineValueScalar(last),
                                  TIMELINE_INTERPOLATION_STEP) ==
                  TIMELINE_STATUS_OK);
  return track;
}

static TimelineDocument snapshot_document(const char *object_target,
                                          double roughness_last,
                                          bool include_tracks) {
  TimelineDocument document;
  TimelineRate rate = {20u, 1u};
  TimelineRange range = {0, 11u};
  assert_true("snapshot_document_init",
              TimelineDocumentInit(&document, rate, range) ==
                  TIMELINE_STATUS_OK);
  if (include_tracks) {
    TimelineTrack position = snapshot_position_track(object_target);
    TimelineTrack intensity =
        snapshot_scalar_track("track_intensity", "light/key", "light/intensity",
                              TIMELINE_UNIT_RELATIVE_INTENSITY, 0.0, 4.0);
    TimelineTrack roughness = snapshot_scalar_track(
        "track_roughness", "material/glass", "material/roughness",
        TIMELINE_UNIT_UNITLESS, 0.2, roughness_last);
    assert_true("snapshot_document_position",
                TimelineDocumentAddTrack(&document, &position) ==
                    TIMELINE_STATUS_OK);
    assert_true("snapshot_document_intensity",
                TimelineDocumentAddTrack(&document, &intensity) ==
                    TIMELINE_STATUS_OK);
    assert_true("snapshot_document_roughness",
                TimelineDocumentAddTrack(&document, &roughness) ==
                    TIMELINE_STATUS_OK);
  }
  return document;
}

static SnapshotTestScene snapshot_base_scene(void) {
  SnapshotTestScene scene;
  memset(&scene, 0, sizeof(scene));
  snprintf(scene.object.target_id, sizeof(scene.object.target_id), "%s",
           "object/cube");
  scene.object.position.x = 100.0;
  scene.object.position.y = 200.0;
  scene.object.position.z = 300.0;
  snprintf(scene.light.target_id, sizeof(scene.light.target_id), "%s",
           "light/key");
  scene.light.intensity = 0.25;
  snprintf(scene.material.target_id, sizeof(scene.material.target_id), "%s",
           "material/glass");
  scene.material.roughness = 0.1;
  return scene;
}

static TimelineStatus
snapshot_apply_property(void *copied_scene,
                        const TimelinePropertyEvaluationResult *property,
                        void *user_data) {
  SnapshotTestScene *scene = (SnapshotTestScene *)copied_scene;
  SnapshotTestAdapterState *state = (SnapshotTestAdapterState *)user_data;
  if (!scene || !property || !state)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  state->apply_calls += 1u;
  if (strcmp(property->track.property_id, "object/transform/position") == 0) {
    if (strcmp(property->track.target_id, scene->object.target_id) != 0) {
      return TIMELINE_STATUS_TARGET_NOT_FOUND;
    }
    scene->object.position = property->track.value.as.vec3;
    return TIMELINE_STATUS_OK;
  }
  if (strcmp(property->track.property_id, "light/intensity") == 0) {
    if (strcmp(property->track.target_id, scene->light.target_id) != 0) {
      return TIMELINE_STATUS_TARGET_NOT_FOUND;
    }
    scene->light.intensity = property->track.value.as.scalar;
    return TIMELINE_STATUS_OK;
  }
  if (strcmp(property->track.property_id, "material/roughness") == 0) {
    if (strcmp(property->track.target_id, scene->material.target_id) != 0) {
      return TIMELINE_STATUS_TARGET_NOT_FOUND;
    }
    scene->material.roughness = property->track.value.as.scalar;
    return TIMELINE_STATUS_OK;
  }
  return TIMELINE_STATUS_UNKNOWN_PROPERTY;
}

static TimelineStatus snapshot_validate_scene(const void *copied_scene,
                                              void *user_data) {
  const SnapshotTestScene *scene = (const SnapshotTestScene *)copied_scene;
  const SnapshotTestAdapterState *state =
      (const SnapshotTestAdapterState *)user_data;
  if (!scene || !state)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  if (state->reject_validation) {
    return TIMELINE_STATUS_SCENE_VALIDATION_FAILED;
  }
  if (!isfinite(scene->object.position.x) ||
      !isfinite(scene->object.position.y) ||
      !isfinite(scene->object.position.z) ||
      !isfinite(scene->light.intensity) ||
      !isfinite(scene->material.roughness) || scene->light.intensity < 0.0 ||
      scene->material.roughness < 0.0 || scene->material.roughness > 1.0) {
    return TIMELINE_STATUS_SCENE_VALIDATION_FAILED;
  }
  return TIMELINE_STATUS_OK;
}

static TimelineFrameApplicationAdapter snapshot_adapter(void) {
  TimelineFrameApplicationAdapter adapter;
  adapter.apply_property = snapshot_apply_property;
  adapter.validate_scene = snapshot_validate_scene;
  return adapter;
}

static int test_snapshot_builds_immutable_provenance(void) {
  TimelinePropertyRegistry registry;
  TimelineDocument document = snapshot_document("object/cube", 0.8, true);
  TimelineEvaluationContext context = snapshot_context();
  TimelineFrameSnapshot snapshot;
  assert_true("snapshot_registry_defaults",
              TimelinePropertyRegistryInitFoundationDefaults(&registry) ==
                  TIMELINE_STATUS_OK);
  assert_true("snapshot_build",
              TimelineFrameSnapshotBuild(&registry, &document, &context,
                                         &snapshot) == TIMELINE_STATUS_OK);
  assert_true("snapshot_validate",
              TimelineFrameSnapshotValidate(&registry, &snapshot) ==
                  TIMELINE_STATUS_OK);
  assert_true("snapshot_property_count", snapshot.property_count == 3u);
  assert_true("snapshot_domains", snapshot.invalidation_domains ==
                                      (TIMELINE_INVALIDATION_RIGID_TRANSFORM |
                                       TIMELINE_INVALIDATION_LIGHTING |
                                       TIMELINE_INVALIDATION_MATERIAL));
  assert_true("snapshot_context_frame",
              snapshot.context.sample.absolute_frame == 5);
  assert_close("snapshot_position_x",
               snapshot.properties[0].track.value.as.vec3.x, 5.0, 1e-12);
  assert_close("snapshot_light", snapshot.properties[1].track.value.as.scalar,
               2.0, 1e-12);
  assert_close("snapshot_roughness",
               snapshot.properties[2].track.value.as.scalar, 0.5, 1e-12);
  document.tracks[0].keys[0].value.as.vec3.x = 999.0;
  assert_close("snapshot_document_detached",
               snapshot.properties[0].track.value.as.vec3.x, 5.0, 1e-12);
  assert_true("snapshot_status_label",
              strcmp(TimelineStatusLabel(TIMELINE_STATUS_INVALID_SNAPSHOT),
                     "invalid_snapshot") == 0);
  return 0;
}

static int test_snapshot_applies_transactional_scene_copy(void) {
  TimelinePropertyRegistry registry;
  TimelineDocument document = snapshot_document("object/cube", 0.8, true);
  TimelineEvaluationContext context = snapshot_context();
  TimelineFrameSnapshot snapshot;
  TimelineFrameApplicationAdapter adapter = snapshot_adapter();
  TimelineFrameApplicationReport report;
  SnapshotTestAdapterState state = {0u, false};
  SnapshotTestScene base = snapshot_base_scene();
  SnapshotTestScene base_before = base;
  SnapshotTestScene scratch;
  SnapshotTestScene evaluated;
  SnapshotTestScene in_place;
  memset(&evaluated, 0x5a, sizeof(evaluated));
  TimelinePropertyRegistryInitFoundationDefaults(&registry);
  TimelineFrameSnapshotBuild(&registry, &document, &context, &snapshot);
  assert_true("snapshot_apply_copy",
              TimelineFrameSnapshotApplyToCopy(
                  &registry, &snapshot, &base, &scratch, sizeof(base), &adapter,
                  &state, &evaluated, &report) == TIMELINE_STATUS_OK);
  assert_true("snapshot_base_nonmutation",
              memcmp(&base, &base_before, sizeof(base)) == 0);
  assert_true("snapshot_apply_calls", state.apply_calls == 3u);
  assert_close("snapshot_applied_position", evaluated.object.position.x, 5.0,
               1e-12);
  assert_close("snapshot_applied_light", evaluated.light.intensity, 2.0, 1e-12);
  assert_close("snapshot_applied_material", evaluated.material.roughness, 0.5,
               1e-12);
  assert_true("snapshot_report_count", report.applied_property_count == 3u);
  assert_true("snapshot_report_changed", report.scene_changed);
  assert_true("snapshot_report_domains",
              report.invalidation_domains == snapshot.invalidation_domains);
  in_place = base;
  state.apply_calls = 0u;
  assert_true("snapshot_apply_in_place",
              TimelineFrameSnapshotApplyToCopy(
                  &registry, &snapshot, &in_place, &scratch, sizeof(in_place),
                  &adapter, &state, &in_place, &report) == TIMELINE_STATUS_OK);
  assert_close("snapshot_in_place_position", in_place.object.position.x, 5.0,
               1e-12);
  assert_true("snapshot_in_place_calls", state.apply_calls == 3u);
  return 0;
}

static int test_snapshot_static_scene_copy(void) {
  TimelinePropertyRegistry registry;
  TimelineDocument document = snapshot_document("object/cube", 0.8, false);
  TimelineEvaluationContext context = snapshot_context();
  TimelineFrameSnapshot snapshot;
  TimelineFrameApplicationAdapter adapter = snapshot_adapter();
  TimelineFrameApplicationReport report;
  SnapshotTestAdapterState state = {0u, false};
  SnapshotTestScene base = snapshot_base_scene();
  SnapshotTestScene scratch;
  SnapshotTestScene evaluated;
  TimelinePropertyRegistryInitFoundationDefaults(&registry);
  assert_true("snapshot_static_build",
              TimelineFrameSnapshotBuild(&registry, &document, &context,
                                         &snapshot) == TIMELINE_STATUS_OK);
  assert_true("snapshot_static_empty",
              snapshot.property_count == 0u &&
                  snapshot.invalidation_domains == TIMELINE_INVALIDATION_NONE);
  assert_true("snapshot_static_apply",
              TimelineFrameSnapshotApplyToCopy(
                  &registry, &snapshot, &base, &scratch, sizeof(base), &adapter,
                  &state, &evaluated, &report) == TIMELINE_STATUS_OK);
  assert_true("snapshot_static_exact_copy",
              memcmp(&base, &evaluated, sizeof(base)) == 0);
  assert_true("snapshot_static_no_apply", state.apply_calls == 0u);
  assert_true("snapshot_static_report",
              !report.scene_changed && report.applied_property_count == 0u &&
                  report.invalidation_domains == TIMELINE_INVALIDATION_NONE);
  return 0;
}

static int test_snapshot_refusal_nonmutation(void) {
  TimelinePropertyRegistry registry;
  TimelineDocument missing_document =
      snapshot_document("object/missing", 0.8, true);
  TimelineDocument invalid_document =
      snapshot_document("object/cube", 1.2, true);
  TimelineEvaluationContext context = snapshot_context();
  TimelineFrameSnapshot snapshot;
  TimelineFrameSnapshot rejected_snapshot;
  TimelineFrameSnapshot rejected_before;
  TimelineFrameApplicationAdapter adapter = snapshot_adapter();
  TimelineFrameApplicationReport report;
  TimelineFrameApplicationReport report_before;
  SnapshotTestAdapterState state = {0u, false};
  SnapshotTestScene base = snapshot_base_scene();
  SnapshotTestScene scratch;
  SnapshotTestScene evaluated;
  SnapshotTestScene evaluated_before;
  TimelinePropertyRegistryInitFoundationDefaults(&registry);
  TimelineFrameSnapshotBuild(&registry, &missing_document, &context, &snapshot);
  memset(&evaluated, 0x5a, sizeof(evaluated));
  evaluated_before = evaluated;
  memset(&report, 0x5a, sizeof(report));
  report_before = report;
  assert_true("snapshot_missing_target",
              TimelineFrameSnapshotApplyToCopy(&registry, &snapshot, &base,
                                               &scratch, sizeof(base), &adapter,
                                               &state, &evaluated, &report) ==
                  TIMELINE_STATUS_TARGET_NOT_FOUND);
  assert_true("snapshot_missing_output_nonmutation",
              memcmp(&evaluated, &evaluated_before, sizeof(evaluated)) == 0);
  assert_true("snapshot_missing_report_nonmutation",
              memcmp(&report, &report_before, sizeof(report)) == 0);

  snapshot.invalidation_domains ^= TIMELINE_INVALIDATION_CAMERA;
  state.apply_calls = 0u;
  assert_true("snapshot_tamper_refused",
              TimelineFrameSnapshotApplyToCopy(&registry, &snapshot, &base,
                                               &scratch, sizeof(base), &adapter,
                                               &state, &evaluated, &report) ==
                  TIMELINE_STATUS_INVALID_SNAPSHOT);
  assert_true("snapshot_tamper_not_applied", state.apply_calls == 0u);
  assert_true("snapshot_tamper_output_nonmutation",
              memcmp(&evaluated, &evaluated_before, sizeof(evaluated)) == 0);

  memset(&rejected_snapshot, 0x5a, sizeof(rejected_snapshot));
  rejected_before = rejected_snapshot;
  assert_true("snapshot_build_refusal",
              TimelineFrameSnapshotBuild(&registry, &invalid_document, &context,
                                         &rejected_snapshot) ==
                  TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
  assert_true("snapshot_build_refusal_nonmutation",
              memcmp(&rejected_snapshot, &rejected_before,
                     sizeof(rejected_snapshot)) == 0);
  return 0;
}

static int test_snapshot_scene_validation_refusal(void) {
  TimelinePropertyRegistry registry;
  TimelineDocument document = snapshot_document("object/cube", 0.8, true);
  TimelineEvaluationContext context = snapshot_context();
  TimelineFrameSnapshot snapshot;
  TimelineFrameApplicationAdapter adapter = snapshot_adapter();
  TimelineFrameApplicationReport report;
  TimelineFrameApplicationReport report_before;
  SnapshotTestAdapterState state = {0u, true};
  SnapshotTestScene base = snapshot_base_scene();
  SnapshotTestScene scratch;
  SnapshotTestScene evaluated;
  SnapshotTestScene evaluated_before;
  TimelinePropertyRegistryInitFoundationDefaults(&registry);
  TimelineFrameSnapshotBuild(&registry, &document, &context, &snapshot);
  memset(&evaluated, 0x5a, sizeof(evaluated));
  evaluated_before = evaluated;
  memset(&report, 0x5a, sizeof(report));
  report_before = report;
  assert_true("snapshot_scene_validation_refused",
              TimelineFrameSnapshotApplyToCopy(&registry, &snapshot, &base,
                                               &scratch, sizeof(base), &adapter,
                                               &state, &evaluated, &report) ==
                  TIMELINE_STATUS_SCENE_VALIDATION_FAILED);
  assert_true("snapshot_validation_output_nonmutation",
              memcmp(&evaluated, &evaluated_before, sizeof(evaluated)) == 0);
  assert_true("snapshot_validation_report_nonmutation",
              memcmp(&report, &report_before, sizeof(report)) == 0);
  return 0;
}

int run_test_runtime_timeline_frame_snapshot_tests(void) {
  test_snapshot_builds_immutable_provenance();
  test_snapshot_applies_transactional_scene_copy();
  test_snapshot_static_scene_copy();
  test_snapshot_refusal_nonmutation();
  test_snapshot_scene_validation_refusal();
  return test_support_failures();
}
