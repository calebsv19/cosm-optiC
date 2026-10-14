#include "editor/scene_editor_motion_paths.h"
#include <stdio.h>
#include <string.h>
bool SceneEditorMotionPathInsertPoint(MotionPath *p, size_t at, char *id, size_t size) {
  if(!p || at>=p->count || p->count<2 || p->count>=MOTION_POINT_CAPACITY) return false;
  MotionPathPoint next = {0};
  unsigned serial = 1;
  bool used;
  do {
    snprintf(next.id, sizeof(next.id), "point-%u", serial++);
    used = false;
    for (size_t i = 0; i < p->count; ++i)
      if (!strcmp(next.id, p->points[i].id))
        used = true;
  } while (used);
  MotionPathPoint *a = &p->points[at];
  next.linear = a->linear;
  if (at == p->count - 1) {
    for (int k = 0; k < 3; ++k) {
      double delta = a->position[k] - p->points[at - 1].position[k];
      next.position[k] = a->position[k] + delta;
      next.incoming[k] = -delta / 3;
      next.outgoing[k] = delta / 3;
    }
  } else {
    MotionPathPoint *b = &p->points[at + 1];
    for (int k = 0; k < 3; ++k) {
      double x = a->position[k], y = x + a->outgoing[k],
             z = b->position[k] + b->incoming[k], w = b->position[k];
      if (a->linear) {
        y = x + (w - x) / 3;
        z = x + 2 * (w - x) / 3;
      }
      double xy = (x + y) / 2, yz = (y + z) / 2, zw = (z + w) / 2,
             left = (xy + yz) / 2, right = (yz + zw) / 2,
             mid = (left + right) / 2;
      next.position[k] = mid;
      next.incoming[k] = left - mid;
      next.outgoing[k] = right - mid;
      if(!a->linear) {a->outgoing[k] = xy - x; b->incoming[k] = zw - w;}
    }
  }
  memmove(&p->points[at + 2], &p->points[at + 1],
          (p->count - at - 1) * sizeof(next));
  p->points[at + 1] = next;
  ++p->count;
  if(id && size) snprintf(id,size,"%s",next.id);
  return true;
}
