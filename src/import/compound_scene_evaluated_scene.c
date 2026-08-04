#include "import/compound_scene_evaluated_scene.h"

#include "animation/timeline_property_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAY_COMPOUND_SCENE_FIXED_RATE_NUMERATOR 240u
#define RAY_COMPOUND_SCENE_FIXED_RATE_DENOMINATOR 1u

static void set_failure(RayCompoundSceneEvaluatedSceneFailure* failure,
                        RayCompoundSceneEvaluatedSceneFailure value) {
    if (failure) *failure = value;
}

static RayEvaluatedQuaternion evaluated_quaternion(
    RayCompoundSceneQuat value) {
    return (RayEvaluatedQuaternion){value.w, value.x, value.y, value.z};
}

/* Euler compatibility view for the runtime's Rz * Ry * Rx convention. */
static TimelineVec3 quaternion_to_euler_xyz(RayCompoundSceneQuat value) {
    const double sin_x = 2.0 * (value.w * value.x + value.y * value.z);
    const double cos_x = 1.0 - 2.0 * (value.x * value.x + value.y * value.y);
    double sin_y = 2.0 * (value.w * value.y - value.z * value.x);
    const double sin_z = 2.0 * (value.w * value.z + value.x * value.y);
    const double cos_z = 1.0 - 2.0 * (value.y * value.y + value.z * value.z);
    if (sin_y > 1.0) sin_y = 1.0;
    if (sin_y < -1.0) sin_y = -1.0;
    return (TimelineVec3){atan2(sin_x, cos_x), asin(sin_y),
                          atan2(sin_z, cos_z)};
}

static RayEvaluatedObjectTransform* find_target(
    RayEvaluatedSceneSnapshot* snapshot,
    const char* object_id) {
    if (!snapshot || !object_id) return NULL;
    for (size_t i = 0u; i < snapshot->object_transform_count; ++i) {
        if (strcmp(snapshot->object_transforms[i].target_id, object_id) == 0)
            return &snapshot->object_transforms[i];
    }
    return NULL;
}

static RayEvaluatedObjectTransform* find_or_append_target(
    RayEvaluatedSceneSnapshot* snapshot, const char* object_id) {
    RayEvaluatedObjectTransform* target = find_target(snapshot, object_id);
    if (target || !snapshot || !object_id || !object_id[0] ||
        snapshot->object_transform_count >= RAY_EVALUATED_OBJECT_TRANSFORM_CAPACITY) {
        return target;
    }
    target = &snapshot->object_transforms[snapshot->object_transform_count++];
    memset(target, 0, sizeof(*target));
    target->valid = true;
    snprintf(target->target_id, sizeof(target->target_id), "%s", object_id);
    target->frame = snapshot->frame;
    return target;
}

static const RayCompoundSceneSourceBinding* find_source_binding(
    const RayCompoundSceneHandoff* handoff,
    int body_id) {
    if (!handoff) return NULL;
    for (size_t i = 0u; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        if (handoff->bindings[i].body_id == body_id)
            return &handoff->bindings[i];
    }
    return NULL;
}

static bool fixed_rate_supported(const RayCompoundSceneHandoff* handoff) {
    const double expected =
        (double)RAY_COMPOUND_SCENE_FIXED_RATE_DENOMINATOR /
        (double)RAY_COMPOUND_SCENE_FIXED_RATE_NUMERATOR;
    return handoff && fabs(handoff->fixed_dt_s - expected) <= 1e-15;
}

