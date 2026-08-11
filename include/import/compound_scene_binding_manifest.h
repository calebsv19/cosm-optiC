#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/compound_scene_handoff_import.h"

#define RAY_COMPOUND_SCENE_BINDING_MANIFEST_SCHEMA \
    "ray_tracing_compound_scene_binding_manifest_v1"
#define RAY_COMPOUND_SCENE_BINDING_COMPOSITION "replace_world_rigid"
#define RAY_COMPOUND_SCENE_BINDING_PLAYBACK "exact_step"

typedef struct RayCompoundSceneRendererBinding {
    int body_id;
    char source_asset_id[64];
    char source_sha256[65];
    char object_id[64];
    char mesh_asset_id[64];
} RayCompoundSceneRendererBinding;

typedef struct RayCompoundSceneBindingManifest {
    char schema[64];
    uint64_t handoff_digest;
    char composition[32];
    char playback[32];
    RayCompoundSceneVec3 world_from_simulation_translation_m;
    RayCompoundSceneQuat world_from_simulation_orientation;
    size_t binding_count;
    RayCompoundSceneRendererBinding
        bindings[RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT];
} RayCompoundSceneBindingManifest;

void ray_compound_scene_binding_manifest_init(
    RayCompoundSceneBindingManifest* manifest,
    const RayCompoundSceneHandoff* handoff);

bool ray_compound_scene_binding_manifest_validate(
    const RayCompoundSceneBindingManifest* manifest,
    const RayCompoundSceneHandoff* handoff);

const RayCompoundSceneRendererBinding*
ray_compound_scene_binding_manifest_find_body(
    const RayCompoundSceneBindingManifest* manifest,
    int body_id);
