#include "animation/sequence_time.h"
#include "event_camera_internal.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_result_internal.h"
#endif
#include <stdlib.h>
#define EVENT_CAP 8
static json_object *loaded;
static struct {
  bool ready;
  EventRequest request;
  InspectionOrbit orbit;
  EventCameraLead approach;
  EventFocus focus, pivot;
  char revision[65];
} cache[EVENT_CAP];
static json_object *camera(json_object *scene, const char *id) {
  json_object *list = eg(eg(ea(scene), "camera_rig"), "cameras");
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *c = json_object_array_get_idx(list, i);
    if (!strcmp(es(c, "id"), id))
      return c;
  }
  return NULL;
}
bool RuntimeEventCameraValidate(json_object *scene, char *d, size_t n) {
  json_object *list = eg(ea(scene), "event_cameras");
  if (!list)
    return true;
  if (!json_object_is_type(list, json_type_array) ||
      json_object_array_length(list) > EVENT_CAP)
    return ef(d, n, "event_cameras capacity/type");
  for (size_t i = 0; i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    EventRequest r;
    uint64_t selection;
    if (!eo(e, 7) || strcmp(es(e, "schema"), "ray_event_camera_v1") ||
        !EventRequestRead(scene, eg(e, "intent"), &r, d, n) ||
        strcmp(es(e, "id"), r.id) || strcmp(es(e, "camera_id"), r.camera_id) ||
        !eu(e, "selection", &selection) || selection > 2 ||
        !json_object_is_type(eg(e, "resolved"), json_type_object) ||
        !json_object_is_type(eg(e, "dependency"), json_type_object) ||
        !camera(scene, r.camera_id))
      return ef(d, n, "invalid retained event record or camera reference");
    json_object *dep = RuntimeEventCameraDependencies(scene, e);
    bool equal = EventEqual(dep, eg(e, "dependency"));
    json_object_put(dep);
    if (!equal)
      return ef(d, n,
                "stale event source/camera/sequence dependency; remove and "
                "regenerate the plan");
    json_object *tracks = eg(eg(ea(scene), "scene_timeline"), "tracks"),
                *bindings = eg(eg(ea(scene), "motion_paths"), "bindings"),
                *moves = eg(ea(scene), "inspection_moves");
    json_object *conflicts[] = {tracks, bindings, moves};
    const char *keys[] = {"target_id", "target_id", "camera_id"};
    for (int k = 0; k < 3; k++)
      for (size_t j = 0;
           conflicts[k] && j < json_object_array_length(conflicts[k]); j++)
        if (!strcmp(es(json_object_array_get_idx(conflicts[k], j), keys[k]),
                    r.camera_id))
          return ef(d, n,
                    "event camera has competing track, route or inspection "
                    "ownership");
    /* View behavior is independent of shot selection. The selecting command
     * installs a default shot; later cuts and detached views do not retime it.
     */
    for (size_t j = 0; j < i; j++) {
      json_object *p = json_object_array_get_idx(list, j);
      if (!strcmp(es(p, "id"), r.id) ||
          !strcmp(es(p, "camera_id"), r.camera_id))
        return ef(d, n, "duplicate event ID or camera");
    }
  }
  return true;
}
bool RuntimeEventCameraValidateResult(json_object *scene,
                                      const RuntimeRigidResult *result, char *d,
                                      size_t n) {
  json_object *list = eg(ea(scene), "event_cameras");
  if (!list)
    return true;
  if (!RuntimeEventCameraValidate(scene, d, n))
    return false;
  for (size_t i = 0; i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    EventRequest r;
    InspectionOrbit orbit;
    if (!EventRetainedResolve(scene, result, e, &r, &orbit, NULL, NULL, d, n))
      return false;
    json_object *c = camera(scene, r.camera_id);
    double p[3], scale = json_object_get_double(eg(scene, "world_scale"));
    if (!isfinite(scale) || scale <= 0 || !ev(eg(c, "position"), p, 3) ||
        fabs(p[0] * scale - orbit.anchor.position.x) > 1e-10 ||
        fabs(p[1] * scale - orbit.anchor.position.y) > 1e-10 ||
        fabs(p[2] * scale - orbit.anchor.position.z) > 1e-10 ||
        json_object_get_double(eg(c, "fov_y")) != r.framing.fov_y ||
        json_object_get_double(eg(c, "zoom")) != 1)
      return ef(d, n, "event camera seed differs from its resolved pose/lens");
  }
  return true;
}
void RuntimeEventCameraReset(void) {
  if (loaded)
    json_object_put(loaded);
  loaded = NULL;
  memset(cache, 0, sizeof(cache));
}
bool RuntimeEventCameraLoad(json_object *scene) {
  RuntimeEventCameraReset();
  if (!eg(ea(scene), "event_cameras"))
    return true;
  return json_object_deep_copy(scene, &loaded, NULL) == 0;
}
bool RuntimeEventCameraOwns(const char *id) {
  json_object *list = eg(ea(loaded), "event_cameras");
  for (size_t i = 0; list && i < json_object_array_length(list); i++)
    if (!strcmp(es(json_object_array_get_idx(list, i), "camera_id"), id))
      return true;
  return false;
}
double RuntimeEventCameraMinimumAspect(const char *id) {
  json_object *list = eg(ea(loaded), "event_cameras");
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    if (!strcmp(es(e, "camera_id"), id))
      return json_object_get_double(
          eg(eg(eg(e, "intent"), "framing"), "aspect"));
  }
  return 0;
}
bool RuntimeEventCameraApply(const TimelineEvaluationContext *c,
                             RayEvaluatedCamera *camera_value) {
  json_object *list = eg(ea(loaded), "event_cameras");
  if (!list)
    return true;
  for (size_t i = 0; i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    if (strcmp(es(e, "camera_id"), camera_value->target_id))
      continue;
#ifdef RAY_RIGID_RESULT_ENABLED
    const RuntimeRigidResult *result = RuntimeRigidResultCurrent();
    if (!result)
      return false;
    if (!cache[i].ready) {
      if (!EventRetainedResolve(loaded, result, e, &cache[i].request,
                                &cache[i].orbit, &cache[i].focus,
                                &cache[i].pivot, NULL, 0))
        return false;
      if (cache[i].request.dolly &&
          !EventLeadBuild(&cache[i].request, &cache[i].orbit, &cache[i].focus,
                          &cache[i].approach))
        return false;
      snprintf(cache[i].revision, 65, "%s", result->revision);
      cache[i].ready = true;
    }
    if (strcmp(cache[i].revision, result->revision))
      return false;
    EventRequest *r = &cache[i].request;
    InspectionOrbit *orbit = &cache[i].orbit;
    double elapsed = c->local_frame < r->start
                         ? 0
                         : ((double)(c->local_frame - r->start) + c->subframe) *
                               c->rate.frames_per_second_denominator /
                               c->rate.frames_per_second_numerator;
    if (elapsed > orbit->duration)
      elapsed = orbit->duration;
    RayEvaluatedCamera pose;
    if (r->dolly && c->local_frame < r->start) {
      double lead_elapsed =
          c->local_frame < r->lead
              ? 0
              : ((double)(c->local_frame - r->lead) + c->subframe) *
                    c->rate.frames_per_second_denominator /
                    c->rate.frames_per_second_numerator;
      if (!EventCameraLeadSample(&cache[i].approach, lead_elapsed, &pose))
        return false;
    } else if (!InspectionOrbitSample(
                   orbit,
                   (c->local_frame >= r->start + r->count && r->entry)
                       ? 0
                       : elapsed,
                   &pose))
      return false;
    EventFocus focus = cache[i].focus;
    if (c->local_frame >= r->start + r->count && r->track) {
      SequenceEvaluation sample;
      bool present = false;
      TimelineEvaluationContext final_context;
      const TimelineEvaluationContext *tracking_context = c;
      if (c->local_frame >= r->end) {
        if (TimelineEvaluationContextBuild(
                c->rate, c->range,
                (TimelineSample){c->range.start_frame + (int64_t)r->end - 1, 0,
                                 1},
                &final_context) != TIMELINE_STATUS_OK)
          return false;
        tracking_context = &final_context;
      }
      if (RuntimeRigidResultSequence(tracking_context, &sample, &present) !=
              TIMELINE_STATUS_OK ||
          !present ||
          !EventFocusResolve(result, sample.source_tick, r->focus, &focus))
        return false;
    }
    pose.aspect_ratio = fmax(camera_value->aspect_ratio, r->framing.aspect);
    camera_value->aspect_ratio = pose.aspect_ratio;
    if (!EventCameraAim(&pose, focus.point, r->framing.offset))
      return false;
    if (c->local_frame >= r->lead && c->local_frame < r->start + r->count &&
        !EventCameraContains(&pose, cache[i].focus.low, cache[i].focus.high,
                             r->framing.margin))
      return false;
    camera_value->position = pose.position;
    camera_value->yaw_radians = pose.yaw_radians;
    camera_value->pitch_radians = pose.pitch_radians;
    camera_value->fov_y_degrees = pose.fov_y_degrees;
    camera_value->zoom = pose.zoom;
    camera_value->orientation_frame = pose.orientation_frame;
    camera_value->has_orientation_frame = true;
    camera_value->orientation_fallback = false;
    camera_value->uses_authored_path = true;
    snprintf(camera_value->inspection_id, 64, "event_%s", r->id);
    uint64_t frame = c->local_frame < r->start ? 0 : c->local_frame - r->start;
    if (frame >= r->count)
      frame = r->count - 1;
    SequenceTime frames = {frame, 1};
    if (c->local_frame >= r->start && frame < r->count - 1 &&
        SequenceTimeAdd(frames,
                        (SequenceTime){c->sample.subframe_numerator,
                                       c->sample.subframe_denominator},
                        &frames) != SEQUENCE_OK)
      return false;
    return SequenceTimeMultiply(
               frames,
               (SequenceTime){c->rate.frames_per_second_denominator,
                              c->rate.frames_per_second_numerator},
               &camera_value->inspection_time) == SEQUENCE_OK;
#else
    (void)c;
    return false;
#endif
  }
  return true;
}
