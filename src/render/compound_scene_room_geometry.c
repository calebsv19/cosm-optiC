#include "render/compound_scene_room_geometry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

static void set_failure(RayCompoundSceneRoomGeometryFailure *failure,
                        RayCompoundSceneRoomGeometryFailure value) {
  if (failure)
    *failure = value;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
  const unsigned char *bytes = data;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

static uint64_t hash_string(uint64_t hash, const char *value) {
  return hash_bytes(hash, value, strlen(value) + 1u);
}

static uint64_t hash_vec3(uint64_t hash, RayCompoundSceneVec3 value) {
  hash = hash_bytes(hash, &value.x, sizeof(value.x));
  hash = hash_bytes(hash, &value.y, sizeof(value.y));
  return hash_bytes(hash, &value.z, sizeof(value.z));
}

static double dot(RayCompoundSceneVec3 a, RayCompoundSceneVec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool unit(RayCompoundSceneVec3 value) {
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z) &&
         fabs(dot(value, value) - 1.0) <= 1e-12;
}

static bool same_vec3(RayCompoundSceneVec3 a, RayCompoundSceneVec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static const char *role_name(RayCompoundSceneStaticRoomRole role) {
  static const char *names[] = {"floor", "ceiling", "x_min",
                                "x_max", "z_min",   "z_max"};
  return (unsigned)role < RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT
             ? names[(size_t)role]
             : NULL;
}

uint64_t ray_compound_scene_room_geometry_digest(
    const RayCompoundSceneRoomGeometry *geometry) {
  if (!geometry)
    return 0;
  uint64_t hash = FNV_OFFSET;
  hash = hash_string(hash, geometry->schema);
  hash = hash_string(hash, geometry->visibility_policy);
  hash = hash_bytes(hash, &geometry->handoff_digest,
                    sizeof(geometry->handoff_digest));
  hash = hash_bytes(hash, &geometry->source_artifact_digest,
                    sizeof(geometry->source_artifact_digest));
  hash = hash_bytes(hash, &geometry->source_surface_set_digest,
                    sizeof(geometry->source_surface_set_digest));
  hash =
      hash_bytes(hash, &geometry->basis_digest, sizeof(geometry->basis_digest));
  hash = hash_bytes(hash, &geometry->mapped_room_digest,
                    sizeof(geometry->mapped_room_digest));
  hash =
      hash_bytes(hash, &geometry->plane_count, sizeof(geometry->plane_count));
  hash = hash_bytes(hash, &geometry->visible_plane_count,
                    sizeof(geometry->visible_plane_count));
  for (size_t i = 0; i < geometry->plane_count; ++i) {
    const RayCompoundSceneRoomPlane *plane = &geometry->planes[i];
    hash = hash_bytes(hash, &plane->role, sizeof(plane->role));
    hash = hash_string(hash, plane->object_id);
    hash = hash_string(hash, plane->material_id);
    hash = hash_bytes(hash, &plane->producer_body_id,
                      sizeof(plane->producer_body_id));
    hash = hash_bytes(hash, &plane->contact_mask_bit,
                      sizeof(plane->contact_mask_bit));
    hash = hash_bytes(hash, &plane->producer_surface_digest,
                      sizeof(plane->producer_surface_digest));
    hash = hash_vec3(hash, plane->origin_m);
    hash = hash_vec3(hash, plane->axis_u);
    hash = hash_vec3(hash, plane->axis_v);
    hash = hash_vec3(hash, plane->inward_normal);
    hash = hash_bytes(hash, &plane->width_m, sizeof(plane->width_m));
    hash = hash_bytes(hash, &plane->height_m, sizeof(plane->height_m));
    hash =
        hash_bytes(hash, &plane->render_visible, sizeof(plane->render_visible));
  }
  return hash;
}

static bool plane_matches(const RayCompoundSceneRoomPlane *plane,
                          const RayCompoundSceneStaticRoomSurface *surface,
                          size_t index) {
  const bool expected_visible = index != RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX;
  return plane && surface && plane->role == surface->role &&
         plane->role == (RayCompoundSceneStaticRoomRole)index &&
         plane->producer_body_id == surface->body_id &&
         plane->contact_mask_bit == surface->contact_mask_bit &&
         plane->producer_surface_digest == surface->surface_digest &&
         same_vec3(plane->origin_m, surface->interior_plane_origin_m) &&
         same_vec3(plane->axis_u, surface->tangent_u) &&
         same_vec3(plane->axis_v, surface->tangent_v) &&
         same_vec3(plane->inward_normal, surface->inward_normal) &&
         plane->width_m == 2.0 * surface->half_extent_u_m &&
         plane->height_m == 2.0 * surface->half_extent_v_m &&
         plane->render_visible == expected_visible && unit(plane->axis_u) &&
         unit(plane->axis_v) && unit(plane->inward_normal) &&
         fabs(dot(plane->axis_u, plane->axis_v)) <= 1e-12 &&
         fabs(dot(plane->axis_u, plane->inward_normal)) <= 1e-12 &&
         fabs(dot(plane->axis_v, plane->inward_normal)) <= 1e-12;
}

bool ray_compound_scene_room_geometry_validate(
    const RayCompoundSceneRoomGeometry *geometry,
    const RayCompoundSceneMappedRoom *mapped_room) {
  if (!geometry || !mapped_room || !geometry->valid ||
      strcmp(geometry->schema, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_SCHEMA) ||
      strcmp(geometry->visibility_policy,
             RAY_COMPOUND_SCENE_ROOM_VISIBILITY_POLICY) ||
      geometry->handoff_digest != mapped_room->handoff_digest ||
      geometry->source_artifact_digest != mapped_room->source_artifact_digest ||
      geometry->source_surface_set_digest !=
          mapped_room->source_surface_set_digest ||
      geometry->basis_digest != mapped_room->basis_digest ||
      geometry->mapped_room_digest != mapped_room->mapped_digest ||
      geometry->plane_count != RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT ||
      geometry->visible_plane_count != 5u)
    return false;
  for (size_t i = 0; i < geometry->plane_count; ++i)
    if (!plane_matches(&geometry->planes[i], &mapped_room->surfaces[i], i))
      return false;
  return geometry->geometry_digest != 0 &&
         geometry->geometry_digest ==
             ray_compound_scene_room_geometry_digest(geometry);
}

bool ray_compound_scene_room_geometry_build(
    const RayCompoundSceneHandoff *handoff,
    const RayCompoundSceneStaticRoom *source_room,
    const RayCompoundSceneRoomBasis *basis,
    const RayCompoundSceneMappedRoom *mapped_room,
    RayCompoundSceneRoomGeometry *output,
    RayCompoundSceneRoomGeometryFailure *failure) {
  set_failure(failure, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_NONE);
  if (!handoff || !source_room || !basis || !mapped_room || !output) {
    set_failure(failure, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_INPUT);
    return false;
  }
  if (!ray_compound_scene_mapped_room_validate(mapped_room, handoff,
                                               source_room, basis)) {
    set_failure(failure, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PROVENANCE);
    return false;
  }
  RayCompoundSceneRoomGeometry candidate = {0};
  candidate.valid = true;
  snprintf(candidate.schema, sizeof(candidate.schema), "%s",
           RAY_COMPOUND_SCENE_ROOM_GEOMETRY_SCHEMA);
  snprintf(candidate.visibility_policy, sizeof(candidate.visibility_policy),
           "%s", RAY_COMPOUND_SCENE_ROOM_VISIBILITY_POLICY);
  candidate.handoff_digest = mapped_room->handoff_digest;
  candidate.source_artifact_digest = mapped_room->source_artifact_digest;
  candidate.source_surface_set_digest = mapped_room->source_surface_set_digest;
  candidate.basis_digest = mapped_room->basis_digest;
  candidate.mapped_room_digest = mapped_room->mapped_digest;
  candidate.plane_count = RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT;
  for (size_t i = 0; i < candidate.plane_count; ++i) {
    const RayCompoundSceneStaticRoomSurface *surface =
        &mapped_room->surfaces[i];
    RayCompoundSceneRoomPlane *plane = &candidate.planes[i];
    const char *name = role_name(surface->role);
    if (!name) {
      set_failure(failure, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PLANE_MATCH);
      return false;
    }
    plane->role = surface->role;
    snprintf(plane->object_id, sizeof(plane->object_id), "sim_room_%s", name);
    snprintf(plane->material_id, sizeof(plane->material_id), "mat_sim_room_%s",
             name);
    plane->producer_body_id = surface->body_id;
    plane->contact_mask_bit = surface->contact_mask_bit;
    plane->producer_surface_digest = surface->surface_digest;
    plane->origin_m = surface->interior_plane_origin_m;
    plane->axis_u = surface->tangent_u;
    plane->axis_v = surface->tangent_v;
    plane->inward_normal = surface->inward_normal;
    plane->width_m = 2.0 * surface->half_extent_u_m;
    plane->height_m = 2.0 * surface->half_extent_v_m;
    plane->render_visible =
        surface->role != RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX;
    if (plane->render_visible)
      ++candidate.visible_plane_count;
  }
  candidate.geometry_digest =
      ray_compound_scene_room_geometry_digest(&candidate);
  if (!ray_compound_scene_room_geometry_validate(&candidate, mapped_room)) {
    set_failure(failure, RAY_COMPOUND_SCENE_ROOM_GEOMETRY_PLANE_MATCH);
    return false;
  }
  *output = candidate;
  return true;
}

const RayCompoundSceneRoomPlane *ray_compound_scene_room_geometry_find_role(
    const RayCompoundSceneRoomGeometry *geometry,
    RayCompoundSceneStaticRoomRole role) {
  if (!geometry)
    return NULL;
  for (size_t i = 0; i < geometry->plane_count; ++i)
    if (geometry->planes[i].role == role)
      return &geometry->planes[i];
  return NULL;
}
