#include "render/compound_scene_assembly.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

static void fail(RayCompoundSceneAssemblyFailure* output,
                 RayCompoundSceneAssemblyFailure value) {
    if (output) *output = value;
}

static uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = data;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint64_t hash_string(uint64_t hash, const char* value) {
    return hash_bytes(hash, value, strlen(value) + 1u);
}

static bool token(const char* value, size_t capacity) {
    return value && memchr(value, '\0', capacity) && value[0] != '\0';
}

static uint64_t geometry_digest(const RayCompoundSceneVec3* positions,
                                size_t count) {
    uint64_t hash = FNV_OFFSET;
    hash = hash_bytes(hash, &count, sizeof(count));
    for (size_t i = 0u; i < count; ++i) {
        hash = hash_bytes(hash, &positions[i].x, sizeof(double));
        hash = hash_bytes(hash, &positions[i].y, sizeof(double));
        hash = hash_bytes(hash, &positions[i].z, sizeof(double));
    }
    return hash;
}

static bool unique_object_id(const RayCompoundSceneAssembly* assembly,
                             const char* object_id) {
    for (size_t i = 0u; i < assembly->object_count; ++i)
        if (!strcmp(assembly->objects[i].object_id, object_id)) return false;
    return true;
}

uint64_t ray_compound_scene_assembly_digest(
    const RayCompoundSceneAssembly* assembly) {
    uint64_t hash = FNV_OFFSET;
    if (!assembly) return 0u;
    hash = hash_string(hash, assembly->schema);
    hash = hash_bytes(hash, &assembly->handoff_digest,
                      sizeof(assembly->handoff_digest));
    hash = hash_bytes(hash, &assembly->tick, sizeof(assembly->tick));
    hash = hash_bytes(hash, &assembly->simulated_count,
                      sizeof(assembly->simulated_count));
    hash = hash_bytes(hash, &assembly->static_count,
                      sizeof(assembly->static_count));
    hash = hash_bytes(hash, &assembly->object_count,
                      sizeof(assembly->object_count));
    for (size_t i = 0u; i < assembly->object_count; ++i) {
        const RayCompoundSceneObjectRecord* object = &assembly->objects[i];
        hash = hash_bytes(hash, &object->membership,
                          sizeof(object->membership));
        hash = hash_string(hash, object->object_id);
        hash = hash_string(hash, object->geometry_id);
        hash = hash_string(hash, object->material_id);
        hash = hash_bytes(hash, &object->body_id, sizeof(object->body_id));
        hash = hash_string(hash, object->source_asset_id);
        hash = hash_string(hash, object->source_sha256);
        hash = hash_bytes(hash, &object->source_binding_digest,
                          sizeof(object->source_binding_digest));
        hash = hash_bytes(hash, &object->source_tick,
                          sizeof(object->source_tick));
        hash = hash_bytes(hash, &object->vertex_count,
                          sizeof(object->vertex_count));
        hash = hash_bytes(hash, &object->bounds_min,
                          sizeof(object->bounds_min));
        hash = hash_bytes(hash, &object->bounds_max,
                          sizeof(object->bounds_max));
        hash = hash_bytes(hash, &object->geometry_digest,
                          sizeof(object->geometry_digest));
    }
    return hash;
}

