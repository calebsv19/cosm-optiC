#include "cJSON.h"
#include "core_scene_rigid_json.h"
#include <stdio.h>
#include <stdlib.h>
static char *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) || ftell(f) < 1 || ftell(f) > 1024 * 1024) {
    fclose(f);
    return NULL;
  }
  *size = (size_t)ftell(f);
  rewind(f);
  char *bytes = calloc(*size + 1, 1);
  if (!bytes || fread(bytes, 1, *size, f) != *size) {
    free(bytes);
    fclose(f);
    return NULL;
  }
  if (fclose(f)) {
    free(bytes);
    return NULL;
  }
  return bytes;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s scene.json asset_metadata.json\n", argv[0]);
    return 2;
  }
  size_t sn = 0, an = 0;
  char *s = read_file(argv[1], &sn), *a = read_file(argv[2], &an);
  CoreSceneRigidContract *c = calloc(1, sizeof(*c));
  CoreSceneRigidDiagnostic d = {0};
  CoreResult r = core_scene_rigid_json_read(
      s, sn, a, an, (CoreSceneRigidLimits){8, 28, 64}, c, &d);
  cJSON *reply = cJSON_CreateObject();
  cJSON_AddBoolToObject(reply, "ok", r.code == CORE_OK);
  cJSON_AddStringToObject(reply, "proof_layer", "contract_metadata_only");
  cJSON_AddBoolToObject(reply, "execution_supported", false);
  cJSON_AddStringToObject(reply, "code", r.code == CORE_OK ? "ok" : d.code);
  cJSON_AddStringToObject(reply, "object_id", d.object_id);
  cJSON_AddStringToObject(reply, "message",
                          r.code == CORE_OK
                              ? "roles and bindings valid; payload "
                                "verification and execution remain separate"
                              : d.message);
  if (r.code == CORE_OK) {
    cJSON_AddStringToObject(reply, "schema", CORE_SCENE_RIGID_SCHEMA);
    cJSON_AddStringToObject(reply, "scene_id", c->scene_id);
    cJSON *limits = cJSON_AddObjectToObject(reply, "consumer_limits");
    cJSON_AddNumberToObject(limits, "bodies", 8);
    cJSON_AddNumberToObject(limits, "pairs", 28);
    cJSON_AddNumberToObject(limits, "children_per_body", 64);
    cJSON *bindings = cJSON_AddArrayToObject(reply, "bindings");
    for (size_t i = 0; i < c->binding_count; ++i) {
      const CoreSceneRigidBinding *b = &c->bindings[i];
      cJSON *row = cJSON_CreateObject();
      cJSON_AddItemToArray(bindings, row);
      cJSON_AddStringToObject(row, "object_id", b->object_id);
      cJSON_AddNumberToObject(row, "body_id", b->body_id);
      cJSON_AddStringToObject(row, "role",
                              b->role == CORE_SCENE_RIGID_VISUAL_ONLY
                                  ? "visual_only"
                              : b->role == CORE_SCENE_RIGID_STATIC ? "static"
                                                                   : "dynamic");
      cJSON_AddStringToObject(
          row, "motion_owner",
          b->owner == CORE_SCENE_RIGID_AUTHORED ? "authored" : "simulation");
      cJSON_AddBoolToObject(row, "collides", b->collides);
      if (b->collides) {
        cJSON_AddStringToObject(row, "mesh_asset_id", b->mesh_id);
        cJSON_AddStringToObject(row, "collision_asset_id", b->asset_id);
        cJSON_AddNumberToObject(row, "mass_kg", b->mass_kg);
        cJSON_AddItemToObject(row, "source_center_m",
                              cJSON_CreateDoubleArray(b->source_center_m, 3));
        cJSON_AddItemToObject(
            row, "principal_to_source",
            cJSON_CreateDoubleArray(b->principal_to_source, 9));
      }
    }
    cJSON_AddNumberToObject(reply, "object_count", (double)c->object_count);
    cJSON_AddNumberToObject(reply, "binding_count", (double)c->binding_count);
  }
  char *out = cJSON_PrintUnformatted(reply);
  if (out) {
    puts(out);
    free(out);
  }
  cJSON_Delete(reply);
  free(s);
  free(a);
  free(c);
  return r.code == CORE_OK ? 0 : 1;
}
