#include "render/compound_scene_room_geometry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "room geometry contract failed line=%d check=%s\n",      \
              __LINE__, #condition);                                           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static double dot(RayCompoundSceneVec3 a, RayCompoundSceneVec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static RayCompoundSceneVec3 add_scaled(RayCompoundSceneVec3 origin,
                                       RayCompoundSceneVec3 u, double su,
                                       RayCompoundSceneVec3 v, double sv) {
  return (RayCompoundSceneVec3){origin.x + u.x * su + v.x * sv,
                                origin.y + u.y * su + v.y * sv,
                                origin.z + u.z * su + v.z * sv};
}

int main(int argc, char **argv) {
  CHECK(argc == 3);
  RayCompoundSceneHandoff handoff;
  RayCompoundSceneImportFailure handoff_failure;
  RayCompoundSceneStaticRoom source_room;
  RayCompoundSceneStaticRoomImportFailure room_failure;
  RayCompoundSceneRoomBasis basis;
  RayCompoundSceneMappedRoom mapped;
  RayCompoundSceneRoomBasisFailure basis_failure;
  RayCompoundSceneRoomGeometry geometry;
  RayCompoundSceneRoomGeometryFailure geometry_failure;
  ray_compound_scene_handoff_init(&handoff);
  ray_compound_scene_static_room_init(&source_room);
  CHECK(ray_compound_scene_handoff_read(argv[1], &handoff, &handoff_failure));
  CHECK(ray_compound_scene_static_room_read(argv[2], &source_room,
                                            &room_failure));
  ray_compound_scene_room_basis_init(&basis);
  CHECK(ray_compound_scene_room_basis_bind(&handoff, &source_room, &basis,
                                           &mapped, &basis_failure));
  CHECK(ray_compound_scene_room_geometry_build(
      &handoff, &source_room, &basis, &mapped, &geometry, &geometry_failure));
  CHECK(ray_compound_scene_room_geometry_validate(&geometry, &mapped));
  CHECK(geometry.plane_count == 6u);
  CHECK(geometry.visible_plane_count == 5u);
  CHECK(geometry.source_surface_set_digest == UINT64_C(0xc8bb28d5a68a6511));
  CHECK(geometry.source_artifact_digest == UINT64_C(0x9f2a72c9dba0bab3));
  CHECK(geometry.basis_digest == UINT64_C(0xbbbb6543d3f3ccd2));
    CHECK(geometry.mapped_room_digest == UINT64_C(0xa37ee71b3810d6ee));
    CHECK(geometry.geometry_digest == UINT64_C(0x6d3bd95497f8e37b));
  for (size_t i = 0; i < geometry.plane_count; ++i) {
    const RayCompoundSceneRoomPlane *plane = &geometry.planes[i];
    const RayCompoundSceneStaticRoomSurface *surface = &mapped.surfaces[i];
    CHECK(plane->producer_surface_digest == surface->surface_digest);
    CHECK(plane->producer_body_id == 4300 + (int)i);
    CHECK(plane->contact_mask_bit == (uint8_t)(1u << i));
    CHECK(plane->render_visible == (i != RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX));
    for (int su = -1; su <= 1; su += 2) {
      for (int sv = -1; sv <= 1; sv += 2) {
        RayCompoundSceneVec3 corner = add_scaled(
            plane->origin_m, plane->axis_u, su * plane->width_m * 0.5,
            plane->axis_v, sv * plane->height_m * 0.5);
        RayCompoundSceneVec3 delta = {
            corner.x - surface->interior_plane_origin_m.x,
            corner.y - surface->interior_plane_origin_m.y,
            corner.z - surface->interior_plane_origin_m.z};
        CHECK(fabs(dot(delta, surface->inward_normal)) <= 1e-12);
      }
    }
  }
  const RayCompoundSceneRoomPlane *opening =
      ray_compound_scene_room_geometry_find_role(
          &geometry, RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX);
  CHECK(opening && !opening->render_visible);
  CHECK(opening->origin_m.y == -6.0);
  CHECK(opening->inward_normal.y == 1.0);
  CHECK(geometry.geometry_digest != 0);

  RayCompoundSceneRoomGeometry sentinel = geometry;
  RayCompoundSceneMappedRoom tampered = mapped;
  tampered.surfaces[0].interior_plane_origin_m.z += 0.01;
  CHECK(!ray_compound_scene_room_geometry_build(
      &handoff, &source_room, &basis, &tampered, &geometry, &geometry_failure));
  CHECK(geometry_failure == RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PROVENANCE);
  CHECK(!memcmp(&geometry, &sentinel, sizeof(geometry)));

  RayCompoundSceneRoomGeometry bad_visibility = sentinel;
  bad_visibility.planes[RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX].render_visible =
      true;
  bad_visibility.visible_plane_count = 6u;
  bad_visibility.geometry_digest =
      ray_compound_scene_room_geometry_digest(&bad_visibility);
  CHECK(!ray_compound_scene_room_geometry_validate(&bad_visibility, &mapped));

  printf("compound scene room geometry contract passed digest=%016llx "
         "planes=6 visible=5 opening=z_max\n",
         (unsigned long long)sentinel.geometry_digest);
  ray_compound_scene_handoff_free(&handoff);
  return 0;
}
