#include "render/runtime_water_contact_3d.h"
#include "render/runtime_material_payload_3d.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Larger than camera/photon offsets; this is numerical overlap, not an air
 * film. Water owns the overlap and the named container is its underlay. */
static const double overlap = 1e-4;
static const double max_clearance = .01;
static double component(Vec3 p, int axis) {
  return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
}
typedef struct {
  double plane, lo[3], hi[3], area;
  int count;
} ContactFace;

/* Accept a complete rectangular planar face, not proximity to arbitrary
 * geometry. Area/bounds checks also reject partial walls and ordinary holes. */
static bool face(const RuntimeScene3D *scene, int object, int axis, int sign,
                 double reference, double reach, ContactFace *out) {
  double best = INFINITY;
  memset(out, 0, sizeof(*out));
  for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
    const RuntimeTriangle3D *t = &scene->triangleMesh.triangles[i];
    if (t->sceneObjectIndex != object)
      continue;
    Vec3 n = vec3_normalize(
        vec3_cross(vec3_sub(t->p1, t->p0), vec3_sub(t->p2, t->p0)));
    double p = component(t->p0, axis);
    double distance = (reference - p) * sign;
    if (component(n, axis) * sign < 1 - 1e-8 || distance < -1e-8 ||
        distance > reach || fabs(component(t->p1, axis) - p) > 1e-8 ||
        fabs(component(t->p2, axis) - p) > 1e-8)
      continue;
    if (distance < best) {
      best = distance;
      out->plane = p;
    }
  }
  if (!isfinite(best))
    return false;
  for (int k = 0; k < 3; ++k) {
    out->lo[k] = INFINITY;
    out->hi[k] = -INFINITY;
  }
  for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
    const RuntimeTriangle3D *t = &scene->triangleMesh.triangles[i];
    if (t->sceneObjectIndex != object)
      continue;
    Vec3 cross = vec3_cross(vec3_sub(t->p1, t->p0), vec3_sub(t->p2, t->p0));
    if (component(vec3_normalize(cross), axis) * sign < 1 - 1e-8)
      continue;
    Vec3 v[3] = {t->p0, t->p1, t->p2};
    if (fabs(component(v[0], axis) - out->plane) > 1e-8 ||
        fabs(component(v[1], axis) - out->plane) > 1e-8 ||
        fabs(component(v[2], axis) - out->plane) > 1e-8)
      continue;
    ++out->count;
    out->area += .5 * sqrt(vec3_dot(cross, cross));
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k) {
        out->lo[k] = fmin(out->lo[k], component(v[j], k));
        out->hi[k] = fmax(out->hi[k], component(v[j], k));
      }
  }
  int a = (axis + 1) % 3, b = (axis + 2) % 3;
  double rectangle = (out->hi[a] - out->lo[a]) * (out->hi[b] - out->lo[b]);
  /* Exactly two triangles with rectangle corner vertices: deliberately bounded
   * support, avoiding area-only acceptance of overlapping/holey tessellations.
   */
  if (out->count != 2 || rectangle <= 0 ||
      fabs(out->area - rectangle) > rectangle * 1e-8)
    return false;
  unsigned corners = 0, shared_corners = 15u;
  for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
    const RuntimeTriangle3D *t = &scene->triangleMesh.triangles[i];
    if (t->sceneObjectIndex != object)
      continue;
    Vec3 v[3] = {t->p0, t->p1, t->p2};
    Vec3 n =
        vec3_normalize(vec3_cross(vec3_sub(v[1], v[0]), vec3_sub(v[2], v[0])));
    if (component(n, axis) * sign < 1 - 1e-8 ||
        fabs(component(v[0], axis) - out->plane) > 1e-8)
      continue;
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k)
        if (k != axis && fmin(fabs(component(v[j], k) - out->lo[k]),
                              fabs(component(v[j], k) - out->hi[k])) > 1e-8)
          return false;
    unsigned triangle_corners = 0;
    for (int j = 0; j < 3; ++j)
      triangle_corners |=
          1u << ((fabs(component(v[j], a) - out->hi[a]) < 1e-8 ? 1 : 0) |
                 (fabs(component(v[j], b) - out->hi[b]) < 1e-8 ? 2 : 0));
    corners |= triangle_corners;
    shared_corners &= triangle_corners;
  }
  /* Both triangles must share a diagonal, not a perimeter edge. */
  return corners == 15u && (shared_corners == 9u || shared_corners == 6u);
}