bool ray_compound_scene_assembly_validate(
    const RayCompoundSceneAssembly* assembly) {
    if (!assembly || !assembly->valid ||
        strcmp(assembly->schema, RAY_COMPOUND_SCENE_ASSEMBLY_SCHEMA) ||
        assembly->simulated_count !=
            RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT ||
        assembly->static_count > RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT ||
        assembly->object_count !=
            assembly->simulated_count + assembly->static_count ||
        assembly->assembly_digest !=
            ray_compound_scene_assembly_digest(assembly)) return false;
    for (size_t i = 0u; i < assembly->object_count; ++i) {
        const RayCompoundSceneObjectRecord* object = &assembly->objects[i];
        if (!token(object->object_id, sizeof(object->object_id)) ||
            !token(object->geometry_id, sizeof(object->geometry_id))) return false;
        for (size_t j = 0u; j < i; ++j)
            if (!strcmp(object->object_id, assembly->objects[j].object_id))
                return false;
        if (i < assembly->simulated_count) {
            if (object->membership != RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED ||
                object->body_id < 0 ||
                !token(object->source_asset_id,
                       sizeof(object->source_asset_id)) ||
                !token(object->source_sha256, sizeof(object->source_sha256)) ||
                object->source_tick != assembly->tick ||
                object->vertex_count == 0u || object->geometry_digest == 0u ||
                !assembly->simulated_geometry[i].valid) return false;
        } else if (object->membership !=
                       RAY_COMPOUND_SCENE_MEMBERSHIP_STATIC ||
                   !token(object->material_id, sizeof(object->material_id)) ||
                   object->body_id != -1 || object->source_asset_id[0] ||
                   object->source_sha256[0] || object->source_binding_digest ||
                   object->source_tick || object->vertex_count ||
                   object->geometry_digest) return false;
    }
    return true;
}

