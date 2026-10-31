#include "animation/sequence_time.h"
#include "event_camera_internal.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_inspection.h"
static json_object *ratio(uint64_t n, uint64_t d) {
  json_object *r = json_object_new_object();
  json_object_object_add(r, "numerator", json_object_new_int64(n));
  json_object_object_add(r, "denominator", json_object_new_int64(d));
  return r;
}
static json_object *piece(json_object *old, uint64_t start, uint64_t count,
                          const char *id) {
  json_object *s = ec(old);
  json_object_object_add(s, "id", json_object_new_string(id));
  json_object_object_add(s, "start_frame", json_object_new_int64(start));
  json_object_object_add(s, "frame_count", json_object_new_int64(count));
  return s;
}
static bool attach(json_object *scene, json_object *entry, char *d, size_t n) {
  EventRequest r;
  if (!EventRequestRead(scene, eg(entry, "intent"), &r, d, n))
    return false;
  json_object *a = ea(scene), *rig = eg(a, "camera_rig"),
              *cameras = eg(rig, "cameras"), *events = eg(a, "event_cameras");
  if (!json_object_is_type(cameras, json_type_array) ||
      (events && !json_object_is_type(events, json_type_array)))
    return ef(d, n, "invalid camera/event collection");
  bool replacing = false;
  for (size_t i = 0; events && i < json_object_array_length(events); i++) {
    json_object *old = json_object_array_get_idx(events, i);
    if (!strcmp(es(old, "id"), r.id)) {
      if (strcmp(es(old, "camera_id"), r.camera_id))
        return ef(d, n, "event camera ownership mismatch");
      json_object_array_del_idx(events, i, 1);
      replacing = true;
      break;
    }
  }
  for (size_t i = 0; i < json_object_array_length(cameras); i++) {
    if (!strcmp(es(json_object_array_get_idx(cameras, i), "id"), r.camera_id)) {
      if (!replacing)
        return ef(d, n, "camera ID belongs to another authoring owner");
      json_object_array_del_idx(cameras, i, 1);
      break;
    }
  }
  if (json_object_array_length(cameras) >= 16 ||
      (events && json_object_array_length(events) >= 8))
    return ef(d, n, "camera/event capacity");
  double pos[3], scale = json_object_get_double(eg(scene, "world_scale"));
  json_object *resolved = eg(entry, "resolved");
  if (!isfinite(scale) || scale <= 0 || !ev(eg(resolved, "position"), pos, 3))
    return false;
  for (int k = 0; k < 3; k++)
    pos[k] /= scale;
  json_object *c = json_object_new_object(), *clock = json_object_new_object();
  json_object_object_add(c, "id", json_object_new_string(r.camera_id));
  json_object_object_add(c, "position", earr(pos, 3));
  json_object_object_add(c, "yaw", ec(eg(resolved, "yaw")));
  json_object_object_add(c, "pitch", ec(eg(resolved, "pitch")));
  json_object_object_add(c, "fov_y", json_object_new_double(r.framing.fov_y));
  json_object_object_add(c, "zoom", json_object_new_double(1));
  json_object_object_add(clock, "mode", json_object_new_string("continuous"));
  json_object_object_add(clock, "offset", ratio(0, 1));
  json_object_object_add(clock, "speed", ratio(1, 1));
  json_object_object_add(clock, "duration_frames",
                         ec(eg(eg(rig, "range"), "frame_count")));
  json_object_object_add(clock, "end_behavior", json_object_new_string("hold"));
  json_object_object_add(c, "clock", clock);
  json_object_array_add(cameras, c);
  json_object *old = eg(rig, "shots"), *shots = json_object_new_array();
  bool inserted = false;
  for (size_t i = 0; i < json_object_array_length(old); i++) {
    json_object *s = json_object_array_get_idx(old, i);
    uint64_t begin = json_object_get_uint64(eg(s, "start_frame")),
             end = begin + json_object_get_uint64(eg(s, "frame_count"));
    if (end <= r.lead || begin >= r.end) {
      json_object_array_add(shots, ec(s));
      continue;
    }
    char id[64];
    if (begin < r.lead) {
      snprintf(id, sizeof(id), "%.20s_b_%.24s", es(s, "id"), r.id);
      json_object_array_add(shots, piece(s, begin, r.lead - begin, id));
    }
    if (!inserted) {
      snprintf(id, sizeof(id), "event_%s", r.id);
      json_object *event = piece(s, r.lead, r.end - r.lead, id);
      json_object_object_add(event, "camera_id",
                             json_object_new_string(r.camera_id));
      json_object_object_add(event, "clip_start", ratio(0, 1));
      json_object_array_add(shots, event);
      inserted = true;
    }
    if (end > r.end) {
      snprintf(id, sizeof(id), "%.20s_a_%.24s", es(s, "id"), r.id);
      json_object *tail = piece(s, r.end, end - r.end, id);
      json_object *source = NULL;
      for (size_t j = 0; j < json_object_array_length(cameras); j++) {
        json_object *v = json_object_array_get_idx(cameras, j);
        if (!strcmp(es(v, "id"), es(s, "camera_id")))
          source = v;
      }
      if (!strcmp(es(eg(source, "clock"), "mode"), "clip")) {
        json_object *rate = eg(rig, "rate"), *base = eg(s, "clip_start");
        SequenceTime delta, sum;
        if (SequenceTimeMultiply(
                (SequenceTime){r.end - begin, 1},
                (SequenceTime){json_object_get_uint64(eg(rate, "denominator")),
                               json_object_get_uint64(eg(rate, "numerator"))},
                &delta) != SEQUENCE_OK ||
            SequenceTimeAdd(
                (SequenceTime){json_object_get_uint64(eg(base, "numerator")),
                               json_object_get_uint64(eg(base, "denominator"))},
                delta, &sum) != SEQUENCE_OK) {
          json_object_put(tail);
          json_object_put(shots);
          return false;
        }
        json_object_object_add(tail, "clip_start",
                               ratio(sum.numerator, sum.denominator));
      }
      json_object_array_add(shots, tail);
    }
  }
  json_object_object_add(rig, "shots", shots);
  if (!inserted)
    return false;
  if (!events) {
    events = json_object_new_array();
    json_object_object_add(a, "event_cameras", events);
  }
  json_object *retained = ec(entry);
  json_object_object_add(retained, "dependency",
                         RuntimeEventCameraDependencies(scene, retained));
  json_object_array_add(events, retained);
  if (!RuntimeEventCameraValidate(scene, d, n) ||
      !RuntimeCameraRigValidateScene(scene, d, n) ||
      !RuntimeInspectionValidate(scene, d, n))
    return false;
#ifdef RAY_RIGID_RESULT_ENABLED
  return RuntimeEventCameraValidateResult(scene, RuntimeRigidResultCurrent(), d,
                                          n);
#else
  return false;
#endif
}
bool RuntimeEventCameraAttach(json_object *scene, json_object *entry, char *d,
                              size_t n) {
  json_object *copy = ec(scene);
  if (!copy)
    return false;
  bool ok = attach(copy, entry, d, n);
  if (ok) {
    json_object_object_add(ea(scene), "camera_rig",
                           ec(eg(ea(copy), "camera_rig")));
    json_object_object_add(ea(scene), "event_cameras",
                           ec(eg(ea(copy), "event_cameras")));
  }
  json_object_put(copy);
  return ok;
}
