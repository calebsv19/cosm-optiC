#include "app/evaluated_scene_service.h"
static RayEvaluatedLight dm3_light_sample(int frame) {
  static RayEvaluatedSceneSnapshot snapshot;
  assert(SceneEditorTimelineSeek(frame));
  if (!SceneEditorTimelineCopyEvaluated(&snapshot)) {
    RayEvaluatedSceneServiceResult result;
    RayEvaluatedSceneCaptureSample((TimelineSample){frame, 0, 1}, &result);
    fprintf(stderr, "Light sample frame %d: %s\n", frame, result.status_line);
    assert(false);
  }
  return snapshot.light;
}
static json_object *dm3_light_row(RayEvaluatedLight light) {
  json_object *row = json_object_new_array();
  double v[] = {light.position.x, light.position.y, light.position.z, light.intensity};
  for (int i = 0; i < 4; ++i) json_object_array_add(row, json_object_new_double(v[i]));
  return row;
}
static void dm3_light_acceptance(SceneEditor *editor, const char *scene, bool reopen) {
  const int times[] = {0, 30, 60, 119};
  char message[256], id[64]; MotionPaths paths;
  static TimelineDocument doc, original;
  RayEvaluatedLight baseline[4];
  choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
  if (reopen) {
    json_object *expected = json_object_from_file("dm3_light_expected.json");
    assert(expected && SceneEditorMotionPathsRead(&paths));
    for (int i = 0; i < 4; ++i) {
      json_object *actual = dm3_light_row(dm3_light_sample(times[i]));
      json_object *row = json_object_array_get_idx(expected, i);
      for (int k = 0; k < 4; ++k)
        assert(fabs(json_object_get_double(json_object_array_get_idx(actual, k)) -
                    json_object_get_double(json_object_array_get_idx(row, k))) < 1e-8);
      json_object_put(actual);
    }
    json_object_put(expected);
    fprintf(stderr, "D-M3 light fresh reopen PASS\n"); return;
  }
  assert(SceneEditorTimelineActivate());
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  bool has_intensity = false;
  for (size_t i = 0; i < doc.track_count; ++i)
    if (!strcmp(doc.tracks[i].property_id, "light/intensity")) has_intensity = true;
  if (!has_intensity) {
    TimelineTrack intensity;
    assert(TimelineTrackInit(&intensity, "light-intensity-proof", "light/light_key", "light/intensity", TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(&intensity, TIMELINE_UNIT_RELATIVE_INTENSITY) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&intensity, 0, TimelineValueScalar(2.8), TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&intensity, 119, TimelineValueScalar(1.4), TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&doc, &intensity) == TIMELINE_STATUS_OK);
  }
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (!strcmp(t->property_id, "light/path_progress"))
      assert(TimelineTrackInsertKey(t, (TimelineKeyframe){.frame=30, .value=TimelineValueScalar(.65), .interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR}, &(size_t){0}) == TIMELINE_STATUS_OK);
    if (!strcmp(t->property_id, "light/intensity"))
      assert(TimelineTrackInsertKey(t, (TimelineKeyframe){.frame=60, .value=TimelineValueScalar(5), .interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR}, &(size_t){0}) == TIMELINE_STATUS_OK);
  }
  { bool ok = SceneEditorDocumentSetTimeline(&doc, SceneEditorDocumentRevision(), message, sizeof(message)); if (!ok) fprintf(stderr, "Light fixture timeline: %s\n", message); assert(ok); }
  TimelineTrack disabled;
  assert(TimelineTrackInit(&disabled, "inactive-light-history", "light/light_key", "light/position", TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
  assert(TimelineTrackSetUnit(&disabled, TIMELINE_UNIT_WORLD_DISTANCE) == TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(&disabled, 0, TimelineValueVec3(9, 9, 9), TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
  disabled.enabled = false;
  assert(TimelineDocumentAddTrack(&doc, &disabled) == TIMELINE_STATUS_OK);
  { bool ok = SceneEditorDocumentSetTimeline(&doc, SceneEditorDocumentRevision(), message, sizeof(message)); if (!ok) fprintf(stderr, "Light fixture timeline: %s\n", message); assert(ok); }
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  original = doc;
  for (int i = 0; i < 4; ++i) baseline[i] = dm3_light_sample(times[i]);
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  json_object *legacy = json_object_from_file(scene);
  assert(legacy && json_object_to_file_ext("dm3_legacy.scene.json", legacy, JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(legacy);
  double origin[] = {-2, -2, 4};
  assert(SceneEditorMotionPathCreate("Light route", origin, 4, SceneEditorDocumentRevision(), id, sizeof(id), message, sizeof(message)));
  SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor, "paths");
  authoring_control(editor, "path_followers");
  unsigned long long rev = SceneEditorDocumentRevision();
  authoring_control(editor, "path_light_attach");
  assert(SceneEditorDocumentRevision() == rev + 1);
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[0].enabled &&
         !strcmp(paths.bindings[0].target_id, "light/light_key"));
  char original_source[TIMELINE_ID_CAPACITY], alternate[64];
  snprintf(original_source, sizeof(original_source), "%s", paths.bindings[0].restore_xyz_tracks[0]);
  assert(SceneEditorMotionPathCreate("Alternate light route", origin, 3, SceneEditorDocumentRevision(), alternate, sizeof(alternate), message, sizeof(message)));
  assert(SceneEditorMotionPathBindLight(alternate, true, SceneEditorDocumentRevision(), message, sizeof(message)));
  assert(SceneEditorMotionPathBindLight(id, true, SceneEditorDocumentRevision(), message, sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths));
  assert(paths.bindings[0].restore_count == 1 && !strcmp(paths.bindings[0].restore_xyz_tracks[0], original_source));
  MotionPaths bad = paths;
  snprintf(bad.bindings[0].target_id, TIMELINE_ID_CAPACITY, "light/missing");
  rev = SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathsSet(&bad, rev, message, sizeof(message)));
  assert(SceneEditorDocumentRevision() == rev);
  bad = paths;
  snprintf(bad.bindings[0].restore_xyz_tracks[0], TIMELINE_ID_CAPACITY, "missing-source");
  assert(!SceneEditorMotionPathsSet(&bad, rev, message, sizeof(message)));
  assert(SceneEditorDocumentRevision() == rev);
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < doc.track_count; ++i)
    if (!strcmp(doc.tracks[i].track_id, paths.bindings[0].restore_xyz_tracks[0])) doc.tracks[i].enabled = true;
  assert(!SceneEditorDocumentSetTimeline(&doc, rev, message, sizeof(message)));
  assert(SceneEditorDocumentRevision() == rev);
  authoring_control(editor, "path_light_timing");
  assert(!SceneEditorMotionPathPanelActive());
  const char *frames[] = {"30", "60", "119"};
  const char *values[] = {"0.25", "0.25", "1"};
  for (int i = 0; i < 3; ++i) {
    authoring_control(editor, "frame"); authoring_text(editor, frames[i]);
    authoring_control(editor, "value"); authoring_text(editor, values[i]);
  }
  json_object *expected = json_object_new_array();
  for (int i = 0; i < 4; ++i) {
    RayEvaluatedLight c = dm3_light_sample(times[i]);
    assert(fabs(c.intensity - baseline[i].intensity) < 1e-8);
    double progress[] = {0, .25, .25, 1};
    assert(fabs(c.position.x - (-2 + 4 * progress[i]) * SceneEditorDocumentWorldScale()) < 1e-8);
    assert(fabs(c.position.y + 2 * SceneEditorDocumentWorldScale()) < 1e-8);
    assert(fabs(c.position.z - 4 * SceneEditorDocumentWorldScale()) < 1e-8);
    json_object_array_add(expected, dm3_light_row(c));
  }
  assert(fabs(baseline[0].intensity - baseline[2].intensity) > 1);
  assert(json_object_to_file_ext("dm3_light_expected.json", expected, JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(expected);
  authoring_control(editor, "paths");
  authoring_control(editor, "path_light_detach");
  for (int i = 0; i < 4; ++i) {
    RayEvaluatedLight c = dm3_light_sample(times[i]);
    assert(fabs(c.position.x - baseline[i].position.x) < 1e-8 &&
           fabs(c.position.y - baseline[i].position.y) < 1e-8 &&
           fabs(c.position.z - baseline[i].position.z) < 1e-8);
  }
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < original.track_count; ++i) {
    if (memcmp(&original.tracks[i], &doc.tracks[i], sizeof(TimelineTrack)))
      fprintf(stderr, "Restoration mismatch: %s enabled %d -> %d\n", original.tracks[i].track_id, original.tracks[i].enabled, doc.tracks[i].enabled);
    assert(!memcmp(&original.tracks[i], &doc.tracks[i], sizeof(TimelineTrack)));
  }
  choose_menu(editor, 1, 0); /* restore route attachment */
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[0].enabled);
  choose_menu(editor, 1, 1);
  assert(SceneEditorMotionPathsRead(&paths) && !paths.bindings[0].enabled);
  choose_menu(editor, 1, 0);
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  capture(editor, "dm3_light_route.ppm");
  fprintf(stderr, "D-M3 light PASS: UI attachment/timing/detach, independent intensity, hold, invalid target refusal, undo/redo and exact legacy track restoration.\n");
}
