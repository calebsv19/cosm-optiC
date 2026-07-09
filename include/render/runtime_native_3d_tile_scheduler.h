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
    size_t completedTilesInSubpass;
    size_t totalTilesInSubpass;
} RuntimeNative3DTileSchedulerProgress;

typedef struct RuntimeNative3DAdaptiveTilePlanSnapshot {
    bool valid;
    int frameWidth;
    int frameHeight;
    int tileSize;
    int temporalFrames;
    RayTracing3DIntegratorId integratorId;
    size_t splitEntryCount;
    IntegratorTile splitEntries[4];
} RuntimeNative3DAdaptiveTilePlanSnapshot;

typedef bool (*RuntimeNative3DTileSchedulerProgressCallback)(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data);

typedef struct RuntimeNative3DTileSchedulerCancelToken {
    const volatile bool* cancelRequested;
    uint64_t generation;
} RuntimeNative3DTileSchedulerCancelToken;

typedef struct RuntimeNative3DTileSchedulerControl {
    const RuntimeNative3DTileSchedulerCancelToken* cancelToken;
} RuntimeNative3DTileSchedulerControl;

int RuntimeNative3DTileSchedulerResolveTileSize(int requested);
int RuntimeNative3DTileSchedulerResolveTileSizeForScale(int requested, int render_scale);
int RuntimeNative3DTileSchedulerAdaptiveMinChildTileSize(void);
size_t RuntimeNative3DTileSchedulerAdaptiveMaxSplitParents(void);
bool RuntimeNative3DTileSchedulerTileEligibleForAdaptiveSplit(int tile_width,
                                                              int tile_height,
                                                              int min_child_tile_size);
bool RuntimeNative3DTileSchedulerTileShouldAdaptiveSplit(int tile_width,
                                                         int tile_height,
                                                         double total_tile_ms,
                                                         double average_tile_ms);
size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(size_t job_count,
                                                            int cpu_count,
                                                            bool interactive_preview);
size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpuBudgeted(
    size_t job_count,
    int cpu_count,
    bool interactive_preview,
    const RuntimeNative3DResourceBudget* resource_budget);
size_t RuntimeNative3DTileSchedulerResolveWorkerCount(size_t job_count,
                                                      bool interactive_preview);
void RuntimeNative3DTileSchedulerResetAdaptivePlan(void);
void RuntimeNative3DTileSchedulerSnapshotAdaptivePlan(
    RuntimeNative3DAdaptiveTilePlanSnapshot* out_snapshot);
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
bool RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    const RuntimeNative3DResourceBudget* resource_budget,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    const RuntimeNative3DResourceBudget* resource_budget,
    const RuntimeNative3DTileSchedulerControl* scheduler_control,
    RuntimeNative3DRenderStats* out_stats);

#endif
