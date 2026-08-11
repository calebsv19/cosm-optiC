#include "import/compound_scene_static_room_import.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Reader {
  const unsigned char *bytes;
  size_t size, offset;
  bool ok;
} Reader;

static void fail(RayCompoundSceneStaticRoomImportFailure *out,
                 RayCompoundSceneStaticRoomImportFailure value) {
  if (out)
    *out = value;
}
static uint64_t hash_bytes(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}
static uint64_t hash_u64(uint64_t h, uint64_t v) {
  unsigned char b[8];
  for (size_t i = 0; i < 8; ++i)
    b[i] = (unsigned char)(v >> (8u * i));
  return hash_bytes(h, b, 8);
}
static uint64_t hash_double(uint64_t h, double v) {
  uint64_t bits = 0;
  memcpy(&bits, &v, 8);
  return hash_u64(h, bits);
}
static uint64_t hash_string(uint64_t h, const char *v) {
  return v ? hash_bytes(h, v, strlen(v) + 1u) : hash_u64(h, 0);
}
static uint64_t hash_vec3(uint64_t h, RayCompoundSceneVec3 v) {
  h = hash_double(h, v.x);
  h = hash_double(h, v.y);
  return hash_double(h, v.z);
}
static uint64_t hash_quat(uint64_t h, RayCompoundSceneQuat v) {
  h = hash_double(h, v.w);
  h = hash_double(h, v.x);
  h = hash_double(h, v.y);
  return hash_double(h, v.z);
}
static bool finite_vec3(RayCompoundSceneVec3 v) {
  return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
static bool same_vec3(RayCompoundSceneVec3 a, RayCompoundSceneVec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

void ray_compound_scene_static_room_init(RayCompoundSceneStaticRoom *room) {
  if (room)
    memset(room, 0, sizeof(*room));
}

static const char *role_name(size_t i) {
  static const char *names[] = {"floor", "ceiling", "x_min",
                                "x_max", "z_min",   "z_max"};
  return i < RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT ? names[i] : NULL;
}

static bool derive_plane(RayCompoundSceneStaticRoomRole role,
                         RayCompoundSceneVec3 center, RayCompoundSceneVec3 half,
                         RayCompoundSceneVec3 *origin,
                         RayCompoundSceneVec3 *normal, RayCompoundSceneVec3 *u,
                         RayCompoundSceneVec3 *v, double *hu, double *hv) {
  static const RayCompoundSceneVec3 normals[6] = {
      {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
  static const RayCompoundSceneVec3 us[6] = {{1, 0, 0}, {1, 0, 0}, {0, 1, 0},
                                             {0, 1, 0}, {1, 0, 0}, {1, 0, 0}};
  static const RayCompoundSceneVec3 vs[6] = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1},
                                             {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
  if ((unsigned)role >= 6u || !finite_vec3(center) || !finite_vec3(half) ||
      half.x <= 0 || half.y <= 0 || half.z <= 0 || !origin || !normal || !u ||
      !v || !hu || !hv)
    return false;
  *normal = normals[role];
  *u = us[role];
  *v = vs[role];
  *origin = (RayCompoundSceneVec3){center.x + normal->x * half.x,
                                   center.y + normal->y * half.y,
                                   center.z + normal->z * half.z};
  if (role <= RAY_COMPOUND_SCENE_STATIC_ROOM_CEILING) {
    *hu = half.x;
    *hv = half.z;
  } else if (role <= RAY_COMPOUND_SCENE_STATIC_ROOM_X_MAX) {
    *hu = half.y;
    *hv = half.z;
  } else {
    *hu = half.x;
    *hv = half.y;
  }
  return true;
}

uint64_t ray_compound_scene_static_room_surface_digest(
    const RayCompoundSceneStaticRoomSurface *s) {
  if (!s)
    return 0;
  uint64_t h = UINT64_C(1469598103934665603);
  h = hash_u64(h, (uint64_t)s->role);
  h = hash_string(h, s->surface_id);
  h = hash_u64(h, (uint64_t)s->body_id);
  h = hash_u64(h, s->contact_mask_bit);
  h = hash_vec3(h, s->collision_box_center_m);
  h = hash_vec3(h, s->collision_box_half_extent_m);
  h = hash_quat(h, s->collision_box_orientation);
  h = hash_vec3(h, s->interior_plane_origin_m);
  h = hash_vec3(h, s->inward_normal);
  h = hash_vec3(h, s->tangent_u);
  h = hash_vec3(h, s->tangent_v);
  h = hash_double(h, s->half_extent_u_m);
  h = hash_double(h, s->half_extent_v_m);
  h = hash_double(h, s->restitution);
  return hash_double(h, s->friction);
}
uint64_t ray_compound_scene_static_room_surface_set_digest(
    const RayCompoundSceneStaticRoom *room) {
  if (!room)
    return 0;
  uint64_t h = UINT64_C(1469598103934665603);
  h = hash_u64(h, room->provenance.room_spec_digest);
  h = hash_u64(h, room->surface_count);
  for (size_t i = 0; i < 6; ++i)
    h = hash_u64(h, room->surfaces[i].surface_digest);
  return h;
}
uint64_t
ray_compound_scene_static_room_digest(const RayCompoundSceneStaticRoom *room) {
  if (!room)
    return 0;
  uint64_t h = UINT64_C(1469598103934665603);
  h = hash_string(h, room->schema);
  h = hash_u64(h, room->schema_version);
  h = hash_string(h, room->artifact_id);
  h = hash_string(h, room->room_id);
  h = hash_string(h, room->coordinate_system);
  h = hash_u64(h, room->provenance.pair_request_digest);
  h = hash_u64(h, room->provenance.room_spec_digest);
  h = hash_u64(h, room->provenance.pair_room_result_digest);
  h = hash_u64(h, room->provenance.transform_fixture_digest);
  h = hash_u64(h, room->provenance.seed);
  h = hash_double(h, room->provenance.fixed_dt_s);
  h = hash_u64(h, room->surface_count);
  h = hash_u64(h, room->surface_set_digest);
  for (size_t i = 0; i < 6; ++i)
    h = hash_u64(h, room->surfaces[i].surface_digest);
  return h;
}

static bool surface_valid(const RayCompoundSceneStaticRoomSurface *s,
                          size_t i) {
  RayCompoundSceneVec3 o, n, u, v;
  double hu = 0, hv = 0;
  return s && s->role == (RayCompoundSceneStaticRoomRole)i &&
         !strcmp(s->surface_id, role_name(i)) &&
         s->body_id == RAY_COMPOUND_SCENE_STATIC_ROOM_BODY_ID_BASE + (int)i &&
         s->contact_mask_bit == (uint8_t)(1u << i) &&
         s->collision_box_orientation.w == 1 &&
         s->collision_box_orientation.x == 0 &&
         s->collision_box_orientation.y == 0 &&
         s->collision_box_orientation.z == 0 &&
         derive_plane(s->role, s->collision_box_center_m,
                      s->collision_box_half_extent_m, &o, &n, &u, &v, &hu,
                      &hv) &&
         same_vec3(s->interior_plane_origin_m, o) &&
         same_vec3(s->inward_normal, n) && same_vec3(s->tangent_u, u) &&
         same_vec3(s->tangent_v, v) && s->half_extent_u_m == hu &&
         s->half_extent_v_m == hv && isfinite(s->restitution) &&
         s->restitution >= 0 && s->restitution <= 1 && isfinite(s->friction) &&
         s->friction >= 0 && s->surface_digest &&
         s->surface_digest == ray_compound_scene_static_room_surface_digest(s);
}
bool ray_compound_scene_static_room_validate(
    const RayCompoundSceneStaticRoom *room) {
  if (!room || strcmp(room->schema, RAY_COMPOUND_SCENE_STATIC_ROOM_SCHEMA) ||
      room->schema_version != 1 ||
      strcmp(room->artifact_id, RAY_COMPOUND_SCENE_STATIC_ROOM_ID) ||
      strcmp(room->room_id, RAY_COMPOUND_SCENE_STATIC_ROOM_ROOM_ID) ||
      strcmp(room->coordinate_system,
             RAY_COMPOUND_SCENE_STATIC_ROOM_COORDINATE_SYSTEM) ||
      !room->provenance.pair_request_digest ||
      !room->provenance.room_spec_digest ||
      !room->provenance.pair_room_result_digest ||
      !room->provenance.transform_fixture_digest || !room->provenance.seed ||
      room->provenance.fixed_dt_s != 1.0 / 240.0 || room->surface_count != 6)
    return false;
  for (size_t i = 0; i < 6; ++i)
    if (!surface_valid(&room->surfaces[i], i))
      return false;
  return room->surface_set_digest ==
             RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_SET_DIGEST &&
         room->artifact_digest ==
             RAY_COMPOUND_SCENE_STATIC_ROOM_ARTIFACT_DIGEST &&
         room->surface_set_digest ==
             ray_compound_scene_static_room_surface_set_digest(room) &&
         room->artifact_digest == ray_compound_scene_static_room_digest(room);
}

static void get(Reader *r, void *out, size_t n) {
  if (!r || !r->ok || !out || n > r->size - r->offset) {
    if (r)
      r->ok = false;
    return;
  }
  memcpy(out, r->bytes + r->offset, n);
  r->offset += n;
}
static uint32_t u32(Reader *r) {
  unsigned char b[4] = {0};
  get(r, b, 4);
  uint32_t v = 0;
  for (size_t i = 0; i < 4; ++i)
    v |= (uint32_t)b[i] << (8u * i);
  return v;
}
static uint64_t u64(Reader *r) {
  unsigned char b[8] = {0};
  get(r, b, 8);
  uint64_t v = 0;
  for (size_t i = 0; i < 8; ++i)
    v |= (uint64_t)b[i] << (8u * i);
  return v;
}
static double f64(Reader *r) {
  uint64_t b = u64(r);
  double v = 0;
  memcpy(&v, &b, 8);
  return v;
}
static void str(Reader *r, char *out, size_t cap) {
  uint32_t n = u32(r);
  if (!r->ok || !out || !cap || n >= cap || n > r->size - r->offset) {
    r->ok = false;
    return;
  }
  get(r, out, n);
  out[n] = 0;
}
static RayCompoundSceneVec3 vec3(Reader *r) {
  return (RayCompoundSceneVec3){f64(r), f64(r), f64(r)};
}
static RayCompoundSceneQuat quat(Reader *r) {
  return (RayCompoundSceneQuat){f64(r), f64(r), f64(r), f64(r)};
}
static bool decode(const unsigned char *p, size_t n,
                   RayCompoundSceneStaticRoom *out) {
  Reader r = {p, n, 0, true};
  RayCompoundSceneStaticRoom x;
  ray_compound_scene_static_room_init(&x);
  str(&r, x.schema, sizeof x.schema);
  x.schema_version = u32(&r);
  str(&r, x.artifact_id, sizeof x.artifact_id);
  str(&r, x.room_id, sizeof x.room_id);
  str(&r, x.coordinate_system, sizeof x.coordinate_system);
  x.provenance.pair_request_digest = u64(&r);
  x.provenance.room_spec_digest = u64(&r);
  x.provenance.pair_room_result_digest = u64(&r);
  x.provenance.transform_fixture_digest = u64(&r);
  x.provenance.seed = u64(&r);
  x.provenance.fixed_dt_s = f64(&r);
  x.surface_count = u32(&r);
  if (x.surface_count != 6)
    r.ok = false;
  for (size_t i = 0; r.ok && i < 6; ++i) {
    RayCompoundSceneStaticRoomSurface *s = &x.surfaces[i];
    s->role = (RayCompoundSceneStaticRoomRole)u32(&r);
    str(&r, s->surface_id, sizeof s->surface_id);
    s->body_id = (int)u32(&r);
    s->contact_mask_bit = (uint8_t)u32(&r);
    s->collision_box_center_m = vec3(&r);
    s->collision_box_half_extent_m = vec3(&r);
    s->collision_box_orientation = quat(&r);
    s->interior_plane_origin_m = vec3(&r);
    s->inward_normal = vec3(&r);
    s->tangent_u = vec3(&r);
    s->tangent_v = vec3(&r);
    s->half_extent_u_m = f64(&r);
    s->half_extent_v_m = f64(&r);
    s->restitution = f64(&r);
    s->friction = f64(&r);
    s->surface_digest = u64(&r);
  }
  x.surface_set_digest = u64(&r);
  x.artifact_digest = u64(&r);
  if (!r.ok || r.offset != r.size ||
      !ray_compound_scene_static_room_validate(&x))
    return false;
  *out = x;
  return true;
}
static int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}
bool ray_compound_scene_static_room_parse(
    const char *text, RayCompoundSceneStaticRoom *out,
    RayCompoundSceneStaticRoomImportFailure *failure) {
  fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_NONE);
  if (!text || !out) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_INPUT);
    return false;
  }
  const char *prefix =
      RAY_COMPOUND_SCENE_STATIC_ROOM_SCHEMA "\ncodec_version=1\npayload_hex=";
  size_t pn = strlen(prefix);
  const char *payload = !strncmp(text, prefix, pn) ? text + pn : NULL;
  const char *digest = payload ? strstr(payload, "\npayload_digest=") : NULL;
  if (!digest || ((size_t)(digest - payload) & 1u) ||
      strlen(digest) != strlen("\npayload_digest=") + 17u ||
      digest[strlen("\npayload_digest=") + 16u] != '\n') {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
    return false;
  }
  size_t n = (size_t)(digest - payload) / 2u;
  if (n > RAY_COMPOUND_SCENE_STATIC_ROOM_PAYLOAD_CAPACITY) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
    return false;
  }
  unsigned char bytes[RAY_COMPOUND_SCENE_STATIC_ROOM_PAYLOAD_CAPACITY];
  for (size_t i = 0; i < n; ++i) {
    int hi = hex(payload[2 * i]), lo = hex(payload[2 * i + 1]);
    if (hi < 0 || lo < 0) {
      fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
      return false;
    }
    bytes[i] = (unsigned char)((hi << 4) | lo);
  }
  uint64_t wanted = 0;
  const char *d = digest + strlen("\npayload_digest=");
  for (size_t i = 0; i < 16; ++i) {
    int x = hex(d[i]);
    if (x < 0) {
      fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
      return false;
    }
    wanted = (wanted << 4) | (uint64_t)x;
  }
  if (hash_bytes(UINT64_C(1469598103934665603), bytes, n) != wanted) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
    return false;
  }
  RayCompoundSceneStaticRoom candidate;
  if (!decode(bytes, n, &candidate)) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_PROVENANCE);
    return false;
  }
  *out = candidate;
  return true;
}
bool ray_compound_scene_static_room_read(
    const char *path, RayCompoundSceneStaticRoom *out,
    RayCompoundSceneStaticRoomImportFailure *failure) {
  fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_NONE);
  if (!path || !out) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_INPUT);
    return false;
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_IO);
    return false;
  }
  char *text = malloc(RAY_COMPOUND_SCENE_STATIC_ROOM_TEXT_CAPACITY);
  if (!text) {
    fclose(f);
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_IO);
    return false;
  }
  size_t n =
      fread(text, 1, RAY_COMPOUND_SCENE_STATIC_ROOM_TEXT_CAPACITY - 1u, f);
  bool ok = !ferror(f) && feof(f) && fclose(f) == 0;
  text[n] = 0;
  bool parsed = ok && ray_compound_scene_static_room_parse(text, out, failure);
  free(text);
  if (!ok)
    fail(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_IO);
  return parsed;
}
const char *ray_compound_scene_static_room_import_failure_name(
    RayCompoundSceneStaticRoomImportFailure f) {
  static const char *n[] = {"none",     "input",      "io",
                            "envelope", "provenance", "geometry"};
  return (unsigned)f < 6u ? n[f] : "unknown";
}
