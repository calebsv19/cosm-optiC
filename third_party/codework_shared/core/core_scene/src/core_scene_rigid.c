#include "core_scene_rigid.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static CoreResult fail(CoreSceneRigidDiagnostic *d, const char *code,
                       const char *id, const char *message) {
  if (d) {
    snprintf(d->code, sizeof(d->code), "%s", code);
    snprintf(d->object_id, sizeof(d->object_id), "%s", id ? id : "");
    snprintf(d->message, sizeof(d->message), "%s", message);
  }
  return (CoreResult){CORE_ERR_FORMAT, code};
}
static bool token(const char *s, size_t cap) {
  size_t n = 0;
  if (!s || !s[0])
    return false;
  while (n < cap && s[n]) {
    unsigned char c = (unsigned char)s[n++];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
      return false;
  }
  return n < cap;
}
static bool hash(const char s[65]) {
  for (size_t i = 0; i < 64; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return false;
  return s[64] == 0;
}
static bool finite_values(const double *a, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (!isfinite(a[i]))
      return false;
  return true;
}
static bool zero(const double *a, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (a[i] != 0)
      return false;
  return true;
}
static bool close_values(const double *a, const double *b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (fabs(a[i] - b[i]) > 1e-10 * fmax(1, fmax(fabs(a[i]), fabs(b[i]))))
      return false;
  return true;
}
static bool basis(const double *m) {
  if (!finite_values(m, 9))
    return false;
  for (size_t i = 0; i < 3; ++i)
    for (size_t j = i; j < 3; ++j) {
      double dot = 0;
      for (size_t k = 0; k < 3; ++k)
        dot += m[3 * k + i] * m[3 * k + j];
      if (fabs(dot - (i == j ? 1 : 0)) > 1e-9)
        return false;
    }
  double det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
               m[1] * (m[3] * m[8] - m[5] * m[6]) +
               m[2] * (m[3] * m[7] - m[4] * m[6]);
  return fabs(det - 1) < 1e-9;
}
CoreResult core_scene_rigid_validate(const CoreSceneRigidContract *c,
                                     CoreSceneRigidLimits lim,
                                     CoreSceneRigidDiagnostic *d) {
  if (c && c->contact_policy != CORE_SCENE_RIGID_CONTACT_DEFAULT &&
      c->contact_policy != CORE_SCENE_RIGID_CONTACT_ZERO_CUTOFF)
    return fail(d,"contact_policy","","unsupported contact policy");
  if (c && c->integration_policy != CORE_SCENE_RIGID_INTEGRATION_DEFAULT &&
      c->integration_policy != CORE_SCENE_RIGID_INTEGRATION_BOUNDED_SUBSEGMENTS)
    return fail(d, "integration_policy", "", "unsupported integration policy");
  if (d)
    memset(d, 0, sizeof(*d));
  if (!c)
    return fail(d, "argument", "", "contract required");
  if (!token(c->scene_id, sizeof(c->scene_id)) || c->schema_version != 1 ||
      !c->rate_numerator || !c->rate_denominator || c->origin_tick != 0 ||
      !c->end_tick || !finite_values(c->gravity_mps2, 3))
    return fail(
        d, "world", "",
        "explicit finite gravity and positive rational clock/range required");
  if (!lim.body_capacity || lim.body_capacity > CORE_SCENE_RIGID_MAX_RECORDS ||
      !lim.children_per_body || !c->object_count ||
      c->object_count > CORE_SCENE_RIGID_MAX_RECORDS ||
      c->asset_count > CORE_SCENE_RIGID_MAX_RECORDS || !c->binding_count ||
      c->binding_count > CORE_SCENE_RIGID_MAX_RECORDS)
    return fail(d, "capacity", "",
                "invalid or exceeded document/consumer capacity");
  for (size_t i = 0; i < c->object_count; ++i) {
    const CoreSceneRigidObject *o = &c->objects[i];
    if (!token(o->id, sizeof(o->id)) || !finite_values(o->position, 3) ||
        !finite_values(o->rotation_deg, 3) || !finite_values(o->scale, 3))
      return fail(d, "object", "", "invalid object identity or transform");
    if (o->parent_id[0] && !token(o->parent_id, sizeof(o->parent_id)))
      return fail(d, "object", o->id, "invalid parent identity");
    if (o->mesh_instance && !token(o->mesh_id, sizeof(o->mesh_id)))
      return fail(d, "mesh_binding", o->id,
                  "mesh instance requires a mesh identity");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(o->id, c->objects[j].id))
        return fail(d, "duplicate_object", o->id, "duplicate object identity");
  }
  for (size_t i = 0; i < c->asset_count; ++i) {
    const CoreSceneRigidAsset *a = &c->assets[i];
    if (!token(a->id, sizeof(a->id)) ||
        !token(a->mesh_id, sizeof(a->mesh_id)) ||
        !token(a->source_id, sizeof(a->source_id)) ||
        !hash(a->collision_sha256) || !hash(a->source_sha256) ||
        !hash(a->mesh_sha256) || !finite_values(a->source_center_m, 3) ||
        !basis(a->principal_to_source) || !finite_values(a->inertia_per_kg, 3))
      return fail(d, "asset_metadata", "",
                  "invalid source, visible/collision identity or mass frame");
    if ((a->shape != CORE_SCENE_RIGID_CONVEX_COMPOUND &&
         a->shape != CORE_SCENE_RIGID_STATIC_TRIANGLES &&
         a->shape != CORE_SCENE_RIGID_ANALYTIC_SPHERE) ||
        !a->child_count || a->child_count > lim.children_per_body)
      return fail(d, "shape_capacity", a->id,
                  "unsupported shape or collision-child capacity");
    for (size_t k = 0; k < 3; ++k)
      if (a->inertia_per_kg[k] <= 0)
        return fail(d, "inertia", a->id, "principal inertia must be positive");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(a->id, c->assets[j].id))
        return fail(d, "duplicate_asset", a->id, "duplicate asset identity");
  }
  size_t physical = 0;
  for (size_t i = 0; i < c->binding_count; ++i) {
    const CoreSceneRigidBinding *b = &c->bindings[i];
    const CoreSceneRigidObject *o = NULL;
    const CoreSceneRigidAsset *a = NULL;
    if (!token(b->object_id, sizeof(b->object_id)))
      return fail(d, "binding", "", "invalid bound object identity");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(b->object_id, c->bindings[j].object_id))
        return fail(d, "duplicate_binding", b->object_id,
                    "one role/binding per object");
    for (size_t j = 0; j < c->object_count; ++j)
      if (!strcmp(b->object_id, c->objects[j].id))
        o = &c->objects[j];
    if (!o)
      return fail(d, "missing_object", b->object_id,
                  "binding target does not exist");
    if (b->role == CORE_SCENE_RIGID_KINEMATIC ||
        b->owner == CORE_SCENE_RIGID_PRESCRIBED)
      return fail(d, "kinematic_unsupported", b->object_id,
                  "prescribed collision motion requires a later velocity/sweep "
                  "contract");
    if (b->role == CORE_SCENE_RIGID_VISUAL_ONLY) {
      if ((o->transform_claims & (o->transform_claims - 1)) ||
          (o->transform_claims & CORE_SCENE_RIGID_OTHER_SIM_CLAIM))
        return fail(d, "motion_conflict", b->object_id,
                    "multiple transform owners on a presentation object");
      if (b->collides || b->body_id || b->owner != CORE_SCENE_RIGID_AUTHORED ||
          b->asset_id[0])
        return fail(d, "role_owner", b->object_id,
                    "visual-only requires authored ownership and no collision "
                    "body/asset");
      continue;
    }
    if ((b->role != CORE_SCENE_RIGID_STATIC &&
         b->role != CORE_SCENE_RIGID_DYNAMIC) ||
        !b->collides || !b->body_id ||
        b->owner != (b->role == CORE_SCENE_RIGID_STATIC
                         ? CORE_SCENE_RIGID_AUTHORED
                         : CORE_SCENE_RIGID_SIMULATION))
      return fail(
          d, "role_owner", b->object_id,
          "invalid physical role, collision participation or motion owner");
    if (o->transform_claims)
      return fail(d, "motion_conflict", b->object_id,
                  "authored path/track or another simulation already owns this "
                  "transform");
    const double one[3] = {1, 1, 1};
    if (o->parent_id[0] || !zero(o->rotation_deg, 3) ||
        (o->scale[0] != one[0] || o->scale[1] != one[1] ||
         o->scale[2] != one[2]))
      return fail(d, "unsupported_transform", b->object_id,
                  "physical objects require world-space COM position, zero "
                  "Euler rotation and unit baked scale");
    for (size_t j = 0; j < i; ++j)
      if (c->bindings[j].role != CORE_SCENE_RIGID_VISUAL_ONLY &&
          b->body_id == c->bindings[j].body_id)
        return fail(d, "duplicate_body", b->object_id,
                    "duplicate physical body identity");
    if (!token(b->asset_id, sizeof(b->asset_id)) ||
        !token(b->source_id, sizeof(b->source_id)) ||
        !token(b->mesh_id, sizeof(b->mesh_id)) || !hash(b->collision_sha256) ||
        !hash(b->source_sha256) || !hash(b->mesh_sha256))
      return fail(d, "asset_binding", b->object_id,
                  "missing or invalid geometry identities");
    for (size_t j = 0; j < c->asset_count; ++j)
      if (!strcmp(b->asset_id, c->assets[j].id))
        a = &c->assets[j];
    if (!a)
      return fail(d, "missing_asset", b->object_id,
                  "collision asset is absent from supplied catalog");
    if (!o->mesh_instance || strcmp(o->mesh_id, b->mesh_id) ||
        strcmp(a->mesh_id, b->mesh_id) || strcmp(a->source_id, b->source_id) ||
        strcmp(a->collision_sha256, b->collision_sha256) ||
        strcmp(a->source_sha256, b->source_sha256) ||
        strcmp(a->mesh_sha256, b->mesh_sha256))
      return fail(d, "asset_mismatch", b->object_id,
                  "visible mesh, collision and source identities disagree with "
                  "catalog");
    if (!finite_values(b->source_center_m, 3) ||
        !basis(b->principal_to_source) ||
        !close_values(b->source_center_m, a->source_center_m, 3) ||
        !close_values(b->principal_to_source, a->principal_to_source, 9))
      return fail(d, "mass_frame_mismatch", b->object_id,
                  "COM/principal frame differs from asset metadata");
    if (!finite_values(b->orientation_wxyz, 4) ||
        !finite_values(b->linear_velocity_mps, 3) ||
        !finite_values(b->angular_velocity_radps, 3))
      return fail(d, "initial_state", b->object_id,
                  "finite quaternion and velocities required");
    double qnorm = 0;
    for (size_t k = 0; k < 4; ++k)
      qnorm += b->orientation_wxyz[k] * b->orientation_wxyz[k];
    if (fabs(qnorm - 1) > 1e-9)
      return fail(d, "quaternion", b->object_id,
                  "normalized wxyz principal quaternion required");
    if (!isfinite(b->mass_kg) || !finite_values(b->principal_inertia_kgm2, 3) ||
        !isfinite(b->restitution) || b->restitution < 0 || b->restitution > 1 ||
        !isfinite(b->friction) || b->friction < 0 || b->friction > 1 ||
        !isfinite(b->rolling_resistance_length_m) || b->rolling_resistance_length_m < 0 ||
        b->rolling_resistance_length_m > 1e100 ||
        !isfinite(b->torsional_resistance_length_m) || b->torsional_resistance_length_m < 0 ||
        b->torsional_resistance_length_m > 1e100)
      return fail(d, "material_mass", b->object_id,
                  "unsupported physical material or mass values");
    if (b->role == CORE_SCENE_RIGID_STATIC) {
      if (b->mass_kg != 0 || !zero(b->principal_inertia_kgm2, 3) ||
          !zero(b->linear_velocity_mps, 3) ||
          !zero(b->angular_velocity_radps, 3))
        return fail(d, "static_state", b->object_id,
                    "static colliders require zero response mass, inertia and "
                    "velocity");
    } else {
      double expected[3];
      for (size_t k = 0; k < 3; ++k)
        expected[k] = a->inertia_per_kg[k] * b->mass_kg;
      if (a->shape != CORE_SCENE_RIGID_CONVEX_COMPOUND &&
          a->shape != CORE_SCENE_RIGID_ANALYTIC_SPHERE)
        return fail(d, "dynamic_shape_unsupported", b->object_id,
                    "moving triangle-mesh collision is unsupported");
      if (b->mass_kg <= 0 || !finite_values(expected, 3) ||
          !close_values(b->principal_inertia_kgm2, expected, 3))
        return fail(
            d, "inertia", b->object_id,
            "dynamic mass/inertia must match asset-derived principal inertia");
      for (size_t k = 0; k < 3; ++k)
        if (b->principal_inertia_kgm2[k] <= 0 ||
            fabs(b->principal_inertia_kgm2[k] - expected[k]) >
                1e-10 * expected[k])
          return fail(d, "inertia", b->object_id,
                      "principal inertia must be positive");
    }
    ++physical;
  }
  if (physical > lim.body_capacity ||
      (physical > 1 && physical * (physical - 1) / 2 > lim.pair_capacity))
    return fail(d, "body_capacity", "",
                "physical body/pair count exceeds consumer limits");
  return core_result_ok();
}
