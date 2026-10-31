/* Full recorded source mesh feature query; no preview LOD or document edits. */
#include "event_camera_internal.h"
#include "render/runtime_ray_3d.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_result_internal.h"
#endif
json_object *RuntimeEventCameraPickFeature(json_object *scene, uint64_t tick,
                                           const double origin[3],
                                           const double direction[3],
                                           double radius, char *d, size_t n) {
#ifdef RAY_RIGID_RESULT_ENABLED
  const RuntimeRigidResult *r = RuntimeRigidResultCurrent();
  if (!r || !scene || !origin || !direction || !isfinite(radius) ||
      radius <= 0 || radius > 1e6 ||
      strcmp(es(eg(eg(eg(scene, "extensions"), "ray_tracing"), "rigid_result"),
                "result_revision"),
             r->revision) ||
      tick > r->result.endpoint_tick) {
    ef(d, n,
       "Requires current authenticated recorded source and valid radius/tick.");
    return NULL;
  }
  for (int k = 0; k < 3; k++)
    if (!isfinite(origin[k]) || !isfinite(direction[k]))
      return NULL;
  double norm = sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                     direction[2] * direction[2]);
  if (!isfinite(norm) || norm < 1e-12)
    return NULL;
  Ray3D ray = {.origin = vec3(origin[0], origin[1], origin[2]),
               .direction = vec3(direction[0] / norm, direction[1] / norm,
                                 direction[2] / norm)};
  double nearest = RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE;
  json_object *answer = NULL;
  for (size_t b = 0; b < r->result.body_count; b++) {
    if (r->hidden[b])
      continue;
    const cJSON *m = NULL;
    for (size_t a = 0; a < r->asset_count; a++)
      if (!strcmp(r->assets[a].id, r->result.bindings[b].physical.mesh_id))
        m = cJSON_GetObjectItemCaseSensitive(r->assets[a].visible, "mesh");
    const cJSON *vertices = cJSON_GetObjectItemCaseSensitive(m, "vertices"),
                *triangles = cJSON_GetObjectItemCaseSensitive(m, "triangles");
    for (int t = 0; t < cJSON_GetArraySize(triangles); t++) {
      double source[3][3], world[3][3];
      RuntimeTriangle3D tri = {0};
      Vec3 *p[] = {&tri.p0, &tri.p1, &tri.p2};
      const char *keys[] = {"a", "b", "c"}, *xyz[] = {"x", "y", "z"};
      bool valid = true;
      for (int k = 0; k < 3; k++) {
        const cJSON *ix = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetArrayItem(triangles, t), keys[k]);
        if (!cJSON_IsNumber(ix)) {
          valid = false;
          break;
        }
        const cJSON *v = cJSON_GetArrayItem(vertices, ix->valueint);
        for (int j = 0; j < 3; j++) {
          const cJSON *x = cJSON_GetObjectItemCaseSensitive(v, xyz[j]);
          if (!cJSON_IsNumber(x) || !isfinite(x->valuedouble)) {
            valid = false;
            break;
          }
          source[k][j] = x->valuedouble;
        }
        if (!valid || core_scene_rigid_result_vertex(&r->result, (uint32_t)tick,
                                                     b, source[k], world[k])
                              .code != CORE_OK) {
          valid = false;
          break;
        }
        *p[k] = vec3(world[k][0], world[k][1], world[k][2]);
      }
      HitInfo3D hit;
      if (!valid ||
          !RuntimeRay3D_IntersectTriangle(&ray, &tri, t, 1e-8, nearest, &hit))
        continue;
      /* Compute source barycentrics independently from the exact world hit. */
      double a[3], c[3], q[3], aa = 0, ac = 0, cc = 0, qa = 0, qc = 0;
      double hp[] = {hit.position.x, hit.position.y, hit.position.z};
      for (int k = 0; k < 3; k++) {
        a[k] = world[1][k] - world[0][k];
        c[k] = world[2][k] - world[0][k];
        q[k] = hp[k] - world[0][k];
        aa += a[k] * a[k];
        ac += a[k] * c[k];
        cc += c[k] * c[k];
        qa += q[k] * a[k];
        qc += q[k] * c[k];
      }
      double det = aa * cc - ac * ac;
      if (det <= 1e-24)
        continue;
      double v = (cc * qa - ac * qc) / det, z = (aa * qc - ac * qa) / det,
             weights[] = {1 - v - z, v, z}, point[3] = {0};
      for (int k = 0; k < 3; k++)
        for (int j = 0; j < 3; j++)
          point[j] += weights[k] * source[k][j];
      json_object *ref = json_object_new_object(),
                  *ids = json_object_new_array();
      json_object_array_add(ids, json_object_new_string(
                                     r->result.bindings[b].physical.object_id));
      json_object_object_add(ref, "kind", json_object_new_string("body_point"));
      json_object_object_add(ref, "targets", ids);
      json_object_object_add(ref, "point", evec(etvec(point)));
      json_object_object_add(ref, "radius", json_object_new_double(radius));
      if (answer)
        json_object_put(answer);
      answer = json_object_new_object();
      json_object_object_add(answer, "feature", ref);
      json_object_object_add(answer, "world", evec(etvec(hp)));
      json_object_object_add(answer, "barycentric", evec(etvec(weights)));
      json_object_object_add(answer, "triangle", json_object_new_int(t));
      json_object_object_add(answer, "source_tick",
                             json_object_new_int64(tick));
      json_object_object_add(answer, "source_revision",
                             json_object_new_string(r->revision));
      nearest = hit.t;
    }
  }
  if (!answer)
    ef(d, n, "No recorded mesh surface at this point.");
  return answer;
#else
  (void)scene;
  (void)tick;
  (void)origin;
  (void)direction;
  (void)radius;
  ef(d, n, "Recorded rigid support unavailable.");
  return NULL;
#endif
}

bool RuntimeEventCameraRecordedPoint(const char *id, uint64_t tick,
                                     const double source[3], double world[3]) {
#ifdef RAY_RIGID_RESULT_ENABLED
  const RuntimeRigidResult *r = RuntimeRigidResultCurrent();
  if (!r || !id || !source || !world || tick > r->result.endpoint_tick)
    return false;
  for (size_t b = 0; b < r->result.body_count; b++)
    if (!r->hidden[b] && !strcmp(id, r->result.bindings[b].physical.object_id))
      return core_scene_rigid_result_vertex(&r->result, (uint32_t)tick, b,
                                            source, world)
                 .code == CORE_OK;
#else
  (void)id;
  (void)tick;
  (void)source;
  (void)world;
#endif
  return false;
}
