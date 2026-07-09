#ifndef RENDER_RUNTIME_NATIVE_3D_TILE_SCHEDULER_INTERNAL_H
#define RENDER_RUNTIME_NATIVE_3D_TILE_SCHEDULER_INTERNAL_H

#include "render/runtime_native_3d_tile_scheduler.h"

#include <stdint.h>

#include "render/runtime_native_3d_render_unit.h"

#include "render/runtime_volume_3d_integrate.h"

typedef struct RuntimeNative3DTileSchedulerJob {
    RuntimeNative3DRenderUnit renderUnit;
    IntegratorTile tile;
    IntegratorTile parentTile;
    RuntimeNative3DRenderStats subpassStats;
    int subpassIndex;
    int activePixelCount;
    int activeTileCount;
    int inactiveTileCount;
    uint64_t totalRunTicks;
    uint64_t maxSubpassTicks;
    size_t parentMetricIndex;
    bool dispatched;
    bool ok;
    bool occupancyLikely;
} RuntimeNative3DTileSchedulerJob;

typedef struct RuntimeNative3DTileSchedulerParentMetric {
    IntegratorTile tile;
    uint64_t totalRunTicks;
    uint64_t maxSubpassTicks;
    int activePixelCount;
    int activeTileCount;
    int inactiveTileCount;
    int childTileCount;
    bool splitApplied;
} RuntimeNative3DTileSchedulerParentMetric;

typedef struct RuntimeNative3DTileScheduler {
    RuntimeNative3DTileSchedulerJob* jobs;
    RuntimeNative3DTileSchedulerParentMetric* parentMetrics;
    IntegratorTile* progressTiles;
    size_t jobCount;
    size_t parentMetricCount;
    size_t completionCount;
    int plannedParentTileCount;
    int occupancySkippedTileCount;
    int dispatchedTileJobCount;
    int completedTileJobCount;
    int progressDirtyTileBatchCount;
    int progressDirtyTileCount;
    bool firstFrameConservativeTileRender;
    const RuntimeNative3DTileSchedulerControl* control;
    uint64_t cancelGeneration;
    int jobArrayOwnerCount;
    int parentMetricArrayOwnerCount;
    int progressTileArrayOwnerCount;
    int completionQueueOwnerCount;
    int workerPoolOwnerCount;
    int cancelCheckCount;
    int cancelRequestedCount;
    int cancelBeforeDispatchCount;
    int cancelDuringWaitCount;
    int cancelBeforeFinalResolveCount;
    int finalResolveBlockedByCancelCount;
    int workerDrainShutdownCount;
    int workerCancelShutdownCount;
    bool cancelRequested;
    RuntimeNative3DRenderStats stats;
    int committedSubpasses;
    int activePixelCount;
    int activeTileCount;
    int inactiveTileCount;
} RuntimeNative3DTileScheduler;

enum {
    kRuntimeNative3DTileSchedulerMaxWorkers = 4,
    /*
     * Dirty progress presents upload the bounding host rect of each reported
     * dirty set. After priority scheduling, adjacent completions are not
     * guaranteed to be spatially adjacent, so batching can upload unresolved
     * pixels between completed tiles as black.
     */
    kRuntimeNative3DTileSchedulerPreviewBatchDirtyTiles = 1,
    kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents = 4,
    kRuntimeNative3DTileSchedulerAdaptiveEligibleCapacity = 64,
    kRuntimeNative3DTileSchedulerAdaptiveMinChildTileSize = 8
};

static const double kRuntimeNative3DTileSchedulerAdaptiveAbsoluteSplitMs = 12.0;
static const double kRuntimeNative3DTileSchedulerAdaptiveRelativeSplitScale = 2.0;

typedef struct RuntimeNative3DAdaptiveSplitEntry {
    IntegratorTile parentTile;
} RuntimeNative3DAdaptiveSplitEntry;

typedef struct RuntimeNative3DAdaptiveTilePlan {
    RuntimeNative3DAdaptiveSplitEntry splitEntries[kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents];
    size_t splitEntryCount;
    int frameWidth;
    int frameHeight;
    int tileSize;
    int temporalFrames;
    RayTracing3DIntegratorId integratorId;
    bool valid;
} RuntimeNative3DAdaptiveTilePlan;


double runtime_native_3d_tile_scheduler_ticks_to_ms(uint64_t ticks);
bool runtime_native_3d_tile_scheduler_capture_black_hit_pixels(
    const RuntimeNative3DTileSchedulerJob* job,
    const uint8_t* pixel_buffer,
    int pixel_width,
    int subpass_index,
    const char* phase);
bool runtime_native_3d_tile_scheduler_parent_tile_should_split(
    const RuntimeNative3DAdaptiveTilePlan* plan,
    const IntegratorTile* parent_tile);
const RuntimeNative3DAdaptiveTilePlan* runtime_native_3d_tile_scheduler_active_plan(
    const RuntimeNative3DPreparedFrame* frame,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    int tile_size);
int runtime_native_3d_tile_scheduler_compare_dispatch_priority(const void* lhs,
                                                               const void* rhs);
int runtime_native_3d_tile_scheduler_compare_first_subpass_priority(const void* lhs,
                                                                    const void* rhs);
void runtime_native_3d_tile_scheduler_collect_activity(
    RuntimeNative3DTileScheduler* scheduler);
void runtime_native_3d_tile_scheduler_collect_adaptive_state(
    RuntimeNative3DTileScheduler* scheduler);
void runtime_native_3d_tile_scheduler_collect_metrics(
    RuntimeNative3DTileScheduler* scheduler);
void runtime_native_3d_tile_scheduler_update_adaptive_plan(
    const RuntimeNative3DTileScheduler* scheduler,
    const RuntimeNative3DPreparedFrame* frame,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    int tile_size);

#endif
