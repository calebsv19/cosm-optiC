#ifndef RENDER_RUNTIME_NATIVE_3D_PROGRESS_HUD_H
#define RENDER_RUNTIME_NATIVE_3D_PROGRESS_HUD_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_tile_scheduler.h"

typedef struct RuntimeNative3DProgressHUDSnapshot {
    bool active;
    RayTracing3DIntegratorId integratorId;
    int startedSubpasses;
    int completedSubpasses;
    int totalSubpasses;
    int frameIndex;
    int pathFrameCount;
    size_t completedTilesInSubpass;
    size_t totalTilesInSubpass;
    double progress01;
    bool estimateReady;
    double estimatedRemainingSeconds;
    double estimatedFrameRemainingSeconds;
    double estimatedBatchRemainingSeconds;
} RuntimeNative3DProgressHUDSnapshot;

void RuntimeNative3DProgressHUD_Reset(void);
void RuntimeNative3DProgressHUD_BeginFrame(RayTracing3DIntegratorId integrator_id,
                                           int total_subpasses,
                                           int frame_index,
                                           int path_frame_count);
void RuntimeNative3DProgressHUD_UpdateTemporal(int started_subpasses,
                                               int completed_subpasses,
                                               int total_subpasses);
void RuntimeNative3DProgressHUD_UpdateTileProgress(
    const RuntimeNative3DTileSchedulerProgress* progress);
void RuntimeNative3DProgressHUD_CompleteFrame(void);
void RuntimeNative3DProgressHUD_Snapshot(RuntimeNative3DProgressHUDSnapshot* out_snapshot);
void RuntimeNative3DProgressHUD_Draw(SDL_Renderer* renderer);

#endif
