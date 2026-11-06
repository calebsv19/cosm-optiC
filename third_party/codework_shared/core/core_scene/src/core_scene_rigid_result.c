#include "core_scene_rigid_result.h"
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define BAD ((CoreResult){CORE_ERR_FORMAT, "rigid result admission refused"})
#define OK ((CoreResult){CORE_OK, NULL})
static const cJSON *get(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
static bool text(const cJSON *j, const char *k, const char *s) {
  const cJSON *v = get(j, k);
  return cJSON_IsString(v) && !strcmp(v->valuestring, s);
}
static bool copy(const cJSON *j, const char *k, char *s, size_t n) {
  const cJSON *v = get(j, k);
  if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring) >= n)
    return false;
  strcpy(s, v->valuestring);
  return true;
}
static bool digest(const char *s) {
  if (strlen(s) != 64)
    return false;
  for (size_t i = 0; i < 64; i++)
    if (!strchr("0123456789abcdef", s[i]))
      return false;
  return true;
}
static bool num(const cJSON *v, double lo, double hi) {
  return cJSON_IsNumber(v) && isfinite(v->valuedouble) &&
         v->valuedouble >= lo && v->valuedouble <= hi;
}
static bool integer(const cJSON *v, double lo, double hi) {
  return num(v, lo, hi) && floor(v->valuedouble) == v->valuedouble;
}
static bool array(const cJSON *v, double *o, int n) {
  if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != n)
    return false;
  for (int i = 0; i < n; i++) {
    const cJSON *x = cJSON_GetArrayItem(v, i);
    if (!num(x, -1e100, 1e100))
      return false;
    o[i] = x->valuedouble;
  }
  return true;
}
static bool equal(const double *a, const double *b, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i])
      return false;
  return true;
}
static bool quat(const cJSON *v, double o[4]) {
  return array(v, o, 4) && fabs(o[0] * o[0] + o[1] * o[1] + o[2] * o[2] +
                                o[3] * o[3] - 1) <= 1e-9;
}
static bool keys(const cJSON *v, const char *names, size_t count) {
  if (!cJSON_IsObject(v))
    return false;
  size_t n = 0;
  for (const cJSON *x = v->child; x; x = x->next) {
    char k[96];
    if (!x->string || strlen(x->string) > 80)
      return false;
    k[0] = '|';
    strcpy(k + 1, x->string);
    strcat(k, "|");
    if (!strstr(names, k))
      return false;
    n++;
  }
  return n == count;
}
static bool strict(const cJSON *v, unsigned d, size_t *n) {
  if (d > 32 || ++*n > CORE_SCENE_RIGID_RESULT_JSON_NODES)
    return false;
  if (cJSON_IsNumber(v) && !isfinite(v->valuedouble))
    return false;
  size_t fields = 0;
  for (const cJSON *x = v->child; x; x = x->next) {
    if (cJSON_IsObject(v)) {
      if (++fields > 256 || !x->string)
        return false;
      for (const cJSON *y = x->next; y; y = y->next)
        if (!strcmp(x->string, y->string))
          return false;
    }
    if (!strict(x, d + 1, n))
      return false;
  }
  return true;
}
static cJSON *parse(const char *s, size_t n) {
  if (!s || !n || n > CORE_SCENE_RIGID_RESULT_JSON_BYTES || memchr(s, 0, n))
    return NULL;
  for (size_t i = 0; i + 6 <= n; i++)
    if (!memcmp(s + i, "\\u0000", 6))
      return NULL;
  const char *end = NULL;
  cJSON *j = cJSON_ParseWithLengthOpts(s, n, &end, 0);
  size_t count = 0;
  if (!j || !strict(j, 0, &count)) {
    cJSON_Delete(j);
    return NULL;
  }
  while (end < s + n && strchr(" \n\r\t", *end))
    end++;
  if (end != s + n) {
    cJSON_Delete(j);
    return NULL;
  }
  return j;
}
void core_scene_rigid_result_free(CoreSceneRigidResult *r) {
  if (r) {
    free(r->poses);
    memset(r, 0, sizeof(*r));
  }
}
static bool binding(const cJSON *j, CoreSceneRigidResultBinding *o) {
  if (!keys(j,
            "|object_id||body_id||role||collision_asset_id||source_asset_id||"
            "mesh_asset_id||source_sha256||collision_sha256||source_center_m||"
            "principal_to_source||orientation_wxyz||linear_velocity_mps||"
            "angular_velocity_radps||position_m||mass_kg||principal_inertia_"
            "kgm2||restitution||friction||rolling_resistance_length_m||torsional_resistance_length_m|",
            18+(get(j,"rolling_resistance_length_m")!=NULL)+(get(j,"torsional_resistance_length_m")!=NULL)))
    return false;
  CoreSceneRigidBinding *b = &o->physical;
  if (!copy(j, "object_id", b->object_id, 64) ||
      !copy(j, "collision_asset_id", b->asset_id, 64) ||
      !copy(j, "source_asset_id", b->source_id, 64) ||
      !copy(j, "mesh_asset_id", b->mesh_id, 64) ||
      !copy(j, "source_sha256", b->source_sha256, 65) ||
      !copy(j, "collision_sha256", b->collision_sha256, 65) ||
      !digest(b->source_sha256) || !digest(b->collision_sha256) ||
      !integer(get(j, "body_id"), 1, INT32_MAX))
    return false;
  b->body_id = (uint32_t)get(j, "body_id")->valuedouble;
  b->role = text(j, "role", "static")    ? CORE_SCENE_RIGID_STATIC
            : text(j, "role", "dynamic") ? CORE_SCENE_RIGID_DYNAMIC
                                         : CORE_SCENE_RIGID_ROLE_UNKNOWN;
  b->owner = b->role == CORE_SCENE_RIGID_DYNAMIC ? CORE_SCENE_RIGID_SIMULATION
                                                 : CORE_SCENE_RIGID_AUTHORED;
  b->collides = true;
  if (!b->role || !array(get(j, "source_center_m"), b->source_center_m, 3) ||
      !array(get(j, "principal_to_source"), b->principal_to_source, 9) ||
      !quat(get(j, "orientation_wxyz"), b->orientation_wxyz) ||
      !array(get(j, "linear_velocity_mps"), b->linear_velocity_mps, 3) ||
      !array(get(j, "angular_velocity_radps"), b->angular_velocity_radps, 3) ||
      !array(get(j, "position_m"), o->initial_position_m, 3) ||
      !array(get(j, "principal_inertia_kgm2"), b->principal_inertia_kgm2, 3) ||
      !num(get(j, "mass_kg"), 0, 1e100) || !num(get(j, "restitution"), 0, 1) ||
      !num(get(j, "friction"), 0, 1))
    return false;
  b->mass_kg = get(j, "mass_kg")->valuedouble;
  b->restitution = get(j, "restitution")->valuedouble;
  b->friction = get(j, "friction")->valuedouble;
  const char *resistance_keys[] = {"rolling_resistance_length_m", "torsional_resistance_length_m"};
  double *resistance_values[] = {&b->rolling_resistance_length_m, &b->torsional_resistance_length_m};
  for (int k=0;k<2;k++) {
    const cJSON *v=get(j,resistance_keys[k]);
    if (v && !num(v,0,1e100)) return false;
    *resistance_values[k]=v?v->valuedouble:0;
  }
  double *m = b->principal_to_source;
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < 3; k++) {
      double x = 0;
      for (int t = 0; t < 3; t++)
        x += m[t * 3 + i] * m[t * 3 + k];
      if (fabs(x - (i == k)) > 1e-8)
        return false;
    }
  if (fabs(m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]) - 1) > 1e-8)
    return false;
  for (int k = 0; k < 3; k++)
    if (b->role == CORE_SCENE_RIGID_STATIC
            ? (b->mass_kg != 0 || b->principal_inertia_kgm2[k] != 0 ||
               b->linear_velocity_mps[k] != 0 ||
               b->angular_velocity_radps[k] != 0)
            : (b->mass_kg <= 0 || b->principal_inertia_kgm2[k] <= 0))
      return false;
  return true;
}
static bool pose(const cJSON *j, const CoreSceneRigidResultBinding *b,
                 bool first, CoreSceneRigidPose *o) {
  if (!keys(j,
            "|object_id||body_id||static||position_m||orientation_wxyz||"
            "velocity_mps||angular_velocity_radps|",
            7) ||
      !text(j, "object_id", b->physical.object_id) ||
      !integer(get(j, "body_id"), b->physical.body_id, b->physical.body_id) ||
      !cJSON_IsBool(get(j, "static")) ||
      cJSON_IsTrue(get(j, "static")) !=
          (b->physical.role == CORE_SCENE_RIGID_STATIC) ||
      !array(get(j, "position_m"), o->position_m, 3) ||
      !quat(get(j, "orientation_wxyz"), o->orientation_wxyz) ||
      !array(get(j, "velocity_mps"), o->velocity_mps, 3) ||
      !array(get(j, "angular_velocity_radps"), o->angular_velocity_radps, 3))
    return false;
  return !(first || b->physical.role == CORE_SCENE_RIGID_STATIC) ||
         (equal(o->position_m, b->initial_position_m, 3) &&
          equal(o->orientation_wxyz, b->physical.orientation_wxyz, 4) &&
          equal(o->velocity_mps, b->physical.linear_velocity_mps, 3) &&
          equal(o->angular_velocity_radps, b->physical.angular_velocity_radps,
                3));
}
static int body_index(const CoreSceneRigidResult *r, const cJSON *id) {
  if (!integer(id, 1, INT32_MAX))
    return -1;
  for (size_t i = 0; i < r->body_count; i++)
    if (r->bindings[i].physical.body_id == id->valuedouble)
      return (int)i;
  return -1;
}
CoreResult core_scene_rigid_result_read(const char *s, size_t n,
                                        CoreSceneRigidResult *out,
                                        CoreSceneRigidDigestFn hashfn) {
  CoreSceneRigidResult r = {0};
  cJSON *j = parse(s, n), *canonical = NULL;
  bool ok = false;
  if (!out || !hashfn)
    goto done;
  if (!keys(j,
            "|schema||engine_sha256||physical_input_sha256||physical_input||"
            "physical_input_canonical||endpoint_tick||frames|",
            7) ||
      !text(j, "schema", "core_rigid_motion_result_v1") ||
      !copy(j, "engine_sha256", r.engine_sha256, 65) ||
      !copy(j, "physical_input_sha256", r.physical_input_sha256, 65) ||
      !digest(r.engine_sha256) || !digest(r.physical_input_sha256))
    goto done;
  const cJSON *p = get(j, "physical_input"),
              *cs = get(j, "physical_input_canonical");
  char hash[65];
  if (!cJSON_IsString(cs) ||
      !(canonical = parse(cs->valuestring, strlen(cs->valuestring))) ||
      !cJSON_Compare(canonical, p, 1) ||
      !hashfn(cs->valuestring, strlen(cs->valuestring), hash) ||
      strcmp(hash, r.physical_input_sha256))
    goto done;
  if (!keys(p,
            "|schema||scene_id||coordinate_system||quaternion_order||rate_"
            "numerator||rate_denominator||origin_tick||end_tick||gravity_mps2||"
            "contact_policy||integration_policy||bindings|",
            11 + (get(p, "integration_policy") != NULL)) ||
      !text(p, "schema", "core_rigid_physical_input_v1") ||
      !text(p, "coordinate_system", "right_handed_z_up_meters") ||
      !text(p, "quaternion_order", "wxyz") ||
      (!text(p, "contact_policy", "min_restitution_geometric_friction_v1") &&
       !text(p,"contact_policy","min_restitution_geometric_friction_zero_cutoff_v1")) ||
      (get(p, "integration_policy") &&
       !text(p, "integration_policy", "bounded_subsegments_v1")) ||
      !copy(p, "scene_id", r.scene_id, 64) ||
      !integer(get(p, "rate_numerator"), 240, 240) ||
      !integer(get(p, "rate_denominator"), 1, 1) ||
      !integer(get(p, "origin_tick"), 0, 0) ||
      !integer(get(p, "end_tick"), 1, CORE_SCENE_RIGID_RESULT_TICKS) ||
      !array(get(p, "gravity_mps2"), r.gravity_mps2, 3))
    goto done;
  r.contact_policy = text(p,"contact_policy","min_restitution_geometric_friction_zero_cutoff_v1")
      ? CORE_SCENE_RIGID_CONTACT_ZERO_CUTOFF : CORE_SCENE_RIGID_CONTACT_DEFAULT;
  r.integration_policy = get(p, "integration_policy")
      ? CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS
      : CORE_SCENE_RIGID_INTEGRATION_DEFAULT;
  r.end_tick = (uint32_t)get(p, "end_tick")->valuedouble;
  double g = 0;
  for (int i = 0; i < 3; i++)
    g += r.gravity_mps2[i] * r.gravity_mps2[i];
  if (g > 2500 + 1e-9)
    goto done;
  const cJSON *bs = get(p, "bindings"), *fs = get(j, "frames");
  if (!cJSON_IsArray(bs) || cJSON_GetArraySize(bs) < 1 ||
      cJSON_GetArraySize(bs) > (int)CORE_SCENE_RIGID_RESULT_BODIES ||
      !integer(get(j, "endpoint_tick"), 1, r.end_tick))
    goto done;
  r.body_count = (size_t)cJSON_GetArraySize(bs);
  r.endpoint_tick = (uint32_t)get(j, "endpoint_tick")->valuedouble;
  for (size_t b = 0; b < r.body_count; b++) {
    if (!binding(cJSON_GetArrayItem(bs, (int)b), &r.bindings[b]))
      goto done;
    for (size_t k = 0; k < b; k++)
      if (r.bindings[k].physical.body_id >= r.bindings[b].physical.body_id ||
          !strcmp(r.bindings[k].physical.object_id,
                  r.bindings[b].physical.object_id))
        goto done;
  }
  if (!cJSON_IsArray(fs) ||
      cJSON_GetArraySize(fs) != (int)r.endpoint_tick + 1 ||
      !(r.poses =
            calloc((r.endpoint_tick + 1) * r.body_count, sizeof(*r.poses))))
    goto done;
  for (uint32_t tick = 0; tick <= r.endpoint_tick; tick++) {
    const cJSON *f = cJSON_GetArrayItem(fs, (int)tick),
                *states = get(f, "bodies"), *contacts = get(f, "contacts");
    if (!keys(f,
              "|tick||time_s||bodies||constraints_solved||impulse_kg_mps||"
              "maximum_penetration_m||contacts|",
              7) ||
        !integer(get(f, "tick"), tick, tick) ||
        !num(get(f, "time_s"), (double)tick / 240, (double)tick / 240) ||
        !cJSON_IsArray(states) ||
        cJSON_GetArraySize(states) != (int)r.body_count ||
        !integer(get(f, "constraints_solved"), 0, CORE_SCENE_RIGID_RESULT_CONSTRAINTS) ||
        !num(get(f, "impulse_kg_mps"), 0, 1e100) ||
        !num(get(f, "maximum_penetration_m"), 0, 1e100) ||
        !cJSON_IsArray(contacts) ||
        cJSON_GetArraySize(contacts) !=
            get(f, "constraints_solved")->valuedouble)
      goto done;
    for (size_t b = 0; b < r.body_count; b++)
      if (!pose(cJSON_GetArrayItem(states, (int)b), &r.bindings[b], tick == 0,
                &r.poses[tick * r.body_count + b]))
        goto done;
    bool seen[CORE_SCENE_RIGID_RESULT_BODIES][CORE_SCENE_RIGID_RESULT_BODIES][4] = {0};
    for (const cJSON *c = contacts->child; c; c = c->next) {
      int a = body_index(&r, get(c, "body_a_id")),
          b = body_index(&r, get(c, "body_b_id"));
      if (!keys(c,
                "|body_a_id||body_b_id||cluster||points||normal_impulse_kg_mps|"
                "|tangent_impulse_kg_mps||maximum_penetration_m|",
                7) ||
          a < 0 || b <= a || !integer(get(c, "cluster"), 0, 3) ||
          !integer(get(c, "points"), 1, 4) ||
          !num(get(c, "normal_impulse_kg_mps"), 0, 1e100) ||
          !num(get(c, "tangent_impulse_kg_mps"), 0, 1e100) ||
          !num(get(c, "maximum_penetration_m"), 0, 1e100))
        goto done;
      int cluster = (int)get(c, "cluster")->valuedouble;
      if (seen[a][b][cluster])
        goto done;
      seen[a][b][cluster] = true;
    }
  }
  *out = r;
  memset(&r, 0, sizeof(r));
  ok = true;
done:
  cJSON_Delete(j);
  cJSON_Delete(canonical);
  core_scene_rigid_result_free(&r);
  return ok ? OK : BAD;
}
CoreResult core_scene_rigid_result_match(const CoreSceneRigidResult *r,
                                         const CoreSceneRigidContract *c,
                                         CoreSceneRigidDigestFn hashfn) {
  if (!r || !c || !r->poses || !hashfn ||
      core_scene_rigid_validate(c, (CoreSceneRigidLimits){CORE_SCENE_RIGID_RESULT_BODIES, CORE_SCENE_RIGID_RESULT_PAIRS, CORE_SCENE_RIGID_RESULT_CHILDREN}, NULL)
              .code != CORE_OK ||
      c->rate_numerator != 240 || c->rate_denominator != 1 ||
      c->origin_tick != 0 || r->integration_policy != c->integration_policy)
    return BAD;
  /* Reproduce this profile's native producer projection, including its chosen
     round-trip serialization. Comparing parsed doubles with a tolerance would
     silently admit edits; comparing supplied hash strings alone would miss
     edits. cJSON's printer may choose 15 digits within its round-trip
     comparison, so direct equality against reparsed cache numbers is also not
     equivalent. */
  cJSON *p = cJSON_CreateObject(), *rows = cJSON_CreateArray();
  if (!p || !rows) {
    cJSON_Delete(p);
    cJSON_Delete(rows);
    return BAD;
  }
  cJSON_AddStringToObject(p, "schema", "core_rigid_physical_input_v1");
  cJSON_AddStringToObject(p, "scene_id", c->scene_id);
  cJSON_AddStringToObject(p, "coordinate_system", "right_handed_z_up_meters");
  cJSON_AddStringToObject(p, "quaternion_order", "wxyz");
  cJSON_AddNumberToObject(p, "rate_numerator", c->rate_numerator);
  cJSON_AddNumberToObject(p, "rate_denominator", c->rate_denominator);
  cJSON_AddNumberToObject(p, "origin_tick", 0);
  cJSON_AddNumberToObject(p, "end_tick", c->end_tick);
  cJSON_AddItemToObject(p, "gravity_mps2",
                        cJSON_CreateDoubleArray(c->gravity_mps2, 3));
  cJSON_AddStringToObject(p, "contact_policy",
                          c->contact_policy == CORE_SCENE_RIGID_CONTACT_ZERO_CUTOFF
                            ? "min_restitution_geometric_friction_zero_cutoff_v1"
                            : "min_restitution_geometric_friction_v1");
  if (c->integration_policy == CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS)
    cJSON_AddStringToObject(p, "integration_policy", "bounded_subsegments_v1");
  cJSON_AddItemToObject(p, "bindings", rows);
  size_t count = 0;
  for (size_t rank = 0; rank < r->body_count; rank++) {
    const CoreSceneRigidBinding *v = NULL;
    const CoreSceneRigidObject *o = NULL;
    for (size_t k = 0; k < c->binding_count; k++)
      if (c->bindings[k].body_id == r->bindings[rank].physical.body_id)
        v = &c->bindings[k];
    if (!v) {
      cJSON_Delete(p);
      return BAD;
    }
    for (size_t k = 0; k < c->object_count; k++)
      if (!strcmp(c->objects[k].id, v->object_id))
        o = &c->objects[k];
    if (!o) {
      cJSON_Delete(p);
      return BAD;
    }
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "object_id", v->object_id);
    cJSON_AddNumberToObject(j, "body_id", v->body_id);
    cJSON_AddStringToObject(
        j, "role", v->role == CORE_SCENE_RIGID_STATIC ? "static" : "dynamic");
    cJSON_AddStringToObject(j, "collision_asset_id", v->asset_id);
    cJSON_AddStringToObject(j, "source_asset_id", v->source_id);
    cJSON_AddStringToObject(j, "mesh_asset_id", v->mesh_id);
    cJSON_AddStringToObject(j, "source_sha256", v->source_sha256);
    cJSON_AddStringToObject(j, "collision_sha256", v->collision_sha256);
    cJSON_AddItemToObject(j, "source_center_m",
                          cJSON_CreateDoubleArray(v->source_center_m, 3));
    cJSON_AddItemToObject(j, "principal_to_source",
                          cJSON_CreateDoubleArray(v->principal_to_source, 9));
    cJSON_AddItemToObject(j, "orientation_wxyz",
                          cJSON_CreateDoubleArray(v->orientation_wxyz, 4));
    cJSON_AddItemToObject(j, "linear_velocity_mps",
                          cJSON_CreateDoubleArray(v->linear_velocity_mps, 3));
    cJSON_AddItemToObject(
        j, "angular_velocity_radps",
        cJSON_CreateDoubleArray(v->angular_velocity_radps, 3));
    cJSON_AddItemToObject(j, "position_m",
                          cJSON_CreateDoubleArray(o->position, 3));
    cJSON_AddNumberToObject(j, "mass_kg", v->mass_kg);
    cJSON_AddItemToObject(
        j, "principal_inertia_kgm2",
        cJSON_CreateDoubleArray(v->principal_inertia_kgm2, 3));
    cJSON_AddNumberToObject(j, "restitution", v->restitution);
    cJSON_AddNumberToObject(j, "friction", v->friction);
    if (v->rolling_resistance_length_m > 0) cJSON_AddNumberToObject(j,"rolling_resistance_length_m",v->rolling_resistance_length_m);
    if (v->torsional_resistance_length_m > 0) cJSON_AddNumberToObject(j,"torsional_resistance_length_m",v->torsional_resistance_length_m);
    cJSON_AddItemToArray(rows, j);
  }
  for (size_t k = 0; k < c->binding_count; k++)
    if (c->bindings[k].role != CORE_SCENE_RIGID_VISUAL_ONLY)
      count++;
  char *bytes = cJSON_PrintUnformatted(p), hash[65];
  bool ok = count == r->body_count && bytes &&
            hashfn(bytes, strlen(bytes), hash) &&
            !strcmp(hash, r->physical_input_sha256);
  free(bytes);
  cJSON_Delete(p);
  return ok ? OK : BAD;
}
CoreResult core_scene_rigid_result_tick(const CoreSceneRigidResult *r,
                                        int64_t f, uint32_t sn, uint32_t sd,
                                        uint32_t fn, uint32_t fd,
                                        uint32_t *tick) {
  if (!r || !r->poses || !tick || f < 0 || !sd || sn >= sd || !fn || !fd)
    return BAD;
  /* Bounded arithmetic, exact integer division. No double rounding of seeks. */
  if ((uint64_t)f > UINT64_MAX / sd)
    return BAD;
  uint64_t n = (uint64_t)f * sd;
  if (n > UINT64_MAX - sn)
    return BAD;
  n += sn;
  if (n > UINT64_MAX / 240 || n * 240 > UINT64_MAX / fd)
    return BAD;
  n = n * 240 * fd;
  uint64_t d = (uint64_t)sd * fn;
  if (n % d || n / d > r->endpoint_tick)
    return BAD;
  *tick = (uint32_t)(n / d);
  return OK;
}
CoreResult core_scene_rigid_result_vertex(const CoreSceneRigidResult *r,
                                          uint32_t t, size_t b,
                                          const double s[3], double out[3]) {
  if (!r || !r->poses || !s || !out || t > r->endpoint_tick ||
      b >= r->body_count)
    return BAD;
  const CoreSceneRigidBinding *v = &r->bindings[b].physical;
  const CoreSceneRigidPose *p = &r->poses[t * r->body_count + b];
  double x[3] = {0}, y[3], q = p->orientation_wxyz[0],
         a = p->orientation_wxyz[1], c = p->orientation_wxyz[2],
         d = p->orientation_wxyz[3];
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < 3; k++)
      x[i] +=
          v->principal_to_source[k * 3 + i] * (s[k] - v->source_center_m[k]);
  double m[9] = {1 - 2 * (c * c + d * d), 2 * (a * c - d * q),
                 2 * (a * d + c * q),     2 * (a * c + d * q),
                 1 - 2 * (a * a + d * d), 2 * (c * d - a * q),
                 2 * (a * d - c * q),     2 * (c * d + a * q),
                 1 - 2 * (a * a + c * c)};
  for (int i = 0; i < 3; i++) {
    y[i] = p->position_m[i];
    for (int k = 0; k < 3; k++)
      y[i] += m[i * 3 + k] * x[k];
    if (!isfinite(y[i]))
      return BAD;
  }
  memcpy(out, y, sizeof(y));
  return OK;
}

