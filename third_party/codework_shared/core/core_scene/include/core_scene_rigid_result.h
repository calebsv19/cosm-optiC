#ifndef CORE_SCENE_RIGID_RESULT_H
#define CORE_SCENE_RIGID_RESULT_H
#include "core_scene_rigid.h"
/* Optional cJSON adapter. No solver, renderer, file I/O or UI dependency.
 * Callers authenticate/snapshot container bytes before admission. */
#define CORE_SCENE_RIGID_RESULT_CHILDREN 64u
#define CORE_SCENE_RIGID_RESULT_BODIES 32u
#define CORE_SCENE_RIGID_RESULT_PAIRS (CORE_SCENE_RIGID_RESULT_BODIES * (CORE_SCENE_RIGID_RESULT_BODIES - 1u) / 2u)
#define CORE_SCENE_RIGID_RESULT_CONSTRAINTS CORE_SCENE_RIGID_RESULT_PAIRS
#define CORE_SCENE_RIGID_RESULT_TICKS 14400u
#define CORE_SCENE_RIGID_RESULT_JSON_BYTES (64u * 1024u * 1024u)
#define CORE_SCENE_RIGID_RESULT_JSON_NODES 4000000u
typedef struct CoreSceneRigidPose {
  double position_m[3], orientation_wxyz[4], velocity_mps[3],
      angular_velocity_radps[3];
} CoreSceneRigidPose;
typedef struct CoreSceneRigidResultBinding {
  CoreSceneRigidBinding physical;
  double initial_position_m[3];
} CoreSceneRigidResultBinding;
typedef struct CoreSceneRigidResult {
  CoreSceneRigidContactPolicy contact_policy;
  CoreSceneRigidIntegrationPolicy integration_policy;
  char scene_id[64], engine_sha256[65], physical_input_sha256[65];
  uint32_t endpoint_tick, end_tick;
  double gravity_mps2[3];
  size_t body_count;
  CoreSceneRigidResultBinding bindings[CORE_SCENE_RIGID_RESULT_BODIES];
  /* Owned contiguous [endpoint_tick+1][body_count]. */
  CoreSceneRigidPose *poses;
} CoreSceneRigidResult;
/* Host supplies digest implementation; core_scene does not depend on its
 * downstream scene compiler or introduce a second SHA implementation. */
typedef bool (*CoreSceneRigidDigestFn)(const void *bytes, size_t size,
                                       char hex[65]);
void core_scene_rigid_result_free(CoreSceneRigidResult *result);
/* Output must be zero initialized; assigned only on success. */
CoreResult core_scene_rigid_result_read(const char *json, size_t size,
                                        CoreSceneRigidResult *output,
                                        CoreSceneRigidDigestFn digest);
/* Geometry catalog was verified under the caller's accepted immutable identity.
 * Compare physical meaning, ignoring presentation; never runs numerical
 * physics. */
CoreResult core_scene_rigid_result_match(const CoreSceneRigidResult *result,
                                         const CoreSceneRigidContract *current,
                                         CoreSceneRigidDigestFn digest);
/* Exact rational timeline seconds -> source tick. No rounding/interpolation. */
CoreResult core_scene_rigid_result_tick(const CoreSceneRigidResult *result,
                                        int64_t local_frame, uint32_t sub_n,
                                        uint32_t sub_d, uint32_t fps_n,
                                        uint32_t fps_d, uint32_t *tick);
/* Pure source->world affine map, computed without subtracting large world
 * points. */
CoreResult core_scene_rigid_result_affine(const CoreSceneRigidResult *result,
                                          uint32_t tick, size_t body,
                                          double rotation[9],
                                          double translation[3]);
/* Source-space vertex -> world, principal frame and COM each applied once. */
CoreResult core_scene_rigid_result_vertex(const CoreSceneRigidResult *result,
                                          uint32_t tick, size_t body,
                                          const double source[3],
                                          double world[3]);
#endif
