#pragma once

#include "import/compound_scene_room_basis.h"
#include "render/compound_scene_assembly.h"
#include "render/compound_scene_room_geometry.h"

#define RAY_COMPOUND_SCENE_INGESTION_SCHEMA "ray_tracing_compound_scene_ingestion_v1"

/* App-local, request-independent descriptor.  I-2 alone may decode it from a
 * render request; I-1 deliberately accepts this typed form only. */
typedef struct RayCompoundSceneIngestionDescriptor {
    char schema[64];
    uint64_t expected_handoff_digest;
    uint64_t expected_room_digest;
    uint64_t tick;
    RayCompoundSceneBindingManifest bindings;
    /* These are existing renderer-owned plane objects.  The sidecar supplies
     * exact geometry; the base runtime scene retains material ownership. */
    char room_object_ids[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT][64];
    char room_material_ids[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT][64];
    bool room_visible[RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT];
} RayCompoundSceneIngestionDescriptor;

typedef struct RayCompoundSceneIngestionResult {
    bool valid;
    uint64_t handoff_digest;
    uint64_t room_digest;
    uint64_t basis_digest;
    uint64_t tick;
    RayEvaluatedSceneSnapshot snapshot;
    RayCompoundSceneRoomGeometry room;
    RayCompoundSceneAssembly assembly;
    uint64_t result_digest;
} RayCompoundSceneIngestionResult;

typedef enum RayCompoundSceneIngestionFailure {
    RAY_COMPOUND_SCENE_INGESTION_FAILURE_NONE = 0,
    RAY_COMPOUND_SCENE_INGESTION_FAILURE_INPUT,
    RAY_COMPOUND_SCENE_INGESTION_FAILURE_DESCRIPTOR,
    RAY_COMPOUND_SCENE_INGESTION_FAILURE_PROVENANCE,
    RAY_COMPOUND_SCENE_INGESTION_FAILURE_RESOLUTION
} RayCompoundSceneIngestionFailure;

void ray_compound_scene_ingestion_descriptor_init(
    RayCompoundSceneIngestionDescriptor* descriptor,
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneStaticRoom* room);
bool ray_compound_scene_ingestion_descriptor_validate(
    const RayCompoundSceneIngestionDescriptor* descriptor,
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneStaticRoom* room);
uint64_t ray_compound_scene_ingestion_result_digest(
    const RayCompoundSceneIngestionResult* result);
bool ray_compound_scene_ingestion_resolve_exact(
    const RayCompoundSceneIngestionDescriptor* descriptor,
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneStaticRoom* room,
    const RayEvaluatedSceneSnapshot* base_snapshot,
    RayCompoundSceneAssemblyRequest* assembly_request,
    RayCompoundSceneIngestionResult* output,
    RayCompoundSceneIngestionFailure* failure);
