#include "scene_editor_motion_path_viewport.h"
#include "editor/scene_editor_document.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
bool MotionPathViewportFrame(const MotionPath *path, const SDL_Rect *viewport) {
  if (!path || path->count < 2 || !viewport || viewport->w < 1 ||
      viewport->h < 1)
    return false;
  RuntimeSceneBridge3DDigestState digest;
  if (!SceneEditorDigestOverlayResolve(&digest))
    return false;
  SceneEditorDigestOverlayNavState nav = *SceneEditorGetViewportNavState();
  double scale = SceneEditorDocumentWorldScale(), minimum[3], maximum[3];
  for (int k = 0; k < 3; ++k)
    minimum[k] = maximum[k] = path->points[0].position[k] * scale;
  for (size_t i = 0; i < path->count; ++i)
    for (int h = 0; h < 3; ++h)
      for (int k = 0; k < 3; ++k) {
        double v = (path->points[i].position[k] +
                    (h == 1   ? path->points[i].incoming[k]
                     : h == 2 ? path->points[i].outgoing[k]
                              : 0)) *
                   scale;
        minimum[k] = fmin(minimum[k], v);
        maximum[k] = fmax(maximum[k], v);
      }
  nav.target_valid = true;
  nav.target_x = (minimum[0] + maximum[0]) / 2;
  nav.target_y = (minimum[1] + maximum[1]) / 2;
  nav.target_z = (minimum[2] + maximum[2]) / 2;
  SceneEditorDigestOverlayProjector p;
  if (!SceneEditorDigestOverlayBuildProjector(&digest, viewport, &nav, &p))
    return false;
  double left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
  for (int i = 0; i < 8; ++i) {
    double x, y;
    if (!SceneEditorDigestOverlayProjectPointF(
            &p, (i & 1) ? maximum[0] : minimum[0],
            (i & 2) ? maximum[1] : minimum[1],
            (i & 4) ? maximum[2] : minimum[2], &x, &y))
      return false;
    left = fmin(left, x);
    right = fmax(right, x);
    top = fmin(top, y);
    bottom = fmax(bottom, y);
  }
  double factor = fmin(fmax(40, viewport->w - 100) / fmax(20, right - left),
                       fmax(40, viewport->h - 160) / fmax(20, bottom - top));
  nav.overlay_zoom = fmax(.00005, fmin(240, nav.overlay_zoom * factor));
  nav.zoom_limits_valid = true;
  nav.zoom_min = fmax(.00005, nav.overlay_zoom * .05);
  nav.zoom_max = fmin(240, nav.overlay_zoom * 20);
  SceneEditorRestoreViewportNav(&nav);
  return true;
}
bool MotionPathViewportAppend(MotionPath *path, const double point[3]) {
  if (!path || path->count < 2 || path->count >= MOTION_POINT_CAPACITY)
    return false;
  MotionPathPoint next = {0};
  unsigned serial = 1;
  bool used;
  do {
    snprintf(next.id, sizeof(next.id), "point-%u", serial++);
    used = false;
    for (size_t i = 0; i < path->count; ++i)
      if (!strcmp(path->points[i].id, next.id))
        used = true;
  } while (used);
  MotionPathPoint *end = &path->points[path->count - 1];
  for (int k = 0; k < 3; ++k) {
    if (!isfinite(point[k]))
      return false;
    next.position[k] = point[k];
    double delta = (point[k] - end->position[k]) / 3;

    next.incoming[k] = -delta;
    next.outgoing[k] = delta;
  }
  double outgoing[3]; for(int k=0;k<3;++k) outgoing[k]=(point[k]-end->position[k])/3;
  MotionPathEditHandle(end,false,outgoing);
  next.linear = end->linear;
  path->points[path->count++] = next;
  return true;
}
bool MotionPathViewportPlacement(const SceneEditorDigestOverlayProjector *p,
                                 int x, int y, double z, double out[3]) {
  double scale = SceneEditorDocumentWorldScale();
  if (!SceneEditorDigestOverlayScreenRayToPlanePoint(p, x, y, z * scale,
                                                     &out[0], &out[1], &out[2]))
    return false;
  for (int k = 0; k < 3; ++k)
    out[k] /= scale;
  return true;
}
void MotionPathViewportGuide(SDL_Renderer *r,
                             const SceneEditorDigestOverlayProjector *p,
                             const MotionPath *path, double z) {
  int x, y;
  SDL_GetMouseState(&x, &y);
  SDL_Point cursor = {x, y};
  double point[3];
  if (!SDL_PointInRect(&cursor, &p->viewport) ||
      !MotionPathViewportPlacement(p, x, y, z, point))
    return;
  double scale = SceneEditorDocumentWorldScale();
  const double *end = path->points[path->count - 1].position;
  int ex, ey;
  SDL_SetRenderDrawColor(r, 80, 230, 245, 220);
  if (SceneEditorDigestOverlayProjectPoint(p, end[0] * scale, end[1] * scale,
                                           end[2] * scale, &ex, &ey)) {
    for (int i = 0; i < 24; i += 2)
      SDL_RenderDrawLine(r, ex + (x - ex) * i / 24, ey + (y - ey) * i / 24,
                         ex + (x - ex) * (i + 1) / 24,
                         ey + (y - ey) * (i + 1) / 24);
  }
  SDL_RenderDrawLine(r, x - 7, y, x + 7, y);
  SDL_RenderDrawLine(r, x, y - 7, x, y + 7);
}
