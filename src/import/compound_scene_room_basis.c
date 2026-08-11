#include "import/compound_scene_room_basis.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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
static uint64_t hash_string(uint64_t h, const char *v) {
  return hash_bytes(h, v, strlen(v) + 1u);
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
static bool near(double a, double b) { return fabs(a - b) <= 1e-12; }
static bool finite_quat(RayCompoundSceneQuat q) {
  return isfinite(q.w) && isfinite(q.x) && isfinite(q.y) && isfinite(q.z);
}
static RayCompoundSceneQuat mul(RayCompoundSceneQuat a,
                                RayCompoundSceneQuat b) {
  return (RayCompoundSceneQuat){a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
static RayCompoundSceneQuat conjugate(RayCompoundSceneQuat q) {
  return (RayCompoundSceneQuat){q.w, -q.x, -q.y, -q.z};
}

void ray_compound_scene_room_basis_init(RayCompoundSceneRoomBasis *b) {
  if (!b)
    return;
  memset(b, 0, sizeof(*b));
  snprintf(b->schema, sizeof b->schema, "%s",
           RAY_COMPOUND_SCENE_ROOM_BASIS_SCHEMA);
  b->schema_version = 1;
  snprintf(b->mapping_id, sizeof b->mapping_id, "%s",
           RAY_COMPOUND_SCENE_ROOM_BASIS_ID);
  snprintf(b->source_coordinate_system, sizeof b->source_coordinate_system,
           "%s", RAY_COMPOUND_SCENE_STATIC_ROOM_COORDINATE_SYSTEM);
  snprintf(b->target_coordinate_system, sizeof b->target_coordinate_system,
           "%s", RAY_COMPOUND_SCENE_RENDER_COORDINATE_SYSTEM);
  b->render_from_simulation.m[0][0] = 1.0;
  b->render_from_simulation.m[1][2] = -1.0;
  b->render_from_simulation.m[2][1] = 1.0;
  b->render_from_simulation_orientation =
      (RayCompoundSceneQuat){0x1.6a09e667f3bcdp-1, 0x1.6a09e667f3bcdp-1, 0, 0};
  b->basis_digest = ray_compound_scene_room_basis_digest(b);
}
uint64_t
ray_compound_scene_room_basis_digest(const RayCompoundSceneRoomBasis *b) {
  if (!b)
    return 0;
  uint64_t h = UINT64_C(1469598103934665603);
  h = hash_string(h, b->schema);
  h = hash_u64(h, b->schema_version);
  h = hash_string(h, b->mapping_id);
  h = hash_string(h, b->source_coordinate_system);
  h = hash_string(h, b->target_coordinate_system);
  for (size_t r = 0; r < 3; ++r)
    for (size_t c = 0; c < 3; ++c)
      h = hash_double(h, b->render_from_simulation.m[r][c]);
  return hash_quat(h, b->render_from_simulation_orientation);
}
bool ray_compound_scene_room_basis_validate(
    const RayCompoundSceneRoomBasis *b) {
  if (!b || strcmp(b->schema, RAY_COMPOUND_SCENE_ROOM_BASIS_SCHEMA) ||
      b->schema_version != 1 ||
      strcmp(b->mapping_id, RAY_COMPOUND_SCENE_ROOM_BASIS_ID) ||
      strcmp(b->source_coordinate_system,
             RAY_COMPOUND_SCENE_STATIC_ROOM_COORDINATE_SYSTEM) ||
      strcmp(b->target_coordinate_system,
             RAY_COMPOUND_SCENE_RENDER_COORDINATE_SYSTEM))
    return false;
  static const double expected[3][3] = {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}};
  for (size_t r = 0; r < 3; ++r)
    for (size_t c = 0; c < 3; ++c)
      if (!near(b->render_from_simulation.m[r][c], expected[r][c]))
        return false;
  const double s = 0x1.6a09e667f3bcdp-1;
  RayCompoundSceneQuat q = b->render_from_simulation_orientation;
  return finite_quat(q) && near(q.w, s) && near(q.x, s) && near(q.y, 0) &&
         near(q.z, 0) && b->basis_digest &&
         b->basis_digest == ray_compound_scene_room_basis_digest(b);
}
RayCompoundSceneVec3
ray_compound_scene_room_basis_map_vec3(const RayCompoundSceneRoomBasis *b,
                                       RayCompoundSceneVec3 v) {
  if (!ray_compound_scene_room_basis_validate(b))
    return (RayCompoundSceneVec3){NAN, NAN, NAN};
  return (RayCompoundSceneVec3){v.x, -v.z, v.y};
}
RayCompoundSceneQuat
ray_compound_scene_room_basis_map_quat(const RayCompoundSceneRoomBasis *b,
                                       RayCompoundSceneQuat q) {
  if (!ray_compound_scene_room_basis_validate(b) || !finite_quat(q))
    return (RayCompoundSceneQuat){NAN, NAN, NAN, NAN};
  return mul(mul(b->render_from_simulation_orientation, q),
             conjugate(b->render_from_simulation_orientation));
}

static RayCompoundSceneStaticRoomSurface
map_surface(const RayCompoundSceneRoomBasis *b,
            const RayCompoundSceneStaticRoomSurface *s) {
  RayCompoundSceneStaticRoomSurface m = *s;
  m.collision_box_center_m =
      ray_compound_scene_room_basis_map_vec3(b, s->collision_box_center_m);
  m.collision_box_half_extent_m = (RayCompoundSceneVec3){
      s->collision_box_half_extent_m.x, s->collision_box_half_extent_m.z,
      s->collision_box_half_extent_m.y};
  m.collision_box_orientation =
      ray_compound_scene_room_basis_map_quat(b, s->collision_box_orientation);
  m.interior_plane_origin_m =
      ray_compound_scene_room_basis_map_vec3(b, s->interior_plane_origin_m);
  m.inward_normal = ray_compound_scene_room_basis_map_vec3(b, s->inward_normal);
  m.tangent_u = ray_compound_scene_room_basis_map_vec3(b, s->tangent_u);
  m.tangent_v = ray_compound_scene_room_basis_map_vec3(b, s->tangent_v);
  return m;
}
static uint64_t mapped_digest(const RayCompoundSceneMappedRoom *m) {
  uint64_t h = UINT64_C(1469598103934665603);
  h = hash_u64(h, m->handoff_digest);
  h = hash_u64(h, m->source_artifact_digest);
  h = hash_u64(h, m->source_surface_set_digest);
  h = hash_u64(h, m->basis_digest);
  for (size_t i = 0; i < 6; ++i) {
    const RayCompoundSceneStaticRoomSurface *s = &m->surfaces[i];
    h = hash_u64(h, (uint64_t)s->role);
    h = hash_string(h, s->surface_id);
    h = hash_u64(h, (uint64_t)s->body_id);
    h = hash_vec3(h, s->collision_box_center_m);
    h = hash_vec3(h, s->collision_box_half_extent_m);
    h = hash_quat(h, s->collision_box_orientation);
    h = hash_vec3(h, s->interior_plane_origin_m);
    h = hash_vec3(h, s->inward_normal);
    h = hash_vec3(h, s->tangent_u);
    h = hash_vec3(h, s->tangent_v);
    h = hash_double(h, s->half_extent_u_m);
    h = hash_double(h, s->half_extent_v_m);
  }
  return h;
}
bool ray_compound_scene_mapped_room_validate(
    const RayCompoundSceneMappedRoom *m, const RayCompoundSceneHandoff *h,
    const RayCompoundSceneStaticRoom *r, const RayCompoundSceneRoomBasis *b) {
  if (!m || !ray_compound_scene_handoff_validate(h) ||
      !ray_compound_scene_static_room_validate(r) ||
      !ray_compound_scene_room_basis_validate(b) ||
      m->handoff_digest != h->handoff_digest ||
      m->source_artifact_digest != r->artifact_digest ||
      m->source_surface_set_digest != r->surface_set_digest ||
      m->basis_digest != b->basis_digest)
    return false;
  for (size_t i = 0; i < 6; ++i) {
    RayCompoundSceneStaticRoomSurface expected =
        map_surface(b, &r->surfaces[i]);
    const RayCompoundSceneStaticRoomSurface *got = &m->surfaces[i];
    if (got->role != expected.role ||
        strcmp(got->surface_id, expected.surface_id) ||
        got->body_id != expected.body_id ||
        memcmp(&got->collision_box_center_m, &expected.collision_box_center_m,
               sizeof got->collision_box_center_m) ||
        memcmp(&got->collision_box_half_extent_m,
               &expected.collision_box_half_extent_m,
               sizeof got->collision_box_half_extent_m) ||
        memcmp(&got->collision_box_orientation,
               &expected.collision_box_orientation,
               sizeof got->collision_box_orientation) ||
        memcmp(&got->interior_plane_origin_m, &expected.interior_plane_origin_m,
               sizeof got->interior_plane_origin_m) ||
        memcmp(&got->inward_normal, &expected.inward_normal,
               sizeof got->inward_normal) ||
        memcmp(&got->tangent_u, &expected.tangent_u, sizeof got->tangent_u) ||
        memcmp(&got->tangent_v, &expected.tangent_v, sizeof got->tangent_v))
      return false;
  }
  return m->mapped_digest && m->mapped_digest == mapped_digest(m);
}
bool ray_compound_scene_room_basis_bind(const RayCompoundSceneHandoff *h,
                                        const RayCompoundSceneStaticRoom *r,
                                        const RayCompoundSceneRoomBasis *b,
                                        RayCompoundSceneMappedRoom *out,
                                        RayCompoundSceneRoomBasisFailure *f) {
  if (f)
    *f = RAY_COMPOUND_SCENE_ROOM_BASIS_NONE;
  if (!h || !r || !b || !out) {
    if (f)
      *f = RAY_COMPOUND_SCENE_ROOM_BASIS_INPUT;
    return false;
  }
  if (!ray_compound_scene_handoff_validate(h) ||
      !ray_compound_scene_static_room_validate(r) ||
      !ray_compound_scene_room_basis_validate(b)) {
    if (f)
      *f = RAY_COMPOUND_SCENE_ROOM_BASIS_INVALID;
    return false;
  }
  if (h->fixture_digest != r->provenance.transform_fixture_digest ||
      h->seed != r->provenance.seed ||
      h->fixed_dt_s != r->provenance.fixed_dt_s) {
    if (f)
      *f = RAY_COMPOUND_SCENE_ROOM_BASIS_PROVENANCE;
    return false;
  }
  RayCompoundSceneMappedRoom m = {0};
  m.handoff_digest = h->handoff_digest;
  m.source_artifact_digest = r->artifact_digest;
  m.source_surface_set_digest = r->surface_set_digest;
  m.basis_digest = b->basis_digest;
  for (size_t i = 0; i < 6; ++i)
    m.surfaces[i] = map_surface(b, &r->surfaces[i]);
  m.mapped_digest = mapped_digest(&m);
  if (!ray_compound_scene_mapped_room_validate(&m, h, r, b)) {
    if (f)
      *f = RAY_COMPOUND_SCENE_ROOM_BASIS_INVALID;
    return false;
  }
  *out = m;
  return true;
}
bool ray_compound_scene_room_basis_map_frame(
    const RayCompoundSceneRoomBasis *b, const RayCompoundSceneFrame *source,
    RayCompoundSceneFrame *out) {
  if (!ray_compound_scene_room_basis_validate(b) || !source || !out)
    return false;
  RayCompoundSceneFrame m = *source;
  for (size_t i = 0; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
    m.bodies[i].position_m =
        ray_compound_scene_room_basis_map_vec3(b, source->bodies[i].position_m);
    m.bodies[i].orientation = ray_compound_scene_room_basis_map_quat(
        b, source->bodies[i].orientation);
  }
  *out = m;
  return true;
}
