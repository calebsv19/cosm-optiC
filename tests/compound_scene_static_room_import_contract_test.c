#include "import/compound_scene_room_basis.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "static room import contract failed line=%d check=%s\n", \
              __LINE__, #c);                                                   \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static uint64_t hash_bytes(uint64_t h, const void *d, size_t n) {
  const unsigned char *p = d;
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
  uint64_t b = 0;
  memcpy(&b, &v, 8);
  return hash_u64(h, b);
}
static uint64_t hash_vec3(uint64_t h, RayCompoundSceneVec3 v) {
  h = hash_double(h, v.x);
  h = hash_double(h, v.y);
  return hash_double(h, v.z);
}
static uint64_t hash_quat(uint64_t h, RayCompoundSceneQuat q) {
  h = hash_double(h, q.w);
  h = hash_double(h, q.x);
  h = hash_double(h, q.y);
  return hash_double(h, q.z);
}
static bool close_to(double a, double b) { return fabs(a - b) < 1e-12; }

static char *read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  char *t = malloc(RAY_COMPOUND_SCENE_STATIC_ROOM_TEXT_CAPACITY);
  if (!t) {
    fclose(f);
    return NULL;
  }
  size_t n = fread(t, 1, RAY_COMPOUND_SCENE_STATIC_ROOM_TEXT_CAPACITY - 1u, f);
  bool ok = !ferror(f) && feof(f) && fclose(f) == 0;
  if (!ok) {
    free(t);
    return NULL;
  }
  t[n] = 0;
  return t;
}

