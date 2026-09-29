/* Regression cases for prior-source retention and edit-selection ownership. */
#include "import/scene_timeline_document_io.h"
static void dm2_repair_draft(SceneEditor *editor, const char *control) {
  authoring_control(editor, control);
  SDL_Event e = {0};
  e.type = SDL_TEXTINPUT;
  snprintf(e.text.text, sizeof(e.text.text), "987");
  SceneEditorSessionRuntimeHandleEvent(editor, &e);
}
static void dm2_repair_select_row(SceneEditor *editor, const char *id) {
  char control[96];
  snprintf(control, sizeof(control), "path_row/%s", id);
  authoring_control(editor, control);
}
static void dm2_repair_fields(SceneEditor *editor, const char *a, const char *b) {
  MotionPaths before, after;
  SceneEditorMotionPathPanelSelect(true);
  SceneEditorSessionRuntimeRender(editor);
  dm2_repair_select_row(editor, a);
  assert(SceneEditorMotionPathsRead(&before));
  unsigned long long rev = SceneEditorDocumentRevision();
  dm2_repair_draft(editor, "path_x");
  authoring_control(editor, "path_next");
  key(editor, SDLK_RETURN);
  assert(SceneEditorDocumentRevision() == rev);
  dm2_repair_draft(editor, "path_y");
  dm2_repair_select_row(editor, b);
  key(editor, SDLK_RETURN);
  assert(SceneEditorDocumentRevision() == rev);
  dm2_repair_draft(editor, "path_name");
  dm2_repair_select_row(editor, a);
  key(editor, SDLK_RETURN);
  assert(SceneEditorDocumentRevision() == rev);
  authoring_control(editor, "path_frame_selected");
  SceneEditorDigestOverlayProjector projector = dm1_projector();
  int x, y;
  double scale = SceneEditorDocumentWorldScale();
  const double *point = before.paths[0].points[1].position;
  assert(SceneEditorDigestOverlayProjectPoint(&projector, point[0] * scale,
      point[1] * scale, point[2] * scale, &x, &y));
  dm2_repair_draft(editor, "path_x");
  click(editor, (SDL_Rect){x - 1, y - 1, 2, 2});
  key(editor, SDLK_RETURN);
  assert(SceneEditorDocumentRevision() == rev);
  assert(SceneEditorMotionPathsRead(&after));
  assert(!memcmp(&before, &after, sizeof(before)));
  /* Normal editing still works after cancellation; one undo restores it. */
  authoring_control(editor, "path_x");
  authoring_text(editor, "123");
  assert(SceneEditorDocumentRevision() == rev + 1);
  choose_menu(editor, 1, 0);
  assert(SceneEditorMotionPathsRead(&after));
  assert(!memcmp(&before, &after, sizeof(before)));
}
static void dm2_repair_tracks(bool detached) {
  static TimelineDocument doc;
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  int active = 0, inactive = 0;
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (RuntimeObjectTimelineAxis(t->property_id) < 0) continue;
    if (!strcmp(t->target_id, "object/obj_sphere_medium")) {
      assert(t->enabled == detached);
      ++active;
    }
    if (!strcmp(t->target_id, "object/obj_sphere_second")) {
      assert(!t->enabled);
      ++inactive;
    }
  }
  assert(active == 3 && inactive == 2); /* intentionally incomplete inactive XYZ */
}
static void dm2_repairs(SceneEditor *editor, const char *scene, bool reopen) {
  char message[256], a[64], b[64];
  MotionPaths paths;
  static TimelineDocument doc;
  choose_menu(editor, -1, SCENE_WORKSPACE_RENDER);
  if (!reopen) {
    assert(SceneEditorObjectTimelineAdd("obj_sphere_medium", message, sizeof(message)));
    assert(SceneEditorObjectTimelineAdd("obj_sphere_second", message, sizeof(message)));
    assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
    for (size_t i = 0; i < doc.track_count;) {
      TimelineTrack *t = &doc.tracks[i];
      if (!strcmp(t->target_id, "object/obj_sphere_second") &&
          RuntimeObjectTimelineAxis(t->property_id) >= 0) {
        t->enabled = false;
        if (RuntimeObjectTimelineAxis(t->property_id) == 2) {
          memmove(t, t + 1, (--doc.track_count - i) * sizeof(*t));
          continue;
        }
      }
      ++i;
    }
    assert(SceneEditorDocumentSetTimeline(&doc, SceneEditorDocumentRevision(), message, sizeof(message)));
    json_object *expected = SceneTimelineDocumentToJson(&doc);
    assert(expected && json_object_to_file_ext("prior_timeline.json", expected,
        JSON_C_TO_STRING_PRETTY) == 0);
    json_object_put(expected);
    const double origin[3] = {0, 0, 1};
    assert(SceneEditorMotionPathCreate("Repair A", origin, 2, SceneEditorDocumentRevision(), a, sizeof(a), message, sizeof(message)));
    assert(SceneEditorMotionPathCreate("Repair B", origin, 3, SceneEditorDocumentRevision(), b, sizeof(b), message, sizeof(message)));
    dm2_repair_fields(editor, a, b);
    assert(SceneEditorMotionPathBind("obj_sphere_medium", a, true, SceneEditorDocumentRevision(), message, sizeof(message)));
    assert(SceneEditorMotionPathBind("obj_sphere_second", a, true, SceneEditorDocumentRevision(), message, sizeof(message)));
    /* Rebinding must retain the original XYZ source rather than capture disabled tracks. */
    assert(SceneEditorMotionPathBind("obj_sphere_medium", b, true, SceneEditorDocumentRevision(), message, sizeof(message)));
    assert(SceneEditorMotionPathsRead(&paths));
    assert(paths.bindings[0].restore_known && paths.bindings[0].restore_count == 3);
    assert(paths.bindings[1].restore_known && paths.bindings[1].restore_count == 0);
    dm2_repair_tracks(false);
    /* Missing restoration references must reject without losing the active binding. */
    MotionPaths bad = paths;
    snprintf(bad.bindings[0].restore_xyz_tracks[0], TIMELINE_ID_CAPACITY, "missing-track");
    assert(SceneEditorMotionPathsSet(&bad, SceneEditorDocumentRevision(), message, sizeof(message)));
    unsigned long long rev = SceneEditorDocumentRevision();
    assert(!SceneEditorMotionPathBind("obj_sphere_medium", b, false, rev, message, sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev);
    assert(SceneEditorMotionPathsSet(&paths, rev, message, sizeof(message)));
    assert(SceneEditorDocumentSave(message, sizeof(message)));
    fprintf(stderr, "D-M2 repair prepare PASS: drafts cancelled, active/static predecessors, rebind, missing-track refusal, saved attached state.\n");
    return;
  }
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count == 2);
  dm2_repair_tracks(false);
  assert(paths.bindings[0].restore_count == 3 && paths.bindings[1].restore_count == 0);
  for (size_t i = 0; i < paths.binding_count; ++i) {
    unsigned long long rev = SceneEditorDocumentRevision();
    assert(SceneEditorMotionPathBind(paths.bindings[i].object_id, paths.bindings[i].path_id, false, rev, message, sizeof(message)));
    assert(SceneEditorDocumentRevision() == rev + 1);
    choose_menu(editor, 1, 0);
    MotionPaths undo;
    assert(SceneEditorMotionPathsRead(&undo) && undo.bindings[i].enabled);
    choose_menu(editor, 1, 1);
    assert(SceneEditorMotionPathsRead(&undo) && !undo.bindings[i].enabled);
  }
  dm2_repair_tracks(true);
  /* Compare all original track bytes, including key times, values and handles;
   * only the new disabled progress tracks may be additional. */
  static TimelineDocument expected;
  json_object *prior = json_object_from_file("prior_timeline.json");
  assert(prior && SceneTimelineDocumentFromJson(prior, &expected) == TIMELINE_STATUS_OK);
  json_object_put(prior);
  assert(SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK);
  for (size_t i = 0; i < expected.track_count; ++i) {
    bool found = false;
    for (size_t j = 0; j < doc.track_count; ++j)
      if (!strcmp(expected.tracks[i].track_id, doc.tracks[j].track_id)) {
        assert(!memcmp(&expected.tracks[i], &doc.tracks[j], sizeof(TimelineTrack)));
        found = true;
      }
    assert(found);
  }
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  assert(SceneEditorDocumentOpen(scene, message, sizeof(message)));
  dm2_repair_tracks(true);
  /* Old bindings lack provenance: round-trip as unknown and safely detach static. */
  assert(SceneEditorMotionPathBind("obj_sphere_medium", paths.paths[0].id, true, SceneEditorDocumentRevision(), message, sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths));
  paths.bindings[0].restore_known = false;
  paths.bindings[0].restore_count = 0;
  assert(SceneEditorMotionPathsSet(&paths, SceneEditorDocumentRevision(), message, sizeof(message)));
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  assert(SceneEditorDocumentOpen(scene, message, sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths) && !paths.bindings[0].restore_known);
  assert(SceneEditorMotionPathBind("obj_sphere_medium", paths.paths[0].id, false, SceneEditorDocumentRevision(), message, sizeof(message)));
  assert(strstr(message, "old binding"));
  dm2_repair_tracks(false);
  fprintf(stderr, "D-M2 repair reopen PASS: exact prior XYZ/static restoration, undo/redo, persistence, legacy static fallback.\n");
}