CoreResult core_scene_rigid_result_affine(const CoreSceneRigidResult *r,
                                          uint32_t t, size_t b,
                                          double rotation[9],
                                          double translation[3]) {
  if (!r || !r->poses || !rotation || !translation || t > r->endpoint_tick ||
      b >= r->body_count)
    return BAD;
  const CoreSceneRigidBinding *v = &r->bindings[b].physical;
  const CoreSceneRigidPose *p = &r->poses[t * r->body_count + b];
  double q = p->orientation_wxyz[0], a = p->orientation_wxyz[1],
         c = p->orientation_wxyz[2], d = p->orientation_wxyz[3];
  double m[9] = {1 - 2 * (c * c + d * d), 2 * (a * c - d * q),
                 2 * (a * d + c * q),     2 * (a * c + d * q),
                 1 - 2 * (a * a + d * d), 2 * (c * d - a * q),
                 2 * (a * d - c * q),     2 * (c * d + a * q),
                 1 - 2 * (a * a + c * c)};
  for (int i = 0; i < 3; i++) {
    translation[i] = p->position_m[i];
    for (int k = 0; k < 3; k++) {
      rotation[i * 3 + k] = 0;
      for (int j = 0; j < 3; j++)
        rotation[i * 3 + k] += m[i * 3 + j] * v->principal_to_source[k * 3 + j];
      translation[i] -= rotation[i * 3 + k] * v->source_center_m[k];
    }
    if (!isfinite(translation[i]))
      return BAD;
  }
  return OK;
}