static int check_import(const char *room_path,
                        RayCompoundSceneStaticRoom *room) {
  RayCompoundSceneStaticRoomImportFailure failure;
  ray_compound_scene_static_room_init(room);
  CHECK(ray_compound_scene_static_room_read(room_path, room, &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_NONE);
  CHECK(ray_compound_scene_static_room_validate(room));
  CHECK(room->provenance.pair_request_digest == UINT64_C(0x56985ac703b66429));
  CHECK(room->provenance.room_spec_digest == UINT64_C(0x5ff3c3cd27903c79));
  CHECK(room->provenance.pair_room_result_digest ==
        UINT64_C(0x2cb6ed499a6bf072));
  CHECK(room->provenance.transform_fixture_digest ==
        UINT64_C(0x4c1eb5781cb35a5a));
  CHECK(room->provenance.seed == UINT64_C(0x2026072700410001));
  CHECK(room->surface_set_digest == UINT64_C(0xc8bb28d5a68a6511));
  CHECK(room->artifact_digest == UINT64_C(0x9f2a72c9dba0bab3));
  CHECK(room->surface_count == 6);
  for (size_t i = 0; i < 6; ++i) {
    CHECK(room->surfaces[i].body_id == 4300 + (int)i);
    CHECK(room->surfaces[i].contact_mask_bit == (uint8_t)(1u << i));
  }
  return 0;
}

static int check_envelope_rejections(const char *room_path) {
  char *canonical = read_text(room_path);
  CHECK(canonical);
  size_t n = strlen(canonical);
  CHECK(n > 100);
  char *tampered = malloc(n + 2);
  CHECK(tampered);
  memcpy(tampered, canonical, n + 1);
  char *p = strstr(tampered, "payload_hex=");
  CHECK(p);
  p += strlen("payload_hex=");
  p[20] = p[20] == '0' ? '1' : '0';
  RayCompoundSceneStaticRoom out;
  RayCompoundSceneStaticRoomImportFailure failure;
  CHECK(!ray_compound_scene_static_room_parse(tampered, &out, &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
  memcpy(tampered, canonical, n + 1);
  p = strstr(tampered, "payload_hex=") + strlen("payload_hex=");
  p[0] = 'A';
  CHECK(!ray_compound_scene_static_room_parse(tampered, &out, &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
  memcpy(tampered, canonical, n + 1);
  tampered[n] = 'x';
  tampered[n + 1] = 0;
  CHECK(!ray_compound_scene_static_room_parse(tampered, &out, &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
  free(tampered);
  free(canonical);
  return 0;
}

static int check_join_and_mapping(const char *handoff_path,
                                  const RayCompoundSceneStaticRoom *room) {
  RayCompoundSceneHandoff handoff;
  RayCompoundSceneImportFailure import_failure;
  ray_compound_scene_handoff_init(&handoff);
  CHECK(
      ray_compound_scene_handoff_read(handoff_path, &handoff, &import_failure));
  RayCompoundSceneRoomBasis basis;
  ray_compound_scene_room_basis_init(&basis);
  CHECK(ray_compound_scene_room_basis_validate(&basis));
  CHECK(basis.basis_digest == UINT64_C(0xbbbb6543d3f3ccd2));
  CHECK(close_to(basis.render_from_simulation.m[0][0], 1));
  CHECK(close_to(basis.render_from_simulation.m[1][2], -1));
  CHECK(close_to(basis.render_from_simulation.m[2][1], 1));
  RayCompoundSceneMappedRoom mapped;
  RayCompoundSceneRoomBasisFailure failure;
  CHECK(ray_compound_scene_room_basis_bind(&handoff, room, &basis, &mapped,
                                           &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_ROOM_BASIS_NONE);
  CHECK(
      ray_compound_scene_mapped_room_validate(&mapped, &handoff, room, &basis));
  CHECK(mapped.mapped_digest == UINT64_C(0xa37ee71b3810d6ee));
  CHECK(close_to(mapped.surfaces[0].interior_plane_origin_m.z, 0));
  CHECK(close_to(mapped.surfaces[0].inward_normal.z, 1));
  CHECK(close_to(mapped.surfaces[1].interior_plane_origin_m.z, 10));
  CHECK(close_to(mapped.surfaces[1].inward_normal.z, -1));
  CHECK(close_to(mapped.surfaces[4].interior_plane_origin_m.y, 6));
  CHECK(close_to(mapped.surfaces[4].inward_normal.y, -1));
  CHECK(close_to(mapped.surfaces[5].interior_plane_origin_m.y, -6));
  CHECK(close_to(mapped.surfaces[5].inward_normal.y, 1));
  uint64_t stream = UINT64_C(1469598103934665603);
  stream = hash_u64(stream, basis.basis_digest);
  stream = hash_u64(stream, mapped.mapped_digest);
  for (size_t i = 0; i < handoff.frame_count; ++i) {
    RayCompoundSceneFrame f, m;
    CHECK(ray_compound_scene_handoff_replay_exact(&handoff, i, &f));
    CHECK(ray_compound_scene_room_basis_map_frame(&basis, &f, &m));
    CHECK(m.tick == f.tick);
    for (size_t j = 0; j < 2; ++j) {
      CHECK(m.bodies[j].body_id == f.bodies[j].body_id);
      CHECK(close_to(m.bodies[j].position_m.x, f.bodies[j].position_m.x));
      CHECK(close_to(m.bodies[j].position_m.y, -f.bodies[j].position_m.z));
      CHECK(close_to(m.bodies[j].position_m.z, f.bodies[j].position_m.y));
      stream = hash_u64(stream, (uint64_t)m.bodies[j].body_id);
      stream = hash_vec3(stream, m.bodies[j].position_m);
      stream = hash_quat(stream, m.bodies[j].orientation);
    }
  }
  RayCompoundSceneHandoff bad_handoff = handoff;
  bad_handoff.seed ^= 1;
  bad_handoff.handoff_digest = ray_compound_scene_handoff_digest(&bad_handoff);
  RayCompoundSceneMappedRoom sentinel = mapped;
  CHECK(ray_compound_scene_handoff_validate(&bad_handoff));
  CHECK(!ray_compound_scene_room_basis_bind(&bad_handoff, room, &basis, &mapped,
                                            &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_ROOM_BASIS_PROVENANCE);
  CHECK(!memcmp(&mapped, &sentinel, sizeof mapped));
  CHECK(stream == UINT64_C(0xa5524c9ae4f83920));
  RayCompoundSceneRoomBasis bad_basis = basis;
  bad_basis.render_from_simulation.m[1][2] = 1;
  CHECK(!ray_compound_scene_room_basis_bind(&handoff, room, &bad_basis, &mapped,
                                            &failure));
  CHECK(failure == RAY_COMPOUND_SCENE_ROOM_BASIS_INVALID);
  printf("basis_digest=%016llx mapped_room_digest=%016llx "
         "mapped_stream_digest=%016llx\n",
         (unsigned long long)basis.basis_digest,
         (unsigned long long)sentinel.mapped_digest,
         (unsigned long long)stream);
  ray_compound_scene_handoff_free(&handoff);
  return 0;
}

int main(int argc, char **argv) {
  CHECK(argc == 3);
  RayCompoundSceneStaticRoom room;
  CHECK(check_import(argv[2], &room) == 0);
  CHECK(check_envelope_rejections(argv[2]) == 0);
  CHECK(check_join_and_mapping(argv[1], &room) == 0);
  puts("compound scene static room import contract passed");
  return 0;
}
