/* Native fixture driver: same authoring/materialization and evaluated renderer
 * owners as MCP/Preview. It never steps a numerical simulation. */
#include "../src/import/event_camera_internal.h"
#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_event_camera.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_native_3d_render.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "../src/import/runtime_rigid_result_internal.h"
#endif
#include <stdlib.h>
static bool save(const char *path, json_object *o) {
  FILE *f = fopen(path, "wx");
  if (!f)
    return false;
  fputs(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PRETTY), f);
  fputc('\n', f);
  return fclose(f) == 0;
}
int main(int argc, char **argv) {
  if (argc < 3)
    return 2;
  LoadAllSettings();
  RuntimeSceneBridgePreflight preflight;
  if (!runtime_scene_bridge_apply_file(argv[1], &preflight)) {
    fprintf(stderr, "admission: %s\n", preflight.diagnostics);
    return 3;
  }
  json_object *scene = json_object_from_file(argv[1]);
  char diagnostic[512] = {0};
  if (!strcmp(argv[2], "--propose")) {
    if (argc != 5)
      return 4;
    json_object *intent = json_object_from_file(argv[3]),
                *p = RuntimeEventCameraPropose(scene, intent, diagnostic,
                                               sizeof(diagnostic));
    if (!p) {
      fprintf(stderr, "%s\n", diagnostic);
      return 5;
    }
    bool ok = save(argv[4], p);
    json_object_put(p);
    json_object_put(intent);
    json_object_put(scene);
    return ok ? 0 : 6;
  }
  if (!strcmp(argv[2], "--select")) {
    if (argc != 7)
      return 7;
    json_object *intent = json_object_from_file(argv[3]);
    int index = atoi(argv[4]);
    json_object *entry = RuntimeEventCameraSelect(
        scene, intent, index, argv[5], diagnostic, sizeof(diagnostic));
    if (!entry || !RuntimeEventCameraAttach(scene, entry, diagnostic,
                                            sizeof(diagnostic))) {
      fprintf(stderr, "select: %s\n", diagnostic);
      return 8;
    }
    bool ok = save(argv[6], scene);
    json_object_put(entry);
    json_object_put(intent);
    json_object_put(scene);
    return ok ? 0 : 9;
  }
  if (!strcmp(argv[2], "--replacement")) {
    if (argc != 4)
      return 10;
    RayEvaluatedSceneServiceResult a, b;
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){27, 0, 1}, &a))
      return 11;
    if (runtime_scene_bridge_apply_file(argv[3], &preflight))
      return 12;
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){27, 0, 1}, &b) ||
        memcmp(&a.snapshot.camera, &b.snapshot.camera,
               sizeof(a.snapshot.camera)))
      return 13;
    puts("EVENT rejected replacement preserved evaluated scene");
    return 0;
  }
  int first = 2;
  const char *view = NULL;
  if (!strcmp(argv[2], "--view")) {
    if (argc < 5)
      return 19;
    view = argv[3];
    first = 4;
  }
  for (int arg = first; arg < argc; arg++) {
    int local = atoi(argv[arg]);
    TimelineRate rate;
    TimelineRange range;
    if (RayEvaluatedSceneResolveTimelineClock(&rate, &range) !=
        TIMELINE_STATUS_OK)
      return 14;
    TimelineSample t = {range.start_frame + local, 0, 1};
    if (getenv("OPTIC_EVENT_HALF_FRAME")) {
      t.subframe_numerator = 1;
      t.subframe_denominator = 2;
    }
    RayEvaluatedSceneServiceResult value;
    if (!(view ? RayEvaluatedSceneCaptureViewSample(t, view, &value)
               : RayEvaluatedSceneCaptureSample(t, &value))) {
      fprintf(stderr, "capture: %s\n", value.status_line);
      return 15;
    }
    if (!getenv("OPTIC_EVENT_EVALUATE_ONLY")) {
      RuntimeNative3DPreparedFrame prepared = {0};
      if (!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
              &prepared, 64, 48, &value.snapshot, NULL)) {
        fprintf(stderr, "prepare: %s\n",
                RuntimeNative3DPrepareFrameLastDiagnostics());
        return 16;
      }
      if (getenv("OPTIC_EVENT_CHECK_PROJECTION")) {
#ifdef RAY_RIGID_RESULT_ENABLED
        json_object *events = eg(ea(scene), "event_cameras");
        for (size_t j = 0; events && j < json_object_array_length(events);
             j++) {
          json_object *event = json_object_array_get_idx(events, j);
          if (strcmp(es(event, "camera_id"), value.snapshot.camera.target_id))
            continue;
          /* A render narrower than the reviewed minimum must fail atomically.
           */
          RayEvaluatedSceneSnapshot untouched = value.snapshot;
          RayEvaluatedSceneSnapshot source_copy = value.snapshot;
          if (RayEvaluatedSceneBindViewport(&value.snapshot, 32, 48,
                                            &untouched) ||
              memcmp(&untouched, &source_copy, sizeof(untouched)) ||
              memcmp(&value.snapshot, &source_copy, sizeof(source_copy)))
            return 41;
          EventRequest request;
          EventFocus focus;
          if (!EventRequestRead(scene, eg(event, "intent"), &request, NULL, 0))
            return 30;
          uint64_t tick = request.tick;
          if ((uint64_t)local >= request.start + request.count &&
              request.track) {
            TimelineEvaluationContext c = value.snapshot.frame;
            if ((uint64_t)local >= request.end &&
                TimelineEvaluationContextBuild(
                    c.rate, c.range,
                    (TimelineSample){
                        c.range.start_frame + (int64_t)request.end - 1, 0, 1},
                    &c) != TIMELINE_STATUS_OK)
              return 31;
            SequenceEvaluation e;
            bool present = false;
            if (RuntimeRigidResultSequence(&c, &e, &present) !=
                    TIMELINE_STATUS_OK ||
                !present)
              return 32;
            tick = e.source_tick;
          }
          if (!EventFocusResolve(RuntimeRigidResultCurrent(), tick,
                                 request.focus, &focus))
            return 33;
          double x, y, z;
          bool inside;
          if (!RuntimeCameraProjector3D_ProjectPoint(
                  &prepared.projector,
                  vec3(focus.point.x, focus.point.y, focus.point.z), &x, &y, &z,
                  &inside) ||
              !inside)
            return 34;
          if (fabs(x / 64 * 2 - 1 - request.framing.offset[0]) > 1e-9 ||
              fabs(1 - y / 48 * 2 - request.framing.offset[1]) > 1e-9)
            return 35;
          if (fabs(prepared.projector.tanHalfFovY -
                   tan(request.framing.fov_y * acos(-1.) / 360)) > 1e-12)
            return 36;
          if (fabs(prepared.evaluatedScene.camera.aspect_ratio - 64. / 48) >
              1e-12)
            return 37;
          Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(
              &prepared.projector, x - .5, y - .5);
          Vec3 target = vec3_normalize(vec3_sub(
              vec3(focus.point.x, focus.point.y, focus.point.z), ray.origin));
          if (vec3_dot(target, ray.direction) < 1 - 1e-12)
            return 38;
        }
#endif
      }
      RuntimeNative3DPreparedFrame_Free(&prepared);
      RayEvaluatedSceneSnapshot forged = value.snapshot;
      forged.camera.position.x += .001;
      if (RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
              &prepared, 64, 48, &forged, NULL))
        return 17;
      forged = value.snapshot;
      forged.camera.inspection_time.numerator++;
      if (RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
              &prepared, 64, 48, &forged, NULL))
        return 18;
    }
    RayEvaluatedCamera *c = &value.snapshot.camera;
    json_object *row = json_object_new_object();
    json_object_object_add(row, "frame", json_object_new_int(local));
    json_object_object_add(
        row, "tick",
        json_object_new_int64(value.snapshot.sequence.source_tick));
    json_object_object_add(row, "position", evec(c->position));
    json_object_object_add(row, "yaw", json_object_new_double(c->yaw_radians));
    json_object_object_add(row, "pitch",
                           json_object_new_double(c->pitch_radians));
    json_object_object_add(row, "fov_y",
                           json_object_new_double(c->fov_y_degrees));
    json_object_object_add(row, "aspect",
                           json_object_new_double(c->aspect_ratio));
    json_object_object_add(row, "forward",
                           earr(c->orientation_frame.forward, 3));
    json_object_object_add(row, "right", earr(c->orientation_frame.right, 3));
    json_object_object_add(row, "up", earr(c->orientation_frame.up, 3));
    json_object_object_add(row, "inspection_id",
                           json_object_new_string(c->inspection_id));
    json_object *bodies = json_object_new_array();
    for (size_t i = 0; i < value.snapshot.object_transform_count; i++) {
      RayEvaluatedObjectTransform *b = &value.snapshot.object_transforms[i];
      json_object *body = json_object_new_object();
      json_object_object_add(body, "id", json_object_new_string(b->target_id));
      json_object_object_add(body, "position", evec(b->position));
      double q[4] = {b->orientation_quaternion.w, b->orientation_quaternion.x,
                     b->orientation_quaternion.y, b->orientation_quaternion.z};
      json_object_object_add(body, "q", earr(q, 4));
      json_object_array_add(bodies, body);
    }
    json_object_object_add(row, "bodies", bodies);
    printf("EVENT %s\n",
           json_object_to_json_string_ext(row, JSON_C_TO_STRING_PLAIN));
    json_object_put(row);
  }
  json_object_put(scene);
  return 0;
}
