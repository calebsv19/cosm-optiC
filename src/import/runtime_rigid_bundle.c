#ifdef RAY_RIGID_RESULT_ENABLED
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "core_pack.h"
#include "core_scene_compile.h"
#include "core_scene_rigid_json.h"
#include "runtime_rigid_result_internal.h"
#include "runtime_rigid_asset_metadata.h"
#include "runtime_rigid_bundle_v2.h"
_Static_assert(RAY_RIGID_RESULT_MAX_CHILDREN >= CORE_SCENE_RIGID_RESULT_CHILDREN, "App storage must cover the shared result profile");
_Static_assert(RAY_RIGID_RESULT_MAX_BODIES == CORE_SCENE_RIGID_RESULT_BODIES,
               "Rigid producer/consumer body envelopes must agree");

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static bool digest_bytes(const void *p, size_t n, char hex[65]) {
  return core_scene_compile_sha256(p, n, hex).code == CORE_OK;
}
typedef RuntimeRigidBundleFile File;
typedef struct Bundle {
  File files[256];
  size_t count, total;
} Bundle;
static const cJSON *get(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
static const char *str(const cJSON *j, const char *k) {
  const cJSON *v = get(j, k);
  return cJSON_IsString(v) ? v->valuestring : "";
}
static bool schema(const cJSON *j, const char *s) {
  return !strcmp(str(j, "schema"), s);
}
static bool fields(const cJSON *j, const char *keys, int n) {
  int count = 0;
  if (!cJSON_IsObject(j))
    return false;
  for (const cJSON *x = j->child; x; x = x->next) {
    char name[100];
    if (!x->string ||
        snprintf(name, sizeof(name), "|%s|", x->string) >= (int)sizeof(name) ||
        !strstr(keys, name))
      return false;
    count++;
  }
  return count == n;
}
static bool nodes(const cJSON *j, unsigned depth, size_t *count) {
  if (depth > 32 || ++*count > CORE_SCENE_RIGID_RESULT_JSON_NODES)
    return false;
  for (const cJSON *x = j->child; x; x = x->next) {
    if (cJSON_IsObject(j))
      for (const cJSON *y = x->next; y; y = y->next)
        if (!strcmp(x->string, y->string))
          return false;
    if (!nodes(x, depth + 1, count))
      return false;
  }
  return true;
}
static cJSON *parse(const char *s, size_t n) {
  if (memchr(s, 0, n))
    return NULL;
  for (size_t i = 0; i + 6 <= n; i++)
    if (!memcmp(s + i, "\\u0000", 6))
      return NULL;
  const char *end = NULL;
  cJSON *j = cJSON_ParseWithLengthOpts(s, n, &end, 0);
  size_t count = 0;
  if (!j || !nodes(j, 0, &count)) {
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
bool RuntimeRigidResultStrictJSON(const char *text) {
  if (!text || strlen(text) > CORE_SCENE_RIGID_RESULT_JSON_BYTES)
    return false;
  cJSON *j = parse(text, strlen(text));
  bool ok = j != NULL;
  cJSON_Delete(j);
  return ok;
}
static bool allowed(const char *p) {
  if (!strcmp(p, "poses.pack") || !strcmp(p, "input/scene.json") ||
      !strcmp(p, "input/assets/asset_set.json"))
    return true;
  if (strncmp(p, "input/assets/asset_", 19) || p[19] < '0' || p[19] > '7' ||
      p[20] != '/')
    return false;
  return !strcmp(p + 21, "source.stl") || !strcmp(p + 21, "visible.json") ||
         !strcmp(p + 21, "collision.bcr3d");
}
/* Walk each component with nofollow, retain bounded bytes once. No later bundle
 * reads. */
static int descend(int root, const char *path) {
  char p[128];
  if (strlen(path) >= sizeof(p))
    return -1;
  strcpy(p, path);
  char *save = NULL, *part = strtok_r(p, "/", &save);
  int fd = dup(root);
  while (part && fd >= 0) {
    char *next = strtok_r(NULL, "/", &save);
    if (!strcmp(part, ".") || !strcmp(part, "..")) {
      close(fd);
      return -1;
    }
    int child =
        openat(fd, part, O_RDONLY | O_NOFOLLOW | (next ? O_DIRECTORY : 0));
    close(fd);
    fd = child;
    part = next;
  }
  return fd;
}
static char *read_at(int root, const char *path, size_t limit, size_t *size) {
  int fd = descend(root, path);
  struct stat st;
  char *out = NULL;
  if (fd < 0)
    return NULL;
  if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      (uint64_t)st.st_size > limit)
    goto done;
  *size = (size_t)st.st_size;
  out = malloc(*size + 1);
  if (!out)
    goto done;
  size_t n = 0;
  while (n < *size) {
    ssize_t x = read(fd, out + n, *size - n);
    if (x <= 0) {
      free(out);
      out = NULL;
      goto done;
    }
    n += (size_t)x;
  }
  char extra;
  if (read(fd, &extra, 1) != 0) {
    free(out);
    out = NULL;
  } else
    out[*size] = 0;
done:
  close(fd);
  return out;
}
static File *find(Bundle *b, const char *p) {
  for (size_t i = 0; i < b->count; i++)
    if (!strcmp(p, b->files[i].path))
      return &b->files[i];
  return NULL;
}
static bool inventory(int fd, const char *base, Bundle *b, size_t *files) {
  DIR *d = fdopendir(dup(fd));
  if (!d)
    return false;
  bool ok = true;
  struct dirent *e;
  while (ok && (e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    char p[128];
    struct stat st;
    if (snprintf(p, sizeof(p), "%s%s%s", base, base[0] ? "/" : "", e->d_name) >=
            (int)sizeof(p) ||
        fstatat(fd, e->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
      ok = false;
      break;
    }
    if (S_ISDIR(st.st_mode)) {
      bool prefix = false;
      size_t n = strlen(p);
      for (size_t i = 0; i < b->count; i++)
        if (!strncmp(b->files[i].path, p, n) && b->files[i].path[n] == '/')
          prefix = true;
      int sub = openat(fd, e->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
      ok = prefix && sub >= 0 && inventory(sub, p, b, files);
      if (sub >= 0)
        close(sub);
    } else {
      ok = S_ISREG(st.st_mode) && (!strcmp(p, "manifest.json") || find(b, p));
      (*files)++;
    }
  }
  closedir(d);
  return ok;
}
static bool payload(File *f, char **out, size_t *size) {
  FILE *tmp = tmpfile();
  if (!tmp)
    return false;
  bool ok = fwrite(f->bytes, 1, f->size, tmp) == f->size && !fflush(tmp);
  rewind(tmp);
  char path[80];
  snprintf(path, sizeof(path), "/dev/fd/%d", fileno(tmp));
  CorePackReader r = {0};
  if (ok)
    ok = core_pack_reader_open(path, &r).code == CORE_OK;
  CorePackChunkInfo ch = {0};
  if (ok)
    ok = core_pack_reader_chunk_count(&r) == 1 &&
         core_pack_reader_get_chunk(&r, 0, &ch).code == CORE_OK &&
         !strcmp(ch.type, "RPOS") && ch.size > 0 &&
         ch.size < CORE_SCENE_RIGID_RESULT_JSON_BYTES;
  *out = ok ? malloc((size_t)ch.size + 1) : NULL;
  ok = ok && *out &&
       core_pack_reader_read_chunk_data(&r, &ch, *out, ch.size).code == CORE_OK;
  if (ok) {
    *size = (size_t)ch.size;
    (*out)[*size] = 0;
  } else {
    free(*out);
    *out = NULL;
  }
  if (r.file)
    core_pack_reader_close(&r);
  fclose(tmp);
  return ok;
}
/* Manifest canonicalization has only objects, arrays, strings and small
 * integers. Refuse rather than guessing a floating-point canonicalization. */
static cJSON *sorted(const cJSON *j) {
  if (cJSON_IsObject(j)) {
    const cJSON *items[32];
    size_t n = 0;
    for (const cJSON *v = j->child; v; v = v->next) {
      if (n == 32)
        return NULL;
      items[n++] = v;
    }
    for (size_t i = 0; i < n; i++)
      for (size_t k = i + 1; k < n; k++)
        if (strcmp(items[i]->string, items[k]->string) > 0) {
          const cJSON *t = items[i];
          items[i] = items[k];
          items[k] = t;
        }
    cJSON *out = cJSON_CreateObject();
    for (size_t i = 0; i < n; i++) {
      cJSON *v = sorted(items[i]);
      if (!v) {
        cJSON_Delete(out);
        return NULL;
      }
      cJSON_AddItemToObject(out, items[i]->string, v);
    }
    return out;
  }
  if (cJSON_IsArray(j)) {
    cJSON *out = cJSON_CreateArray();
    for (const cJSON *v = j->child; v; v = v->next) {
      cJSON *x = sorted(v);
      if (!x) {
        cJSON_Delete(out);
        return NULL;
      }
      cJSON_AddItemToArray(out, x);
    }
    return out;
  }
  if (cJSON_IsString(j))
    return cJSON_Duplicate(j, 0);
  if (cJSON_IsNumber(j) && j->valuedouble >= 0 && j->valuedouble < 67108865 &&
      j->valuedouble == (double)j->valueint)
    return cJSON_Duplicate(j, 0);
  return NULL;
}
static bool digest_manifest(cJSON *m, const char *revision) {
  char hash[65];
  cJSON *copy = cJSON_Duplicate(m, 1);
  if (!copy)
    return false;
  cJSON_DeleteItemFromObjectCaseSensitive(copy, "result_revision");
  cJSON *order = sorted(copy);
  char *text = order ? cJSON_PrintUnformatted(order) : NULL;
  bool ok = false;
  if (text) {
    size_t n = strlen(text);
    char *line = malloc(n + 2);
    if (line) {
      memcpy(line, text, n);
      line[n] = '\n';
      ok = core_scene_compile_sha256(line, n + 1, hash).code == CORE_OK &&
           !strcmp(hash, revision);
      free(line);
    }
  }
  free(text);
  cJSON_Delete(order);
  cJSON_Delete(copy);
  return ok;
}
static cJSON *catalog(Bundle *files, RuntimeRigidResult *r, cJSON *original) {
  const cJSON *rows =
      get(get(get(original, "extensions"), "rigid_scene"), "bindings");
  cJSON *cat = cJSON_CreateObject(), *assets = cJSON_CreateArray();
  cJSON_AddStringToObject(cat, "schema", "core_rigid_asset_metadata_v1");
  cJSON_AddItemToObject(cat, "assets", assets);
  File *set = find(files, "input/assets/asset_set.json");
  cJSON *setj = set ? parse(set->bytes, set->size) : NULL;
  const cJSON *defs = get(setj, "assets");
  bool ok = schema(setj, "ball_rigid_asset_set_v1") &&
            fields(setj, "|schema||assets|", 2) && cJSON_IsArray(defs) &&
            cJSON_GetArraySize(defs) > 0 && cJSON_GetArraySize(defs) <= 8;
  for (const cJSON *a = ok ? defs->child : NULL; a && ok; a = a->next) {
    const char *id = str(a, "asset_id"), *root = str(a, "root");
    if (!fields(a, "|asset_id||root|", 2) || strlen(id) > 48 || !id[0] ||
        strlen(root) != 7 || strncmp(root, "asset_", 6) || root[6] < '0' ||
        root[6] > '7') {
      ok = false;
      break;
    }
    /* Asset libraries may include unused solids. Their inventory/digests are
     * still authenticated, but only scene bindings supply physical metadata.
     * Reject aliases before skipping unused entries. */
    for (const cJSON *prior = defs->child; prior != a; prior = prior->next)
      if (!strcmp(str(prior, "asset_id"), id) ||
          !strcmp(str(prior, "root"), root))
        ok = false;
    if (!ok)
      break;
    char path[128];
    snprintf(path, sizeof(path), "input/assets/%s/visible.json", root);
    File *visible = find(files, path);
    snprintf(path, sizeof(path), "input/assets/%s/source.stl", root);
    File *source = find(files, path);
    snprintf(path, sizeof(path), "input/assets/%s/collision.bcr3d", root);
    File *collision = find(files, path);
    const cJSON *binding = NULL;
    for (const cJSON *b = rows ? rows->child : NULL; b; b = b->next)
      if (!strcmp(str(b, "collision_asset_id"), id)) {
        binding = b;
        break;
      }
    if (!visible || !source || !collision) {
      ok = false;
      break;
    }
    if (!binding)
      continue;
    if (strcmp(source->hash, str(binding, "source_sha256")) ||
        strcmp(visible->hash, str(binding, "mesh_sha256")) ||
        strcmp(collision->hash, str(binding, "collision_sha256"))) {
      ok = false;
      break;
    }
    cJSON *meta = cJSON_CreateObject();
    const char *names[] = {"source_asset_id",    "mesh_asset_id",
                           "collision_sha256",   "source_sha256",
                           "mesh_sha256",        "source_center_m",
                           "principal_to_source"};
    cJSON_AddStringToObject(meta, "asset_id", id);
    for (size_t i = 0; i < 7; i++)
      cJSON_AddItemToObject(meta, names[i],
                            cJSON_Duplicate(get(binding, names[i]), 1));
    bool sphere=!strncmp(collision->bytes,"BALL_RIGID_ANALYTIC_SPHERE_V1\n",strlen("BALL_RIGID_ANALYTIC_SPHERE_V1\n"));
    uint32_t children=1;double inertia[3];
    if(sphere) {
      const char *moments=strstr(collision->bytes,"\nframe_moments ");
      if(!moments||sscanf(moments,"\nframe_moments %la %la %la",inertia,inertia+1,inertia+2)!=3){cJSON_Delete(meta);ok=false;break;}
    } else {
      double center[3],basis[9];char source_id[64],source_hash[65];
      if(!RuntimeRigidCompoundMetadata(collision->bytes,collision->size,&children,source_id,source_hash,center,basis,inertia)||
         strcmp(source_id,str(binding,"source_asset_id"))||strcmp(source_hash,source->hash)) {
        cJSON_Delete(meta);ok=false;break;
      }
      cJSON_ReplaceItemInObjectCaseSensitive(meta,"source_center_m",cJSON_CreateDoubleArray(center,3));
      cJSON_ReplaceItemInObjectCaseSensitive(meta,"principal_to_source",cJSON_CreateDoubleArray(basis,9));
    }
    cJSON_AddStringToObject(meta,"shape",sphere?"analytic_sphere":"convex_compound");
    cJSON_AddNumberToObject(meta,"child_count",children);
    cJSON_AddItemToObject(meta, "inertia_per_kg",
                          cJSON_CreateDoubleArray(inertia, 3));
    cJSON_AddItemToArray(assets, meta);
    size_t ix = r->asset_count++;
    r->assets[ix].child_count=children;
    snprintf(r->assets[ix].id, 64, "%s", str(binding, "mesh_asset_id"));
    strcpy(r->assets[ix].sha256, visible->hash);
    r->assets[ix].visible = parse(visible->bytes, visible->size);
    if (!r->assets[ix].visible)
      ok = false;
    for (size_t k = 0; k < ix; k++)
      if (!strcmp(r->assets[k].id, r->assets[ix].id))
        ok = false;
  }
  if (ok && files->count != 3 + (size_t)cJSON_GetArraySize(defs) * 3)
    ok = false;
  cJSON_Delete(setj);
  if (!ok) {
    cJSON_Delete(cat);
    return NULL;
  }
  return cat;
}
bool RuntimeRigidBundleRead(const char *path, const char *revision,
                            const char *current, RuntimeRigidResult **out,
                            char *diagnostic, size_t capacity) {
  CoreSceneRigidDiagnostic diag = {0};
  bool ok = false, v2 = false;
  const char *stage = "root";
  int fd = -1;
  Bundle b = {0};
  RuntimeRigidResult *r = calloc(1, sizeof(*r));
  cJSON *m = NULL, *cat = NULL, *original = NULL, *current_root = NULL;
  char *raw = NULL, *poses = NULL, *cat_text = NULL;
  size_t n = 0, pose_n = 0;
  CoreSceneRigidContract *contract = calloc(1, sizeof(*contract));
  if (!out || !r || !contract || strlen(revision) != 64 ||
      (fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW)) < 0)
    goto done;
  stage = "manifest revision";
  raw = read_at(fd, "manifest.json", 256u * 1024u, &n);
  if (!raw || !(m = parse(raw, n)) ||
      (!schema(m, "ball_rigid_result_bundle_v1") &&
       !schema(m, "ball_rigid_result_bundle_v2")) ||
      !fields(m,
              "|schema||physical_input_sha256||engine_sha256||endpoint_tick||"
              "artifacts||result_revision|",
              6) ||
      strcmp(str(m, "result_revision"), revision) ||
      !digest_manifest(m, revision))
    goto done;
  v2 = schema(m, "ball_rigid_result_bundle_v2");
  r->complex_bundle = v2;
  if (!v2 && n > 65536) goto done;
  stage = "artifact inventory";
  const cJSON *artifacts = get(m, "artifacts");
  if (!cJSON_IsArray(artifacts) || cJSON_GetArraySize(artifacts) < 1 ||
      cJSON_GetArraySize(artifacts) > (v2 ? 256 : 31))
    goto done;
  for (const cJSON *a = artifacts->child; a; a = a->next) {
    File *f = &b.files[b.count++];
    const char *p = str(a, "path");
    const cJSON *size = get(a, "size_bytes");
    if (!fields(a, "|path||size_bytes||sha256|", 3) || !(v2 ? RuntimeRigidBundleV2Path(p) : allowed(p)) ||
        strlen(p) >= sizeof(f->path) || !cJSON_IsNumber(size) ||
        size->valuedouble < 1 || size->valuedouble > CORE_SCENE_RIGID_RESULT_JSON_BYTES ||
        size->valuedouble != (double)(size_t)size->valuedouble ||
        strlen(str(a, "sha256")) != 64 ||
        (b.count > 1 && strcmp(b.files[b.count - 2].path, p) >= 0))
      goto done;
    strcpy(f->path, p);
    f->bytes = read_at(
        fd, p, v2 || !strcmp(p, "poses.pack") ? CORE_SCENE_RIGID_RESULT_JSON_BYTES : 1024u * 1024u,
        &f->size);
    if (!f->bytes || f->size != size->valuedouble ||
        core_scene_compile_sha256(f->bytes, f->size, f->hash).code != CORE_OK ||
        strcmp(f->hash, str(a, "sha256")) ||
        (b.total += f->size) > (v2 ? 256u : 64u) * 1024u * 1024u)
      goto done;
  }
  stage = "directory inventory";
  size_t files = 0;
  if (!inventory(fd, "", &b, &files) || files != b.count + 1)
    goto done;
  stage = "cache payload";
  File *pack = find(&b, "poses.pack"), *scene = find(&b, "input/scene.json");
  if (!pack || !scene || !payload(pack, &poses, &pose_n) ||
      core_scene_rigid_result_read(poses, pose_n, &r->result, digest_bytes)
              .code != CORE_OK ||
      strcmp(r->result.engine_sha256, str(m, "engine_sha256")) ||
      strcmp(r->result.physical_input_sha256,
             str(m, "physical_input_sha256")) ||
      !cJSON_IsNumber(get(m, "endpoint_tick")) ||
      r->result.endpoint_tick != get(m, "endpoint_tick")->valuedouble)
    goto done;
  stage = "physical compatibility";
  original = parse(scene->bytes, scene->size);
  cat = original ? (v2 ? RuntimeRigidBundleV2Catalog(b.files, b.count, str(m, "engine_sha256"), r) : catalog(&b, r, original)) : NULL;
  cat_text = cat ? cJSON_PrintUnformatted(cat) : NULL;
  if (!cat_text)
    goto done;
  stage = "original physical contract";
  if (core_scene_rigid_json_read(
          scene->bytes, scene->size, cat_text, strlen(cat_text),
          (CoreSceneRigidLimits){RAY_RIGID_RESULT_MAX_BODIES, RAY_RIGID_RESULT_MAX_PAIRS, v2 ? 64 : 12}, contract, &diag)
          .code != CORE_OK) {
    stage = diag.message;
    goto done;
  }
  stage = "original physical projection";
  if (core_scene_rigid_result_match(&r->result, contract, digest_bytes).code !=
      CORE_OK)
    goto done;
  current_root = parse(current, strlen(current));
  const cJSON *current_bindings =
      get(get(get(current_root, "extensions"), "rigid_scene"), "bindings");
  cJSON *metadata = cJSON_GetObjectItemCaseSensitive(cat, "assets");
  for (size_t i = 0; i < r->asset_count; i++) {
    const char *sha = NULL;
    for (const cJSON *b = current_bindings ? current_bindings->child : NULL; b;
         b = b->next)
      if (!strcmp(str(b, "mesh_asset_id"), r->assets[i].id)) {
        sha = str(b, "mesh_sha256");
        break;
      }
    if (!sha || strlen(sha) != 64)
      goto done;
    strcpy(r->assets[i].sha256, sha);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(metadata, (int)i),
                                           "mesh_sha256",
                                           cJSON_CreateString(sha));
  }
  free(cat_text);
  cat_text = cJSON_PrintUnformatted(cat);
  if (!cat_text)
    goto done;
  stage = "current physical contract";
  if (core_scene_rigid_json_read(
          current, strlen(current), cat_text, strlen(cat_text),
          (CoreSceneRigidLimits){RAY_RIGID_RESULT_MAX_BODIES, RAY_RIGID_RESULT_MAX_PAIRS, v2 ? 64 : 12}, contract, &diag)
          .code != CORE_OK) {
    stage = diag.message;
    goto done;
  }
  stage = "current physical projection";
  if (core_scene_rigid_result_match(&r->result, contract, digest_bytes).code !=
      CORE_OK)
    goto done;
  strcpy(r->revision, revision);
  *out = r;
  r = NULL;
  ok = true;
done:
  if (fd >= 0)
    close(fd);
  for (size_t i = 0; i < b.count; i++)
    free(b.files[i].bytes);
  free(raw);
  free(poses);
  free(cat_text);
  free(contract);
  cJSON_Delete(cat);
  cJSON_Delete(m);
  cJSON_Delete(original);
  cJSON_Delete(current_root);
  RuntimeRigidResultFree(r);
  if (diagnostic && capacity)
    snprintf(diagnostic, capacity, "%s: %s", ok ? "ok" : "rigid result refused",
             stage);
  return ok;
}

#endif