bool ray_compound_scene_evaluated_scene_apply_exact(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneBindingManifest* manifest,
    uint64_t tick,
    const RayEvaluatedSceneSnapshot* base_snapshot,
    RayEvaluatedSceneSnapshot* output,
    RayCompoundSceneEvaluatedSceneFailure* failure) {
    RayCompoundSceneFrame frame = {0};
    RayEvaluatedSceneSnapshot candidate;
    set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_NONE);
    if (!handoff || !manifest || !base_snapshot || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_INPUT);
        return false;
    }
    if (!ray_compound_scene_handoff_validate(handoff) ||
        !fixed_rate_supported(handoff)) {
        set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_HANDOFF);
        return false;
    }
    if (!ray_compound_scene_binding_manifest_validate(manifest, handoff)) {
        set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_MANIFEST);
        return false;
    }
    if (RayEvaluatedSceneSnapshotValidate(base_snapshot) != TIMELINE_STATUS_OK) {
        set_failure(failure,
                    RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_BASE_SNAPSHOT);
        return false;
    }
    if (!ray_compound_scene_handoff_replay_exact(handoff, tick, &frame)) {
        set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TICK);
        return false;
    }

    candidate = *base_snapshot;
    candidate.schema_version = RAY_EVALUATED_SCENE_SNAPSHOT_SCHEMA_VERSION;
    for (size_t i = 0u; i < manifest->binding_count; ++i) {
        const RayCompoundSceneRendererBinding* renderer_binding =
            &manifest->bindings[i];
        const RayCompoundSceneSourceBinding* source_binding =
            find_source_binding(handoff, renderer_binding->body_id);
        const RayCompoundSceneBodyTransform* body = NULL;
        RayEvaluatedObjectTransform* target =
            find_or_append_target(&candidate, renderer_binding->object_id);
        for (size_t body_index = 0u;
             body_index < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
             ++body_index) {
            if (frame.bodies[body_index].body_id == renderer_binding->body_id) {
                body = &frame.bodies[body_index];
                break;
            }
        }
        if (!source_binding || !body || !target) {
            set_failure(failure,
                        RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TARGET);
            return false;
        }
        target->source =
            RAY_EVALUATED_OBJECT_TRANSFORM_COMPOUND_SCENE_EXACT;
        target->has_position = true;
        target->has_rotation = true;
        target->position = (TimelineVec3){body->position_m.x,
                                          body->position_m.y,
                                          body->position_m.z};
        target->rotation_radians = quaternion_to_euler_xyz(body->orientation);
        target->has_orientation_quaternion = true;
        target->orientation_quaternion =
            evaluated_quaternion(body->orientation);
        target->source_handoff_digest = handoff->handoff_digest;
        target->source_binding_digest = source_binding->binding_digest;
        target->source_tick = tick;
        target->frame = candidate.frame;
    }

    candidate.simulation.source = RAY_EVALUATED_SIMULATION_CACHE;
    candidate.simulation.valid = true;
    snprintf(candidate.simulation.cache_id,
             sizeof(candidate.simulation.cache_id), "%s", handoff->handoff_id);
    candidate.simulation.cache_revision = handoff->handoff_digest;
    candidate.simulation.frame_index = (int64_t)tick;
    candidate.simulation.source_frame_index = (int64_t)tick;
    candidate.simulation.source_rate = (TimelineRate){
        RAY_COMPOUND_SCENE_FIXED_RATE_NUMERATOR,
        RAY_COMPOUND_SCENE_FIXED_RATE_DENOMINATOR};
    candidate.simulation.frame_offset = 0;
    candidate.simulation.frame_stride = 1u;
    candidate.simulation.subframe_numerator = 0u;
    candidate.simulation.subframe_denominator = 1u;
    candidate.simulation.interpolation =
        RAY_EVALUATED_SIMULATION_INTERPOLATION_STEP;
    snprintf(candidate.simulation.content_digest,
             sizeof(candidate.simulation.content_digest), "fnv64:%016llx",
             (unsigned long long)handoff->handoff_digest);
    candidate.invalidation_domains |=
        TIMELINE_INVALIDATION_RIGID_TRANSFORM |
        TIMELINE_INVALIDATION_SIMULATION_CACHE;
    snprintf(candidate.diagnostics, sizeof(candidate.diagnostics),
             "compound-scene exact tick=%llu handoff=%016llx",
             (unsigned long long)tick,
             (unsigned long long)handoff->handoff_digest);

    if (RayEvaluatedSceneSnapshotValidate(&candidate) != TIMELINE_STATUS_OK) {
        set_failure(failure, RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_RESULT);
        return false;
    }
    *output = candidate;
    return true;
}

const char* ray_compound_scene_evaluated_scene_failure_name(
    RayCompoundSceneEvaluatedSceneFailure failure) {
    switch (failure) {
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_NONE: return "none";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_INPUT: return "input";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_HANDOFF: return "handoff";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_MANIFEST: return "manifest";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_BASE_SNAPSHOT: return "base_snapshot";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TICK: return "tick";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TARGET: return "target";
        case RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_RESULT: return "result";
    }
    return "unknown";
}
