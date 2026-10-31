#include "../src/editor/scene_editor_event_planner_internal.h"
#include "editor/scene_editor_event_planner.h"
static void planner_click(SceneEditor *e, const char *name) {
  SDL_Rect b;
  if (!SceneEditorRenderAuthoringControl(name, &b))
    fprintf(stderr, "Missing planner control: %s\n", name);
  assert(SceneEditorRenderAuthoringControl(name, &b));
  assert(b.w > 0);
  click(e, b);
}
static json_object *planner_pick(int x, int y, unsigned long long token) {
  json_object *args = json_object_new_object(), *out = json_object_new_object();
  pi(args, "x", x);
  pi(args, "y", y);
  pi(args, "viewport_token", token);
  pi(args, "expected_revision", SceneEditorDocumentRevision());
  char d[256];
  bool ok = SceneEditorEventPlannerPick(args, out, d, sizeof(d));
  json_object_put(args);
  if (!ok) {
    json_object_put(out);
    return NULL;
  }
  json_object *v = pc(pg(out, "event_feature"));
  json_object_put(out);
  return v;
}
static void event_planner_acceptance(SceneEditor *editor, const char *scene,
                                     bool reopen) {
  SceneEditorPaneLayout layout;
  char message[512];
  assert(SceneEditorTimelineActivate());
  SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_RENDER);
  SceneEditorRenderAuthoringSelect(editor, true);
  SceneEditorRenderAuthoringSetTiming(true);
  SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_SOLID);
  SceneEditorSessionRuntimeRender(editor);
  SDL_Rect rect;
  assert(!SceneEditorEventPlannerVisible());
  assert(!SceneEditorEventPlannerControl("event_pick_focus", &rect));
  capture(editor,
          reopen ? "planner-reopened-closed.ppm" : "planner-closed.ppm");
  unsigned long long rev = SceneEditorDocumentRevision();
  planner_click(editor, "events");
  assert(SceneEditorEventPlannerVisible());
  assert(SceneEditorDocumentRevision() == rev);
  planner_click(editor, "event_entry");
  if (!reopen) {
    json_object *read = SceneEditorEventPlannerRead();
    assert(json_object_get_uint64(pg(read, "viewport_tick")) == 150);
    unsigned long long token =
        json_object_get_uint64(pg(read, "viewport_token"));
    json_object_put(read);
    assert(token > 0);
    assert(SceneEditorGetPaneLayout(&layout));
    int px = -1, py = -1, qx = -1, qy = -1;
    json_object *hit = NULL, *other = NULL;
    for (int y = layout.viewport_rect.y + 8;
         y < layout.viewport_rect.y + layout.viewport_rect.h - 8; y += 6) {
      for (int x = layout.viewport_rect.x + 8;
           x < layout.viewport_rect.x + layout.viewport_rect.w - 8; x += 6) {
        json_object *h = planner_pick(x, y, token);
        if (h) {
          const char *id =
              json_object_get_string(pa(pg(pg(h, "feature"), "targets"), 0));
          if (!strcmp(id, "copper_bracket")) {
            if (!hit) {
              hit = h;
              px = x;
              py = y;
            } else if (abs(x - px) > 12 || abs(y - py) > 12) {
              other = h;
              qx = x;
              qy = y;
              break;
            } else
              json_object_put(h);
          } else
            json_object_put(h);
        }
      }
      if (other)
        break;
    }
    assert(hit && other);
    double displayed_depth;
    int displayed = SceneEditorMeshPreviewPickObjectHit(
        &event_planner.projector, editor->currentMode, -1, px, py,
        &displayed_depth);
    char displayed_id[64];
    assert(runtime_scene_bridge_get_last_object_id_for_scene_index(
        displayed, displayed_id, sizeof(displayed_id)));
    assert(!strcmp(displayed_id, "copper_bracket"));
    json_object *surface = pg(hit, "world");
    double wx = json_object_get_double(pa(surface, 0)),
           wy = json_object_get_double(pa(surface, 1)),
           wz = json_object_get_double(pa(surface, 2));
    assert(fabs(SceneEditorDigestOverlayViewDepth(&event_planner.projector, wx,
                                                  wy, wz) -
                displayed_depth) < .05);

    assert(!planner_pick(0, 0, token));
    planner_click(editor, "event_pick_focus");
    assert(SceneEditorEventPlannerInteraction());
    click(editor, (SDL_Rect){px, py, 1, 1});
    assert(!SceneEditorEventPlannerInteraction());
    read = SceneEditorEventPlannerRead();
    assert(!strcmp(ps(pg(pg(read, "intent"), "focus"), "kind"), "body_point"));
    assert(!strcmp(json_object_to_json_string_ext(
                       pg(pg(read, "intent"), "focus"), JSON_C_TO_STRING_PLAIN),
                   json_object_to_json_string_ext(pg(hit, "feature"),
                                                  JSON_C_TO_STRING_PLAIN)));
    json_object_put(read);
    planner_click(editor, "event_pick_pivot");
    click(editor, (SDL_Rect){qx, qy, 1, 1});
    assert(!SceneEditorEventPlannerInteraction());
    assert(
        strcmp(json_object_to_json_string(pg(event_planner.intent, "focus")),
               json_object_to_json_string(pg(event_planner.intent, "pivot"))));
    if (getenv("OPTIC_TEST_DOLLY")) {
      planner_click(editor, "event_lead_kind");
      assert(!strcmp(ps(pg(event_planner.intent, "lead"), "kind"), "dolly"));
      assert(pn(pg(event_planner.intent, "lead"), "distance_scale") == 1.5);
      planner_click(editor, "event_lead_scale");
      key(editor, SDLK_ESCAPE);
      assert(!SceneEditorEventPlannerInteraction());
    }
    capture(editor, "planner-feature.ppm");
    read = SceneEditorEventPlannerRead();
    token = json_object_get_uint64(pg(read, "viewport_token"));
    json_object_put(read);
    assert(SceneEditorTimelineSeek(28));
    SceneEditorSessionRuntimeRender(editor);
    assert(!planner_pick(px, py, token));
    planner_click(editor, "event_entry");
    planner_click(editor, "event_pick_focus");
    key(editor, SDLK_ESCAPE);
    assert(!SceneEditorEventPlannerInteraction());
    planner_click(editor, "event_view_page");
    assert(!SceneEditorEventPlannerControl("event_pick_focus", &rect));
    planner_click(editor, "event_fov");
    assert(SceneEditorEventPlannerInteraction());
    key(editor, SDLK_ESCAPE);
    assert(!SceneEditorEventPlannerInteraction());
    assert(pn(pg(event_planner.intent, "framing"), "fov_y") == 55);
    planner_click(editor, "event_fov");
    for (int i = 0; i < 24; i++)
      key(editor, SDLK_BACKSPACE);
    SDL_Event text = {.type = SDL_TEXTINPUT};
    snprintf(text.text.text, sizeof(text.text.text), "60");
    SceneEditorSessionRuntimeHandleEvent(editor, &text);
    key(editor, SDLK_RETURN);
    assert(!SceneEditorEventPlannerInteraction());
    assert(pn(pg(event_planner.intent, "framing"), "fov_y") == 60);
    capture(editor, "planner-view.ppm");
    assert(SceneEditorDocumentRevision() == rev);
    /* Real stdio MCP client loads the same transient draft and queries the hit.
     */
    json_object *request = pc(event_planner.intent);
    json_object_to_file_ext("planner-intent.json", request,
                            JSON_C_TO_STRING_PRETTY);
    json_object_put(request);
    json_object *xy = json_object_new_object();
    pi(xy, "x", px);
    pi(xy, "y", py);
    json_object_to_file("planner-pick.json", xy);
    json_object_put(xy);
    char *script = getenv("OPTIC_PLANNER_MCP_TEST");
    assert(script);
    char *args[] = {"python3", "-B", script, (char *)scene, NULL};
    pid_t pid;
    assert(posix_spawnp(&pid, "python3", NULL, NULL, args, environ) == 0);
    int status = 0;
    bool done = false;
    for (int i = 0; i < 2000; i++) {
      SceneEditorAgentControlPoll();
      SDL_PumpEvents();
      SceneEditorSessionRuntimeRender(editor);
      if (waitpid(pid, &status, WNOHANG) == pid) {
        done = true;
        break;
      }
      SDL_Delay(10);
    }
    if (!done) {
      kill(pid, SIGTERM);
      waitpid(pid, &status, 0);
    }
    assert(done && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    planner_click(editor, "event_review_page");
    planner_click(editor, "event_generate");
    json_object *candidates = pg(event_planner.proposals, "candidates");
    assert(plen(candidates) == 3);
    int best = -1;
    for (int i = 0; i < 3; i++)
      if (json_object_get_boolean(pg(pa(candidates, i), "usable")))
        best = i;
    assert(best >= 0);
    for (int i = 0; i < best; i++)
      planner_click(editor, "event_candidate");
    capture(editor, "planner-review.ppm");
    planner_click(editor, "event_apply");
    assert(SceneEditorDocumentRevision() > rev);
    json_object *saved = SceneEditorSequenceRead();
    assert(plen(pg(saved, "event_cameras")) == 1);
    json_object_put(saved);
    assert(SceneEditorDocumentUndo(message, sizeof(message)));
    saved = SceneEditorSequenceRead();
    assert(!plen(pg(saved, "event_cameras")));
    json_object_put(saved);
    assert(SceneEditorDocumentRedo(message, sizeof(message)));
    assert(SceneEditorDocumentSave(message, sizeof(message)));
    json_object_put(hit);
    json_object_put(other);
  } else {
    json_object *read = SceneEditorSequenceRead();
    json_object *intent = pg(pa(pg(read, "event_cameras"), 0), "intent");
    assert(!strcmp(ps(pg(intent, "focus"), "kind"), "body_point"));
    assert(!strcmp(ps(pg(intent, "pivot"), "kind"), "body_point"));
    assert(pn(pg(intent, "framing"), "fov_y") == 60);
    if (getenv("OPTIC_TEST_DOLLY")) {
      assert(!strcmp(ps(pg(intent, "lead"), "kind"), "dolly"));
      assert(pn(pg(intent, "lead"), "distance_scale") == 1.5);
    }
    json_object_put(read);
  }
  planner_click(editor, "event_target_page");
  planner_click(editor, "event_reload");
  planner_click(editor, "event_middle");
  RayEvaluatedSceneSnapshot mid;
  assert(SceneEditorTimelineCopyEvaluated(&mid));
  assert(mid.sequence.source_tick == 150);
  assert(mid.camera.inspection_id[0]);
  planner_click(editor, "event_resume");
  RayEvaluatedSceneSnapshot resume;
  assert(SceneEditorTimelineCopyEvaluated(&resume));
  assert(resume.sequence.source_tick == 150);
  capture(editor, reopen ? "planner-reopened.ppm" : "planner-resume.ppm");
  /* Compact layout: both pages' controls stay reachable and separate. */
  SDL_SetWindowSize(editor->window, 1024, 720);
  for (int i = 0; i < 4; i++) {
    SDL_Delay(30);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(editor);
  }
  planner_click(editor, "event_target_page");
  assert(SceneEditorGetPaneLayout(&layout));
  assert(SceneEditorEventPlannerControl("event_pick_focus", &rect));
  assert(rect.y >= layout.left_content_rect.y &&
         rect.y + rect.h <=
             layout.left_content_rect.y + layout.left_content_rect.h);
  assert(SceneEditorEventPlannerControl("event_radius", &rect));
  assert(SceneEditorEventPlannerControl("event_lead_kind", &rect));
  if (getenv("OPTIC_TEST_DOLLY")) {
    assert(SceneEditorEventPlannerControl("event_lead_scale", &rect));
    assert(rect.y >= layout.left_content_rect.y &&
           rect.y + rect.h <=
               layout.left_content_rect.y + layout.left_content_rect.h);
  }
  assert(SceneEditorEventPlannerControl("event_resume", &rect));
  planner_click(editor, "event_view_page");
  assert(SceneEditorEventPlannerControl("event_resume_aim", &rect));
  assert(SceneEditorEventPlannerControl("event_resume_position", &rect));
  capture(editor, reopen ? "planner-view-compact-reopen.ppm"
                         : "planner-view-compact.ppm");
  planner_click(editor, "event_target_page");
  capture(editor,
          reopen ? "planner-compact-reopen.ppm" : "planner-compact.ppm");
  planner_click(editor, "event_pick_focus");
  assert(SceneEditorEventPlannerInteraction());
  planner_click(editor, "camera");
  assert(!SceneEditorEventPlannerVisible() &&
         !SceneEditorEventPlannerInteraction());
  assert(!SceneEditorEventPlannerControl("event_apply", &rect));
  capture(editor, reopen ? "planner-exit-reopen.ppm" : "planner-exit.ppm");
  assert(!SceneEditorDocumentIsDirty());
  fprintf(stderr,
          "Event planner %s: real "
          "tab/picking/draft/clocks/isolation/history/save PASS\n",
          reopen ? "fresh reopen" : "native + MCP");
}
