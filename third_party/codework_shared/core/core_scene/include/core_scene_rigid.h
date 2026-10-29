#ifndef CORE_SCENE_RIGID_H
#define CORE_SCENE_RIGID_H
#include "core_base.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CORE_SCENE_RIGID_SCHEMA "core_rigid_scene_v1"
#define CORE_SCENE_RIGID_MAX_RECORDS 64u
#define CORE_SCENE_RIGID_ID_SIZE 64u
/* These are document limits, not evidence of solver capability. */
typedef enum CoreSceneRigidRole {
  CORE_SCENE_RIGID_ROLE_UNKNOWN,
  CORE_SCENE_RIGID_VISUAL_ONLY,
  CORE_SCENE_RIGID_STATIC,
  CORE_SCENE_RIGID_DYNAMIC,
  CORE_SCENE_RIGID_KINEMATIC
} CoreSceneRigidRole;
typedef enum CoreSceneRigidOwner {
  CORE_SCENE_RIGID_OWNER_UNKNOWN,
  CORE_SCENE_RIGID_AUTHORED,
  CORE_SCENE_RIGID_SIMULATION,
  CORE_SCENE_RIGID_PRESCRIBED
} CoreSceneRigidOwner;
typedef enum CoreSceneRigidShape {
  CORE_SCENE_RIGID_SHAPE_UNKNOWN,
  CORE_SCENE_RIGID_CONVEX_COMPOUND,
  CORE_SCENE_RIGID_STATIC_TRIANGLES,
  CORE_SCENE_RIGID_ANALYTIC_SPHERE
} CoreSceneRigidShape;
enum {
  CORE_SCENE_RIGID_PATH_CLAIM = 1u,
  CORE_SCENE_RIGID_TRACK_CLAIM = 2u,
  CORE_SCENE_RIGID_OTHER_SIM_CLAIM = 4u
};
typedef struct CoreSceneRigidObject {
  char id[64], mesh_id[64], parent_id[64];
  bool mesh_instance;
  double position[3], rotation_deg[3], scale[3];
  unsigned transform_claims;
} CoreSceneRigidObject;
/* Caller-supplied asset metadata must originate from a verified asset loader
 * before execution. Structural contract validation does not prove mesh bytes.
 */
typedef struct CoreSceneRigidAsset {
  char id[64], source_id[64], mesh_id[64];
  char collision_sha256[65], source_sha256[65], mesh_sha256[65];
  CoreSceneRigidShape shape;
  uint32_t child_count;
  double source_center_m[3], principal_to_source[9], inertia_per_kg[3];
} CoreSceneRigidAsset;
typedef struct CoreSceneRigidBinding {
  char object_id[64], asset_id[64], source_id[64], mesh_id[64];
  char collision_sha256[65], source_sha256[65], mesh_sha256[65];
  uint32_t body_id;
  CoreSceneRigidRole role;
  CoreSceneRigidOwner owner;
  bool collides;
  double orientation_wxyz[4], linear_velocity_mps[3], angular_velocity_radps[3];
  double source_center_m[3], principal_to_source[9];
  double mass_kg, principal_inertia_kgm2[3], restitution, friction;
  /* Optional dry contact torque lengths in metres; zero preserves legacy.
   * Pair lengths combine by maximum; torque limits are normal load * length. */
  double rolling_resistance_length_m, torsional_resistance_length_m;
} CoreSceneRigidBinding;
typedef enum CoreSceneRigidContactPolicy {
  CORE_SCENE_RIGID_CONTACT_DEFAULT = 0,
  CORE_SCENE_RIGID_CONTACT_ZERO_CUTOFF = 1
} CoreSceneRigidContactPolicy;
typedef struct CoreSceneRigidContract {
  CoreSceneRigidContactPolicy contact_policy;
  char scene_id[64];
  uint32_t schema_version, rate_numerator, rate_denominator, origin_tick,
      end_tick;
  double gravity_mps2[3];
  size_t object_count, asset_count, binding_count;
  CoreSceneRigidObject objects[CORE_SCENE_RIGID_MAX_RECORDS];
  CoreSceneRigidAsset assets[CORE_SCENE_RIGID_MAX_RECORDS];
  CoreSceneRigidBinding bindings[CORE_SCENE_RIGID_MAX_RECORDS];
} CoreSceneRigidContract;
typedef struct CoreSceneRigidLimits {
  size_t body_capacity, pair_capacity, children_per_body;
} CoreSceneRigidLimits;
typedef struct CoreSceneRigidDiagnostic {
  char code[64], object_id[64], message[192];
} CoreSceneRigidDiagnostic;
CoreResult core_scene_rigid_validate(const CoreSceneRigidContract *contract,
                                     CoreSceneRigidLimits limits,
                                     CoreSceneRigidDiagnostic *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