bool ray_compound_scene_assembly_build_exact(
    const RayCompoundSceneAssemblyRequest* request,
    RayCompoundSceneAssembly* output,
    RayCompoundSceneAssemblyFailure* failure) {
    RayCompoundSceneAssembly candidate = {0};
    RayCompoundSceneVec3* temporary[2] = {NULL, NULL};
    bool seen_body[2] = {false, false};
    bool ok = false;
    fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_NONE);
    if (!request || !output || !request->handoff || !request->manifest ||
        !request->snapshot ||
        (request->static_object_count && !request->static_objects)) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_INPUT);
        return false;
    }
    if (!ray_compound_scene_handoff_validate(request->handoff) ||
        !ray_compound_scene_binding_manifest_validate(
            request->manifest, request->handoff) ||
        RayEvaluatedSceneSnapshotValidate(request->snapshot) !=
            TIMELINE_STATUS_OK ||
        request->snapshot->simulation.frame_index < 0 ||
        request->static_object_count >
            RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_PROVENANCE);
        return false;
    }
    snprintf(candidate.schema, sizeof(candidate.schema), "%s",
             RAY_COMPOUND_SCENE_ASSEMBLY_SCHEMA);
    candidate.handoff_digest = request->handoff->handoff_digest;
    candidate.tick = (uint64_t)request->snapshot->simulation.frame_index;
    candidate.simulated_count =
        RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT;
    candidate.static_count = request->static_object_count;

    for (size_t i = 0u; i < candidate.simulated_count; ++i) {
        const RayCompoundSceneSourceGeometryView* source =
            &request->simulated_sources[i];
        const RayCompoundSceneGeometryTarget* target =
            &request->simulated_targets[i];
        RayCompoundSceneDetachedGeometryFailure geometry_failure;
        size_t body_slot = candidate.simulated_count;
        for (size_t j = 0u; j < candidate.simulated_count; ++j)
            if (request->handoff->bindings[j].body_id == source->body_id)
                body_slot = j;
        if (body_slot == candidate.simulated_count || seen_body[body_slot]) {
            fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
            goto done;
        }
        if (!target->world_positions ||
            target->world_position_capacity < source->vertex_count) {
            fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_CAPACITY);
            goto done;
        }
        seen_body[body_slot] = true;
        temporary[i] = calloc(source->vertex_count, sizeof(*temporary[i]));
        if (!temporary[i]) {
            fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_ALLOCATION);
            goto done;
        }
        candidate.simulated_geometry[i].world_positions = temporary[i];
        candidate.simulated_geometry[i].world_position_capacity =
            source->vertex_count;
        if (!ray_compound_scene_detached_geometry_apply_exact(
                request->handoff, request->manifest, request->snapshot,
                source, &candidate.simulated_geometry[i], &geometry_failure)) {
            fail(failure,
                 geometry_failure ==
                         RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_CAPACITY
                     ? RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_CAPACITY
                     : RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_GEOMETRY);
            goto done;
        }
        RayCompoundSceneObjectRecord* record =
            &candidate.objects[candidate.object_count++];
        record->membership = RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED;
        snprintf(record->object_id, sizeof(record->object_id), "%s",
                 candidate.simulated_geometry[i].object_id);
        snprintf(record->geometry_id, sizeof(record->geometry_id), "%s",
                 candidate.simulated_geometry[i].mesh_asset_id);
        record->body_id = candidate.simulated_geometry[i].body_id;
        snprintf(record->source_asset_id, sizeof(record->source_asset_id), "%s",
                 candidate.simulated_geometry[i].source_asset_id);
        snprintf(record->source_sha256, sizeof(record->source_sha256), "%s",
                 candidate.simulated_geometry[i].source_sha256);
        record->source_binding_digest =
            candidate.simulated_geometry[i].source_binding_digest;
        record->source_tick = candidate.simulated_geometry[i].source_tick;
        record->vertex_count = candidate.simulated_geometry[i].vertex_count;
        record->bounds_min = candidate.simulated_geometry[i].bounds_min;
        record->bounds_max = candidate.simulated_geometry[i].bounds_max;
        record->geometry_digest = geometry_digest(temporary[i],
                                                  source->vertex_count);
    }
    for (size_t i = 0u; i < request->static_object_count; ++i) {
        const RayCompoundSceneStaticObjectSpec* spec =
            &request->static_objects[i];
        if (!token(spec->object_id, sizeof(spec->object_id)) ||
            !token(spec->geometry_id, sizeof(spec->geometry_id)) ||
            !token(spec->material_id, sizeof(spec->material_id)) ||
            !unique_object_id(&candidate, spec->object_id)) {
            fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
            goto done;
        }
        RayCompoundSceneObjectRecord* record =
            &candidate.objects[candidate.object_count++];
        record->membership = RAY_COMPOUND_SCENE_MEMBERSHIP_STATIC;
        snprintf(record->object_id, sizeof(record->object_id), "%s",
                 spec->object_id);
        snprintf(record->geometry_id, sizeof(record->geometry_id), "%s",
                 spec->geometry_id);
        snprintf(record->material_id, sizeof(record->material_id), "%s",
                 spec->material_id);
        record->body_id = -1;
    }
    candidate.valid = true;
    candidate.assembly_digest = ray_compound_scene_assembly_digest(&candidate);
    if (!ray_compound_scene_assembly_validate(&candidate)) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_PROVENANCE);
        goto done;
    }
    for (size_t i = 0u; i < candidate.simulated_count; ++i) {
        const size_t count = candidate.simulated_geometry[i].vertex_count;
        memcpy(request->simulated_targets[i].world_positions, temporary[i],
               count * sizeof(*temporary[i]));
        candidate.simulated_geometry[i].world_positions =
            request->simulated_targets[i].world_positions;
        candidate.simulated_geometry[i].world_position_capacity =
            request->simulated_targets[i].world_position_capacity;
    }
    *output = candidate;
    ok = true;
done:
    for (size_t i = 0u; i < 2u; ++i) free(temporary[i]);
    return ok;
}

const char* ray_compound_scene_membership_name(
    RayCompoundSceneMembership membership) {
    switch (membership) {
        case RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED: return "simulated";
        case RAY_COMPOUND_SCENE_MEMBERSHIP_STATIC: return "static";
        case RAY_COMPOUND_SCENE_MEMBERSHIP_UNKNOWN: return "unknown";
    }
    return "unknown";
}

const char* ray_compound_scene_assembly_failure_name(
    RayCompoundSceneAssemblyFailure failure) {
    switch (failure) {
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_NONE: return "none";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_INPUT: return "input";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_PROVENANCE: return "provenance";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP: return "membership";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_CAPACITY: return "capacity";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_ALLOCATION: return "allocation";
        case RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_GEOMETRY: return "geometry";
    }
    return "unknown";
}
