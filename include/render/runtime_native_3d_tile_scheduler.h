#ifndef RENDER_RUNTIME_NATIVE_3D_TILE_SCHEDULER_H
#define RENDER_RUNTIME_NATIVE_3D_TILE_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "render/integrators/integrator_common.h"
#include "render/runtime_native_3d_render.h"

typedef struct RuntimeNative3DTileSchedulerProgress {
    const IntegratorTile* dirtyTiles;
    size_t dirtyTileCount;
    int startedSubpasses;
    int completedSubpasses;
    int totalSubpasses;
} RuntimeNative3DTileSchedulerProgress;

typedef bool (*RuntimeNative3DTileSchedulerProgressCallback)(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data);

int RuntimeNative3DTileSchedulerResolveTileSize(int requested);
int RuntimeNative3DTileSchedulerResolveTileSizeForScale(int requested, int render_scale);
size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(size_t job_count,
                                                            int cpu_count,
                                                            bool interactive_preview);
size_t RuntimeNative3DTileSchedulerResolveWorkerCount(size_t job_count,
                                                      bool interactive_preview);
bool RuntimeNative3DRenderPreparedFrameTemporalTiled(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    RuntimeNative3DRenderStats* out_stats);

#endif
