#include "event_camera_internal.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "render/runtime_ray_3d.h"
#include "runtime_rigid_result_internal.h"
#include <float.h>
#include <stdlib.h>
static const cJSON *mesh(const RuntimeRigidResult *r, size_t b) {
  for (size_t i = 0; i < r->asset_count; i++)
    if (!strcmp(r->assets[i].id, r->result.bindings[b].physical.mesh_id))
      return cJSON_GetObjectItemCaseSensitive(r->assets[i].visible, "mesh");
  return NULL;
}
static bool source_vertex(const cJSON *v, double p[3]) {
  const char *keys[] = {"x", "y", "z"};
  for (int i = 0; i < 3; i++) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(v, keys[i]);
    if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble))
      return false;
    p[i] = n->valuedouble;
  }
  return true;
}
static bool bounds(const RuntimeRigidResult *r, size_t b, uint64_t tick,
                   double low[3], double high[3], bool world) {
  const cJSON *vertices =
      cJSON_GetObjectItemCaseSensitive(mesh(r, b), "vertices");
  int n = cJSON_GetArraySize(vertices);
  if (n < 1)
    return false;
  for (int k = 0; k < 3; k++) {
    low[k] = DBL_MAX;
    high[k] = -DBL_MAX;
  }
  const cJSON *vertex = vertices->child;
  for (int i = 0; i < n; i++, vertex = vertex->next) {
    double p[3], v[3];
    if (!source_vertex(vertex, p))
      return false;
    if (world) {
      if (core_scene_rigid_result_vertex(&r->result, (uint32_t)tick, b, p, v)
              .code != CORE_OK)
        return false;
    } else
      memcpy(v, p, sizeof(v));
    for (int k = 0; k < 3; k++) {
      low[k] = fmin(low[k], v[k]);
      high[k] = fmax(high[k], v[k]);
    }
  }
  return true;
}
bool EventFocusResolve(const RuntimeRigidResult *r, uint64_t tick,
                       json_object *ref, EventFocus *out) {
  if (!r || !out || tick > r->result.endpoint_tick)
    return false;
  EventFocus f = {0};
  const char *kind = es(ref, "kind");
  json_object *ids = eg(ref, "targets");
  double p[3], low[3], high[3];
  if (!strcmp(kind, "world_point")) {
    double radius;
    if (!eo(ref, 3) || !ev(eg(ref, "point"), p, 3) ||
        !en(eg(ref, "radius"), &radius) || radius <= 0 || radius > 1e6)
      return false;
    f.point = etvec(p);
    for (int k = 0; k < 3; k++) {
      low[k] = p[k] - radius;
      high[k] = p[k] + radius;
    }
    f.low = etvec(low);
    f.high = etvec(high);
    *out = f;
    return true;
  }
  bool pair = !strcmp(kind, "pair_com") || !strcmp(kind, "pair_bounds"),
       point = !strcmp(kind, "body_point"), side = !strcmp(kind, "body_side"),
       com = !strcmp(kind, "body_com");
  if ((!pair && !point && !side && !com) || !eo(ref, (point || side) ? 4 : 2) ||
      !json_object_is_type(ids, json_type_array) ||
      json_object_array_length(ids) != (pair ? 2u : 1u))
    return false;
  f.body_count = pair ? 2 : 1;
  for (size_t i = 0; i < f.body_count; i++) {
    json_object *name = json_object_array_get_idx(ids, i);
    if (!json_object_is_type(name, json_type_string) ||
        json_object_get_string_len(name) !=
            (int)strlen(json_object_get_string(name)))
      return false;
    bool found = false;
    for (size_t b = 0; b < r->result.body_count; b++)
      if (!strcmp(json_object_get_string(name),
                  r->result.bindings[b].physical.object_id)) {
        f.bodies[i] = (int)b;
        found = true;
      }
    if (!found || (i && f.bodies[0] == f.bodies[1]))
      return false;
  }
  for (int k = 0; k < 3; k++) {
    low[k] = DBL_MAX;
    high[k] = -DBL_MAX;
    p[k] = 0;
  }
  double total = 0;
  for (size_t i = 0; i < f.body_count; i++) {
    size_t b = (size_t)f.bodies[i];
    double lo[3], hi[3];
    if (!bounds(r, b, tick, lo, hi, true))
      return false;
    for (int k = 0; k < 3; k++) {
      low[k] = fmin(low[k], lo[k]);
      high[k] = fmax(high[k], hi[k]);
    }
    const CoreSceneRigidPose *pose =
        &r->result.poses[tick * r->result.body_count + b];
    double mass = pair ? r->result.bindings[b].physical.mass_kg : 1;
    if (!strcmp(kind, "pair_com") && (!isfinite(mass) || mass <= 0))
      return false;
    for (int k = 0; k < 3; k++)
      p[k] += pose->position_m[k] * mass;
    total += mass;
  }
  if (!strcmp(kind, "pair_bounds"))
    for (int k = 0; k < 3; k++)
      p[k] = (low[k] + high[k]) / 2;
  else if (pair) {
    if (total <= 0)
      return false;
    for (int k = 0; k < 3; k++)
      p[k] /= total;
  }
  if (point || side) {
    double local[3], radius;
    if (!en(eg(ref, "radius"), &radius) || radius <= 0 || radius > 1e6)
      return false;
    double lo[3], hi[3];
    if (!bounds(r, (size_t)f.bodies[0], tick, lo, hi, false))
      return false;
    if (point) {
      if (!ev(eg(ref, "point"), local, 3))
        return false;
      for (int k = 0; k < 3; k++)
        if (local[k] < lo[k] - 1e-9 || local[k] > hi[k] + 1e-9)
          return false;
    } else {
      const char *s = es(ref, "side");
      if (strlen(s) != 2 || s[0] < 'x' || s[0] > 'z' ||
          (s[1] != '+' && s[1] != '-'))
        return false;
      for (int k = 0; k < 3; k++)
        local[k] = (lo[k] + hi[k]) / 2;
      int axis = s[0] - 'x';
      local[axis] = s[1] == '+' ? hi[axis] : lo[axis];
    }
    if (core_scene_rigid_result_vertex(&r->result, (uint32_t)tick,
                                       (size_t)f.bodies[0], local, p)
            .code != CORE_OK)
      return false;
    for (int k = 0; k < 3; k++) {
      low[k] = p[k] - radius;
      high[k] = p[k] + radius;
    }
  }
  f.point = etvec(p);
  f.low = etvec(low);
  f.high = etvec(high);
  *out = f;
  return true;
}
/* Recorded mesh broad phase plus the existing native triangle intersector.
 * Non-result scene geometry still requires the native image review. */
