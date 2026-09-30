#include "editor/scene_editor_motion_paths.h"
#include "import/runtime_scene_timeline.h"
#include "scene_editor_dm2_sampler.h"
static TimelineVec3 dm2_sample(const char *id, int frame) {
  assert(SceneEditorTimelineSeek(frame));
  RayEvaluatedSceneSnapshot snapshot;
  assert(SceneEditorTimelineCopyEvaluated(&snapshot));
  for (size_t i = 0; i < snapshot.object_transform_count; ++i)
    if (!strcmp(snapshot.object_transforms[i].target_id, id))
      return snapshot.object_transforms[i].position;
  assert(false);
  return (TimelineVec3){0};
}
static void dm2_samples(bool compare) {
  const int times[] = {0, 20, 40, 60, 80, 119, 10, 11, 12, 40};
  json_object *rows = compare ? json_object_from_file("dm2_expected.json")
                              : json_object_new_array();
  assert(rows);
  for (int i = 0; i < 10; ++i) {
    TimelineVec3 v = dm2_sample("obj_sphere_medium", times[i]);
    double xyz[] = {v.x, v.y, v.z};
    json_object *row =
        compare ? json_object_array_get_idx(rows, i) : json_object_new_array();
    for (int k = 0; k < 3; ++k) {
      if (compare)
        assert(fabs(xyz[k] - json_object_get_double(
                                 json_object_array_get_idx(row, k))) < 1e-8);
      else
        json_object_array_add(row, json_object_new_double(xyz[k]));
    }
    if (!compare)
      json_object_array_add(rows, row);
  }
  if (!compare)
    assert(json_object_to_file_ext("dm2_expected.json", rows,
                                   JSON_C_TO_STRING_PRETTY) == 0);
  json_object_put(rows);
}
static void dm2_drag(SceneEditor *editor, bool cancel) {
  MotionPaths paths;
  assert(SceneEditorMotionPathsRead(&paths));
  double *point = paths.paths[0].points[0].position;
  SDL_Rect handle;assert(SceneEditorRenderAuthoringControl("path_gizmo_x",&handle));
  int x=handle.x+handle.w/2,y=handle.y+handle.h/2;
  unsigned long long rev = SceneEditorDocumentRevision();
  SDL_Event e = {0};
  e.type = SDL_MOUSEBUTTONDOWN;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = x;
  e.button.y = y;
  SceneEditorSessionRuntimeHandleEvent(editor, &e);
  SceneEditorSessionRuntimeRender(editor);
  e = (SDL_Event){0};
  e.type = SDL_MOUSEMOTION;
  e.motion.x = x + 25;
  e.motion.y = y + 12;
  e.motion.state = SDL_BUTTON_LMASK;
  SceneEditorSessionRuntimeHandleEvent(editor, &e);
  SceneEditorSessionRuntimeRender(editor);
  if (cancel)
    key(editor, SDLK_ESCAPE);
  e = (SDL_Event){0};
  e.type = SDL_MOUSEBUTTONUP;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = x + 25;
  e.button.y = y + 12;
  SceneEditorSessionRuntimeHandleEvent(editor, &e);
  SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorDocumentRevision() == rev + (cancel ? 0 : 1));
  if (!cancel) {
    MotionPaths changed;
    assert(SceneEditorMotionPathsRead(&changed));
    assert(fabs(changed.paths[0].points[0].position[0] - point[0]) > 1e-5);
    choose_menu(editor, 1, 0);
  }
}
static void dm2_compact(SceneEditor *editor) {
  SceneEditorTimelineClearSelection();SceneEditorMotionPathPanelReset();
  SceneEditorMotionPathPanelSelect(true);
  SceneEditorSessionRuntimeRender(editor);
  SDL_SetWindowSize(editor->window, 1024, 640);
  SDL_PumpEvents();
  SceneEditorSessionRuntimeRender(editor);
  SceneEditorSessionRuntimeRender(editor); /* swapchain resize may skip first draw */
  SceneEditorPaneLayout layout;
  assert(SceneEditorGetPaneLayout(&layout));
  authoring_control(editor,"path_point_details");
  SDL_Event wheel = {0};
  wheel.type = SDL_MOUSEWHEEL;
  wheel.wheel.y = -20;
  wheel.wheel.mouseX = layout.right_content_rect.x + 20;
  wheel.wheel.mouseY = layout.right_content_rect.y + 30;
  SceneEditorSessionRuntimeHandleEvent(editor, &wheel);
  SceneEditorSessionRuntimeRender(editor);
  SDL_Rect field;
  capture(editor, "dm2_compact_before.ppm");
  fprintf(stderr, "compact active=%d pane=%d,%d %dx%d\n",
          SceneEditorMotionPathPanelActive(), layout.right_content_rect.x,
          layout.right_content_rect.y, layout.right_content_rect.w,
          layout.right_content_rect.h);
  assert(SceneEditorRenderAuthoringControl("path_out_z", &field));
  wheel.wheel.mouseX = layout.left_content_rect.x + 20;
  SceneEditorSessionRuntimeHandleEvent(editor, &wheel);
  SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor,"path_actions");
  SceneEditorSessionRuntimeHandleEvent(editor,&wheel);SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorRenderAuthoringControl("path_save", &field));
  capture(editor, "dm2_compact.ppm");
}
static void dm2_acceptance(SceneEditor *editor, const char *scene,
                           bool reopen) {
  char message[256];
  MotionPaths paths;
  static TimelineDocument doc;
  SceneEditorObjectReadback read;
  choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
  if (reopen) {
    assert(SceneEditorMotionPathsRead(&paths) && paths.count == 1 &&
           paths.binding_count == 2);
    dm2_samples(true);
    fprintf(stderr, "D-M2 fresh reopen PASS\n");
    return;
  }
  assert(SceneEditorObjectExecute(SCENE_OBJECT_SELECT, "obj_sphere_medium",
                                  NULL, false, SceneEditorDocumentRevision(),
                                  &read, message, sizeof(message)));
  assert(SceneEditorObjectTimelineAdd(
      "obj_sphere_medium", message,
      sizeof(message))); /* prior XYZ must survive */
  SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor, "paths");
  authoring_control(editor, "new_path");
  authoring_text(editor, "Test flight");
  authoring_control(editor,"path_select_tool");
  assert(SceneEditorMotionPathsRead(&paths) && paths.count == 1 &&
         !strcmp(paths.paths[0].name, "Test flight"));
  authoring_control(editor, "path_x");
  authoring_text(editor, "-1");
  authoring_control(editor, "path_y");
  authoring_text(editor, "0");
  authoring_control(editor, "path_z");
  authoring_text(editor, "1");
  authoring_control(editor, "path_next");
  authoring_control(editor, "path_x");
  authoring_text(editor, "1");
  authoring_control(editor, "path_z");
  authoring_text(editor, "2");
  authoring_control(editor, "path_previous");
  authoring_control(editor,"path_point_details");
  authoring_control(editor, "path_out_y");
  authoring_text(editor, "1");
  authoring_control(editor, "path_frame_selected");
  dm2_drag(editor, true);
  dm2_drag(editor, false);
  authoring_control(editor, "path_add");
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].count == 3);
  authoring_control(editor, "path_remove");
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].count == 2);
  choose_menu(editor, 1, 0);
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].count == 3);
  choose_menu(editor, 1, 1);
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].count == 2);
  unsigned long long rev = SceneEditorDocumentRevision();
  authoring_control(editor,"path_followers");
  authoring_control(editor, "path_attach");
  assert(SceneEditorDocumentRevision() == rev + 1);
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count == 1 &&
         paths.bindings[0].enabled);
  /* Deletion while referenced must refuse without a mutation. */
  rev = SceneEditorDocumentRevision();
  authoring_control(editor,"path_actions");
  authoring_control(editor, "path_delete");
  authoring_control(editor,"path_actions");
  assert(SceneEditorDocumentRevision() == rev);
  authoring_control(editor, "path_timing");
  assert(!SceneEditorMotionPathPanelActive());
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  size_t progress = SIZE_MAX;
  for (size_t i = 0; i < doc.track_count; ++i) {
    if (RuntimeObjectTimelineAxis(doc.tracks[i].property_id) >= 0)
      assert(!doc.tracks[i].enabled);
    if (!strcmp(doc.tracks[i].property_id, MOTION_PROGRESS_PROPERTY))
      progress = i;
  }
  assert(progress != SIZE_MAX);
  const char *frames[] = {"20", "40", "60"};
  const char *values[] = {"0.4", "0.4", "1"};
  for (int i = 0; i < 3; ++i) {
    authoring_control(editor, "frame");
    authoring_text(editor, frames[i]);
    authoring_control(editor, "value");
    authoring_text(editor, values[i]);
  }
  TimelineVec3 a = dm2_sample("obj_sphere_medium", 20),
               b = dm2_sample("obj_sphere_medium", 40),
               c = dm2_sample("obj_sphere_medium", 60);
  assert(fabs(a.x - b.x) < 1e-8 && fabs(a.z - b.z) < 1e-8 &&
         fabs(a.x - c.x) > .1);
  /* Shape edits cannot retime or change progress keys. */
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  TimelineTrack original = doc.tracks[progress];
  authoring_control(editor, "paths");
  authoring_control(editor,"path_follower_back");
  authoring_control(editor, "path_previous");
  authoring_control(editor, "path_out_z");
  authoring_text(editor, "1.2");
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK &&
         !memcmp(&original, &doc.tracks[progress], sizeof(original)));
  assert(!SceneEditorMotionPathsSet(&paths, 0, message, sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths));
  /* Invalid references and ownership reject atomically. */
  MotionPaths broken = paths;
  snprintf(broken.bindings[0].path_id, 64, "missing");
  rev = SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathsSet(&broken, rev, message, sizeof(message)) &&
         SceneEditorDocumentRevision() == rev);
  static TimelineDocument bad;
  bad = doc;
  for (size_t i = 0; i < bad.track_count; ++i)
    if (RuntimeObjectTimelineAxis(bad.tracks[i].property_id) >= 0)
      bad.tracks[i].enabled = true;
  assert(!SceneEditorDocumentSetTimeline(&bad, rev, message, sizeof(message)) &&
         SceneEditorDocumentRevision() == rev);
  authoring_control(editor,"path_followers");
  authoring_control(editor,"path_follower_object");
  authoring_control(editor, "path_detach");
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < doc.track_count; ++i)
    if (RuntimeObjectTimelineAxis(doc.tracks[i].property_id) >= 0)
      assert(doc.tracks[i].enabled);
  choose_menu(editor, 1, 0);
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[0].enabled);
  /* Second mesh, same geometry, different timing. */
  assert(SceneEditorMotionPathBind("obj_sphere_second", paths.paths[0].id, true,
                                   SceneEditorDocumentRevision(), message,
                                   sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < doc.track_count; ++i)
    if (!strcmp(doc.tracks[i].target_id, "object/obj_sphere_second")) {
      doc.tracks[i].keys[0].value = TimelineValueScalar(1);
      doc.tracks[i].keys[1].value = TimelineValueScalar(0);
    }
  assert(SceneEditorDocumentSetTimeline(&doc, SceneEditorDocumentRevision(),
                                        message, sizeof(message)));
  a = dm2_sample("obj_sphere_medium", 0);
  b = dm2_sample("obj_sphere_second", 0);
  assert(fabs(a.x - b.x) > 1);
  dm2_samples(false);
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  assert(SceneEditorDocumentOpen(scene, message, sizeof(message)));
  dm2_samples(true);
  SceneEditorSessionRuntimeRender(editor);
  capture(editor, "dm2_paths.ppm");
  dm2_compact(editor);
  fprintf(stderr,
          "D-M2 PASS: native create/name/shape/attach/timing, hold/resume, "
          "ownership/refusal, undo, second follower, save/reopen.\n");
}
