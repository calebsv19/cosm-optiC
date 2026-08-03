#include "render/compound_scene_detached_geometry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void set_failure(RayCompoundSceneDetachedGeometryFailure* failure,
                        RayCompoundSceneDetachedGeometryFailure value) {
    if (failure) *failure = value;
}

static bool finite_vec3(RayCompoundSceneVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static RayCompoundSceneVec3 add(RayCompoundSceneVec3 a,
                                RayCompoundSceneVec3 b) {
    return (RayCompoundSceneVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static RayCompoundSceneVec3 subtract(RayCompoundSceneVec3 a,
                                     RayCompoundSceneVec3 b) {
    return (RayCompoundSceneVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static RayCompoundSceneVec3 transpose_multiply(
    RayCompoundSceneMat3 matrix, RayCompoundSceneVec3 value) {
    return (RayCompoundSceneVec3){
        matrix.m[0][0] * value.x + matrix.m[1][0] * value.y +
            matrix.m[2][0] * value.z,
        matrix.m[0][1] * value.x + matrix.m[1][1] * value.y +
            matrix.m[2][1] * value.z,
        matrix.m[0][2] * value.x + matrix.m[1][2] * value.y +
            matrix.m[2][2] * value.z};
}

static RayCompoundSceneVec3 rotate(RayEvaluatedQuaternion q,
                                   RayCompoundSceneVec3 value) {
    const RayCompoundSceneVec3 u = {q.x, q.y, q.z};
    const double dot_uv = u.x * value.x + u.y * value.y + u.z * value.z;
    const double dot_uu = u.x * u.x + u.y * u.y + u.z * u.z;
    const RayCompoundSceneVec3 cross_uv = {
        u.y * value.z - u.z * value.y,
        u.z * value.x - u.x * value.z,
        u.x * value.y - u.y * value.x};
    return (RayCompoundSceneVec3){
        2.0 * u.x * dot_uv + (q.w * q.w - dot_uu) * value.x +
            2.0 * q.w * cross_uv.x,
        2.0 * u.y * dot_uv + (q.w * q.w - dot_uu) * value.y +
            2.0 * q.w * cross_uv.y,
        2.0 * u.z * dot_uv + (q.w * q.w - dot_uu) * value.z +
            2.0 * q.w * cross_uv.z};
}

static const RayCompoundSceneSourceBinding* find_source_binding(
    const RayCompoundSceneHandoff* handoff, int body_id) {
    for (size_t i = 0u; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        if (handoff->bindings[i].body_id == body_id)
            return &handoff->bindings[i];
    }
    return NULL;
}

static const RayEvaluatedObjectTransform* find_transform(
    const RayEvaluatedSceneSnapshot* snapshot, const char* object_id) {
    for (size_t i = 0u; i < snapshot->object_transform_count; ++i) {
        if (strcmp(snapshot->object_transforms[i].target_id, object_id) == 0)
            return &snapshot->object_transforms[i];
    }
    return NULL;
}

static RayCompoundSceneVec3 source_to_world(
    const RayCompoundSceneSourceBinding* binding,
    const RayEvaluatedObjectTransform* transform,
    const RayCompoundSceneBindingManifest* manifest,
    RayCompoundSceneVec3 source) {
    const RayCompoundSceneVec3 principal = transpose_multiply(
        binding->principal_to_source,
        subtract(source, binding->source_center_m));
    const RayCompoundSceneVec3 simulation = add(
        (RayCompoundSceneVec3){transform->position.x, transform->position.y,
                               transform->position.z},
        rotate(transform->orientation_quaternion, principal));
    return add(manifest->world_from_simulation_translation_m,
               rotate((RayEvaluatedQuaternion){
                          manifest->world_from_simulation_orientation.w,
                          manifest->world_from_simulation_orientation.x,
                          manifest->world_from_simulation_orientation.y,
                          manifest->world_from_simulation_orientation.z},
                      simulation));
}

bool ray_compound_scene_detached_geometry_apply_exact(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneBindingManifest* manifest,
    const RayEvaluatedSceneSnapshot* snapshot,
    const RayCompoundSceneSourceGeometryView* source,
    RayCompoundSceneDetachedGeometry* output,
    RayCompoundSceneDetachedGeometryFailure* failure) {
    const RayCompoundSceneRendererBinding* renderer_binding;
    const RayCompoundSceneSourceBinding* source_binding;
    const RayEvaluatedObjectTransform* transform;
    RayCompoundSceneDetachedGeometry candidate;
    RayCompoundSceneVec3 bounds_min = {0};
    RayCompoundSceneVec3 bounds_max = {0};
    set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_NONE);
    if (!handoff || !manifest || !snapshot || !source || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_INPUT);
        return false;
    }
    if (!ray_compound_scene_handoff_validate(handoff)) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_HANDOFF);
        return false;
    }
    if (!ray_compound_scene_binding_manifest_validate(manifest, handoff)) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_MANIFEST);
        return false;
    }
    if (RayEvaluatedSceneSnapshotValidate(snapshot) != TIMELINE_STATUS_OK) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SNAPSHOT);
        return false;
    }
    if (!source->object_id || !source->mesh_asset_id ||
        !source->source_asset_id || !source->source_sha256 ||
        !source->source_positions || source->vertex_count == 0u) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE);
        return false;
    }
    renderer_binding = ray_compound_scene_binding_manifest_find_body(
        manifest, source->body_id);
    source_binding = find_source_binding(handoff, source->body_id);
    if (!renderer_binding || !source_binding ||
        strcmp(source->object_id, renderer_binding->object_id) ||
        strcmp(source->mesh_asset_id, renderer_binding->mesh_asset_id) ||
        strcmp(source->source_asset_id, source_binding->source_asset_id) ||
        strcmp(source->source_sha256, source_binding->source_sha256)) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE);
        return false;
    }
    transform = find_transform(snapshot, renderer_binding->object_id);
    if (!transform ||
        transform->source !=
            RAY_EVALUATED_OBJECT_TRANSFORM_COMPOUND_SCENE_EXACT ||
        !transform->has_position || !transform->has_orientation_quaternion ||
        transform->source_handoff_digest != handoff->handoff_digest ||
        transform->source_binding_digest != source_binding->binding_digest ||
        snapshot->simulation.source != RAY_EVALUATED_SIMULATION_CACHE ||
        snapshot->simulation.frame_index < 0 ||
        snapshot->simulation.source_frame_index !=
            snapshot->simulation.frame_index ||
        transform->source_tick != (uint64_t)snapshot->simulation.frame_index ||
        snapshot->simulation.cache_revision != handoff->handoff_digest ||
        strcmp(snapshot->simulation.cache_id, handoff->handoff_id) ||
        snapshot->simulation.source_rate.frames_per_second_numerator != 240u ||
        snapshot->simulation.source_rate.frames_per_second_denominator != 1u ||
        snapshot->simulation.interpolation !=
            RAY_EVALUATED_SIMULATION_INTERPOLATION_STEP) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_TARGET);
        return false;
    }
    if (!output->world_positions ||
        output->world_position_capacity < source->vertex_count) {
        set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_CAPACITY);
        return false;
    }

    for (size_t i = 0u; i < source->vertex_count; ++i) {
        RayCompoundSceneVec3 world;
        if (!finite_vec3(source->source_positions[i])) {
            set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_GEOMETRY);
            return false;
        }
        world = source_to_world(source_binding, transform, manifest,
                                source->source_positions[i]);
        if (!finite_vec3(world)) {
            set_failure(failure, RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_GEOMETRY);
            return false;
        }
        if (i == 0u) {
            bounds_min = bounds_max = world;
        } else {
            bounds_min.x = fmin(bounds_min.x, world.x);
            bounds_min.y = fmin(bounds_min.y, world.y);
            bounds_min.z = fmin(bounds_min.z, world.z);
            bounds_max.x = fmax(bounds_max.x, world.x);
            bounds_max.y = fmax(bounds_max.y, world.y);
            bounds_max.z = fmax(bounds_max.z, world.z);
        }
    }

    candidate = (RayCompoundSceneDetachedGeometry){0};
    candidate.valid = true;
    candidate.body_id = source->body_id;
    snprintf(candidate.object_id, sizeof(candidate.object_id), "%s",
             renderer_binding->object_id);
    snprintf(candidate.mesh_asset_id, sizeof(candidate.mesh_asset_id), "%s",
             renderer_binding->mesh_asset_id);
    snprintf(candidate.source_asset_id, sizeof(candidate.source_asset_id), "%s",
             source_binding->source_asset_id);
    snprintf(candidate.source_sha256, sizeof(candidate.source_sha256), "%s",
             source_binding->source_sha256);
    candidate.handoff_digest = handoff->handoff_digest;
    candidate.source_binding_digest = source_binding->binding_digest;
    candidate.source_tick = transform->source_tick;
    candidate.vertex_count = source->vertex_count;
    candidate.world_positions = output->world_positions;
    candidate.world_position_capacity = output->world_position_capacity;
    candidate.bounds_min = bounds_min;
    candidate.bounds_max = bounds_max;
    for (size_t i = 0u; i < source->vertex_count; ++i) {
        candidate.world_positions[i] = source_to_world(
            source_binding, transform, manifest, source->source_positions[i]);
    }
    *output = candidate;
    return true;
}

const char* ray_compound_scene_detached_geometry_failure_name(
    RayCompoundSceneDetachedGeometryFailure failure) {
    switch (failure) {
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_NONE: return "none";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_INPUT: return "input";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_HANDOFF: return "handoff";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_MANIFEST: return "manifest";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SNAPSHOT: return "snapshot";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE: return "source";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_TARGET: return "target";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_CAPACITY: return "capacity";
        case RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_GEOMETRY: return "geometry";
    }
    return "unknown";
}
