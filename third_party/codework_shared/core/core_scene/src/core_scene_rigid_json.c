#include "core_scene_rigid_json.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JSON_LIMIT (1024u * 1024u)
static const cJSON *get(const cJSON *j, const char *key) {
  return cJSON_GetObjectItemCaseSensitive(j, key);
}
static bool text(const cJSON *j, const char *key, const char *s) {
  const cJSON *v = get(j, key);
  return cJSON_IsString(v) && !strcmp(v->valuestring, s);
}
static bool copy(const cJSON *j, const char *key, char *out, size_t n) {
  const cJSON *v = get(j, key);
  if (!cJSON_IsString(v) || strlen(v->valuestring) >= n)
    return false;
  strcpy(out, v->valuestring);
  return true;
}
static bool num(const cJSON *j, const char *key, double *out) {
  const cJSON *v = get(j, key);
  if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble))
    return false;
  *out = v->valuedouble;
  return true;
}
static bool uint32(const cJSON *j, const char *key, uint32_t *out) {
  double x;
  if (!num(j, key, &x) || x < 0 || x > UINT32_MAX || floor(x) != x)
    return false;
  *out = (uint32_t)x;
  return true;
}
static bool boolean(const cJSON *j, const char *key, bool *out) {
  const cJSON *v = get(j, key);
  if (!cJSON_IsBool(v))
    return false;
  *out = cJSON_IsTrue(v);
  return true;
}
static bool array(const cJSON *v, double *out, size_t n) {
  if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != (int)n)
    return false;
  for (size_t i = 0; i < n; ++i) {
    const cJSON *x = cJSON_GetArrayItem(v, (int)i);
    if (!cJSON_IsNumber(x) || !isfinite(x->valuedouble))
      return false;
    out[i] = x->valuedouble;
  }
  return true;
}
static bool xyz(const cJSON *v, double out[3]) {
  return cJSON_IsObject(v) && num(v, "x", out) && num(v, "y", out + 1) &&
         num(v, "z", out + 2);
}
static bool keys(const cJSON *v, const char *allowed) {
  if (!cJSON_IsObject(v))
    return false;
  for (const cJSON *x = v->child; x; x = x->next) {
    char needle[96];
    if (!x->string ||
        snprintf(needle, sizeof(needle), "|%s|", x->string) >=
            (int)sizeof(needle) ||
        !strstr(allowed, needle))
      return false;
  }
  return true;
}
static bool unambiguous(const cJSON *v, unsigned depth, size_t *nodes) {
  if (depth > 32 || ++*nodes > 32768)
    return false;
  if (cJSON_IsNumber(v) && !isfinite(v->valuedouble))
    return false;
  size_t fields = 0;
  for (const cJSON *x = v->child; x; x = x->next) {
    if (cJSON_IsObject(v) && ++fields > 256)
      return false;
    if (cJSON_IsObject(v)) {
      if (!x->string)
        return false;
      for (const cJSON *y = x->next; y; y = y->next)
        if (y->string && !strcmp(x->string, y->string))
          return false;
    }
    if (!unambiguous(x, depth + 1, nodes))
      return false;
  }
  return true;
}
static cJSON *parse(const char *s, size_t n) {
  if (!s || !n || n > JSON_LIMIT || memchr(s, 0, n))
    return NULL;
  /* cJSON C strings cannot faithfully represent escaped NUL. Refuse it before
     decoding, including keys, rather than truncating an identity. */
  for (size_t i = 0; i + 6 <= n; ++i)
    if (!memcmp(s + i, "\\u0000", 6))
      return NULL;
  const char *end = NULL;
  cJSON *j = cJSON_ParseWithLengthOpts(s, n, &end, 0);
  size_t count = 0;
  if (!j || !cJSON_IsObject(j) || !unambiguous(j, 0, &count)) {
    cJSON_Delete(j);
    return NULL;
  }
  while (end < s + n &&
         (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
    ++end;
  if (end != s + n) {
    cJSON_Delete(j);
    return NULL;
  }
  return j;
}
static bool asset(const cJSON *j, CoreSceneRigidAsset *a) {
  if (!keys(j, "|asset_id||source_asset_id||mesh_asset_id||collision_sha256||"
               "source_sha256||mesh_sha256||shape||child_count||source_center_"
               "m||principal_to_source||inertia_per_kg|"))
    return false;
  a->shape =
      text(j, "shape", "convex_compound")    ? CORE_SCENE_RIGID_CONVEX_COMPOUND
      : text(j, "shape", "analytic_sphere") ? CORE_SCENE_RIGID_ANALYTIC_SPHERE
      : text(j, "shape", "static_triangles") ? CORE_SCENE_RIGID_STATIC_TRIANGLES
                                             : CORE_SCENE_RIGID_SHAPE_UNKNOWN;
  return copy(j, "asset_id", a->id, 64) &&
         copy(j, "source_asset_id", a->source_id, 64) &&
         copy(j, "mesh_asset_id", a->mesh_id, 64) &&
         copy(j, "collision_sha256", a->collision_sha256, 65) &&
         copy(j, "source_sha256", a->source_sha256, 65) &&
         copy(j, "mesh_sha256", a->mesh_sha256, 65) &&
         uint32(j, "child_count", &a->child_count) &&
         array(get(j, "source_center_m"), a->source_center_m, 3) &&
         array(get(j, "principal_to_source"), a->principal_to_source, 9) &&
         array(get(j, "inertia_per_kg"), a->inertia_per_kg, 3);
}
static bool binding(const cJSON *j, CoreSceneRigidBinding *b) {
  if (!keys(j,
            "|object_id||body_id||role||motion_owner||collides||collision_"
            "asset_id||source_asset_id||mesh_asset_id||collision_sha256||"
            "source_sha256||mesh_sha256||orientation_wxyz||linear_velocity_mps|"
            "|angular_velocity_radps||source_center_m||principal_to_source||"
            "mass_kg||principal_inertia_kgm2||restitution||friction||rolling_resistance_length_m||torsional_resistance_length_m|"))
    return false;
  b->role = text(j, "role", "visual_only") ? CORE_SCENE_RIGID_VISUAL_ONLY
            : text(j, "role", "static")    ? CORE_SCENE_RIGID_STATIC
            : text(j, "role", "dynamic")   ? CORE_SCENE_RIGID_DYNAMIC
            : text(j, "role", "kinematic") ? CORE_SCENE_RIGID_KINEMATIC
                                           : CORE_SCENE_RIGID_ROLE_UNKNOWN;
  b->owner =
      text(j, "motion_owner", "authored")     ? CORE_SCENE_RIGID_AUTHORED
      : text(j, "motion_owner", "simulation") ? CORE_SCENE_RIGID_SIMULATION
      : text(j, "motion_owner", "prescribed") ? CORE_SCENE_RIGID_PRESCRIBED
                                              : CORE_SCENE_RIGID_OWNER_UNKNOWN;
  if (!copy(j, "object_id", b->object_id, 64) ||
      !uint32(j, "body_id", &b->body_id) ||
      !boolean(j, "collides", &b->collides))
    return false;
  if (b->role == CORE_SCENE_RIGID_VISUAL_ONLY)
    return keys(j, "|object_id||body_id||role||motion_owner||collides|");
  return copy(j, "collision_asset_id", b->asset_id, 64) &&
         copy(j, "source_asset_id", b->source_id, 64) &&
         copy(j, "mesh_asset_id", b->mesh_id, 64) &&
         copy(j, "collision_sha256", b->collision_sha256, 65) &&
         copy(j, "source_sha256", b->source_sha256, 65) &&
         copy(j, "mesh_sha256", b->mesh_sha256, 65) &&
         array(get(j, "orientation_wxyz"), b->orientation_wxyz, 4) &&
         array(get(j, "linear_velocity_mps"), b->linear_velocity_mps, 3) &&
         array(get(j, "angular_velocity_radps"), b->angular_velocity_radps,
               3) &&
         array(get(j, "source_center_m"), b->source_center_m, 3) &&
         array(get(j, "principal_to_source"), b->principal_to_source, 9) &&
         num(j, "mass_kg", &b->mass_kg) &&
         array(get(j, "principal_inertia_kgm2"), b->principal_inertia_kgm2,
               3) &&
         num(j, "restitution", &b->restitution) &&
         num(j, "friction", &b->friction) &&
         (!get(j,"rolling_resistance_length_m") || num(j,"rolling_resistance_length_m",&b->rolling_resistance_length_m)) &&
         (!get(j,"torsional_resistance_length_m") || num(j,"torsional_resistance_length_m",&b->torsional_resistance_length_m));
}
static bool enabled(const cJSON *j, bool *out) {
  const cJSON *v = get(j, "enabled");
  if (!v) {
    *out = true;
    return true;
  }
  return boolean(j, "enabled", out);
}
static bool claims(const cJSON *scene, CoreSceneRigidContract *c) {
  const cJSON *extensions = get(scene, "extensions"),
              *author = get(get(extensions, "ray_tracing"), "authoring");
  const cJSON *paths = get(get(author, "motion_paths"), "bindings"),
              *motions = get(author, "object_motion_tracks"),
              *tracks = get(get(author, "scene_timeline"), "tracks");
  const cJSON *lists[3] = {paths, motions, tracks};
  for (unsigned k = 0; k < 3; ++k) {
    if (lists[k] && !cJSON_IsArray(lists[k]))
      return false;
    for (const cJSON *j = lists[k] ? lists[k]->child : NULL; j; j = j->next) {
      bool on;
      if (!cJSON_IsObject(j) || !enabled(j, &on))
        return false;
      if (!on)
        continue;
      const cJSON *id = get(j, k == 2 ? "target_id" : "object_id");
      if (k == 2) {
        const cJSON *property = get(j, "property_id");
        if (!cJSON_IsString(property))
          return false;
        if (strncmp(property->valuestring, "object/transform/", 17) &&
            strcmp(property->valuestring, "object/path_progress"))
          continue;
      }
      if (k == 0 && id && get(j, "target_id"))
        return false;
      if (!id) {
        const cJSON *target = get(j, "target_id");
        if (k == 0 && cJSON_IsString(target) &&
            (!strcmp(target->valuestring, "camera/main") ||
             (!strncmp(target->valuestring, "light/", 6) &&
              target->valuestring[6])))
          continue;
        return false;
      }
      if (!cJSON_IsString(id))
        return false;
      const char *name = id->valuestring;
      if (k == 2) {
        if (strncmp(name, "object/", 7))
          continue;
        name += 7;
      }
      for (size_t i = 0; i < c->object_count; ++i)
        if (!strcmp(name, c->objects[i].id))
          c->objects[i].transform_claims |= k == 2
                                                ? CORE_SCENE_RIGID_TRACK_CLAIM
                                                : CORE_SCENE_RIGID_PATH_CLAIM;
    }
  }
  /* The finite Ball producer is a second simulation owner. Never blend it
     implicitly with the new generalized physical declaration. */
  const cJSON *finite_bodies =
      get(get(extensions, "ball_bounce_sim"), "bodies");
  if (finite_bodies && !cJSON_IsArray(finite_bodies))
    return false;
  for (const cJSON *j = finite_bodies ? finite_bodies->child : NULL; j;
       j = j->next) {
    const cJSON *id = get(j, "object_id");
    if (!cJSON_IsString(id))
      return false;
    for (size_t i = 0; i < c->object_count; ++i)
      if (!strcmp(id->valuestring, c->objects[i].id))
        c->objects[i].transform_claims |= CORE_SCENE_RIGID_OTHER_SIM_CLAIM;
  }
  return true;
}
CoreResult core_scene_rigid_json_read(const char *scene, size_t n,
                                      const char *catalog, size_t m,
                                      CoreSceneRigidLimits limits,
                                      CoreSceneRigidContract *out,
                                      CoreSceneRigidDiagnostic *diag) {
  CoreResult result = {CORE_ERR_FORMAT, "rigid_json"};
  cJSON *s = NULL, *a = NULL;
  CoreSceneRigidContract *c = NULL;
  if (diag) {
    memset(diag, 0, sizeof(*diag));
    snprintf(diag->code, sizeof(diag->code), "rigid_json");
    snprintf(diag->message, sizeof(diag->message),
             "invalid, ambiguous or unsupported rigid-scene JSON");
  }
  if (!out)
    return (CoreResult){CORE_ERR_INVALID_ARG, "output required"};
  s = parse(scene, n);
  a = parse(catalog, m);
  c = calloc(1, sizeof(*c));
  if (!s || !a || !c)
    goto done;
  const cJSON *root = get(get(s, "extensions"), "rigid_scene"),
              *objects = get(s, "objects"), *assets = get(a, "assets"),
              *bindings = get(root, "bindings");
  if (!copy(s, "scene_id", c->scene_id, 64) ||
      !text(s, "schema_family", "codework_scene") ||
      (!text(s, "schema_variant", "scene_authoring_v1") &&
       !text(s, "schema_variant", "scene_runtime_v1")) ||
      !text(s, "space_mode_default", "3d") || !text(s, "unit_system", "meters"))
    goto done;
  const cJSON *hierarchy = get(s, "hierarchy"),
              *constraints = get(s, "constraints");
  if ((hierarchy &&
       (!cJSON_IsArray(hierarchy) || cJSON_GetArraySize(hierarchy))) ||
      (constraints &&
       (!cJSON_IsArray(constraints) || cJSON_GetArraySize(constraints))))
    goto done;
  double scale, version;
  if (!num(s, "schema_version", &version) || version != 1 ||
      !num(s, "world_scale", &scale) || scale != 1)
    goto done;
  if (!keys(root,
            "|schema||coordinate_system||rate_numerator||rate_denominator||"
            "origin_tick||end_tick||gravity_mps2||contact_policy||integration_policy||bindings|") ||
      !text(root, "schema", CORE_SCENE_RIGID_SCHEMA) ||
      !text(root, "coordinate_system", "right_handed_z_up_meters") ||
      (!text(root, "contact_policy", "min_restitution_geometric_friction_v1") &&
       !text(root, "contact_policy", "min_restitution_geometric_friction_zero_cutoff_v1")) ||
      (get(root, "integration_policy") &&
       !text(root, "integration_policy", "bounded_subsegments_v1")) ||
      !uint32(root, "rate_numerator", &c->rate_numerator) ||
      !uint32(root, "rate_denominator", &c->rate_denominator) ||
      !uint32(root, "origin_tick", &c->origin_tick) ||
      !uint32(root, "end_tick", &c->end_tick) ||
      !array(get(root, "gravity_mps2"), c->gravity_mps2, 3))
    goto done;
  if (!keys(a, "|schema||assets|") ||
      !text(a, "schema", "core_rigid_asset_metadata_v1") ||
      !cJSON_IsArray(objects) || !cJSON_IsArray(assets) ||
      !cJSON_IsArray(bindings))
    goto done;
  c->contact_policy = text(root,"contact_policy","min_restitution_geometric_friction_zero_cutoff_v1")
      ? CORE_SCENE_RIGID_CONTACT_ZERO_CUTOFF : CORE_SCENE_RIGID_CONTACT_DEFAULT;
  c->integration_policy = get(root, "integration_policy")
      ? CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS
      : CORE_SCENE_RIGID_INTEGRATION_DEFAULT;
  c->schema_version = 1;
  c->object_count = (size_t)cJSON_GetArraySize(objects);
  c->asset_count = (size_t)cJSON_GetArraySize(assets);
  c->binding_count = (size_t)cJSON_GetArraySize(bindings);
  if (c->object_count > CORE_SCENE_RIGID_MAX_RECORDS ||
      c->asset_count > CORE_SCENE_RIGID_MAX_RECORDS ||
      c->binding_count > CORE_SCENE_RIGID_MAX_RECORDS)
    goto done;
  for (size_t i = 0; i < c->object_count; ++i) {
    const cJSON *j = cJSON_GetArrayItem(objects, (int)i);
    CoreSceneRigidObject *o = &c->objects[i];
    const cJSON *t = get(j, "transform");
    if (!copy(j, "object_id", o->id, 64))
      goto done;
    o->scale[0] = o->scale[1] = o->scale[2] = 1;
    if (t && (!xyz(get(t, "position"), o->position) ||
              !xyz(get(t, "rotation"), o->rotation_deg) ||
              !xyz(get(t, "scale"), o->scale)))
      goto done;
    if (get(j, "parent_id") && !copy(j, "parent_id", o->parent_id, 64))
      goto done;
    o->mesh_instance = text(j, "object_type", "mesh_asset_instance");
    if (o->mesh_instance &&
        (!text(get(j, "geometry_ref"), "kind", "mesh_asset") ||
         !copy(get(j, "geometry_ref"), "id", o->mesh_id, 64)))
      goto done;
  }
  for (size_t i = 0; i < c->asset_count; ++i)
    if (!asset(cJSON_GetArrayItem(assets, (int)i), &c->assets[i]))
      goto done;
  for (size_t i = 0; i < c->binding_count; ++i)
    if (!binding(cJSON_GetArrayItem(bindings, (int)i), &c->bindings[i]))
      goto done;
  if (!claims(s, c))
    goto done;
  result = core_scene_rigid_validate(c, limits, diag);
  if (result.code == CORE_OK)
    *out = *c;
done:
  free(c);
  cJSON_Delete(s);
  cJSON_Delete(a);
  return result;
}
