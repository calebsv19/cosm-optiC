#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/compound_scene_handoff_import.h"

#define RAY_COMPOUND_SCENE_STATIC_ROOM_SCHEMA                                  \
  "ball_compound_scene_static_room_v1"
#define RAY_COMPOUND_SCENE_STATIC_ROOM_ID "phase43_pair_room_static_room_v1"
#define RAY_COMPOUND_SCENE_STATIC_ROOM_ROOM_ID                                 \
  "phase43_pair_room_2_dynamic_6_static_v1"
#define RAY_COMPOUND_SCENE_STATIC_ROOM_COORDINATE_SYSTEM                       \
  "right_handed_y_up_meters"
#define RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_SET_DIGEST                      \
  UINT64_C(0xc8bb28d5a68a6511)
#define RAY_COMPOUND_SCENE_STATIC_ROOM_ARTIFACT_DIGEST                         \
  UINT64_C(0x9f2a72c9dba0bab3)

enum {
  RAY_COMPOUND_SCENE_STATIC_ROOM_VERSION = 1,
  RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT = 6,
  RAY_COMPOUND_SCENE_STATIC_ROOM_TEXT_CAPACITY = 16384,
  RAY_COMPOUND_SCENE_STATIC_ROOM_PAYLOAD_CAPACITY = 4096,
  RAY_COMPOUND_SCENE_STATIC_ROOM_BODY_ID_BASE = 4300
};

typedef enum RayCompoundSceneStaticRoomRole {
  RAY_COMPOUND_SCENE_STATIC_ROOM_FLOOR = 0,
  RAY_COMPOUND_SCENE_STATIC_ROOM_CEILING,
  RAY_COMPOUND_SCENE_STATIC_ROOM_X_MIN,
  RAY_COMPOUND_SCENE_STATIC_ROOM_X_MAX,
  RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MIN,
  RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX
} RayCompoundSceneStaticRoomRole;

typedef struct RayCompoundSceneStaticRoomProvenance {
  uint64_t pair_request_digest;
  uint64_t room_spec_digest;
  uint64_t pair_room_result_digest;
  uint64_t transform_fixture_digest;
  uint64_t seed;
  double fixed_dt_s;
} RayCompoundSceneStaticRoomProvenance;

typedef struct RayCompoundSceneStaticRoomSurface {
  RayCompoundSceneStaticRoomRole role;
  char surface_id[32];
  int body_id;
  uint8_t contact_mask_bit;
  RayCompoundSceneVec3 collision_box_center_m;
  RayCompoundSceneVec3 collision_box_half_extent_m;
  RayCompoundSceneQuat collision_box_orientation;
  RayCompoundSceneVec3 interior_plane_origin_m;
  RayCompoundSceneVec3 inward_normal;
  RayCompoundSceneVec3 tangent_u;
  RayCompoundSceneVec3 tangent_v;
  double half_extent_u_m;
  double half_extent_v_m;
  double restitution;
  double friction;
  uint64_t surface_digest;
} RayCompoundSceneStaticRoomSurface;

typedef struct RayCompoundSceneStaticRoom {
  char schema[64];
  uint32_t schema_version;
  char artifact_id[64];
  char room_id[64];
  char coordinate_system[64];
  RayCompoundSceneStaticRoomProvenance provenance;
  uint32_t surface_count;
  RayCompoundSceneStaticRoomSurface
      surfaces[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT];
  uint64_t surface_set_digest;
  uint64_t artifact_digest;
} RayCompoundSceneStaticRoom;

typedef enum RayCompoundSceneStaticRoomImportFailure {
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_NONE = 0,
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_INPUT,
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_IO,
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE,
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_PROVENANCE,
  RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_GEOMETRY
} RayCompoundSceneStaticRoomImportFailure;

void ray_compound_scene_static_room_init(RayCompoundSceneStaticRoom *room);
uint64_t ray_compound_scene_static_room_surface_digest(
    const RayCompoundSceneStaticRoomSurface *surface);
uint64_t ray_compound_scene_static_room_surface_set_digest(
    const RayCompoundSceneStaticRoom *room);
uint64_t
ray_compound_scene_static_room_digest(const RayCompoundSceneStaticRoom *room);
bool ray_compound_scene_static_room_validate(
    const RayCompoundSceneStaticRoom *room);
bool ray_compound_scene_static_room_parse(
    const char *text, RayCompoundSceneStaticRoom *output,
    RayCompoundSceneStaticRoomImportFailure *failure);
bool ray_compound_scene_static_room_read(
    const char *path, RayCompoundSceneStaticRoom *output,
    RayCompoundSceneStaticRoomImportFailure *failure);
const char *ray_compound_scene_static_room_import_failure_name(
    RayCompoundSceneStaticRoomImportFailure failure);
