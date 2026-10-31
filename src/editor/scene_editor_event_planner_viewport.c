#include "editor/scene_editor_shared_text.h"
#include "import/runtime_event_camera.h"
#include "scene_editor_document_transaction.h"
#include "scene_editor_event_planner_internal.h"
#include <limits.h>
#define P event_planner
static unsigned long long generation;
void SceneEditorEventPlannerViewportPrepare(
    const SceneEditorDigestOverlayProjector *projector) {
  if (!SceneEditorEventPlannerVisible())
    return;
  EventPlannerSync();
  RayEvaluatedSceneSnapshot v;
  if (!SceneEditorTimelineCopyEvaluated(&v) || !v.has_sequence)
    return;
  if (!P.viewport_valid ||
      P.viewport_revision != SceneEditorDocumentRevision() ||
      memcmp(&P.projector, projector, sizeof(*projector)) ||
      memcmp(&P.drawn_sample, &v.frame.sample, sizeof(P.drawn_sample))) {
    P.viewport_token = ++generation;
  }
  P.projector = *projector;
  P.viewport_revision = SceneEditorDocumentRevision();
  P.drawn_sample = v.frame.sample;
  P.drawn_tick = v.sequence.source_tick;
  P.viewport_valid = true;
}
uint64_t SceneEditorEventPlannerGeometryStamp(void) {
  return SceneEditorEventPlannerVisible() && P.viewport_valid
             ? (P.drawn_tick + 1) ^ (P.viewport_revision << 32)
             : 0;
}
bool SceneEditorEventPlannerWorldPoint(const char *id, const double local[3],
                                       double world[3]) {
  return SceneEditorEventPlannerVisible() && P.viewport_valid &&
         RuntimeEventCameraRecordedPoint(id, P.drawn_tick, local, world);
}
bool SceneEditorEventPlannerWorldDirection(const char *id,
                                           const double local[3],
                                           double world[3]) {
  double zero[3] = {0}, origin[3];
  if (!SceneEditorEventPlannerWorldPoint(id, zero, origin) ||
      !SceneEditorEventPlannerWorldPoint(id, local, world))
    return false;
  double length = 0;
  for (int k = 0; k < 3; k++) {
    world[k] -= origin[k];
    length += world[k] * world[k];
  }
  length = sqrt(length);
  if (length < 1e-15)
    return false;
  for (int k = 0; k < 3; k++)
    world[k] /= length;
  return true;
}
void SceneEditorEventPlannerViewport(
    SDL_Renderer *r, const SceneEditorDigestOverlayProjector *projector) {
  if (!SceneEditorEventPlannerVisible())
    return;
  json_object *world = pg(P.picked, "world");
  if (world) {
    int x, y;
    if (SceneEditorDigestOverlayProjectPoint(
            projector, json_object_get_double(pa(world, 0)),
            json_object_get_double(pa(world, 1)),
            json_object_get_double(pa(world, 2)), &x, &y)) {
      SDL_SetRenderDrawColor(r, 255, 210, 75, 255);
      SDL_RenderDrawLine(r, x - 9, y, x + 9, y);
      SDL_RenderDrawLine(r, x, y - 9, x, y + 9);
      SDL_Rect marker = {x - 5, y - 5, 10, 10};
      SDL_RenderDrawRect(r, &marker);
    }
  }
  json_object *list = pg(P.proposals, "candidates");
  for (size_t i = 0; list && i < plen(list); i++) {
    json_object *c = pa(list, i), *pos = pg(c, "position"),
                *focus = pg(c, "focus");
    if (!pos)
      continue;
    int x, y, fx, fy;
    if (SceneEditorDigestOverlayProjectPoint(
            projector, json_object_get_double(pa(pos, 0)),
            json_object_get_double(pa(pos, 1)),
            json_object_get_double(pa(pos, 2)), &x, &y) &&
        SceneEditorDigestOverlayProjectPoint(
            projector, json_object_get_double(pa(focus, 0)),
            json_object_get_double(pa(focus, 1)),
            json_object_get_double(pa(focus, 2)), &fx, &fy)) {
      json_object *lead = pg(c, "lead_position");
      int lx, ly;
      if (lead && SceneEditorDigestOverlayProjectPoint(
                      projector, json_object_get_double(pa(lead, 0)),
                      json_object_get_double(pa(lead, 1)),
                      json_object_get_double(pa(lead, 2)), &lx, &ly)) {
        SDL_SetRenderDrawColor(r, 100, 180, 230, 180);
        SDL_RenderDrawLine(r, lx, ly, x, y);
        SDL_Rect start = {lx - 3, ly - 3, 6, 6};
        SDL_RenderDrawRect(r, &start);
      }
      bool selected = (int)i == P.candidate;
      SDL_SetRenderDrawColor(r, selected ? 255 : 100, selected ? 204 : 180,
                             selected ? 70 : 230, 220);
      SDL_RenderDrawLine(r, x, y, fx, fy);
      SDL_Rect b = {x - 6, y - 4, 12, 8};
      SDL_RenderDrawRect(r, &b);
      snprintf(P.labels[60 + i], 192, "View %zu", i + 1);
      scene_editor_shared_label_at(r, P.labels[60 + i], x + 8, y,
                                   (SDL_Color){255, 220, 130, 255}, 10);
    }
  }
}
json_object *EventPlannerQuery(int x, int y, char *d, size_t n) {
  RayEvaluatedSceneSnapshot v;
  SDL_Point point = {x, y};
  if (!SceneEditorEventPlannerVisible() || !P.viewport_valid ||
      P.viewport_revision != SceneEditorDocumentRevision() ||
      P.revision != SceneEditorDocumentRevision() ||
      !SDL_PointInRect(&point, &P.projector.viewport) ||
      !SceneEditorTimelineCopyEvaluated(&v) || !v.has_sequence ||
      memcmp(&v.frame.sample, &P.drawn_sample, sizeof(P.drawn_sample)) ||
      v.frame.sample.subframe_numerator ||
      v.sequence.source_tick !=
          json_object_get_uint64(pg(pg(P.intent, "hold"), "source_tick"))) {
    snprintf(d, n,
             "Stale view/time or not at selected event. Seek event first.");
    return NULL;
  }
  double yaw = P.projector.yaw_rad, pitch = P.projector.pitch_rad,
         scale = P.projector.scale;
  if (!isfinite(scale) || scale <= 0)
    return NULL;
  double rx = cos(yaw), ry = -sin(yaw), ux = cos(pitch) * sin(yaw),
         uy = cos(pitch) * cos(yaw), uz = -sin(pitch),
         wx = sin(pitch) * sin(yaw), wy = sin(pitch) * cos(yaw),
         wz = cos(pitch);
  double sx = (x + .5 - P.projector.viewport.x - P.projector.viewport.w * .5) /
              scale,
         sy = (y + .5 - P.projector.viewport.y - P.projector.viewport.h * .5) /
              scale,
         depth = P.projector.span_max * 2 + 10;
  double origin[] = {P.projector.center_x + rx * sx + ux * sy + wx * depth,
                     P.projector.center_y + ry * sx + uy * sy + wy * depth,
                     P.projector.center_z + uz * sy + wz * depth},
         direction[] = {-wx, -wy, -wz};
  double radius = P.feature_radius;
  if (radius <= 0)
    radius = .25;
  /* Match the frontmost displayed object, including ordinary floor/box
   * primitives. Never select a recorded feature hidden by another object. */
  int visible = SceneEditorViewportPickObject(&P.projector, x, y);
  char visible_id[64];
  if (!runtime_scene_bridge_get_last_object_id_for_scene_index(
          visible, visible_id, sizeof(visible_id))) {
    snprintf(d, n, "No displayed surface at this point.");
    return NULL;
  }
  json_object *hit = RuntimeEventCameraPickFeature(
      document_authoring_root(), v.sequence.source_tick, origin, direction,
      radius, d, n);
  if (hit) {
    const char *id =
        json_object_get_string(pa(pg(pg(hit, "feature"), "targets"), 0));
    if (!id || strcmp(id, visible_id)) {
      json_object_put(hit);
      snprintf(
          d, n,
          "Recorded feature is hidden by the displayed foreground surface.");
      return NULL;
    }
  }
  return hit;
}
bool SceneEditorEventPlannerPick(json_object *params, json_object *response,
                                 char *d, size_t n) {
  json_object *x = pg(params, "x"), *y = pg(params, "y"),
              *token = pg(params, "viewport_token"),
              *revision = pg(params, "expected_revision");
  if (!json_object_is_type(params, json_type_object) ||
      json_object_object_length(params) != 4 ||
      !json_object_is_type(x, json_type_int) ||
      !json_object_is_type(y, json_type_int) ||
      !json_object_is_type(token, json_type_int) ||
      !json_object_is_type(revision, json_type_int) ||
      json_object_get_uint64(token) != P.viewport_token || !P.viewport_valid ||
      json_object_get_uint64(revision) != SceneEditorDocumentRevision()) {
    snprintf(d, n, "Stale viewport token/revision or invalid pick arguments.");
    return false;
  }
  int64_t ix = json_object_get_int64(x), iy = json_object_get_int64(y);
  if (ix < 0 || iy < 0 || ix > INT_MAX || iy > INT_MAX)
    return false;
  json_object *hit = EventPlannerQuery((int)ix, (int)iy, d, n);
  if (!hit)
    return false;
  json_object_object_add(response, "event_feature", hit);
  return true;
}
