#ifndef RAY_TRACING_TEMPORAL_CHECKPOINT_H
#define RAY_TRACING_TEMPORAL_CHECKPOINT_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "app/ray_tracing_sha256.h"
#include "app/agent_render_request.h"
#include "render/runtime_native_3d_tile_scheduler.h"

#define RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION 2

typedef struct RayTracingTemporalCheckpointIdentity {
    char requestSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char sceneSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char assetSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char runtimeSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char rendererSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char samplingSha256[RAY_TRACING_SHA256_HEX_SIZE];
    int frameIndex;
    int width;
    int height;
    int tileSize;
    int temporalFrames;
    int integratorId;
} RayTracingTemporalCheckpointIdentity;

typedef struct RayTracingTemporalCheckpointSession {
    bool enabled;
    bool resumeRequested;
    bool resumed;
    int resumedSubpasses;
    int resumedActiveSubpass;
    size_t resumedTilesInSubpass;
    int checkpointsWritten;
    int tileBatchCheckpointsWritten;
    uint64_t nextGeneration;
    uint64_t totalWriteNanoseconds;
    uint64_t lastWriteNanoseconds;
    uint64_t maximumWriteNanoseconds;
    int testExitAfterSubpass;
    char root[PATH_MAX];
    char latestPath[PATH_MAX];
    char latestSha256[RAY_TRACING_SHA256_HEX_SIZE];
    char diagnostics[256];
    RayTracingTemporalCheckpointIdentity identity;
} RayTracingTemporalCheckpointSession;

void ray_tracing_temporal_checkpoint_session_init(
    RayTracingTemporalCheckpointSession* session);
bool ray_tracing_temporal_checkpoint_configure_frame(
    RayTracingTemporalCheckpointSession* session,
    const RayTracingAgentRenderRequest* request,
    const char* request_path,
    int frame_index,
    int integrator_id,
    int effective_tile_size,
    const char* const* resolved_asset_paths,
    size_t resolved_asset_path_count);
bool ray_tracing_temporal_checkpoint_identity_validate(
    const RayTracingTemporalCheckpointIdentity* identity,
    char* diagnostics,
    size_t diagnostics_size);
bool ray_tracing_temporal_checkpoint_restore(
    RuntimeNative3DCheckpointTile* tiles,
    size_t tile_count,
    int temporal_frames,
    int* out_completed_subpasses,
    void* user_data);
bool ray_tracing_temporal_checkpoint_commit(
    const RuntimeNative3DCheckpointTile* tiles,
    size_t tile_count,
    int completed_subpasses,
    int active_subpass,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    int temporal_frames,
    void* user_data);
bool ray_tracing_temporal_checkpoint_latest_reference(
    const char* checkpoint_root,
    int frame_index,
    char* out_path,
    size_t out_path_size,
    char out_sha256[RAY_TRACING_SHA256_HEX_SIZE],
    int* out_completed_subpasses,
    int* out_active_subpass,
    size_t* out_completed_tiles_in_subpass);

#endif
