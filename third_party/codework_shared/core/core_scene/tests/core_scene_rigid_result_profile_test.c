#include "cJSON.h"
#include "core_scene_rigid_result.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Deterministic digest stand-in isolates result-match structural admission.
 * Real digest identity and coordinate matching are covered by sealed consumer
 * tests. */
static char *projection;
static bool digest(const void *p, size_t n, char hex[65]) {
  assert(p && n);
  free(projection);
  projection = malloc(n + 1);
  assert(projection);
  memcpy(projection, p, n);
  projection[n] = 0;
  memset(hex, strstr(projection, "bounded_subsegments_v1") ? 'b' : 'a', 64);
  hex[64] = 0;
  return true;
}
static void seal(cJSON *cache) {
  char *text = cJSON_PrintUnformatted(
      cJSON_GetObjectItemCaseSensitive(cache, "physical_input"));
  assert(text);
  char hash[65];
  digest(text, strlen(text), hash);
  cJSON_ReplaceItemInObjectCaseSensitive(cache, "physical_input_canonical",
                                         cJSON_CreateString(text));
  cJSON_ReplaceItemInObjectCaseSensitive(cache, "physical_input_sha256",
                                         cJSON_CreateString(hash));
  free(text);
}
static bool admits(cJSON *cache, CoreSceneRigidIntegrationPolicy expected) {
  char *text = cJSON_PrintUnformatted(cache);
  assert(text);
  CoreSceneRigidResult out = {0};
  bool ok =
      core_scene_rigid_result_read(text, strlen(text), &out, digest).code ==
      CORE_OK;
  if (ok)
    assert(out.integration_policy == expected);
  core_scene_rigid_result_free(&out);
  free(text);
  return ok;
}
int main(void) {
  CoreSceneRigidContract c = {0};
  strcpy(c.scene_id, "compound");
  c.schema_version = 1;
  c.rate_numerator = 240;
  c.rate_denominator = 1;
  c.end_tick = 1;
  c.object_count = c.asset_count = c.binding_count = 1;
  CoreSceneRigidObject *o = &c.objects[0];
  strcpy(o->id, "body");
  strcpy(o->mesh_id, "mesh");
  o->mesh_instance = true;
  o->scale[0] = o->scale[1] = o->scale[2] = 1;
  CoreSceneRigidAsset *a = &c.assets[0];
  strcpy(a->id, "collision");
  strcpy(a->source_id, "source");
  strcpy(a->mesh_id, "mesh");
  memset(a->source_sha256, 'a', 64);
  memset(a->collision_sha256, 'b', 64);
  memset(a->mesh_sha256, 'c', 64);
  a->shape = CORE_SCENE_RIGID_CONVEX_COMPOUND;
  a->principal_to_source[0] = a->principal_to_source[4] =
      a->principal_to_source[8] = 1;
  a->inertia_per_kg[0] = a->inertia_per_kg[1] = a->inertia_per_kg[2] = 1;
  CoreSceneRigidBinding *b = &c.bindings[0];
  strcpy(b->object_id, "body");
  strcpy(b->asset_id, a->id);
  strcpy(b->source_id, a->source_id);
  strcpy(b->mesh_id, a->mesh_id);
  strcpy(b->source_sha256, a->source_sha256);
  strcpy(b->collision_sha256, a->collision_sha256);
  strcpy(b->mesh_sha256, a->mesh_sha256);
  memcpy(b->principal_to_source, a->principal_to_source,
         sizeof(b->principal_to_source));
  b->body_id = 17;
  b->role = CORE_SCENE_RIGID_DYNAMIC;
  b->owner = CORE_SCENE_RIGID_SIMULATION;
  b->collides = true;
  b->mass_kg = 1;
  b->principal_inertia_kgm2[0] = b->principal_inertia_kgm2[1] =
      b->principal_inertia_kgm2[2] = 1;
  b->orientation_wxyz[0] = 1;
  CoreSceneRigidPose pose = {0};
  CoreSceneRigidResult r = {0};
  r.poses = &pose;
  r.body_count = 1;
  r.bindings[0].physical.body_id = 17;
  memset(r.physical_input_sha256, 'a', 64);
  for (unsigned children = 1; children <= 65; ++children) {
    a->child_count = children;
    CoreSceneRigidContract before = c;
    bool ok = core_scene_rigid_result_match(&r, &c, digest).code == CORE_OK;
    assert(ok == (children <= 64));
    assert(!memcmp(&before, &c, sizeof(c)));
  }
  a->child_count = 64;
  assert(core_scene_rigid_result_match(&r, &c, digest).code == CORE_OK);
  char *legacy = malloc(strlen(projection) + 1);
  assert(legacy);
  strcpy(legacy, projection);
  c.integration_policy = CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS;
  assert(core_scene_rigid_result_match(&r, &c, digest).code != CORE_OK);
  r.integration_policy = c.integration_policy;
  memset(r.physical_input_sha256, 'b', 64);
  assert(core_scene_rigid_result_match(&r, &c, digest).code == CORE_OK);
  assert(
      strstr(projection, "\"integration_policy\":\"bounded_subsegments_v1\""));
  cJSON *cache = cJSON_CreateObject(), *physical = cJSON_Parse(projection),
        *frames = cJSON_CreateArray();
  assert(cache && physical && frames);
  cJSON_AddStringToObject(cache, "schema", "core_rigid_motion_result_v1");
  cJSON_AddStringToObject(cache, "engine_sha256", r.physical_input_sha256);
  cJSON_AddStringToObject(cache, "physical_input_sha256",
                          r.physical_input_sha256);
  cJSON_AddStringToObject(cache, "physical_input_canonical", projection);
  cJSON_AddItemToObject(cache, "physical_input", physical);
  cJSON_AddNumberToObject(cache, "endpoint_tick", 1);
  cJSON_AddItemToObject(cache, "frames", frames);
  for (int tick = 0; tick < 2; tick++) {
    cJSON *f = cJSON_CreateObject(), *bs = cJSON_CreateArray(),
          *state = cJSON_CreateObject();
    cJSON_AddNumberToObject(f, "tick", tick);
    cJSON_AddNumberToObject(f, "time_s", tick / 240.0);
    cJSON_AddItemToObject(f, "bodies", bs);
    cJSON_AddItemToArray(bs, state);
    cJSON_AddItemToArray(frames, f);
    cJSON_AddStringToObject(state, "object_id", "body");
    cJSON_AddNumberToObject(state, "body_id", 17);
    cJSON_AddBoolToObject(state, "static", 0);
    double zero[3] = {0, 0, 0}, q[4] = {1, 0, 0, 0};
    cJSON_AddItemToObject(state, "position_m",
                          cJSON_CreateDoubleArray(zero, 3));
    cJSON_AddItemToObject(state, "orientation_wxyz",
                          cJSON_CreateDoubleArray(q, 4));
    cJSON_AddItemToObject(state, "velocity_mps",
                          cJSON_CreateDoubleArray(zero, 3));
    cJSON_AddItemToObject(state, "angular_velocity_radps",
                          cJSON_CreateDoubleArray(zero, 3));
    cJSON_AddNumberToObject(f, "constraints_solved", 0);
    cJSON_AddNumberToObject(f, "impulse_kg_mps", 0);
    cJSON_AddNumberToObject(f, "maximum_penetration_m", 0);
    cJSON_AddItemToObject(f, "contacts", cJSON_CreateArray());
  }
  assert(admits(cache, CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS));
  cJSON_ReplaceItemInObjectCaseSensitive(physical, "integration_policy",
                                         cJSON_CreateString("unknown"));
  seal(cache);
  assert(!admits(cache, CORE_SCENE_RIGID_INTEGRATION_DEFAULT));
  cJSON_ReplaceItemInObjectCaseSensitive(physical, "integration_policy",
                                         cJSON_CreateNull());
  seal(cache);
  assert(!admits(cache, CORE_SCENE_RIGID_INTEGRATION_DEFAULT));
  cJSON_DeleteItemFromObjectCaseSensitive(physical, "integration_policy");
  seal(cache);
  assert(admits(cache, CORE_SCENE_RIGID_INTEGRATION_DEFAULT));
  c.integration_policy = CORE_SCENE_RIGID_INTEGRATION_DEFAULT;
  r.integration_policy = c.integration_policy;
  memset(r.physical_input_sha256, 'a', 64);
  assert(core_scene_rigid_result_match(&r, &c, digest).code == CORE_OK);
  assert(!strcmp(legacy, projection));
  c.integration_policy = (CoreSceneRigidIntegrationPolicy)99;
  assert(core_scene_rigid_result_match(&r, &c, digest).code != CORE_OK);
  cJSON_Delete(cache);
  free(legacy);
  free(projection);
  projection = NULL;
  puts("rigid result profile: one through sixty-four children accepted; "
       "sixty-five refused; input unchanged");
  return 0;
}
