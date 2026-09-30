#include "editor/scene_editor_motion_plan.h"
#include "import/runtime_scene_timeline.h"
static json_object *dm5_get(json_object *o, const char *k) {
  json_object *v = NULL;
  assert(json_object_object_get_ex(o, k, &v));
  return v;
}
#include "scene_editor_dm5_audit.h"
static void dm5(SceneEditor *editor, const char *scene, bool reopen) {
  const char *targets[] = {"camera/main", "light/light_key",
                           "object/obj_sphere_medium"};
  const int frames[] = {0, 15, 23, 40, 60, 90, 119};
  char message[512];
  choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
  if (!reopen) {
    static TimelineDocument before, after;
    assert(SceneEditorDocumentGetTimeline(&before) == TIMELINE_STATUS_OK);
    MotionTimingScheduleRequest r = {
        .max_speed = 100, .acceleration = 10000, .braking = 5000, .count = 2};
    r.points[1] = (MotionTimingWaypoint){
        .position = 1, .fixed_arrival = true, .arrival = 119.0 / 24};
    /* A live corner must refuse pass-through and accept an explicitly declared
     * stop at its exact arc position, including normalized persistence. */
    MotionPaths original, corner;
    assert(SceneEditorMotionPathsRead(&original));
    corner = original;
    for (size_t k = 0; k < corner.paths[0].count; ++k)
      corner.paths[0].points[k].linear = true;
    assert(SceneEditorMotionPathsSet(&corner, SceneEditorDocumentRevision(),
                                     message, sizeof(message)));
    assert(!SceneEditorMotionPlanApply(targets[0], &r,
                                       SceneEditorDocumentRevision(), message,
                                       sizeof(message)));
    assert(strstr(message, "stop"));
    MotionRouteGeometry *corner_g = malloc(sizeof(*corner_g));
    assert(corner_g && MotionRouteGeometryBuild(&corner.paths[0],
                                                SceneEditorDocumentWorldScale(),
                                                corner_g) == MOTION_ROUTE_OK);
    MotionTimingScheduleRequest corners = r;
    corners.count = corner.paths[0].count;
    for (size_t k = 0; k < corners.count; ++k)
      corners.points[k] = (MotionTimingWaypoint){
          .position = corner_g->point_distances[k] / corner_g->length};
    corners.points[corners.count - 1].fixed_arrival = true;
    corners.points[corners.count - 1].arrival = 119.0 / 24;
    assert(SceneEditorMotionPlanApply(targets[0], &corners,
                                      SceneEditorDocumentRevision(), message,
                                      sizeof(message)));
    MotionTimingScheduleSample stopped;
    const MotionRouteSchedule *corner_plan;
    assert(MotionPlansRuntimeReadback(targets[0], 0, &stopped, &corner_plan));
    assert(MotionPlansRuntimeReadback(
        targets[0], corner_plan->timeline.arrivals[1], &stopped, NULL));
    assert(stopped.velocity == 0);
    assert(SceneEditorMotionPlanRestore(
        targets[0], SceneEditorDocumentRevision(), message, sizeof(message)));
    assert(SceneEditorMotionPathsSet(&original, SceneEditorDocumentRevision(),
                                     message, sizeof(message)));
    free(corner_g);
    unsigned long long rev = SceneEditorDocumentRevision();
    MotionRouteSchedule preview;
    assert(SceneEditorMotionPlanPreview(targets[0], &r, &preview, message,
                                        sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    MotionTimingScheduleRequest bad = r;
    bad.points[1].arrival = .00001;
    assert(!SceneEditorMotionPlanApply(targets[0], &bad, rev, message,
                                       sizeof(message)));
    assert(strstr(message, "waypoint"));
    assert(SceneEditorDocumentRevision() == rev);
    assert(!SceneEditorMotionPlanApply(targets[0], &r, rev - 1, message,
                                       sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    for (int i = 0; i < 3; ++i) {
      MotionTimingScheduleRequest q = r;
      if (i == 1) {
        q.count = 3;
        q.points[1] = (MotionTimingWaypoint){.position = .4, .hold = .3};
        q.points[2] = r.points[1];
      }
      if (i == 2) {
        q.count = 4;
        q.points[1] = (MotionTimingWaypoint){.position = .8};
        q.points[2] = (MotionTimingWaypoint){.position = .3, .hold = .1};
        q.points[3] = r.points[1];
      }
      if (!SceneEditorMotionPlanApply(targets[i], &q,
                                      SceneEditorDocumentRevision(), message,
                                      sizeof(message))) {
        fprintf(stderr, "M5 apply %s: %s\n", targets[i], message);
        assert(false);
      }
      assert(MotionPlansRuntimeActive(targets[i]));
    }
    assert(SceneEditorDocumentGetTimeline(&after) == TIMELINE_STATUS_OK);
    assert(!memcmp(&before, &after, sizeof(before)));
    assert(SceneEditorDocumentUndo(message, sizeof(message)));
    assert(!MotionPlansRuntimeActive(targets[2]));
    assert(SceneEditorDocumentRedo(message, sizeof(message)));
    assert(MotionPlansRuntimeActive(targets[2]));
    rev = SceneEditorDocumentRevision();
    MotionPaths paths;
    assert(SceneEditorMotionPathsRead(&paths));
    paths.paths[0].points[0].position[0] += .1;
    assert(!SceneEditorMotionPathsSet(&paths, rev, message, sizeof(message)));
    assert(strstr(message, "Restore"));
    assert(SceneEditorDocumentRevision() == rev);
    after.rate = (TimelineRate){30, 1};
    assert(
        !SceneEditorDocumentSetTimeline(&after, rev, message, sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    after = before;
    after.tracks[0].keys[0].value.as.scalar +=
        .01; /* Disabled predecessor may remain editable; dependency targets
                only route track. */
    after = before;
    for (size_t i = 0; i < after.track_count; ++i)
      if (!strcmp(after.tracks[i].property_id, MOTION_CAMERA_PROGRESS_PROPERTY))
        after.tracks[i].keys[0].value.as.scalar = .1;
    assert(
        !SceneEditorDocumentSetTimeline(&after, rev, message, sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    assert(!SceneEditorMotionPathBindCamera("path-1", false, rev, message,
                                            sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    /* Independent orientation/lighting channels remain editable. */
    after = before;
    bool changed = false;
    for (size_t i = 0; i < after.track_count; ++i)
      if (!strcmp(after.tracks[i].property_id, "camera/fov_y")) {
        after.tracks[i].keys[0].value.as.scalar += 1;
        changed = true;
      }
    assert(changed);
    assert(
        SceneEditorDocumentSetTimeline(&after, rev, message, sizeof(message)));
    assert(SceneEditorDocumentUndo(message, sizeof(message)));
    SceneEditorMotionPathPanelSelect(true);
    SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"path_followers");
    dm4_inspector_control(editor,"path_follower_camera");
    dm4_inspector_control(editor,"path_follower_plan");
    dm4_inspector_control(editor, "plan_preview");
    assert(MotionPlansRuntimeActive(targets[0]));
    dm4_inspector_control(editor, "plan_restore");
    assert(!MotionPlansRuntimeActive(targets[0]));
    assert(SceneEditorDocumentUndo(message, sizeof(message)));
    assert(MotionPlansRuntimeActive(targets[0]));
    assert(SceneEditorDocumentRedo(message, sizeof(message)));
    assert(!MotionPlansRuntimeActive(targets[0]));
    /* Draft revision changed by undo/redo; reload defaults then use real
     * fields. */
    dm4_inspector_control(editor, "plan_reload");
    dm4_inspector_control(editor, "plan_speed");
    assert(SDL_IsTextInputActive());
    authoring_text(editor, "100");
    dm4_inspector_control(editor, "plan_accel");
    authoring_text(editor, "10000");
    dm4_inspector_control(editor, "plan_brake");
    authoring_text(editor, "5000");
    dm4_inspector_control(editor, "plan_next");
    dm4_inspector_control(editor, "plan_arrival");
    authoring_text(editor, "4.958333333333333");
    dm4_inspector_control(editor, "plan_fixed");
    dm4_inspector_control(editor, "plan_preview");
    dm4_inspector_control(editor, "plan_apply");
    assert(MotionPlansRuntimeActive(targets[0]));
    dm4_inspector_control(editor, "plan_reload");
    capture(editor, "m5_planner.ppm");
    assert(SceneEditorDocumentSave(message, sizeof(message)));
    json_object *saved = json_object_from_file(scene);
    assert(saved);
    json_object *author = MotionPlansAuthor(saved),
                *list = dm5_get(author, "motion_plans");
    assert(json_object_array_length(list) == 3);
    assert(RuntimeSceneTimelineValidateScene(saved, message, sizeof(message)));
    json_object_object_add(saved, "world_scale", json_object_new_double(3));
    assert(!RuntimeSceneTimelineValidateScene(saved, message, sizeof(message)));
    json_object_object_add(saved, "world_scale", json_object_new_double(2));
    json_object *copy = json_object_get(json_object_array_get_idx(list, 0));
    json_object_array_add(list, copy);
    assert(!RuntimeSceneTimelineValidateScene(saved, message, sizeof(message)));
    json_object_array_del_idx(list, 3, 1);
    json_object *entry = json_object_array_get_idx(list, 0);
    json_object_object_add(entry, "schema", json_object_new_string("unknown"));
    assert(!RuntimeSceneTimelineValidateScene(saved, message, sizeof(message)));
    json_object_put(saved);
  }
  if (reopen)
    dm5_audit(scene);
  for (int i = 0; i < 3; ++i)
    assert(MotionPlansRuntimeActive(targets[i]));
  json_object *rows = reopen ? json_object_from_file("dm5_expected.json")
                             : json_object_new_array();
  assert(rows);
  for (int j = 0; j < 7; ++j) {
    int i = reopen ? 6 - j : j;
    RayEvaluatedSceneSnapshot sample = dm3_complete_sample(frames[i], 0);
    dm3_focus_check(&sample.camera);
    json_object *row = dm3_complete_row(&sample, true);
    if (reopen) {
      json_object *prior = json_object_array_get_idx(rows, i);
      for (int k = 0; k < 13; ++k)
        assert(fabs(json_object_get_double(json_object_array_get_idx(row, k)) -
                    json_object_get_double(
                        json_object_array_get_idx(prior, k))) < 1e-8);
      json_object_put(row);
    } else
      json_object_array_add(rows, row);
  }
  if (!reopen)
    assert(json_object_to_file_ext("dm5_expected.json", rows,
                                   JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(rows);
  /* Samples in opposite order must be bit-stable, bounded, and share the
   * retained local clock. Check rendered XYZ against independent route sample.
   */
  MotionPaths paths;
  assert(SceneEditorMotionPathsRead(&paths));
  MotionRouteGeometry *g = malloc(sizeof(*g));
  assert(g && MotionRouteGeometryBuild(&paths.paths[0],
                                       SceneEditorDocumentWorldScale(),
                                       g) == MOTION_ROUTE_OK);
  for (int i = 0; i < 3; ++i) {
    MotionTimingScheduleRequest r;
    assert(SceneEditorMotionPlanRead(targets[i], &r));
    for (size_t k = 0; k < r.count; ++k)
      r.points[k].position *= g->length;
    MotionRouteSchedule p;
    assert(MotionRouteScheduleBuild(g, &r, &p, NULL) == MOTION_TIMING_OK);
    for (int j = 0; j <= 119; ++j) {
      int f = reopen ? 119 - j : j;
      MotionRouteSample expected;
      assert(MotionRouteScheduleSample(g, &p, (double)f / 24, &expected));
      RayEvaluatedSceneSnapshot actual = dm3_complete_sample(f, 0);
      json_object *row = dm3_complete_row(&actual, true);
      int offset = i == 0 ? 0 : i == 1 ? 6 : 10;
      for (int k = 0; k < 3; ++k)
        assert(fabs(expected.position[k] -
                    json_object_get_double(
                        json_object_array_get_idx(row, offset + k))) < 1e-8);
      json_object_put(row);
      double speed = 0, accel = 0;
      for (int k = 0; k < 3; ++k) {
        speed += expected.velocity[k] * expected.velocity[k];
        accel += expected.acceleration[k] * expected.acceleration[k];
      }
      assert(sqrt(speed) <= r.max_speed * (1 + 1e-8));
      assert(sqrt(accel) <= fmax(r.acceleration, r.braking) * (1 + 1e-8));
    }
  }
  free(g);
  fprintf(stderr,
          "D-M5 %s PASS: retained plans, native Apply/Restore, dependency "
          "refusal, undo/redo, saved source preservation, combined "
          "focus/FOV/intensity, reversal/hold and arbitrary-seek XYZ parity\n",
          reopen ? "reopen" : "prepare");
}
