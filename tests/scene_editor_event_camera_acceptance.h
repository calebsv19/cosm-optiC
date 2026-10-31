static void event_camera_acceptance(SceneEditor *editor, const char *scene,
                                    bool reopen) {
  char message[512];
  assert(SceneEditorTimelineActivate());
  SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_RENDER);
  SceneEditorRenderAuthoringSelect(editor, true);
  SceneEditorRenderAuthoringSetTiming(true);
  SceneEditorSessionRuntimeRender(editor);
  if (!reopen) {
    char *script = getenv("OPTIC_EVENT_MCP_TEST");
    assert(script);
    char *args[] = {"python3", "-B", script, (char *)scene, NULL};
    pid_t pid;
    assert(posix_spawnp(&pid, "python3", NULL, NULL, args, environ) == 0);
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
  json_object *read = SceneEditorSequenceRead(),
              *list = sq_get(read, "event_cameras");
  assert(json_object_array_length(list) == 1);
  json_object *entry = json_object_array_get_idx(list, 0),
              *resolved = sq_get(entry, "resolved"),
              *focus = sq_get(resolved, "focus");
  double p[3];
  for (int i = 0; i < 3; i++)
    p[i] = json_object_get_double(json_object_array_get_idx(focus, i));
  json_object_put(read);
  RayEvaluatedSceneSnapshot first, last;
  assert(SceneEditorTimelineSeek(27));
  assert(SceneEditorTimelineCopyEvaluated(&first));
  assert(!strncmp(first.camera.inspection_id, "event_", 6));
  assert(SceneEditorTimelineSeek(60));
  assert(SceneEditorTimelineCopyEvaluated(&last));
  assert(first.sequence.source_tick == last.sequence.source_tick);
  double x, y, z;
  assert(EventCameraProject(&last.camera, (TimelineVec3){p[0], p[1], p[2]}, &x,
                            &y, &z));
  assert(fabs(x) < 1e-9 && fabs(y) < 1e-9);
  assert(SceneEditorTimelineSeek(40));
  SceneEditorSessionRuntimeRender(editor);
  capture(editor,
          reopen ? "event-camera-reopened.ppm" : "event-camera-owner.ppm");
  assert(!SceneEditorDocumentIsDirty());
  fprintf(stderr,
          "Event camera %s: retained owner, exact target framing and saved "
          "state PASS\n",
          reopen ? "fresh reopen" : "MCP/history");
}
