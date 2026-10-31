#include "event_camera_internal.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_result_internal.h"
#endif
#include <stdlib.h>
bool EventRequestRead(json_object *scene, json_object *intent,
                      EventRequest *out, char *d, size_t n) {
  EventRequest r = {0};
  SequenceClock seq;
  json_object *h = eg(intent, "hold"), *f = eg(intent, "framing"),
              *orbit = eg(intent, "orbit"), *lead = eg(intent, "lead"),
              *resume = eg(intent, "resume");
  if (!eo(intent, 9) ||
      strcmp(es(intent, "schema"), "ray_event_camera_intent_v1") ||
      !etoken(es(intent, "id"), 32) ||
      !SequenceClockFromJson(eg(ea(scene), "sequence"), &seq) ||
      !eg(ea(scene), "camera_rig") ||
      !eg(eg(eg(scene, "extensions"), "ray_tracing"), "rigid_result") ||
      eg(ea(scene), "sequence_volume"))
    return ef(
        d, n,
        "requires a strict intent, saved rig and authenticated rigid sequence");
  if (!eo(h, 3) || !eu(h, "start_frame", &r.start) ||
      !eu(h, "frame_count", &r.count) || r.count < 2 ||
      !eu(h, "source_tick", &r.tick) || !EventLeadRead(lead, &r, d, n) ||
      !eo(resume, 3) || !eu(resume, "end_frame", &r.end) || r.lead > r.start ||
      r.count > seq.range.frame_count ||
      r.start > seq.range.frame_count - r.count || r.end < r.start + r.count ||
      r.end > seq.range.frame_count)
    return ef(d, n, "invalid lead/Hold/resume interval");
  if (r.dolly) {
    if (r.lead >= r.start)
      return ef(d, n,
                "Dolly requires a positive lead interval before the Hold.");
    r.approach.duration = (double)(r.start - r.lead) *
                          seq.rate.frames_per_second_denominator /
                          seq.rate.frames_per_second_numerator;
  }
  bool held = false;
  for (size_t i = 0; i < seq.segment_count; i++) {
    SequenceSegment *s = &seq.segments[i];
    SequenceEvaluation e;
    if (s->kind == SEQUENCE_HOLD && s->start_frame == r.start &&
        s->frame_count == r.count &&
        SequenceClockEvaluate(
            &seq,
            (TimelineSample){seq.range.start_frame + (int64_t)r.start, 0, 1},
            &e) == SEQUENCE_OK &&
        e.source_tick == r.tick)
      held = true;
  }
  if (!held)
    return ef(d, n, "event must name one exact recorded Hold and source tick");
  if (!eo(f, 6) || !en(eg(f, "fov_y"), &r.framing.fov_y) ||
      !en(eg(f, "aspect"), &r.framing.aspect) ||
      !en(eg(f, "margin"), &r.framing.margin) ||
      !ev(eg(f, "offset"), r.framing.offset, 2) ||
      !en(eg(f, "azimuth"), &r.framing.azimuth) ||
      !en(eg(f, "elevation"), &r.framing.elevation) ||
      !EventCameraFramingValid(&r.framing) || !eo(orbit, 4) ||
      !en(eg(orbit, "angle"), &r.limits.angle) ||
      !en(eg(orbit, "max_speed"), &r.limits.max_speed) ||
      !en(eg(orbit, "acceleration"), &r.limits.acceleration) ||
      !en(eg(orbit, "braking"), &r.limits.braking) ||
      fabs(r.limits.angle) < 1e-9 || fabs(r.limits.angle) > 8 * acos(-1.) ||
      r.limits.max_speed <= 0 || r.limits.acceleration <= 0 ||
      r.limits.braking <= 0)
    return ef(d, n, "invalid framing or orbit limits");
  const char *position = es(resume, "position"), *aim = es(resume, "aim");
  if ((strcmp(position, "endpoint") && strcmp(position, "entry")) ||
      (strcmp(aim, "event") && strcmp(aim, "track")))
    return ef(d, n, "unsupported resume policy");
  r.entry = !strcmp(position, "entry");
  r.track = !strcmp(aim, "track");
  if (r.entry && fabs(remainder(r.limits.angle, 2 * acos(-1.))) > 1e-12)
    return ef(d, n,
              "entry resume requires a closed orbit; a partial return needs an "
              "explicit future transition/cut");
  r.limits.duration = (double)(r.count - 1) *
                      seq.rate.frames_per_second_denominator /
                      seq.rate.frames_per_second_numerator;
  r.focus = eg(intent, "focus");
  r.pivot = eg(intent, "pivot");
  snprintf(r.id, sizeof(r.id), "%s", es(intent, "id"));
  snprintf(r.camera_id, sizeof(r.camera_id), "camera/event_%s", r.id);
  *out = r;
  return true;
}
static json_object *base_dependencies(json_object *scene) {
  json_object *o = json_object_new_object();
  json_object_object_add(o, "sequence", ec(eg(ea(scene), "sequence")));
  json_object_object_add(o, "world_scale", ec(eg(scene, "world_scale")));
  json_object_object_add(
      o, "rigid_result",
      ec(eg(eg(eg(scene, "extensions"), "ray_tracing"), "rigid_result")));
  return o;
}
json_object *RuntimeEventCameraDependencies(json_object *scene,
                                            json_object *entry) {
  json_object *o = base_dependencies(scene),
              *list = eg(eg(ea(scene), "camera_rig"), "cameras");
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *c = json_object_array_get_idx(list, i);
    if (!strcmp(es(c, "id"), es(entry, "camera_id")))
      json_object_object_add(o, "camera", ec(c));
  }
  json_object *c = EventCanonical(o);
  json_object_put(o);
  return c;
}
bool EventResolve(json_object *scene, const RuntimeRigidResult *result,
                  json_object *intent, int index, EventRequest *request,
                  InspectionOrbit *solved, EventFocus *focus, EventFocus *pivot,
                  json_object **resolved, char *d, size_t n) {
  EventRequest r;
  EventFocus target, p;
  InspectionOrbit orbit;
  if (index < 0 || index > 2 || !EventRequestRead(scene, intent, &r, d, n) ||
      !EventFocusResolve(result, r.tick, r.focus, &target) ||
      !EventFocusResolve(result, r.tick, r.pivot, &p))
    return ef(
        d, n,
        "unresolved recorded focus/pivot, target point, pair mass or tick");
  EventCameraFraming f = r.framing;
  f.azimuth += (index == 1 ? 1 : index == 2 ? -1 : 0) * acos(-1.) / 4;
  if (index == 2)
    f.elevation = fmin(1.3, f.elevation + .25);
  if (!EventCameraSolve(&f, target.point, p.point, target.low, target.high,
                        &r.limits, &orbit))
    return ef(
        d, n,
        "event bounds cannot be framed with the requested orbit/time/limits");
  EventCameraLead approach;
  int lead_occluded = 0;
  bool lead_entry_occluded = false;
  if (r.dolly) {
    if (!EventLeadBuild(&r, &orbit, &target, &approach))
      return ef(d, n,
                "Dolly cannot reach the event pose within the requested "
                "interval/limits.");
    lead_entry_occluded =
        EventFocusOccluded(result, r.tick, &target, approach.start.position);
    for (int i = 0; i <= 64; i++) {
      RayEvaluatedCamera c;
      if (!EventCameraLeadSample(&approach, approach.duration * i / 64, &c) ||
          !EventCameraAim(&c, target.point, f.offset) ||
          !EventCameraContains(&c, target.low, target.high, f.margin))
        return ef(d, n, "Dolly cannot retain event framing.");
    }
    for (int i = 0; i <= 16; i++) {
      RayEvaluatedCamera c;
      if (!EventCameraLeadSample(&approach, approach.duration * i / 16, &c))
        return false;
      lead_occluded += EventFocusOccluded(result, r.tick, &target, c.position);
    }
  }
  int occluded = 0;
  bool entry_occluded =
      EventFocusOccluded(result, r.tick, &target, orbit.anchor.position);
  for (int i = 0; i <= 16; i++) {
    RayEvaluatedCamera c;
    if (!InspectionOrbitSample(&orbit, orbit.duration * i / 16, &c))
      return false;
    occluded += EventFocusOccluded(result, r.tick, &target, c.position);
  }
  json_object *o = json_object_new_object();
  json_object_object_add(o, "index", json_object_new_int(index));
  json_object_object_add(
      o, "usable",
      json_object_new_boolean(!entry_occluded && !lead_entry_occluded));
  json_object_object_add(o, "possible_occlusion_samples",
                         json_object_new_int(occluded));
  json_object_object_add(o, "visibility_samples", json_object_new_int(17));
  json_object_object_add(o, "focus", evec(target.point));
  json_object_object_add(o, "pivot", evec(p.point));
  json_object_object_add(o, "bounds_low", evec(target.low));
  json_object_object_add(o, "bounds_high", evec(target.high));
  json_object_object_add(o, "position", evec(orbit.anchor.position));
  json_object_object_add(o, "yaw",
                         json_object_new_double(orbit.anchor.yaw_radians));
  json_object_object_add(o, "pitch",
                         json_object_new_double(orbit.anchor.pitch_radians));
  json_object_object_add(o, "fov_y",
                         json_object_new_double(orbit.anchor.fov_y_degrees));
  if (r.dolly) {
    json_object_object_add(o, "lead_position", evec(approach.start.position));
    json_object_object_add(o, "lead_duration",
                           json_object_new_double(approach.duration));
    json_object_object_add(o, "lead_distance",
                           json_object_new_double(approach.length));
    json_object_object_add(o, "lead_possible_occlusion_samples",
                           json_object_new_int(lead_occluded));
    json_object_object_add(o, "lead_visibility_samples",
                           json_object_new_int(17));
  }
  json_object *hash_input = base_dependencies(scene);
  json_object_object_add(hash_input, "intent", ec(intent));
  json_object_object_add(hash_input, "resolved", ec(o));
  char hash[65];
  bool ok = EventHash(hash_input, hash);
  json_object_put(hash_input);
  if (!ok) {
    json_object_put(o);
    return false;
  }
  json_object_object_add(o, "proposal_sha256", json_object_new_string(hash));
  if (request)
    *request = r;
  if (solved)
    *solved = orbit;
  if (focus)
    *focus = target;
  if (pivot)
    *pivot = p;
  if (resolved)
    *resolved = o;
  else
    json_object_put(o);
  return true;
}
json_object *RuntimeEventCameraPropose(json_object *scene, json_object *intent,
                                       char *d, size_t n) {
#ifdef RAY_RIGID_RESULT_ENABLED
  const RuntimeRigidResult *r = RuntimeRigidResultCurrent();
  if (!r)
    return NULL;
  EventRequest request;
  if (!EventRequestRead(scene, intent, &request, d, n))
    return NULL;
  json_object *o = json_object_new_object(), *list = json_object_new_array();
  json_object_object_add(o, "intent", ec(intent));
  json_object_object_add(o, "candidates", list);
  for (int i = 0; i < 3; i++) {
    json_object *v = NULL;
    char reason[256];
    if (EventResolve(scene, r, intent, i, NULL, NULL, NULL, NULL, &v, reason,
                     sizeof(reason)))
      json_object_array_add(list, v);
    else {
      v = json_object_new_object();
      json_object_object_add(v, "index", json_object_new_int(i));
      json_object_object_add(v, "usable", json_object_new_boolean(false));
      json_object_object_add(v, "reason", json_object_new_string(reason));
      json_object_array_add(list, v);
    }
  }
  return o;
#else
  (void)scene;
  (void)intent;
  ef(d, n, "rigid result support unavailable");
  return NULL;
#endif
}
json_object *RuntimeEventCameraSelect(json_object *scene, json_object *intent,
                                      int index, const char *hash, char *d,
                                      size_t n) {
#ifdef RAY_RIGID_RESULT_ENABLED
  EventRequest r;
  json_object *resolved = NULL;
  if (!hash || !EventResolve(scene, RuntimeRigidResultCurrent(), intent, index,
                             &r, NULL, NULL, NULL, &resolved, d, n))
    return NULL;
  if (strcmp(hash, es(resolved, "proposal_sha256")) ||
      !json_object_get_boolean(eg(resolved, "usable"))) {
    json_object_put(resolved);
    ef(d, n,
       "stale proposal hash or possible entry obstruction; choose a clear "
       "reviewed candidate");
    return NULL;
  }
  json_object *e = json_object_new_object();
  json_object_object_add(e, "schema",
                         json_object_new_string("ray_event_camera_v1"));
  json_object_object_add(e, "id", json_object_new_string(r.id));
  json_object_object_add(e, "camera_id", json_object_new_string(r.camera_id));
  json_object_object_add(e, "intent", ec(intent));
  json_object_object_add(e, "selection", json_object_new_int(index));
  json_object_object_add(e, "resolved", resolved);
  return e;
#else
  (void)scene;
  (void)intent;
  (void)index;
  (void)hash;
  ef(d, n, "rigid result support unavailable");
  return NULL;
#endif
}
