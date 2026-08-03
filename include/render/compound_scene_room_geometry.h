#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/compound_scene_room_basis.h"

#define RAY_COMPOUND_SCENE_ROOM_GEOMETRY_SCHEMA                                \
  "ray_tracing_compound_scene_room_geometry_v1"
#define RAY_COMPOUND_SCENE_ROOM_VISIBILITY_POLICY                              \
  "five_opaque_z_max_camera_opening_v1"

typedef struct RayCompoundSceneRoomPlane {
  RayCompoundSceneStaticRoomRole role;
  char object_id[64];
  char material_id[64];
  int producer_body_id;
  uint8_t contact_mask_bit;
  uint64_t producer_surface_digest;
  RayCompoundSceneVec3 origin_m;
  RayCompoundSceneVec3 axis_u;
  RayCompoundSceneVec3 axis_v;
  RayCompoundSceneVec3 inward_normal;
  double width_m;
  double height_m;
  bool render_visible;
} RayCompoundSceneRoomPlane;

typedef struct RayCompoundSceneRoomGeometry {
  bool valid;
  char schema[64];
  char visibility_policy[64];
  uint64_t handoff_digest;
  uint64_t source_artifact_digest;
  uint64_t source_surface_set_digest;
  uint64_t basis_digest;
  uint64_t mapped_room_digest;
  size_t plane_count;
  size_t visible_plane_count;
  RayCompoundSceneRoomPlane
      planes[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT];
  uint64_t geometry_digest;
} RayCompoundSceneRoomGeometry;

typedef enum RayCompoundSceneRoomGeometryFailure {
  RAY_COMPOUND_SCENE_ROOM_GEOMETRY_NONE = 0,
  RAY_COMPOUND_SCENE_ROOM_GEOMETRY_INPUT,
  RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PROVENANCE,
  RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PLANE_MATCH,
  RAY_COMPOUND_SCENE_ROOM_GEOMETRY_VISIBILITY
} RayCompoundSceneRoomGeometryFailure;

bool ray_compound_scene_room_geometry_build(
    const RayCompoundSceneHandoff *handoff,
    const RayCompoundSceneStaticRoom *source_room,
    const RayCompoundSceneRoomBasis *basis,
    const RayCompoundSceneMappedRoom *mapped_room,
    RayCompoundSceneRoomGeometry *output,
    RayCompoundSceneRoomGeometryFailure *failure);
bool ray_compound_scene_room_geometry_validate(
    const RayCompoundSceneRoomGeometry *geometry,
    const RayCompoundSceneMappedRoom *mapped_room);
uint64_t ray_compound_scene_room_geometry_digest(
    const RayCompoundSceneRoomGeometry *geometry);
const RayCompoundSceneRoomPlane *ray_compound_scene_room_geometry_find_role(
    const RayCompoundSceneRoomGeometry *geometry,
    RayCompoundSceneStaticRoomRole role);
