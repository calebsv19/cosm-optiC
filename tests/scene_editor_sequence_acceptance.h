#include "app/evaluated_scene_service.h"
#include "editor/scene_editor_sequence.h"
#include "editor/scene_editor_sequence_panel.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_inspection.h"
static json_object *sq_get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static json_object *sq_command(const char *action, json_object *args, bool ok) {
  json_object *r = json_object_new_object();
  char method[64];
  snprintf(method, 64, "sequence.%s", action);
  json_object_object_add(r, "method", json_object_new_string(method));
  json_object_object_add(args, "expected_revision",
                         json_object_new_int64(SceneEditorDocumentRevision()));
  json_object_object_add(r, "params", args);
  json_object *response = SceneEditorAgentControlDispatch(r);
  if (json_object_get_boolean(sq_get(response, "ok")) != ok)
    fprintf(stderr, "Sequence %s: %s\n", action,
            json_object_to_json_string(response));
  assert(json_object_get_boolean(sq_get(response, "ok")) == ok);
  json_object_put(r);
  return response;
}
static void sq_ui(SceneEditor *editor, const char *name) {
  SDL_Rect control;
  SceneEditorPaneLayout layout;
  assert(SceneEditorGetPaneLayout(&layout));
  SDL_Event scroll = {0};
  scroll.type = SDL_MOUSEWHEEL;
  scroll.wheel.mouseX = layout.left_content_rect.x + 20;
  scroll.wheel.mouseY = layout.left_content_rect.y + 70;
  scroll.wheel.y = 100;
  SceneEditorSessionRuntimeHandleEvent(editor, &scroll);
  SceneEditorSessionRuntimeRender(editor);
  for (int i = 0; i < 60 && !SceneEditorRenderAuthoringControl(name, &control);
       i++) {
    scroll.wheel.y = -1;
    SceneEditorSessionRuntimeHandleEvent(editor, &scroll);
    SceneEditorSessionRuntimeRender(editor);
  }
  assert(SceneEditorRenderAuthoringControl(name, &control));
  click(editor, control);
}
static void sequence_acceptance(SceneEditor *editor, const char *scene,
                                bool reopen) {
  char message[512];
  assert(SceneEditorTimelineActivate());
  SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_RENDER);
  SceneEditorRenderAuthoringSelect(editor, true);
  SceneEditorRenderAuthoringSetTiming(true);
  SceneEditorSessionRuntimeRender(editor);
  if (reopen) {
    json_object *read = SceneEditorSequenceRead();
    assert(json_object_array_length(sq_get(read, "inspection_moves")) == 1);
    json_object_put(read);
    assert(SceneEditorTimelineSeek(40));
    RayEvaluatedSceneSnapshot s;
    assert(SceneEditorTimelineCopyEvaluated(&s) && s.camera.inspection_id[0]);
    capture(editor, "sequence-reopened.ppm");
    fprintf(stderr, "Sequence fresh reopen PASS\n");
    return;
  }
  sq_ui(editor, "sequence_open");
  sq_ui(editor, "sequence_camera_duplicate");
  assert(SceneEditorDocumentIsDirty());
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  assert(SceneEditorDocumentRedo(message, sizeof(message)));
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  sq_ui(editor, "sequence_camera_speed");
  assert(SceneEditorSequencePanelActive());
  json_object *refused =
      json_tokener_parse("{\"camera_id\":\"camera/A\",\"clock\":{}}");
  json_object *result = sq_command("clock_set", refused, false);
  json_object_put(result);
  authoring_text(editor, "1.25");
  assert(!SceneEditorSequencePanelActive());
  json_object *read = SceneEditorSequenceRead(),
              *rig = sq_get(read, "camera_rig"),
              *c = json_object_array_get_idx(sq_get(rig, "cameras"), 0);
  assert(fabs((double)json_object_get_int64(
                  sq_get(sq_get(sq_get(c, "clock"), "speed"), "numerator")) /
                  json_object_get_int64(sq_get(
                      sq_get(sq_get(c, "clock"), "speed"), "denominator")) -
              1.25) < 1e-12);
  json_object_put(read);
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  sq_ui(editor, "sequence_camera_x");
  authoring_text(editor, "2.5");
  RayEvaluatedSceneServiceResult moved;
  assert(RayEvaluatedSceneCaptureSample((TimelineSample){12, 0, 1}, &moved) &&
         moved.snapshot.camera.position.x == 2.5);
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  result = sq_command("segment_boundary",
                      json_tokener_parse("{\"id\":\"play\",\"end_frame\":14}"),
                      true);
  json_object_put(result);
  assert(RayEvaluatedSceneCaptureSample((TimelineSample){50, 0, 1}, &moved) &&
         moved.snapshot.sequence.source_tick == 140 &&
         moved.snapshot.frame.range.frame_count == 102);
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  /* Source split preserves mapping; toggling it is a single checked history
   * edit. */
  result =
      sq_command("segment_split", json_tokener_parse("{\"frame\":8}"), true);
  json_object_put(result);
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  unsigned long long rev = SceneEditorDocumentRevision();
  result =
      sq_command("inspect",
                 json_tokener_parse(
                     "{\"id\":\"too_fast\",\"camera_id\":\"camera/"
                     "A\",\"target_id\":\"spill_bracket\",\"start_frame\":15,"
                     "\"frame_count\":72,\"angle\":6.283185307179586,\"max_"
                     "speed\":0.001,\"acceleration\":0.001,\"braking\":0.001}"),
                 false);
  json_object_put(result);
  assert(SceneEditorDocumentRevision() == rev);
  /* The fixture's actual stable body identity is supplied by its native
   * snapshot. */
  RayEvaluatedSceneServiceResult base;
  assert(RayEvaluatedSceneCaptureSample((TimelineSample){27, 0, 1}, &base));
  assert(base.snapshot.object_transform_count > 0);
  const char *target = base.snapshot.object_transforms[0].target_id;
  json_object *args = json_tokener_parse(
      "{\"id\":\"contact_orbit\",\"camera_id\":\"camera/"
      "A\",\"start_frame\":15,\"frame_count\":72,\"angle\":6.283185307179586,"
      "\"max_speed\":30,\"acceleration\":100,\"braking\":100}");
  json_object_object_add(args, "target_id", json_object_new_string(target));
  result = sq_command("inspect", args, true);
  json_object_put(result);
  read = SceneEditorSequenceRead();
  assert(json_object_array_length(sq_get(read, "inspection_moves")) == 1);
  json_object_put(read);
  assert(SceneEditorDocumentUndo(message, sizeof(message)));
  assert(SceneEditorDocumentRedo(message, sizeof(message)));
  int frames[] = {15, 36, 65, 86, 15, 86, 36};
  RayEvaluatedSceneServiceResult first, last;
  for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
    RayEvaluatedSceneServiceResult value;
    assert(RayEvaluatedSceneCaptureSample(
        (TimelineSample){12 + frames[i], 0, 1}, &value));
    assert(value.snapshot.sequence.source_tick == 150 &&
           value.snapshot.camera.inspection_id[0]);
    if (!i)
      first = value;
    if (i == 3)
      last = value;
  }
  assert(!memcmp(&first.snapshot.camera.position,
                 &last.snapshot.camera.position, sizeof(TimelineVec3)));
  /* Locks are immutable even before Save; a candidate cannot mutate its own
   * lock. */
  read = SceneEditorSequenceRead();
  rig = sq_get(read, "camera_rig");
  c = json_object_array_get_idx(sq_get(rig, "cameras"), 0);
  json_object *clock = NULL;
  assert(json_object_deep_copy(sq_get(c, "clock"), &clock, NULL) == 0);
  json_object_object_add(
      clock, "offset",
      json_tokener_parse("{\"numerator\":1,\"denominator\":1}"));
  args = json_object_new_object();
  json_object_object_add(args, "camera_id", json_object_new_string("camera/A"));
  json_object_object_add(args, "clock", clock);
  result = sq_command("clock_set", args, false);
  json_object_put(result);
  json_object_put(read);
  assert(SceneEditorTimelineSeek(50));
  SceneEditorSessionRuntimeRender(editor);
  sq_ui(editor, "sequence_open");
  sq_ui(editor, "sequence_open");
  capture(editor, "sequence-clocks-and-cameras.ppm");
  assert(SceneEditorDocumentSave(message, sizeof(message)));
  assert(!SceneEditorDocumentIsDirty());
  /* Start a real stdio MCP client while the editor pumps its existing endpoint.
   */
  char *script = getenv("OPTIC_SEQUENCE_MCP_TEST");
  if (script) {
    char *args2[] = {"python3", "-B", script, (char *)scene, NULL};
    pid_t pid;
    assert(posix_spawnp(&pid, "python3", NULL, NULL, args2, environ) == 0);
    int status = 0;
    bool done = false;
    for (int i = 0; i < 3000; i++) {
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
  }
  fprintf(stderr, "Sequence retained commands/UI/history/inspection "
                  "locks/stdio MCP PASS\n");
}