bool EventFocusOccluded(const RuntimeRigidResult *r, uint64_t tick,
                        const EventFocus *f, TimelineVec3 camera) {
  double origin[3] = {camera.x, camera.y, camera.z},
         target[3] = {f->point.x, f->point.y, f->point.z};
  for (size_t b = 0; b < r->result.body_count; b++) {
    bool skip = false;
    for (size_t i = 0; i < f->body_count; i++)
      skip |= f->bodies[i] == (int)b;
    if (skip || r->hidden[b])
      continue;
    double lo[3], hi[3];
    if (!bounds(r, b, tick, lo, hi, true))
      return true;
    double near = 0, far = 1 - 1e-6;
    for (int k = 0; k < 3; k++) {
      double d = target[k] - origin[k];
      if (fabs(d) < 1e-12) {
        if (origin[k] < lo[k] || origin[k] > hi[k]) {
          far = -1;
          break;
        }
      } else {
        double a = (lo[k] - origin[k]) / d, z = (hi[k] - origin[k]) / d;
        if (a > z) {
          double t = a;
          a = z;
          z = t;
        }
        near = fmax(near, a);
        far = fmin(far, z);
      }
    }
    if (near <= far && far > 1e-6) {
      const cJSON *m = mesh(r, b),
                  *triangles = cJSON_GetObjectItemCaseSensitive(m, "triangles"),
                  *vertices = cJSON_GetObjectItemCaseSensitive(m, "vertices");
      Ray3D ray = {.origin = vec3(camera.x, camera.y, camera.z),
                   .direction =
                       vec3(target[0] - origin[0], target[1] - origin[1],
                            target[2] - origin[2])};
      int vertex_count=cJSON_GetArraySize(vertices);
      if(vertex_count<1)return true;
      const cJSON **vertex_table=malloc((size_t)vertex_count*sizeof(*vertex_table));
      if(!vertex_table)return true;
      const cJSON *vertex=vertices->child;
      for(int i=0;i<vertex_count;i++,vertex=vertex->next)vertex_table[i]=vertex;
      const cJSON *t=triangles->child;
      for (int i = 0; t; i++,t=t->next) {
        const char *keys[] = {"a", "b", "c"};
        RuntimeTriangle3D triangle = {0};
        Vec3 *points[] = {&triangle.p0, &triangle.p1, &triangle.p2};
        for (int k = 0; k < 3; k++) {
          const cJSON *index = cJSON_GetObjectItemCaseSensitive(t, keys[k]);
          double source[3], world[3];
          if (!cJSON_IsNumber(index) || index->valuedouble<0 ||
              index->valuedouble>=vertex_count || index->valuedouble!=index->valueint ||
              !source_vertex(vertex_table[index->valueint], source) ||
              core_scene_rigid_result_vertex(&r->result, (uint32_t)tick, b,
                                             source, world)
                      .code != CORE_OK) {
            free(vertex_table);return true;
          }
          *points[k] = vec3(world[0], world[1], world[2]);
        }
        HitInfo3D hit;
        if (RuntimeRay3D_IntersectTriangle(&ray, &triangle, i, 1e-6, 1 - 1e-6,
                                           &hit)) {
          free(vertex_table);return true;
        }
      }
      free(vertex_table);
    }
  }
  return false;
}
#else
bool EventFocusResolve(const RuntimeRigidResult *r, uint64_t t, json_object *j,
                       EventFocus *f) {
  (void)r;
  (void)t;
  (void)j;
  (void)f;
  return false;
}
bool EventFocusOccluded(const RuntimeRigidResult *r, uint64_t t,
                        const EventFocus *f, TimelineVec3 p) {
  (void)r;
  (void)t;
  (void)f;
  (void)p;
  return true;
}
#endif
