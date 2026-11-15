#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_bundle_v2.h"
#include "runtime_rigid_asset_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const cJSON *get(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
static const char *str(const cJSON *j, const char *k) {
  const cJSON *v = get(j, k);
  return cJSON_IsString(v) ? v->valuestring : "";
}
static bool fields(const cJSON *j, const char *keys, int n) {
  if (!cJSON_IsObject(j))
    return false;
  int count = 0;
  for (const cJSON *v = j->child; v; v = v->next) {
    char key[100];
    if (!v->string ||
        snprintf(key, sizeof(key), "|%s|", v->string) >= (int)sizeof(key) ||
        !strstr(keys, key))
      return false;
    count++;
  }
  return count == n;
}
static bool root_name(const char *s) {
  if (strncmp(s, "asset_", 6))
    return false;
  unsigned n = 0;
  const char *p = s + 6;
  if (!*p || (*p == '0' && p[1]))
    return false;
  for (; *p; p++) {
    if (*p < '0' || *p > '9' || n > 31)
      return false;
    n = n * 10 + (unsigned)(*p - '0');
  }
  return n < 32;
}
bool RuntimeRigidBundleV2Path(const char *p) {
  if (!strcmp(p, "poses.pack") || !strcmp(p, "input/scene.json") ||
      !strcmp(p, "input/assets/asset_set.json"))
    return true;
  const char *prefix = "input/assets/";
  if (strncmp(p, prefix, strlen(prefix)))
    return false;
  const char *slash = strchr(p + strlen(prefix), '/');
  if (!slash)
    return false;
  size_t n = (size_t)(slash - (p + strlen(prefix)));
  char root[16];
  if (n >= sizeof(root))
    return false;
  memcpy(root, p + strlen(prefix), n);
  root[n] = 0;
  if (!root_name(root))
    return false;
  const char *names[] = {
      "source.stl",           "visible.json",          "collision.bcr3d",
      "authoring.json",       "collision.bcd3",        "accepted-vhacd.json",
      "static-surface.json",  "original.json",         "model.json",
      "proxy-collision.json", "asset-certificate.json"};
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (!strcmp(slash + 1, names[i]))
      return true;
  return false;
}
static RuntimeRigidBundleFile *find(RuntimeRigidBundleFile *fs, size_t n,
                                    const char *path) {
  for (size_t i = 0; i < n; i++)
    if (!strcmp(fs[i].path, path))
      return fs + i;
  return NULL;
}
static RuntimeRigidBundleFile *part(RuntimeRigidBundleFile *fs, size_t n,
                                    const char *root, const char *name) {
  char p[128];
  if (snprintf(p, sizeof(p), "input/assets/%s/%s", root, name) >=
      (int)sizeof(p))
    return NULL;
  return find(fs, n, p);
}
static cJSON *parse(RuntimeRigidBundleFile *f) {
  return f && RuntimeRigidResultStrictJSON(f->bytes) ? cJSON_Parse(f->bytes)
                                                     : NULL;
}
static bool hash_is(const cJSON *j, const char *key,
                    RuntimeRigidBundleFile *f) {
  return f && !strcmp(str(j, key), f->hash);
}
static bool model(const cJSON *m, const cJSON *decl, const cJSON *native,
                  const cJSON *meta, RuntimeRigidBundleFile *original,
                  RuntimeRigidBundleFile *visible,
                  RuntimeRigidBundleFile *source) {
  const char *profile = str(m, "profile");
  return cJSON_IsObject(m) && cJSON_IsString(decl) && !strcmp(profile, decl->valuestring) &&
         !strcmp(str(m, "fidelity"), "approximate") &&
         (!strcmp(profile, "surface_fitted64_v1") ||
          !strcmp(profile, "filled_envelope_v1") ||
          !strcmp(profile, "near_convex_envelope_v1")) &&
         !strcmp(str(m, "asset_id"), str(meta, "asset_id")) &&
         !strcmp(profile, str(native, "profile")) &&
         hash_is(m, "original_input_sha256", original) &&
         hash_is(m, "visual_sha256", visible) &&
         hash_is(m, "collision_model_source_sha256", source);
}
cJSON *RuntimeRigidBundleV2Catalog(RuntimeRigidBundleFile *fs, size_t count,
                                   const char *engine, RuntimeRigidResult *r) {
  RuntimeRigidBundleFile *set = find(fs, count, "input/assets/asset_set.json");
  cJSON *setj = parse(set), *cat = cJSON_CreateObject(),
        *assets = cJSON_CreateArray();
  const cJSON *defs = get(setj, "assets");
  bool ok = cat && assets && fields(setj, "|schema||assets|", 2) &&
            !strcmp(str(setj, "schema"), "ball_rigid_asset_set_v1") &&
            cJSON_IsArray(defs) && cJSON_GetArraySize(defs) > 0 &&
            cJSON_GetArraySize(defs) <= 32;
  cJSON_AddStringToObject(cat, "schema", "core_rigid_asset_metadata_v1");
  cJSON_AddItemToObject(cat, "assets", assets);
  size_t expected = 3;
  for (const cJSON *a = ok ? defs->child : NULL; a && ok; a = a->next) {
    const char *root = str(a, "root"), *id = str(a, "asset_id");
    const cJSON *decl = get(a, "collision_model");
    if (!fields(a, "|asset_id||root||collision_model|", 2 + (decl != NULL)) ||
        !root_name(root) || !id[0] || strlen(id) > 48) {
      ok = false;
      break;
    }
    for (const cJSON *p = defs->child; p != a; p = p->next)
      if (!strcmp(root, str(p, "root")) || !strcmp(id, str(p, "asset_id")))
        ok = false;
    cJSON *cert = parse(part(fs, count, root, "asset-certificate.json"));
    const cJSON *meta = get(cert, "metadata"),
                *cm = get(cert, "collision_model"),
                *inventory = get(cert, "files");
    const char *kind = str(cert, "kind");
    const char *names[5] = {0};
    size_t num = 0;
    if (!strcmp(kind, "bcr3d")) {
      names[0] = "source.stl";
      names[1] = "visible.json";
      names[2] = "collision.bcr3d";
      num = 3;
    } else if (!strcmp(kind, "accepted_vhacd")) {
      names[0] = "source.stl";
      names[1] = "visible.json";
      names[2] = "collision.bcd3";
      names[3] = "authoring.json";
      names[4] = "accepted-vhacd.json";
      num = 5;
    } else if (!strcmp(kind, "static_surface")) {
      names[0] = "source.stl";
      names[1] = "visible.json";
      names[2] = "static-surface.json";
      names[3] = "authoring.json";
      num = 4;
    } else if (!strcmp(kind, "approximate")) {
      names[0] = "model.json";
      names[1] = "visible.json";
      names[2] = "proxy-collision.json";
      names[3] = "original.json";
      num = 4;
    }
    ok = ok && num &&
         fields(
             cert,
             "|schema||engine_sha256||kind||metadata||collision_model||files|",
             6) &&
         !strcmp(str(cert, "schema"), "ball_rigid_asset_certificate_v1") &&
         !strcmp(str(cert, "engine_sha256"), engine) &&
         fields(meta,
                "|asset_id||source_asset_id||mesh_asset_id||source_sha256||"
                "mesh_sha256||collision_sha256||shape||child_count||source_"
                "center_m||principal_to_source||inertia_per_kg|",
                11) &&
         !strcmp(str(meta, "asset_id"), id) && cJSON_IsArray(inventory) &&
         cJSON_GetArraySize(inventory) == (int)num;
    RuntimeRigidBundleFile *parts[5] = {0};
    for (size_t k = 0; k < num && ok; k++) {
      parts[k] = part(fs, count, root, names[k]);
      size_t hits = 0;
      for (const cJSON *v = inventory->child; v; v = v->next)
        if (!strcmp(str(v, "path"), names[k])) {
          const cJSON *sz = get(v, "size_bytes");
          hits++;
          ok = ok && fields(v, "|path||size_bytes||sha256|", 3) && parts[k] &&
               cJSON_IsNumber(sz) &&
               sz->valuedouble == (double)parts[k]->size &&
               hash_is(v, "sha256", parts[k]);
        }
      if (hits != 1)
        ok = false;
    }
    if (ok)
      ok = hash_is(meta, "source_sha256", parts[0]) &&
           hash_is(meta, "mesh_sha256", parts[1]) &&
           hash_is(meta, "collision_sha256", parts[2]);
    cJSON *native = NULL;
    if (ok && !strcmp(kind, "bcr3d")) {
      uint32_t children = 0;
      char sid[64], sha[65];
      double center[3], basis[9], inertia[3];
      ok = RuntimeRigidCompoundMetadata(parts[2]->bytes, parts[2]->size,
                                        &children, sid, sha, center, basis,
                                        inertia) &&
           !strcmp(sid, str(meta, "source_asset_id")) &&
           !strcmp(sha, parts[0]->hash);
      const char *keys[] = {"source_center_m", "principal_to_source",
                            "inertia_per_kg"};
      double *vals[] = {center, basis, inertia};
      int sizes[] = {3, 9, 3};
      for (int k = 0; k < 3 && ok; k++) {
        cJSON *values = cJSON_CreateDoubleArray(vals[k], sizes[k]);
        ok = values && cJSON_Compare(values, get(meta, keys[k]), 1);
        cJSON_Delete(values);
      }
      ok = ok && get(meta, "child_count") &&
           get(meta, "child_count")->valuedouble == children &&
           !strcmp(str(meta, "shape"), "convex_compound");
    } else if (ok) {
      native = parse(!strcmp(kind, "accepted_vhacd") ? parts[4] : parts[2]);
      ok = native && !strcmp(str(native, "asset_id"), id);
      if (ok && !strcmp(kind, "accepted_vhacd"))
        ok = fields(native,
                    "|schema||asset_id||source_sha256||authoring_sha256||"
                    "collision_sha256||visible_sha256|",
                    6) &&
             !strcmp(str(native, "schema"), "ball_accepted_vhacd_scene_v1") &&
             hash_is(native, "source_sha256", parts[0]) &&
             hash_is(native, "visible_sha256", parts[1]) &&
             hash_is(native, "collision_sha256", parts[2]) &&
             hash_is(native, "authoring_sha256", parts[3]) &&
             !strcmp(str(meta, "shape"), "convex_compound");
      else if (ok && !strcmp(kind, "static_surface"))
        ok = fields(native,
                    "|schema||asset_id||source_sha256||authoring_sha256||"
                    "visible_sha256|",
                    5) &&
             !strcmp(str(native, "schema"), "ball_static_surface_scene_v1") &&
             hash_is(native, "source_sha256", parts[0]) &&
             hash_is(native, "visible_sha256", parts[1]) &&
             hash_is(native, "authoring_sha256", parts[3]) &&
             !strcmp(str(meta, "shape"), "static_triangles");
      else if (ok)
        ok =
            fields(native,
                   "|schema||asset_id||profile||visual_sha256||model_sha256||"
                   "original_input_sha256||scale_xyz|",
                   7) &&
            !strcmp(str(native, "schema"), "ball_convex_cell_proxy_scene_v1") &&
            hash_is(native, "visual_sha256", parts[1]) &&
            hash_is(native, "model_sha256", parts[0]) &&
            hash_is(native, "original_input_sha256", parts[3]) &&
            model(cm, decl, native, meta, parts[3], parts[1], parts[0]) &&
            !strcmp(str(meta, "shape"), "convex_compound");
    }
    if (ok && strcmp(kind, "approximate"))
      ok = !decl && cJSON_IsNull(cm);
    if (ok) {
      const char *mesh_id = str(meta, "mesh_asset_id");
      for (size_t k = 0; k < r->asset_count; k++)
        if (!strcmp(mesh_id, r->assets[k].id))
          ok = false;
      if (ok) {
        size_t ix = r->asset_count++;
        snprintf(r->assets[ix].id, 64, "%s", mesh_id);
        snprintf(r->assets[ix].sha256, 65, "%s", parts[1]->hash);
        const cJSON *children = get(meta, "child_count");
        ok = strlen(mesh_id) < 64 && mesh_id[0] && cJSON_IsNumber(children) &&
             children->valuedouble >= 1 && children->valuedouble <= 64 &&
             children->valuedouble == (double)(uint32_t)children->valuedouble;
        r->assets[ix].child_count = ok ? (uint32_t)children->valuedouble : 0;
        r->assets[ix].approximate = !strcmp(kind, "approximate");
        r->assets[ix].visible = parse(parts[1]);
        ok = ok && r->assets[ix].visible;
        cJSON_AddItemToArray(assets, cJSON_Duplicate(meta, 1));
      }
    }
    expected += num + 1;
    cJSON_Delete(native);
    cJSON_Delete(cert);
  }
  ok = ok && count == expected;
  cJSON_Delete(setj);
  if (!ok) {
    cJSON_Delete(cat);
    return NULL;
  }
  return cat;
}
#endif
