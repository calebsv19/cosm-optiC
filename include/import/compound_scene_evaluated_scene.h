#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "animation/evaluated_scene_snapshot.h"
#include "import/compound_scene_binding_manifest.h"

typedef enum RayCompoundSceneEvaluatedSceneFailure {
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_HANDOFF,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_MANIFEST,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_BASE_SNAPSHOT,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TICK,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TARGET,
    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_RESULT
} RayCompoundSceneEvaluatedSceneFailure;

/*
 * Replaces only already-present mapped object transforms in a detached copy.
 * The exact packet quaternion is retained alongside an Euler compatibility
 * view. Renderer-owned source-mesh recentering/principal-frame application is
 * handled separately by compound_scene_detached_geometry.
 */
bool ray_compound_scene_evaluated_scene_apply_exact(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneBindingManifest* manifest,
    uint64_t tick,
    const RayEvaluatedSceneSnapshot* base_snapshot,
    RayEvaluatedSceneSnapshot* output,
    RayCompoundSceneEvaluatedSceneFailure* failure);

/*
 * Request-ingestion variant. Missing mapped transforms may be appended to the
 * detached candidate, while all validation and publication remain
 * transactional. Callers that require a predeclared scene topology must use
 * ray_compound_scene_evaluated_scene_apply_exact instead.
 */
bool ray_compound_scene_evaluated_scene_apply_ingestion_exact(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneBindingManifest* manifest,
    uint64_t tick,
    const RayEvaluatedSceneSnapshot* base_snapshot,
    RayEvaluatedSceneSnapshot* output,
    RayCompoundSceneEvaluatedSceneFailure* failure);

const char* ray_compound_scene_evaluated_scene_failure_name(
    RayCompoundSceneEvaluatedSceneFailure failure);
