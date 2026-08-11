#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "animation/evaluated_scene_snapshot.h"
#include "import/compound_scene_binding_manifest.h"

typedef struct RayCompoundSceneSourceGeometryView {
    int body_id;
    const char* object_id;
    const char* mesh_asset_id;
    const char* source_asset_id;
    const char* source_sha256;
    const RayCompoundSceneVec3* source_positions;
    size_t vertex_count;
} RayCompoundSceneSourceGeometryView;

typedef struct RayCompoundSceneDetachedGeometry {
    bool valid;
    int body_id;
    char object_id[64];
    char mesh_asset_id[64];
    char source_asset_id[64];
    char source_sha256[65];
    uint64_t handoff_digest;
    uint64_t source_binding_digest;
    uint64_t source_tick;
    size_t vertex_count;
    RayCompoundSceneVec3* world_positions;
    size_t world_position_capacity;
    RayCompoundSceneVec3 bounds_min;
    RayCompoundSceneVec3 bounds_max;
} RayCompoundSceneDetachedGeometry;

typedef enum RayCompoundSceneDetachedGeometryFailure {
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_HANDOFF,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_MANIFEST,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SNAPSHOT,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_TARGET,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_CAPACITY,
    RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_GEOMETRY
} RayCompoundSceneDetachedGeometryFailure;

/*
 * Applies one exact evaluated rigid pose to borrowed source positions.
 * Source-to-principal composition is provenance-bound by the handoff. The
 * caller retains topology, normals, materials, camera, light, sampling,
 * acceleration, and final-image ownership.
 */
bool ray_compound_scene_detached_geometry_apply_exact(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneBindingManifest* manifest,
    const RayEvaluatedSceneSnapshot* snapshot,
    const RayCompoundSceneSourceGeometryView* source,
    RayCompoundSceneDetachedGeometry* output,
    RayCompoundSceneDetachedGeometryFailure* failure);

const char* ray_compound_scene_detached_geometry_failure_name(
    RayCompoundSceneDetachedGeometryFailure failure);
