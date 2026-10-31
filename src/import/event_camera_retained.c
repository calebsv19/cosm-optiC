#include "event_camera_internal.h"

bool EventRetainedResolve(json_object *scene, const RuntimeRigidResult *result,
                          json_object *entry, EventRequest *request,
                          InspectionOrbit *out, EventFocus *focus,
                          EventFocus *pivot, char *d, size_t n) {
  EventRequest r;
  InspectionOrbit orbit;
  EventFocus f, p;
  json_object *actual = NULL, *saved = eg(entry, "resolved");
  char hash[65];
  if (!EventResolve(scene, result, eg(entry, "intent"),
                    json_object_get_int(eg(entry, "selection")), &r, &orbit, &f,
                    &p, &actual, d, n))
    return false;
  bool ok = EventProposalHash(scene, eg(entry, "intent"), saved, hash) &&
            !strcmp(hash, es(saved, "proposal_sha256")) &&
            EventResolvedEquivalent(actual, saved) &&
            json_object_get_boolean(eg(actual, "usable"));
  json_object_put(actual);
  if (!ok)
    return ef(
        d, n,
        "retained proposal integrity or portable geometry validation failed");
  double v[3];
  if (!ev(eg(saved, "position"), v, 3))
    return false;
  orbit.anchor.position = etvec(v);
  if (!ev(eg(saved, "focus"), v, 3))
    return false;
  f.point = etvec(v);
  if (!ev(eg(saved, "pivot"), v, 3))
    return false;
  p.point = etvec(v);
  orbit.target = p.point;
  /* Evaluate the reviewed seed, rather than replacing it with a native replan.
   * Fresh source bounds remain authoritative for containment and visibility. */
  if (!EventCameraAim(&orbit.anchor, f.point, r.framing.offset) ||
      !InspectionOrbitBuild(&orbit, &orbit) ||
      EventFocusOccluded(result, r.tick, &f, orbit.anchor.position))
    return ef(d, n, "retained event seed is infeasible or obstructed");
  EventCameraLead lead;
  if (r.dolly && (!EventLeadBuild(&r, &orbit, &f, &lead) ||
                  EventFocusOccluded(result, r.tick, &f, lead.start.position)))
    return ef(d, n, "retained event lead is infeasible or obstructed");
  int orbit_occluded = 0, lead_occluded = 0;
  for (int i = 0; i <= 64; i++) {
    RayEvaluatedCamera pose;
    if (!InspectionOrbitSample(&orbit, orbit.duration * i / 64, &pose) ||
        !EventCameraAim(&pose, f.point, r.framing.offset) ||
        !EventCameraContains(&pose, f.low, f.high, r.framing.margin))
      return ef(d, n,
                "retained event orbit does not frame current source bounds");
    if (i % 4 == 0)
      orbit_occluded += EventFocusOccluded(result, r.tick, &f, pose.position);
    if (r.dolly &&
        (!EventCameraLeadSample(&lead, lead.duration * i / 64, &pose) ||
         !EventCameraAim(&pose, f.point, r.framing.offset) ||
         !EventCameraContains(&pose, f.low, f.high, r.framing.margin)))
      return ef(d, n,
                "retained event lead does not frame current source bounds");
    if (r.dolly && i % 4 == 0)
      lead_occluded += EventFocusOccluded(result, r.tick, &f, pose.position);
  }
  if (orbit_occluded !=
      json_object_get_int(eg(saved, "possible_occlusion_samples")))
    return ef(d, n, "retained orbit visibility metadata differs");
  if (r.dolly && lead_occluded != json_object_get_int(eg(saved, "lead_possible_occlusion_samples")))
    return ef(d, n, "retained lead visibility metadata differs");
  if (request)
    *request = r;
  if (out)
    *out = orbit;
  if (focus)
    *focus = f;
  if (pivot)
    *pivot = p;
  return true;
}
