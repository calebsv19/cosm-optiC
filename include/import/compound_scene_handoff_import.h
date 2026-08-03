#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAY_COMPOUND_SCENE_HANDOFF_SCHEMA \
    "ball_compound_scene_renderer_handoff_v1"
#define RAY_COMPOUND_SCENE_HANDOFF_ID \
    "phase43_pair_room_renderer_handoff_v1"
#define RAY_COMPOUND_SCENE_SOURCE_BINDING_SCHEMA \
    "ball_compound_scene_source_mesh_binding_v1"

enum {
    RAY_COMPOUND_SCENE_HANDOFF_CODEC_VERSION = 1,
    RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT = 2,
    RAY_COMPOUND_SCENE_HANDOFF_MAX_FRAMES = 721,
    RAY_COMPOUND_SCENE_HANDOFF_TEXT_CAPACITY = 262144,
    RAY_COMPOUND_SCENE_HANDOFF_PAYLOAD_CAPACITY = 131072
};

typedef struct RayCompoundSceneVec3 {
    double x;
    double y;
    double z;
} RayCompoundSceneVec3;

typedef struct RayCompoundSceneQuat {
    double w;
    double x;
    double y;
    double z;
} RayCompoundSceneQuat;

typedef struct RayCompoundSceneMat3 {
    double m[3][3];
} RayCompoundSceneMat3;

typedef struct RayCompoundSceneSourceBinding {
    char schema[64];
    uint64_t fixture_digest;
    size_t body_index;
    int body_id;
    char source_asset_id[64];
    char source_sha256[65];
    char representation_role[32];
    RayCompoundSceneVec3 source_center_m;
    RayCompoundSceneMat3 principal_to_source;
    uint64_t binding_digest;
} RayCompoundSceneSourceBinding;

typedef struct RayCompoundSceneBodyTransform {
    int body_id;
    RayCompoundSceneVec3 position_m;
    RayCompoundSceneQuat orientation;
} RayCompoundSceneBodyTransform;

typedef struct RayCompoundSceneFrame {
    uint64_t tick;
    RayCompoundSceneBodyTransform bodies[RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT];
} RayCompoundSceneFrame;

typedef struct RayCompoundSceneHandoff {
    char schema[64];
    uint32_t schema_version;
    char handoff_id[64];
    char fixture_reference[128];
    uint64_t fixture_digest;
    uint64_t seed;
    double fixed_dt_s;
    RayCompoundSceneSourceBinding
        bindings[RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT];
    size_t frame_count;
    RayCompoundSceneFrame* frames;
    uint64_t handoff_digest;
} RayCompoundSceneHandoff;

typedef enum RayCompoundSceneImportFailure {
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_IO,
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_ALLOCATION,
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE,
    RAY_COMPOUND_SCENE_IMPORT_FAILURE_PROVENANCE
} RayCompoundSceneImportFailure;

void ray_compound_scene_handoff_init(RayCompoundSceneHandoff* handoff);
void ray_compound_scene_handoff_free(RayCompoundSceneHandoff* handoff);

uint64_t ray_compound_scene_source_binding_digest(
    const RayCompoundSceneSourceBinding* binding);
bool ray_compound_scene_source_binding_validate(
    const RayCompoundSceneSourceBinding* binding);
uint64_t ray_compound_scene_handoff_digest(
    const RayCompoundSceneHandoff* handoff);
bool ray_compound_scene_handoff_validate(
    const RayCompoundSceneHandoff* handoff);

/* Initialize output before first use and free it before reusing it. */
bool ray_compound_scene_handoff_parse(
    const char* text,
    RayCompoundSceneHandoff* output,
    RayCompoundSceneImportFailure* failure);
bool ray_compound_scene_handoff_read(
    const char* path,
    RayCompoundSceneHandoff* output,
    RayCompoundSceneImportFailure* failure);
bool ray_compound_scene_handoff_replay_exact(
    const RayCompoundSceneHandoff* handoff,
    uint64_t tick,
    RayCompoundSceneFrame* output);

const char* ray_compound_scene_import_failure_name(
    RayCompoundSceneImportFailure failure);
