#include "event_camera_internal.h"
bool EventLeadRead(json_object *lead, EventRequest *r, char *d, size_t n) {
  const char *kind = es(lead, "kind");
  r->dolly = !strcmp(kind, "dolly");
  if ((!r->dolly && strcmp(kind, "stationary")) ||
      !eo(lead, r->dolly ? 6 : 2) || !eu(lead, "start_frame", &r->lead))
    return ef(d, n, "Expected stationary or strict dolly lead.");
  if (!r->dolly)
    return true;
  EventCameraLead *l = &r->approach;
  if (!en(eg(lead, "distance_scale"), &l->distance_scale) ||
      l->distance_scale <= 1 || l->distance_scale > 100 ||
      !en(eg(lead, "max_speed"), &l->max_speed) || l->max_speed <= 0 ||
      !en(eg(lead, "acceleration"), &l->acceleration) || l->acceleration <= 0 ||
      !en(eg(lead, "braking"), &l->braking) || l->braking <= 0)
    return ef(d, n, "Invalid dolly scale or motion limits.");
  return true;
}
bool EventLeadBuild(const EventRequest *r, const InspectionOrbit *orbit,
                    const EventFocus *focus, EventCameraLead *out) {
  if (!r || !r->dolly || !orbit || !focus || !out)
    return false;
  EventCameraLead value = r->approach;
  value.event = orbit->anchor;
  value.focus = focus->point;
  return EventCameraLeadBuild(&value, out);
}
