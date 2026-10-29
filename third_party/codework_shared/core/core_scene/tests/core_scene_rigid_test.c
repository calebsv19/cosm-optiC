#include "core_scene_rigid.h"
#include "core_scene_rigid_json.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void seed(CoreSceneRigidContract *c) {
  memset(c, 0, sizeof(*c));
  strcpy(c->scene_id, "test");
  c->schema_version = 1;
  c->rate_numerator = 240;
  c->rate_denominator = 1;
  c->end_tick = 720;
  c->object_count = c->binding_count = c->asset_count = 1;
  CoreSceneRigidObject *o = &c->objects[0];
  strcpy(o->id, "body");
  strcpy(o->mesh_id, "mesh");
  o->mesh_instance = true;
  o->scale[0] = o->scale[1] = o->scale[2] = 1;
  CoreSceneRigidAsset *a = &c->assets[0];
  strcpy(a->id, "collision");
  strcpy(a->source_id, "source");
  strcpy(a->mesh_id, "mesh");
  memset(a->source_sha256, 'a', 64);
  memset(a->collision_sha256, 'b', 64);
  memset(a->mesh_sha256, 'c', 64);
  a->shape = CORE_SCENE_RIGID_CONVEX_COMPOUND;
  a->child_count = 1;
  a->principal_to_source[0] = a->principal_to_source[4] =
      a->principal_to_source[8] = 1;
  a->inertia_per_kg[0] = a->inertia_per_kg[1] = a->inertia_per_kg[2] = 1;
  CoreSceneRigidBinding *b = &c->bindings[0];
  strcpy(b->object_id, o->id);
  strcpy(b->asset_id, a->id);
  strcpy(b->source_id, a->source_id);
  strcpy(b->mesh_id, a->mesh_id);
  strcpy(b->source_sha256, a->source_sha256);
  strcpy(b->collision_sha256, a->collision_sha256);
  strcpy(b->mesh_sha256, a->mesh_sha256);
  memcpy(b->principal_to_source, a->principal_to_source,
         sizeof(b->principal_to_source));
  b->role = CORE_SCENE_RIGID_DYNAMIC;
  b->owner = CORE_SCENE_RIGID_SIMULATION;
  b->body_id = 17;
  b->collides = true;
  b->mass_kg = 1;
  b->orientation_wxyz[0] = 1;
  b->principal_inertia_kgm2[0] = b->principal_inertia_kgm2[1] =
      b->principal_inertia_kgm2[2] = 1;
}
int main(void) {
  CoreSceneRigidContract *c = malloc(sizeof(*c)),
                         *before = malloc(sizeof(*before));
  assert(c && before);
  seed(c);
  *before = *c;
  CoreSceneRigidDiagnostic d;
  CoreSceneRigidLimits lim = {8, 28, 64};
  assert(core_scene_rigid_validate(c, lim, &d).code == CORE_OK);
  assert(!memcmp(c, before, sizeof(*c)));
  c->object_count = 65;
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  assert(!strcmp(d.code, "capacity"));
  seed(c);
  memset(c->objects[0].id, 'x', 64);
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  seed(c);
  c->bindings[0].mass_kg = NAN;
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  seed(c);
  c->assets[0].principal_to_source[0] = -1;
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  seed(c);
  c->bindings[0].orientation_wxyz[0] = 0;
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  seed(c);
  c->objects[0].transform_claims = CORE_SCENE_RIGID_PATH_CLAIM;
  assert(core_scene_rigid_validate(c, lim, &d).code != CORE_OK);
  assert(!strcmp(d.code, "motion_conflict"));
  memset(c, 0x5a, sizeof(*c));
  *before = *c;
  const char invalid[] = "{\"x\":1,\"x\":2}";
  assert(
      core_scene_rigid_json_read(invalid, strlen(invalid), "{}", 2, lim, c, &d)
          .code != CORE_OK);
  assert(!memcmp(c, before, sizeof(*c)));
  const char nul[] = {'{', '}', 0, 'x'};
  assert(
      core_scene_rigid_json_read(nul, sizeof(nul), "{}", 2, lim, c, &d).code !=
      CORE_OK);
  assert(!memcmp(c, before, sizeof(*c)));
  assert(core_scene_rigid_validate(NULL, lim, &d).code != CORE_OK);
  free(c);
  free(before);
  puts("PASS typed rigid contracts: finite state, bounds, ownership, "
       "right-handed mass frame, immutable/atomic output");
  return 0;
}
