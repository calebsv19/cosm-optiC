#pragma once

#include "import/compound_scene_static_room_import.h"

#define RAY_COMPOUND_SCENE_ROOM_BASIS_SCHEMA                                   \
  "ray_tracing_compound_scene_room_basis_v1"
#define RAY_COMPOUND_SCENE_ROOM_BASIS_ID "ball_y_up_to_ray_z_up_v1"
#define RAY_COMPOUND_SCENE_RENDER_COORDINATE_SYSTEM "right_handed_z_up_meters"

typedef struct RayCompoundSceneRoomBasis {
  char schema[64];
  uint32_t schema_version;
  char mapping_id[64];
  char source_coordinate_system[64];
  char target_coordinate_system[64];
  RayCompoundSceneMat3 render_from_simulation;
  RayCompoundSceneQuat render_from_simulation_orientation;
  uint64_t basis_digest;
} RayCompoundSceneRoomBasis;

typedef struct RayCompoundSceneMappedRoom {
  uint64_t handoff_digest;
  uint64_t source_artifact_digest;
  uint64_t source_surface_set_digest;
  uint64_t basis_digest;
  RayCompoundSceneStaticRoomSurface
      surfaces[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT];
  uint64_t mapped_digest;
} RayCompoundSceneMappedRoom;

typedef enum RayCompoundSceneRoomBasisFailure {
  RAY_COMPOUND_SCENE_ROOM_BASIS_NONE = 0,
  RAY_COMPOUND_SCENE_ROOM_BASIS_INPUT,
  RAY_COMPOUND_SCENE_ROOM_BASIS_INVALID,
  RAY_COMPOUND_SCENE_ROOM_BASIS_PROVENANCE
} RayCompoundSceneRoomBasisFailure;

void ray_compound_scene_room_basis_init(RayCompoundSceneRoomBasis *basis);
uint64_t
ray_compound_scene_room_basis_digest(const RayCompoundSceneRoomBasis *basis);
bool ray_compound_scene_room_basis_validate(
    const RayCompoundSceneRoomBasis *basis);
RayCompoundSceneVec3
ray_compound_scene_room_basis_map_vec3(const RayCompoundSceneRoomBasis *basis,
                                       RayCompoundSceneVec3 value);
RayCompoundSceneQuat
ray_compound_scene_room_basis_map_quat(const RayCompoundSceneRoomBasis *basis,
                                       RayCompoundSceneQuat value);
bool ray_compound_scene_room_basis_bind(
    const RayCompoundSceneHandoff *handoff,
    const RayCompoundSceneStaticRoom *room,
    const RayCompoundSceneRoomBasis *basis, RayCompoundSceneMappedRoom *output,
    RayCompoundSceneRoomBasisFailure *failure);
bool ray_compound_scene_room_basis_map_frame(
    const RayCompoundSceneRoomBasis *basis, const RayCompoundSceneFrame *source,
    RayCompoundSceneFrame *output);
bool ray_compound_scene_mapped_room_validate(
    const RayCompoundSceneMappedRoom *mapped,
    const RayCompoundSceneHandoff *handoff,
    const RayCompoundSceneStaticRoom *room,
    const RayCompoundSceneRoomBasis *basis);
