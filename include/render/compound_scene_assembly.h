#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/compound_scene_detached_geometry.h"

#define RAY_COMPOUND_SCENE_ASSEMBLY_SCHEMA \
    "ray_tracing_compound_scene_assembly_v1"

enum {
    RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT =
        RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT,
    RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT = 8,
    RAY_COMPOUND_SCENE_ASSEMBLY_MAX_OBJECT_COUNT =
        RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT + 8
};

typedef enum RayCompoundSceneMembership {
    RAY_COMPOUND_SCENE_MEMBERSHIP_UNKNOWN = 0,
    RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED,
    RAY_COMPOUND_SCENE_MEMBERSHIP_STATIC
} RayCompoundSceneMembership;

typedef struct RayCompoundSceneStaticObjectSpec {
    char object_id[64];
    char geometry_id[64];
    char material_id[64];
} RayCompoundSceneStaticObjectSpec;

typedef struct RayCompoundSceneObjectRecord {
    RayCompoundSceneMembership membership;
    char object_id[64];
    char geometry_id[64];
    char material_id[64];
    int body_id;
    char source_asset_id[64];
    char source_sha256[65];
    uint64_t source_binding_digest;
    uint64_t source_tick;
    size_t vertex_count;
    RayCompoundSceneVec3 bounds_min;
    RayCompoundSceneVec3 bounds_max;
    uint64_t geometry_digest;
} RayCompoundSceneObjectRecord;

typedef struct RayCompoundSceneGeometryTarget {
    RayCompoundSceneVec3* world_positions;
    size_t world_position_capacity;
} RayCompoundSceneGeometryTarget;

typedef struct RayCompoundSceneAssemblyRequest {
    const RayCompoundSceneHandoff* handoff;
    const RayCompoundSceneBindingManifest* manifest;
    const RayEvaluatedSceneSnapshot* snapshot;
    RayCompoundSceneSourceGeometryView
        simulated_sources[RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT];
    RayCompoundSceneGeometryTarget
        simulated_targets[RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT];
    const RayCompoundSceneStaticObjectSpec* static_objects;
    size_t static_object_count;
} RayCompoundSceneAssemblyRequest;

typedef struct RayCompoundSceneAssembly {
    bool valid;
    char schema[64];
    uint64_t handoff_digest;
    uint64_t tick;
    size_t simulated_count;
    size_t static_count;
    size_t object_count;
    RayCompoundSceneObjectRecord
        objects[RAY_COMPOUND_SCENE_ASSEMBLY_MAX_OBJECT_COUNT];
    RayCompoundSceneDetachedGeometry simulated_geometry[
        RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT];
    uint64_t assembly_digest;
} RayCompoundSceneAssembly;

typedef enum RayCompoundSceneAssemblyFailure {
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_PROVENANCE,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_CAPACITY,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_ALLOCATION,
    RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_GEOMETRY
} RayCompoundSceneAssemblyFailure;

/*
 * Builds a RayTracing-owned membership/result view for one already-evaluated
 * exact tick. Mesh topology and render policy remain caller-owned. On failure,
 * the result and every caller geometry buffer are unchanged.
 */
bool ray_compound_scene_assembly_build_exact(
    const RayCompoundSceneAssemblyRequest* request,
    RayCompoundSceneAssembly* output,
    RayCompoundSceneAssemblyFailure* failure);

bool ray_compound_scene_assembly_validate(
    const RayCompoundSceneAssembly* assembly);
uint64_t ray_compound_scene_assembly_digest(
    const RayCompoundSceneAssembly* assembly);
const char* ray_compound_scene_membership_name(
    RayCompoundSceneMembership membership);
const char* ray_compound_scene_assembly_failure_name(
    RayCompoundSceneAssemblyFailure failure);
