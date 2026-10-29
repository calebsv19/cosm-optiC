#define _DARWIN_C_SOURCE
#ifdef RAY_RIGID_RESULT_ENABLED
#include "core_scene.h"
#include "core_scene_compile.h"
#include "import/runtime_scene_timeline.h"
#include "import/scene_timeline_document_io.h"
#include "render/runtime_surface_sampling.h"
#include "runtime_rigid_result_internal.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static RuntimeRigidResult *current;
static json_object *get(json_object *j, const char *k) {
  json_object *v = NULL;
  if (j)
    json_object_object_get_ex(j, k, &v);
  return v;
}
static const char *str(json_object *j, const char *k) {
  json_object *v = get(j, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static json_object *reference(json_object *s) {
  return get(get(get(s, "extensions"), "ray_tracing"), "rigid_result");
}
static bool fail(char *d, size_t n, const char *s) {
  if (d && n)
    snprintf(d, n, "%s", s);
  return false;
}
bool RuntimeRigidResultValidateReference(json_object *scene, char *d,
                                         size_t n) {
  bool physical = get(get(scene, "extensions"), "rigid_scene") != NULL;
  json_object *r = reference(scene);
  if (!physical && !r)
    return true;
  if (!physical || !json_object_is_type(r, json_type_object) ||
      json_object_object_length(r) != 3 ||
      strcmp(str(r, "schema"), "ray_rigid_result_binding_v1") ||
      !str(r, "bundle_path")[0] || strlen(str(r, "bundle_path")) >= 3500 ||
      strlen(str(r, "result_revision")) != 64)
    return fail(
        d, n, "rigid_scene execution requires a verified rigid_result binding");
  const char *rev = str(r, "result_revision");
  for (size_t i = 0; i < 64; i++)
    if (!strchr("0123456789abcdef", rev[i]))
      return fail(d, n, "rigid result revision must be SHA-256");
  return true;
}
bool RuntimeRigidResultPrepare(json_object *scene, const char *path,
                               RuntimeRigidResult **out, char *d, size_t n) {
  if (!out || !RuntimeRigidResultValidateReference(scene, d, n))
    return false;
  *out = NULL;
  json_object *r = reference(scene);
  if (!r)
    return true;
  char dir[4096], bundle[4096];
  if (!path || !path[0] ||
      core_scene_dirname(path, dir, sizeof(dir)).code != CORE_OK ||
      core_scene_resolve_path(dir, str(r, "bundle_path"), bundle,
                              sizeof(bundle))
              .code != CORE_OK)
    return fail(d, n, "rigid result requires a file-backed scene path");
  if (!RuntimeRigidBundleRead(
          bundle, str(r, "result_revision"),
          json_object_to_json_string_ext(scene, JSON_C_TO_STRING_PLAIN), out, d,
          n))
    return false;
  json_object *objects = get(scene, "objects");
  for (size_t b = 0; b < (*out)->result.body_count; b++) {
    for (size_t i = 0; json_object_is_type(objects, json_type_array) &&
                       i < json_object_array_length(objects);
         i++) {
      json_object *object = json_object_array_get_idx(objects, i);
      if (strcmp(str(object, "object_id"),
                 (*out)->result.bindings[b].physical.object_id))
        continue;
      json_object *visible = get(get(object, "flags"), "visible");
      (*out)->hidden[b] = json_object_is_type(visible, json_type_boolean) &&
                          !json_object_get_boolean(visible);
    }
  }
  /* Current presentation clock must have an exact integral mapping for every
     displayed frame and remain within this accepted result endpoint. */
  json_object *author = get(get(get(scene, "extensions"), "ray_tracing"),
                            "authoring"),
              *timeline = get(author, "scene_timeline");
  if (timeline) {
    TimelineDocument *document = malloc(sizeof(*document));
    uint32_t tick;
    bool valid = document &&
                 SceneTimelineDocumentFromJson(timeline, document) ==
                     TIMELINE_STATUS_OK &&
                 core_scene_rigid_result_tick(
                     &(*out)->result, document->range.frame_count > 1 ? 1 : 0,
                     0, 1, document->rate.frames_per_second_numerator,
                     document->rate.frames_per_second_denominator, &tick)
                         .code == CORE_OK &&
                 document->range.frame_count <= CORE_SCENE_RIGID_RESULT_TICKS + 1u &&
                 core_scene_rigid_result_tick(
                     &(*out)->result, (int64_t)document->range.frame_count - 1,
                     0, 1, document->rate.frames_per_second_numerator,
                     document->rate.frames_per_second_denominator, &tick)
                         .code == CORE_OK;
    free(document);
    if (!valid) {
      RuntimeRigidResultFree(*out);
      *out = NULL;
      return fail(d, n,
                  "rigid result timeline requires exact covered source ticks");
    }
  }
  return true;
}
void RuntimeRigidResultFree(RuntimeRigidResult *r) {
  if (r) {
    core_scene_rigid_result_free(&r->result);
    for (size_t i = 0; i < r->asset_count; i++)
      cJSON_Delete(r->assets[i].visible);
    free(r);
  }
}
void RuntimeRigidResultCommit(RuntimeRigidResult *r) {
  RuntimeRigidResultFree(current);
  current = r;
}
bool RuntimeRigidResultActive(void) { return current != NULL; }
bool RuntimeRigidResultClock(TimelineRate *rate, TimelineRange *range) {
  if (!current)
    return false;
  *rate = (TimelineRate){240, 1};
  *range = (TimelineRange){0, current->result.endpoint_tick + 1};
  return true;
}
json_object *RuntimeRigidResultInspection(void) {
  if (!current) return NULL;
  json_object *o=json_object_new_object(), *bodies=json_object_new_array();
  json_object_object_add(o,"result_revision",json_object_new_string(current->revision));
  json_object_object_add(o,"physical_input_sha256",json_object_new_string(current->result.physical_input_sha256));
  json_object_object_add(o,"engine_sha256",json_object_new_string(current->result.engine_sha256));
  json_object_object_add(o,"endpoint_tick",json_object_new_int64(current->result.endpoint_tick));
  json_object_object_add(o,"rate_hz",json_object_new_int(240));
  for(size_t i=0;i<current->result.body_count;i++) {
    const CoreSceneRigidBinding *b=&current->result.bindings[i].physical;
    json_object *v=json_object_new_object();
    json_object_object_add(v,"object_id",json_object_new_string(b->object_id));
    json_object_object_add(v,"body_id",json_object_new_int64(b->body_id));
    json_object_object_add(v,"role",json_object_new_string(b->role==CORE_SCENE_RIGID_DYNAMIC?"dynamic":"static"));
    json_object_object_add(v,"motion_owner",json_object_new_string(b->owner==CORE_SCENE_RIGID_SIMULATION?"simulation":"authored"));
    json_object_object_add(v,"source_asset_id",json_object_new_string(b->source_id));
    json_object_object_add(v,"mesh_asset_id",json_object_new_string(b->mesh_id));
    json_object_object_add(v,"visible",json_object_new_boolean(!current->hidden[i]));
    for(size_t k=0;k<current->asset_count;++k)if(!strcmp(current->assets[k].id,b->mesh_id))
      json_object_object_add(v,"collision_child_count",json_object_new_int64(current->assets[k].child_count));
    json_object_array_add(bodies,v);
  }
  json_object_object_add(o,"bodies",bodies);return o;
}
TimelineStatus RuntimeRigidResultCapture(const TimelineEvaluationContext *c,
                                         RayEvaluatedObjectTransform *ts,
                                         size_t capacity, size_t *count) {
  if (!current)
    return TIMELINE_STATUS_OK;
  uint32_t tick;
  if (!c || !count || c->local_frame > INT64_MAX ||
      core_scene_rigid_result_tick(&current->result, (int64_t)c->local_frame,
                                   c->sample.subframe_numerator,
                                   c->sample.subframe_denominator,
                                   c->rate.frames_per_second_numerator,
                                   c->rate.frames_per_second_denominator, &tick)
              .code != CORE_OK)
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  if (*count + current->result.body_count > capacity)
    return TIMELINE_STATUS_CAPACITY_EXCEEDED;
  for (size_t b = 0; b < current->result.body_count; b++)
    for (size_t k = 0; k < *count; k++)
      if (!strcmp(ts[k].target_id,
                  current->result.bindings[b].physical.object_id))
        return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
  for (size_t b = 0; b < current->result.body_count; b++) {
    const CoreSceneRigidPose *p =
        &current->result.poses[tick * current->result.body_count + b];
    RayEvaluatedObjectTransform t = {
        .valid = true,
        .source = RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT,
        .has_position = true,
        .has_orientation_quaternion = true,
        .source_tick = tick,
        .frame = *c};
    snprintf(t.target_id, sizeof(t.target_id), "%s",
             current->result.bindings[b].physical.object_id);
    t.position =
        (TimelineVec3){p->position_m[0], p->position_m[1], p->position_m[2]};
    t.orientation_quaternion = (RayEvaluatedQuaternion){
        p->orientation_wxyz[0], p->orientation_wxyz[1], p->orientation_wxyz[2],
        p->orientation_wxyz[3]};
    strcpy(t.rigid_result_revision, current->revision);
    strcpy(t.rigid_physical_input_sha256,
           current->result.physical_input_sha256);
    t.rigid_body_id = current->result.bindings[b].physical.body_id;
    ts[(*count)++] = t;
  }
  return TIMELINE_STATUS_OK;
}
static bool tick_at_t(double t, uint32_t *tick) {
  TimelineRate rate;
  TimelineRange range;
  uint32_t last;
  if (!isfinite(t) || t < 0 || t > 1)
    return false;
  if (RuntimeSceneTimelineClock(&rate, &range) != TIMELINE_STATUS_OK)
    RuntimeRigidResultClock(&rate, &range);
  if (core_scene_rigid_result_tick(&current->result,
                                   (int64_t)range.frame_count - 1, 0, 1,
                                   rate.frames_per_second_numerator,
                                   rate.frames_per_second_denominator, &last)
          .code != CORE_OK)
    return false;
  double x = t * last, nearest = round(x);
  if (fabs(x - nearest) > 1e-9 || nearest < 0 ||
      nearest > current->result.endpoint_tick)
    return false;
  *tick = (uint32_t)nearest;
  return true;
}
bool RuntimeRigidResultInstanceAtT(RayTracingRuntimeMeshAssetInstance *instance,
                                   double t) {
  if (!current || !instance)
    return true;
  size_t b;
  for (b = 0; b < current->result.body_count; b++)
    if (!strcmp(instance->object_id,
                current->result.bindings[b].physical.object_id))
      break;
  if (b == current->result.body_count)
    return true;
  uint32_t tick;
  if (!tick_at_t(t, &tick))
    return false;
  /* Build the pure source->world rigid affine map. Existing BLAS/TLAS and
     normal/tangent code share this instance map, with zero rotation pivot. */
  double origin[3], basis[3][3];
  if (core_scene_rigid_result_affine(&current->result, tick, b, &basis[0][0],
                                     origin)
          .code != CORE_OK)
    return false;
  instance->position_x = origin[0];
  instance->position_y = origin[1];
  instance->position_z = origin[2];
  double horizontal = hypot(basis[0][0], basis[1][0]);
  double pitch = atan2(-basis[2][0], horizontal);
  instance->rotation_y = pitch;
  if (horizontal > 1e-14) {
    instance->rotation_x = atan2(basis[2][1], basis[2][2]);
    instance->rotation_z = atan2(basis[1][0], basis[0][0]);
  } else {
    instance->rotation_x = atan2(-basis[1][2], basis[1][1]);
    instance->rotation_z = 0;
  }
  instance->rotation_pivot_policy =
      RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_AUTHORED_ORIGIN;
  instance->scale_x = instance->scale_y = instance->scale_z = 1;
  return true;
}
static bool current_mesh_digest(const char *path, const char *expected) {
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0)
    return false;
  struct stat st;
  bool ok = !fstat(fd, &st) && S_ISREG(st.st_mode) && st.st_size > 0 &&
            st.st_size <= 1024 * 1024;
  size_t n = ok ? (size_t)st.st_size : 0;
  char *bytes = ok ? malloc(n) : NULL;
  ok = ok && bytes;
  size_t pos = 0;
  while (ok && pos < n) {
    ssize_t count = read(fd, bytes + pos, n - pos);
    if (count <= 0)
      ok = false;
    else
      pos += (size_t)count;
  }
  char extra, hash[65];
  if (ok)
    ok = read(fd, &extra, 1) == 0 &&
         core_scene_compile_sha256(bytes, n, hash).code == CORE_OK &&
         !strcmp(hash, expected);
  free(bytes);
  close(fd);
  return ok;
}
bool RuntimeRigidResultMeshesMatch(const RuntimeRigidResult *r,
                                   const RayTracingRuntimeMeshAssetSet *set,
                                   char *d, size_t n) {
  if (!r)
    return true;
  if (!set)
    return fail(d, n, "rigid result requires full source mesh assets");
  for (size_t b = 0; b < r->result.body_count; b++) {
    const CoreSceneRigidBinding *binding = &r->result.bindings[b].physical;
    const RayTracingRuntimeMeshAsset *asset = NULL;
    size_t instances = 0;
    for (int i = 0; i < set->instance_count; i++)
      if (!strcmp(set->instances[i].object_id, binding->object_id) &&
          !strcmp(set->instances[i].asset_id, binding->mesh_id)) {
        int index = set->instances[i].asset_index;
        if (index < 0 || index >= set->asset_count)
          return false;
        asset = &set->assets[index];
        instances++;
      }
    const cJSON *visible = NULL;
    const char *mesh_sha = NULL;
    for (size_t i = 0; i < r->asset_count; i++)
      if (!strcmp(r->assets[i].id, binding->mesh_id)) {
        visible = r->assets[i].visible;
        mesh_sha = r->assets[i].sha256;
      }
    if (instances == 0 && r->hidden[b])
      continue;
    if (instances != 1 || !asset || !visible ||
        asset->procedural_surface_valid ||
        asset->procedural_solid_material_valid || !mesh_sha ||
        !current_mesh_digest(asset->path, mesh_sha))
      return fail(d, n,
                  "rigid result requires its accepted visible source mesh");
    const cJSON *mesh = cJSON_GetObjectItemCaseSensitive(visible, "mesh"),
                *vertices = cJSON_GetObjectItemCaseSensitive(mesh, "vertices"),
                *triangles =
                    cJSON_GetObjectItemCaseSensitive(mesh, "triangles");
    const CoreMeshAssetRuntimeDocument *doc = &asset->document;
    if ((size_t)cJSON_GetArraySize(vertices) != doc->vertex_count ||
        (size_t)cJSON_GetArraySize(triangles) != doc->triangle_count ||
        strcmp(doc->contract.asset_id, binding->mesh_id) ||
        strcmp(doc->contract.source_asset_id, binding->source_id))
      return fail(d, n, "rigid mesh identity/topology mismatch");
    for (size_t i = 0; i < doc->vertex_count; i++) {
      const cJSON *v = cJSON_GetArrayItem(vertices, (int)i);
      const double xyz[3] = {doc->vertices[i].position.x,
                             doc->vertices[i].position.y,
                             doc->vertices[i].position.z};
      const char *keys[] = {"x", "y", "z"};
      for (int k = 0; k < 3; k++) {
        const cJSON *x = cJSON_GetObjectItemCaseSensitive(v, keys[k]);
        if (!cJSON_IsNumber(x) || x->valuedouble != xyz[k])
          return fail(d, n, "rigid mesh source coordinate mismatch");
      }
    }
    for (size_t i = 0; i < doc->triangle_count; i++) {
      const cJSON *v = cJSON_GetArrayItem(triangles, (int)i);
      const size_t indices[3] = {doc->triangles[i].a, doc->triangles[i].b,
                                 doc->triangles[i].c};
      const char *keys[] = {"a", "b", "c"};
      for (int k = 0; k < 3; k++) {
        const cJSON *x = cJSON_GetObjectItemCaseSensitive(v, keys[k]);
        if (!cJSON_IsNumber(x) || x->valuedouble != indices[k])
          return fail(d, n, "rigid mesh source topology mismatch");
      }
    }
  }
  return true;
}

bool RuntimeRigidResultSnapshotMatches(const RayEvaluatedSceneSnapshot *s) {
  if (!s)
    return false;
  RayEvaluatedObjectTransform expected[RAY_RIGID_RESULT_MAX_BODIES] = {0};
  size_t count = 0;
  if (RuntimeRigidResultCapture(&s->frame, expected, RAY_RIGID_RESULT_MAX_BODIES, &count) !=
      TIMELINE_STATUS_OK)
    return false;
  size_t matched = 0;
  for (size_t i = 0; i < s->object_transform_count; i++) {
    const RayEvaluatedObjectTransform *a = &s->object_transforms[i];
    if (a->source != RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT)
      continue;
    bool found = false;
    for (size_t k = 0; k < count; k++) {
      const RayEvaluatedObjectTransform *b = &expected[k];
      if (strcmp(a->target_id, b->target_id))
        continue;
      found =
          !strcmp(a->rigid_result_revision, b->rigid_result_revision) &&
          !strcmp(a->rigid_physical_input_sha256,
                  b->rigid_physical_input_sha256) &&
          a->rigid_body_id == b->rigid_body_id &&
          a->source_tick == b->source_tick && a->position.x == b->position.x &&
          a->position.y == b->position.y && a->position.z == b->position.z &&
          a->orientation_quaternion.w == b->orientation_quaternion.w &&
          a->orientation_quaternion.x == b->orientation_quaternion.x &&
          a->orientation_quaternion.y == b->orientation_quaternion.y &&
          a->orientation_quaternion.z == b->orientation_quaternion.z;
    }
    if (!found)
      return false;
    matched++;
  }
  return matched == count;
}

bool RuntimeRigidResultValidateScene(json_object *scene, char *d, size_t n) {
  RuntimeRigidResult *r = NULL;
  bool ok = RuntimeRigidResultPrepare(
      scene, RuntimeSurfaceSamplingScenePathContext(), &r, d, n);
  RuntimeRigidResultFree(r);
  return ok;
}

#else
#include "import/runtime_rigid_result.h"
#include <stdio.h>
bool RuntimeRigidResultStrictJSON(const char *s) { return s != NULL; }
bool RuntimeRigidResultSnapshotMatches(const RayEvaluatedSceneSnapshot *s) {
  if (!s)
    return false;
  for (size_t i = 0; i < s->object_transform_count; i++)
    if (s->object_transforms[i].source ==
        RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT)
      return false;
  return true;
}
bool RuntimeRigidResultValidateReference(json_object *s, char *d, size_t n) {
  json_object *ext = NULL, *rigid = NULL, *ray = NULL, *result = NULL;
  json_object_object_get_ex(s, "extensions", &ext);
  if (ext) {
    json_object_object_get_ex(ext, "rigid_scene", &rigid);
    json_object_object_get_ex(ext, "ray_tracing", &ray);
  }
  if (ray)
    json_object_object_get_ex(ray, "rigid_result", &result);
  if (rigid || result) {
    if (d && n)
      snprintf(d, n,
               "rigid_scene execution unsupported: build requires reviewed "
               "core_scene rigid result API");
    return false;
  }
  return true;
}
bool RuntimeRigidResultValidateScene(json_object *s, char *d, size_t n) {
  return RuntimeRigidResultValidateReference(s, d, n);
}
bool RuntimeRigidResultPrepare(json_object *s, const char *p,
                               RuntimeRigidResult **o, char *d, size_t n) {
  (void)p;
  *o = NULL;
  return RuntimeRigidResultValidateReference(s, d, n);
}
bool RuntimeRigidResultMeshesMatch(const RuntimeRigidResult *r,
                                   const RayTracingRuntimeMeshAssetSet *m,
                                   char *d, size_t n) {
  (void)r;
  (void)m;
  (void)d;
  (void)n;
  return true;
}
void RuntimeRigidResultFree(RuntimeRigidResult *r) { (void)r; }
void RuntimeRigidResultCommit(RuntimeRigidResult *r) { (void)r; }
json_object *RuntimeRigidResultInspection(void) { return NULL; }
bool RuntimeRigidResultActive(void) { return false; }
bool RuntimeRigidResultClock(TimelineRate *r, TimelineRange *g) {
  (void)r;
  (void)g;
  return false;
}
TimelineStatus RuntimeRigidResultCapture(const TimelineEvaluationContext *c,
                                         RayEvaluatedObjectTransform *t,
                                         size_t n, size_t *k) {
  (void)c;
  (void)t;
  (void)n;
  (void)k;
  return TIMELINE_STATUS_OK;
}
bool RuntimeRigidResultInstanceAtT(RayTracingRuntimeMeshAssetInstance *i,
                                   double t) {
  (void)i;
  (void)t;
  return true;
}
#endif
