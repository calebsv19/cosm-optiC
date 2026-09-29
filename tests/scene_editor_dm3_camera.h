#include "app/evaluated_scene_service.h"
static RayEvaluatedCamera dm3_camera_sample(int frame) {
  static RayEvaluatedSceneSnapshot snapshot;
  assert(SceneEditorTimelineSeek(frame));
  assert(SceneEditorTimelineCopyEvaluated(&snapshot));
  return snapshot.camera;
}
static json_object *dm3_camera_row(RayEvaluatedCamera camera) {
  json_object *row = json_object_new_array();
  double v[] = {camera.position.x, camera.position.y, camera.position.z,
                camera.yaw_radians, camera.pitch_radians, camera.fov_y_degrees};
  for (int i = 0; i < 6; ++i) json_object_array_add(row, json_object_new_double(v[i]));
  return row;
}
static void dm3_camera_acceptance(SceneEditor *editor, const char *scene, bool reopen) {
  const int times[] = {0, 30, 60, 119};
  char message[256], id[64]; MotionPaths paths;
  static TimelineDocument doc, original;
  RayEvaluatedCamera baseline[4];
  choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
  if (reopen) {
    json_object *expected = json_object_from_file("dm3_camera_expected.json");
    assert(expected && SceneEditorMotionPathsRead(&paths));
    for (int i = 0; i < 4; ++i) {
      json_object *actual = dm3_camera_row(dm3_camera_sample(times[i]));
      json_object *row = json_object_array_get_idx(expected, i);
      for (int k = 0; k < 6; ++k)
        assert(fabs(json_object_get_double(json_object_array_get_idx(actual, k)) -
                    json_object_get_double(json_object_array_get_idx(row, k))) < 1e-8);
      json_object_put(actual);
    }
    json_object_put(expected);
    fprintf(stderr, "D-M3 camera fresh reopen PASS\n"); return;
  }
  assert(SceneEditorTimelineActivate());
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (!strcmp(t->property_id, "camera/path_progress"))
      assert(TimelineTrackInsertKey(t, (TimelineKeyframe){.frame=30, .value=TimelineValueScalar(.65), .interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR}, &(size_t){0}) == TIMELINE_STATUS_OK);
    if (!strcmp(t->property_id, "camera/fov_y"))
      assert(TimelineTrackInsertKey(t, (TimelineKeyframe){.frame=60, .value=TimelineValueScalar(47), .interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR}, &(size_t){0}) == TIMELINE_STATUS_OK);
  }
  assert(SceneEditorDocumentSetTimeline(&doc, SceneEditorDocumentRevision(), message, sizeof(message)));
  original = doc;
  for (int i = 0; i < 4; ++i) baseline[i] = dm3_camera_sample(times[i]);
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  json_object *legacy = json_object_from_file(scene);
  assert(legacy && json_object_to_file_ext("dm3_legacy.scene.json", legacy, JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(legacy);
  double origin[] = {-1, -6.2, 2.6};
  assert(SceneEditorMotionPathCreate("Camera route", origin, 2, SceneEditorDocumentRevision(), id, sizeof(id), message, sizeof(message)));
  SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor, "paths");
  authoring_control(editor, "path_followers");
  unsigned long long rev = SceneEditorDocumentRevision();
  authoring_control(editor, "path_camera_attach");
  assert(SceneEditorDocumentRevision() == rev + 1);
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[0].enabled &&
         !strcmp(paths.bindings[0].target_id, "camera/main"));
  MotionPaths bad = paths;
  snprintf(bad.bindings[0].target_id, TIMELINE_ID_CAPACITY, "camera/missing");
  rev = SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathsSet(&bad, rev, message, sizeof(message)));
  assert(SceneEditorDocumentRevision() == rev);
  authoring_control(editor, "path_camera_timing");
  assert(!SceneEditorMotionPathPanelActive());
  const char *frames[] = {"30", "60", "119"};
  const char *values[] = {"0.25", "0.25", "1"};
  for (int i = 0; i < 3; ++i) {
    authoring_control(editor, "frame"); authoring_text(editor, frames[i]);
    authoring_control(editor, "value"); authoring_text(editor, values[i]);
  }
  json_object *expected = json_object_new_array();
  for (int i = 0; i < 4; ++i) {
    RayEvaluatedCamera c = dm3_camera_sample(times[i]);
    assert(fabs(c.yaw_radians - baseline[i].yaw_radians) < 1e-8);
    assert(fabs(c.pitch_radians - baseline[i].pitch_radians) < 1e-8);
    assert(fabs(c.fov_y_degrees - baseline[i].fov_y_degrees) < 1e-8);
    double progress[] = {0, .25, .25, 1};
    assert(fabs(c.position.x - (-1 + 2 * progress[i]) * SceneEditorDocumentWorldScale()) < 1e-8);
    json_object_array_add(expected, dm3_camera_row(c));
  }
  assert(json_object_to_file_ext("dm3_camera_expected.json", expected, JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(expected);
  authoring_control(editor, "paths");
  authoring_control(editor, "path_camera_detach");
  for (int i = 0; i < 4; ++i) {
    RayEvaluatedCamera c = dm3_camera_sample(times[i]);
    assert(fabs(c.position.x - baseline[i].position.x) < 1e-8 &&
           fabs(c.position.y - baseline[i].position.y) < 1e-8 &&
           fabs(c.position.z - baseline[i].position.z) < 1e-8);
  }
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < original.track_count; ++i)
    assert(!memcmp(&original.tracks[i], &doc.tracks[i], sizeof(TimelineTrack)));
  choose_menu(editor, 1, 0); /* restore route attachment */
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[0].enabled);
  choose_menu(editor, 1, 1);
  assert(SceneEditorMotionPathsRead(&paths) && !paths.bindings[0].enabled);
  choose_menu(editor, 1, 0);
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  capture(editor, "dm3_camera_route.ppm");
  fprintf(stderr, "D-M3 camera PASS: UI attachment/timing/detach, independent orientation/FOV, hold, invalid target refusal, undo/redo and exact legacy track restoration.\n");
}