bool RuntimeWaterContact3D_Prepare(const RuntimeScene3D *scene,
                                   const RuntimeWaterBodyBoundaryV1 *b,
                                   RuntimeWaterBodyMesh3DDesc *d,
                                   int *out_index, char *diagnostics,
                                   size_t size) {
  if (out_index)
    *out_index = -1;
  if (!scene || !b || !d || !out_index)
    return false;
  if (!b->wet_container_contact)
    return true;
  if (d->grid_w < 2 || d->grid_d < 2 || !d->heights_y ||
      !d->map_y_height_to_scene_z)
    return false;
  const char *reason = "container identity";
  int face_index = -1;
  int object = -1;
  for (int i = 0; i < scene->primitiveCount; ++i)
    if (!strcmp(scene->primitives[i].source.objectId, b->container_id)) {
      int index = scene->primitives[i].source.sceneObjectIndex;
      if (object >= 0 && object != index)
        goto invalid;
      object = index;
    }
  RuntimeMaterialPayload3D material;
  reason = "solid explicit glass material";
  if (object < 0 || object == d->scene_object_index ||
      !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(object,
                                                            &material) ||
      !material.hasGlassTransportOverride || material.thinWalled ||
      material.transparency <= 0)
    goto invalid;
  double lo[3] = {d->sample_origin_x, d->sample_origin_z, d->bottom_height};
  double hi[3] = {lo[0] + d->sample_spacing_x * (d->grid_w - 1),
                  lo[1] + d->sample_spacing_z * (d->grid_d - 1), -INFINITY};
  for (uint64_t i = 0; i < (uint64_t)d->grid_w * d->grid_d; ++i)
    hi[2] = fmax(hi[2], d->heights_y[i]);
  ContactFace inner[5], outer;
  int axes[5] = {0, 0, 1, 1, 2}, signs[5] = {1, -1, 1, -1, 1};
  for (int i = 0; i < 5; ++i) {
    int axis = axes[i], sign = signs[i];
    face_index = i;
    reason = "inner rectangular face within 10 mm";
    double ref = sign > 0 ? lo[axis] : hi[axis];
    if (!face(scene, object, axis, sign, ref, max_clearance, &inner[i]))
      goto invalid;
    /* Find the opposing outer face from a point beyond the wall. */
    double far = inner[i].plane - sign * .5;
    reason = "outer rectangular face and wall thickness";
    if (!face(scene, object, axis, -sign, far, .5, &outer) ||
        (inner[i].plane - outer.plane) * sign < 4 * overlap)
      goto invalid;
    reason = "wall coverage of water bounds";
    for (int k = 0; k < 3; ++k)
      if (k != axis &&
          (inner[i].lo[k] > lo[k] + max_clearance ||
           inner[i].hi[k] < hi[k] - 1e-8 ||
           outer.lo[k] > lo[k] + max_clearance || outer.hi[k] < hi[k] - 1e-8))
        goto invalid;
  }
  reason = "inner wall corner coverage";
  lo[0] = inner[0].plane;
  hi[0] = inner[1].plane;
  lo[1] = inner[2].plane;
  hi[1] = inner[3].plane;
  lo[2] = inner[4].plane;
  for (int i = 0; i < 5; ++i)
    for (int k = 0; k < 3; ++k)
      if (k != axes[i] &&
          (inner[i].lo[k] > lo[k] + 1e-8 || inner[i].hi[k] < hi[k] - 1e-8))
        goto invalid;
  d->sample_origin_x = inner[0].plane - overlap;
  d->sample_origin_z = inner[2].plane - overlap;
  d->sample_spacing_x =
      (inner[1].plane + overlap - d->sample_origin_x) / (d->grid_w - 1);
  d->sample_spacing_z =
      (inner[3].plane + overlap - d->sample_origin_z) / (d->grid_d - 1);
  d->bottom_height = inner[4].plane - overlap;
  *out_index = object;
  return true;
invalid:
  if (diagnostics && size)
    snprintf(diagnostics, size, "wet contact validation failed: %s (face %d)",
             reason, face_index);
  return false;
}
