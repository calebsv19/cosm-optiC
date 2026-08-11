#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/compound_scene_assembly.h"

#define RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_SCHEMA \
    "ray_tracing_compound_scene_assembly_archive_v1"

enum {
    RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_VERSION = 1,
    RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_MAX_FRAMES = 16,
    RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY = 65536
};

typedef enum RayCompoundSceneStaticAuthority {
    RAY_COMPOUND_SCENE_STATIC_AUTHORITY_UNKNOWN = 0,
    RAY_COMPOUND_SCENE_STATIC_AUTHORITY_RENDERER_SET_DRESSING,
    RAY_COMPOUND_SCENE_STATIC_AUTHORITY_SIMULATION_COLLISION_SURFACE
} RayCompoundSceneStaticAuthority;

typedef struct RayCompoundSceneExternalAssetReference {
    int body_id;
    char object_id[64];
    char mesh_asset_id[64];
    char runtime_path[192];
    char runtime_sha256[65];
    char source_asset_id[64];
    char source_sha256[65];
    uint64_t source_binding_digest;
} RayCompoundSceneExternalAssetReference;

typedef struct RayCompoundSceneStaticSurfaceRecord {
    char object_id[64];
    char geometry_id[64];
    char material_id[64];
    RayCompoundSceneStaticAuthority authority;
    RayCompoundSceneVec3 origin_m;
    RayCompoundSceneVec3 normal;
    double half_extent_u_m;
    double half_extent_v_m;
    uint64_t collision_surface_digest;
    double minimum_clearance_m;
} RayCompoundSceneStaticSurfaceRecord;

typedef struct RayCompoundSceneAssemblyFrameRecord {
    uint64_t tick;
    uint64_t assembly_digest;
    RayCompoundSceneObjectRecord
        simulated[RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT];
} RayCompoundSceneAssemblyFrameRecord;

typedef struct RayCompoundSceneAssemblyArchive {
    bool valid;
    char schema[64];
    uint32_t schema_version;
    uint64_t handoff_digest;
    uint64_t fixture_digest;
    double fixed_dt_s;
    RayCompoundSceneExternalAssetReference
        assets[RAY_COMPOUND_SCENE_ASSEMBLY_SIMULATED_COUNT];
    size_t static_count;
    RayCompoundSceneStaticSurfaceRecord
        statics[RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT];
    size_t frame_count;
    RayCompoundSceneAssemblyFrameRecord
        frames[RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_MAX_FRAMES];
    uint64_t archive_digest;
} RayCompoundSceneAssemblyArchive;

typedef enum RayCompoundSceneAssemblyCodecFailure {
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_PROVENANCE,
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CAPACITY,
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CODEC,
    RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_IO
} RayCompoundSceneAssemblyCodecFailure;

bool ray_compound_scene_assembly_archive_build(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneExternalAssetReference assets[2],
    const RayCompoundSceneStaticSurfaceRecord* statics, size_t static_count,
    const RayCompoundSceneAssembly* assemblies, size_t frame_count,
    RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure);
bool ray_compound_scene_assembly_archive_validate(
    const RayCompoundSceneAssemblyArchive* archive);
uint64_t ray_compound_scene_assembly_archive_digest(
    const RayCompoundSceneAssemblyArchive* archive);
bool ray_compound_scene_assembly_archive_format(
    const RayCompoundSceneAssemblyArchive* archive, char* output,
    size_t output_size);
bool ray_compound_scene_assembly_archive_parse(
    const char* text, RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure);
bool ray_compound_scene_assembly_archive_write(
    const RayCompoundSceneAssemblyArchive* archive, const char* path);
bool ray_compound_scene_assembly_archive_read(
    const char* path, RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure);
bool ray_compound_scene_assembly_archive_replay_exact(
    const RayCompoundSceneAssemblyArchive* archive, uint64_t tick,
    RayCompoundSceneAssemblyFrameRecord* output);
double ray_compound_scene_assembly_clearance_floor_z(
    const RayCompoundSceneAssembly* assemblies, size_t frame_count,
    double clearance_m);
const char* ray_compound_scene_static_authority_name(
    RayCompoundSceneStaticAuthority authority);
