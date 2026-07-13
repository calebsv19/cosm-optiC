#include "render/runtime_native_3d_tile_scheduler_internal.h"

#include <SDL2/SDL.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_queue.h"
#include "core_workers.h"
#include "render/integrators/integrator_common.h"
#include "render/timer_hud_adapter.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_render_unit.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_volume_3d.h"

static void runtime_native_3d_tile_scheduler_reset(RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) return;
    memset(scheduler, 0, sizeof(*scheduler));
}

static void runtime_native_3d_tile_scheduler_free(RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) return;
    if (scheduler->jobs) {
        for (size_t i = 0; i < scheduler->jobCount; ++i) {
            RuntimeNative3DRenderUnit_ReturnReusable(&scheduler->jobs[i].renderUnit);
        }
    }
    free(scheduler->jobs);
    free(scheduler->parentMetrics);
    free(scheduler->progressTiles);
    runtime_native_3d_tile_scheduler_reset(scheduler);
}

static bool runtime_native_3d_tile_scheduler_cancel_requested(
    RuntimeNative3DTileScheduler* scheduler) {
    const RuntimeNative3DTileSchedulerCancelToken* token = NULL;
    bool requested = false;

    if (!scheduler) {
        return false;
    }
    scheduler->cancelCheckCount += 1;
    token = scheduler->control ? scheduler->control->cancelToken : NULL;
    requested = token && token->cancelRequested && *token->cancelRequested;
    if (requested) {
        scheduler->cancelRequested = true;
        scheduler->cancelRequestedCount += 1;
    }
    return requested;
}

static void runtime_native_3d_tile_scheduler_record_lifetime_stats(
    const RuntimeNative3DTileScheduler* scheduler,
    RuntimeNative3DRenderStats* stats) {
    if (!scheduler || !stats) {
        return;
    }
    stats->temporalTileSchedulerJobArrayOwnerCount = scheduler->jobArrayOwnerCount;
    stats->temporalTileSchedulerParentMetricArrayOwnerCount =
        scheduler->parentMetricArrayOwnerCount;
    stats->temporalTileSchedulerProgressTileArrayOwnerCount =
        scheduler->progressTileArrayOwnerCount;
    stats->temporalTileSchedulerCompletionQueueOwnerCount =
        scheduler->completionQueueOwnerCount;
    stats->temporalTileSchedulerWorkerPoolOwnerCount = scheduler->workerPoolOwnerCount;
    stats->temporalTileSchedulerCancelTokenBound =
        scheduler->control && scheduler->control->cancelToken ? 1 : 0;
    stats->temporalTileSchedulerCancelCheckCount = scheduler->cancelCheckCount;
    stats->temporalTileSchedulerCancelRequestedCount = scheduler->cancelRequestedCount;
    stats->temporalTileSchedulerCancelBeforeDispatchCount =
        scheduler->cancelBeforeDispatchCount;
    stats->temporalTileSchedulerCancelDuringWaitCount = scheduler->cancelDuringWaitCount;
    stats->temporalTileSchedulerCancelBeforeFinalResolveCount =
        scheduler->cancelBeforeFinalResolveCount;
    stats->temporalTileSchedulerFinalResolveBlockedByCancelCount =
        scheduler->finalResolveBlockedByCancelCount;
    stats->temporalTileSchedulerWorkerDrainShutdownCount =
        scheduler->workerDrainShutdownCount;
    stats->temporalTileSchedulerWorkerCancelShutdownCount =
        scheduler->workerCancelShutdownCount;
    stats->temporalTileSchedulerCancelGeneration = scheduler->cancelGeneration;
}


static bool runtime_native_3d_tile_scheduler_emit_job(RuntimeNative3DTileScheduler* scheduler,
                                                      const RuntimeNative3DPreparedFrame* frame,
                                                      RayTracing3DIntegratorId integrator_id,
                                                      int temporal_frames,
                                                      int start_x,
                                                      int start_y,
                                                      int end_x,
                                                      int end_y,
                                                      const IntegratorTile* parent_tile,
                                                      size_t parent_metric_index) {
    RuntimeNative3DTileSchedulerJob* job = NULL;

    if (!scheduler || !frame || !parent_tile) {
        return false;
    }
    if (!RuntimeNative3DPreparedRegionMayContainGeometry(frame,
                                                         start_x,
                                                         start_y,
                                                         end_x,
                                                         end_y)) {
        return true;
    }

    job = &scheduler->jobs[scheduler->jobCount++];
    job->tile.originX = start_x;
    job->tile.originY = start_y;
    job->tile.width = end_x - start_x;
    job->tile.height = end_y - start_y;
    job->tile.energy = NULL;
    job->occupancyLikely =
        RuntimeVolume3D_HasActiveExtinction(&frame->scene.volume) ||
        RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&frame->tileOccupancy,
                                                               start_x,
                                                               start_y,
                                                               end_x,
                                                               end_y);
    job->parentTile = *parent_tile;
    job->parentMetricIndex = parent_metric_index;
    RuntimeNative3DRenderUnit_TakeReusable(&job->renderUnit);
    if (!RuntimeNative3DRenderUnit_Setup(&job->renderUnit,
                                         integrator_id,
                                         frame,
                                         start_x,
                                         start_y,
                                         end_x,
                                         end_y,
                                         &frame->sampling,
                                         temporal_frames,
                                         animSettings.disneyDenoiseEnabled)) {
        RuntimeNative3DRenderUnit_Free(&job->renderUnit);
        return false;
    }

    scheduler->parentMetrics[parent_metric_index].childTileCount += 1;
    return true;
}

static bool runtime_native_3d_tile_scheduler_build_jobs(RuntimeNative3DTileScheduler* scheduler,
                                                        const RuntimeNative3DPreparedFrame* frame,
                                                        RayTracing3DIntegratorId integrator_id,
                                                        int temporal_frames,
                                                        int tile_size) {
    const RuntimeNative3DAdaptiveTilePlan* adaptive_plan = NULL;
    const int tiles_x = (frame->width + tile_size - 1) / tile_size;
    const int tiles_y = (frame->height + tile_size - 1) / tile_size;
    const size_t max_parent_tiles = (size_t)tiles_x * (size_t)tiles_y;
    const size_t max_tiles = max_parent_tiles * 4u;

    scheduler->jobs = (RuntimeNative3DTileSchedulerJob*)calloc(max_tiles, sizeof(*scheduler->jobs));
    if (!scheduler->jobs) {
        return false;
    }
    scheduler->jobArrayOwnerCount = 1;
    scheduler->parentMetrics =
        (RuntimeNative3DTileSchedulerParentMetric*)calloc(max_parent_tiles,
                                                          sizeof(*scheduler->parentMetrics));
    if (!scheduler->parentMetrics) {
        return false;
    }
    scheduler->parentMetricArrayOwnerCount = 1;
    scheduler->progressTiles = (IntegratorTile*)calloc(max_tiles, sizeof(*scheduler->progressTiles));
    if (!scheduler->progressTiles) {
        return false;
    }
    scheduler->progressTileArrayOwnerCount = 1;
    adaptive_plan = runtime_native_3d_tile_scheduler_active_plan(frame,
                                                                   integrator_id,
                                                                   temporal_frames,
                                                                   tile_size);

    for (int tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles_x; ++tile_x) {
            const int start_x = tile_x * tile_size;
            const int start_y = tile_y * tile_size;
            const int end_x = (start_x + tile_size < frame->width) ? (start_x + tile_size)
                                                                    : frame->width;
            const int end_y = (start_y + tile_size < frame->height) ? (start_y + tile_size)
                                                                     : frame->height;
            IntegratorTile parent_tile = {0};
            RuntimeNative3DTileSchedulerParentMetric* parent_metric = NULL;
            const bool split_parent = runtime_native_3d_tile_scheduler_parent_tile_should_split(
                adaptive_plan,
                &(IntegratorTile){
                    .originX = start_x,
                    .originY = start_y,
                    .width = end_x - start_x,
                    .height = end_y - start_y,
                    .energy = NULL});

            scheduler->plannedParentTileCount += 1;
            if (!RuntimeNative3DPreparedRegionMayContainGeometry(frame,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y)) {
                scheduler->occupancySkippedTileCount += 1;
                continue;
            }

            parent_tile.originX = start_x;
            parent_tile.originY = start_y;
            parent_tile.width = end_x - start_x;
            parent_tile.height = end_y - start_y;
            parent_tile.energy = NULL;
            parent_metric = &scheduler->parentMetrics[scheduler->parentMetricCount];
            parent_metric->tile = parent_tile;
            parent_metric->splitApplied = split_parent;
            scheduler->parentMetricCount += 1u;

            if (split_parent) {
                const int mid_x = start_x + (parent_tile.width / 2);
                const int mid_y = start_y + (parent_tile.height / 2);
                const int child_start_x[4] = {start_x, mid_x, start_x, mid_x};
                const int child_end_x[4] = {mid_x, end_x, mid_x, end_x};
                const int child_start_y[4] = {start_y, start_y, mid_y, mid_y};
                const int child_end_y[4] = {mid_y, mid_y, end_y, end_y};

                for (size_t child = 0; child < 4u; ++child) {
                    if (child_start_x[child] >= child_end_x[child] ||
                        child_start_y[child] >= child_end_y[child]) {
                        continue;
                    }
                    if (!runtime_native_3d_tile_scheduler_emit_job(scheduler,
                                                                   frame,
                                                                   integrator_id,
                                                                   temporal_frames,
                                                                   child_start_x[child],
                                                                   child_start_y[child],
                                                                   child_end_x[child],
                                                                   child_end_y[child],
                                                                   &parent_tile,
                                                                   scheduler->parentMetricCount - 1u)) {
                        return false;
                    }
                }
            } else if (!runtime_native_3d_tile_scheduler_emit_job(scheduler,
                                                                  frame,
                                                                  integrator_id,
                                                                  temporal_frames,
                                                                  start_x,
                                                                  start_y,
                                                                  end_x,
                                                                  end_y,
                                                                  &parent_tile,
                                                                  scheduler->parentMetricCount - 1u)) {
                return false;
            }
        }
    }

    return true;
}

static void* runtime_native_3d_tile_scheduler_run_job(void* task_ctx) {
    RuntimeNative3DTileSchedulerJob* job = (RuntimeNative3DTileSchedulerJob*)task_ctx;
    const uint64_t start_ticks = (uint64_t)SDL_GetPerformanceCounter();
    uint64_t elapsed_ticks = 0u;

    if (!job) return NULL;
    memset(&job->subpassStats, 0, sizeof(job->subpassStats));
    job->ok = RuntimeNative3DRenderUnit_RenderSubpass(&job->renderUnit,
                                                      job->subpassIndex,
                                                      &job->subpassStats);
    elapsed_ticks = (uint64_t)SDL_GetPerformanceCounter() - start_ticks;
    job->totalRunTicks += elapsed_ticks;
    if (elapsed_ticks > job->maxSubpassTicks) {
        job->maxSubpassTicks = elapsed_ticks;
    }
    return job;
}

static bool runtime_native_3d_tile_scheduler_flush_progress_tiles(
    RuntimeNative3DTileScheduler* scheduler,
    const IntegratorTile* dirty_tiles,
    size_t dirty_count,
    int subpass_index,
    int temporal_frames,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data) {
    RuntimeNative3DTileSchedulerProgress progress = {0};

    if (!tile_progress_callback || !dirty_tiles || dirty_count == 0u) {
        return true;
    }
    if (scheduler) {
        scheduler->progressDirtyTileBatchCount += 1;
        scheduler->progressDirtyTileCount += (int)dirty_count;
    }
    progress.dirtyTiles = dirty_tiles;
    progress.dirtyTileCount = dirty_count;
    progress.startedSubpasses = subpass_index + 1;
    progress.completedSubpasses =
        (completed_tiles_in_subpass >= total_tiles_in_subpass)
            ? subpass_index + 1
            : subpass_index;
    progress.totalSubpasses = temporal_frames;
    progress.completedTilesInSubpass = completed_tiles_in_subpass;
    progress.totalTilesInSubpass = total_tiles_in_subpass;
    return tile_progress_callback(&progress, tile_progress_user_data);
}

static bool runtime_native_3d_tile_scheduler_complete_minimum_progress_samples(
    RuntimeNative3DTileScheduler* scheduler,
    RuntimeNative3DTileSchedulerJob* job,
    uint8_t* pixel_buffer,
    int pixel_width,
    int temporal_frames) {
    if (!scheduler || !job || !pixel_buffer || pixel_width <= 0 || temporal_frames <= 0) {
        return false;
    }

    while (job->renderUnit.committedSubpasses < RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES &&
           job->renderUnit.committedSubpasses < temporal_frames) {
        RuntimeNative3DRenderStats extra_stats = {0};
        const int next_subpass = job->renderUnit.committedSubpasses;
        if (!RuntimeNative3DRenderUnit_RenderSubpass(&job->renderUnit,
                                                     next_subpass,
                                                     &extra_stats)) {
            return false;
        }
        RuntimeNative3DRenderStats_Accumulate(&scheduler->stats, &extra_stats);
    }

    RuntimeNative3DRenderUnit_GetActivityCounts(&job->renderUnit,
                                                &job->activePixelCount,
                                                &job->activeTileCount,
                                                &job->inactiveTileCount);
    return RuntimeNative3DRenderUnit_ResolveCurrentRawToPixels(&job->renderUnit,
                                                               pixel_buffer,
                                                               pixel_width);
}

static bool runtime_native_3d_tile_scheduler_wait_for_subpass(
    RuntimeNative3DTileScheduler* scheduler,
    CoreQueueMutex* completion_queue,
    size_t expected_completions,
    uint8_t* pixel_buffer,
    int pixel_width,
    int subpass_index,
    int temporal_frames,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data) {
    size_t completions = 0u;
    size_t dirty_count = 0u;

    if (expected_completions == 0u) {
        return true;
    }
    while (completions < expected_completions) {
        void* completion = NULL;
        if (runtime_native_3d_tile_scheduler_cancel_requested(scheduler)) {
            scheduler->cancelDuringWaitCount += 1;
            return false;
        }
        if (!core_queue_mutex_timed_pop(completion_queue, &completion, 100u)) {
            continue;
        }
        RuntimeNative3DTileSchedulerJob* job = (RuntimeNative3DTileSchedulerJob*)completion;
        if (!job || !job->ok) {
            return false;
        }
        RuntimeNative3DRenderUnit_GetActivityCounts(&job->renderUnit,
                                                    &job->activePixelCount,
                                                    &job->activeTileCount,
                                                    &job->inactiveTileCount);
        RuntimeNative3DRenderStats_Accumulate(&scheduler->stats, &job->subpassStats);
        if (tile_progress_callback && pixel_buffer && pixel_width > 0) {
            const size_t completed_tiles_in_subpass = completions + 1u;
            bool black_geometry_hit_tile = false;
            bool hold_underconverged_adaptive_progress = false;
            const bool may_hold_underconverged_adaptive_progress =
                job->renderUnit.useAdaptiveSampling &&
                subpass_index + 1 < RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES;
            if (!RuntimeNative3DRenderUnit_ResolveCurrentRawToPixels(&job->renderUnit,
                                                                     pixel_buffer,
                                                                     pixel_width)) {
                return false;
            }
            black_geometry_hit_tile =
                runtime_native_3d_tile_scheduler_capture_black_hit_pixels(job,
                                                                          pixel_buffer,
                                                                          pixel_width,
                                                                          subpass_index,
                                                                          "progress_raw_resolve");
            hold_underconverged_adaptive_progress =
                may_hold_underconverged_adaptive_progress && black_geometry_hit_tile;
            if (hold_underconverged_adaptive_progress) {
                if (!runtime_native_3d_tile_scheduler_complete_minimum_progress_samples(
                        scheduler,
                        job,
                        pixel_buffer,
                        pixel_width,
                        temporal_frames)) {
                    return false;
                }
                runtime_native_3d_tile_scheduler_capture_black_hit_pixels(job,
                                                                          pixel_buffer,
                                                                          pixel_width,
                                                                          job->renderUnit
                                                                              .committedSubpasses,
                                                                          "progress_min_resolve");
            }
            scheduler->progressTiles[dirty_count++] = job->tile;
            if (dirty_count >= kRuntimeNative3DTileSchedulerPreviewBatchDirtyTiles) {
                if (!runtime_native_3d_tile_scheduler_flush_progress_tiles(
                        scheduler,
                        scheduler->progressTiles,
                        dirty_count,
                        subpass_index,
                        temporal_frames,
                        completed_tiles_in_subpass,
                        expected_completions,
                        tile_progress_callback,
                        tile_progress_user_data)) {
                    return false;
                }
                dirty_count = 0u;
            }
        }
        completions += 1u;
    }
    if (dirty_count > 0u) {
        if (!runtime_native_3d_tile_scheduler_flush_progress_tiles(scheduler,
                                                                   scheduler->progressTiles,
                                                                   dirty_count,
                                                                   subpass_index,
                                                                   temporal_frames,
                                                                   expected_completions,
                                                                   expected_completions,
                                                                   tile_progress_callback,
                                                                   tile_progress_user_data)) {
            return false;
        }
    }
    scheduler->completionCount += completions;
    scheduler->completedTileJobCount += (int)completions;
    return true;
}

static bool runtime_native_3d_tile_scheduler_dispatch_subpass(
    RuntimeNative3DTileScheduler* scheduler,
    CoreWorkers* workers,
    CoreQueueMutex* completion_queue,
    uint8_t* pixel_buffer,
    int pixel_width,
    int subpass_index,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data) {
    size_t dispatched = 0u;
    uint64_t subpass_wait_start_ticks = 0u;

    if (scheduler->jobCount > 1u) {
        qsort(scheduler->jobs,
              scheduler->jobCount,
              sizeof(*scheduler->jobs),
              subpass_index == 0
                  ? runtime_native_3d_tile_scheduler_compare_first_subpass_priority
                  : runtime_native_3d_tile_scheduler_compare_dispatch_priority);
    }

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        job->dispatched = RuntimeNative3DRenderUnit_ShouldRenderSubpass(&job->renderUnit,
                                                                        subpass_index);
        if (!job->dispatched) {
            continue;
        }
        job->subpassIndex = subpass_index;
        job->ok = false;
        dispatched += 1u;
    }
    scheduler->dispatchedTileJobCount += (int)dispatched;

    if (dispatched == 0u) {
        return true;
    }

    if (runtime_native_3d_tile_scheduler_cancel_requested(scheduler)) {
        scheduler->cancelBeforeDispatchCount += 1;
        return false;
    }

    if (progress_callback) {
        progress_callback(subpass_index + 1,
                          scheduler->committedSubpasses,
                          temporal_frames,
                          progress_user_data);
    }

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        if (!job->dispatched) {
            continue;
        }
        if (!core_workers_submit(workers, runtime_native_3d_tile_scheduler_run_job, job)) {
            return false;
        }
    }

    subpass_wait_start_ticks = (uint64_t)SDL_GetPerformanceCounter();
    if (!runtime_native_3d_tile_scheduler_wait_for_subpass(scheduler,
                                                           completion_queue,
                                                           dispatched,
                                                           pixel_buffer,
                                                           pixel_width,
                                                           subpass_index,
                                                           temporal_frames,
                                                           tile_progress_callback,
                                                           tile_progress_user_data)) {
        return false;
    }
    timer_hud_record_duration_ms(
        "Tile Subpass",
        runtime_native_3d_tile_scheduler_ticks_to_ms((uint64_t)SDL_GetPerformanceCounter() -
                                                     subpass_wait_start_ticks));

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        if (!job->dispatched) {
            continue;
        }
        if (!job->ok) {
            return false;
        }
    }
    scheduler->committedSubpasses += 1;
    if (progress_callback) {
        progress_callback(subpass_index + 1,
                          scheduler->committedSubpasses,
                          temporal_frames,
                          progress_user_data);
    }
    return true;
}


bool RuntimeNative3DRenderPreparedFrameTemporalTiled(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(pixel_buffer,
                                                                       integrator_id,
                                                                       frame,
                                                                       temporal_frames,
                                                                       progress_callback,
                                                                       progress_user_data,
                                                                       NULL,
                                                                       NULL,
                                                                       out_stats);
}

bool RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
        pixel_buffer,
        integrator_id,
        frame,
        temporal_frames,
        progress_callback,
        progress_user_data,
        tile_progress_callback,
        tile_progress_user_data,
        NULL,
        out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
        pixel_buffer,
        integrator_id,
        frame,
        temporal_frames,
        progress_callback,
        progress_user_data,
        tile_progress_callback,
        tile_progress_user_data,
        resource_budget,
        NULL,
        out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DTileScheduler scheduler = {0};
    const RuntimeScene3D* trace_scene = NULL;
    const uint64_t frame_start_ticks = (uint64_t)SDL_GetPerformanceCounter();
    const int effective_temporal_frames = (temporal_frames <= 1) ? 1 : temporal_frames;
    const int effective_tile_size = RuntimeNative3DTileSchedulerResolveTileSizeForScale(
        animSettings.tileSize,
        animSettings.renderScale3D);
    bool occupancy_ok = false;
    bool ok = false;
    pthread_t* thread_slots = NULL;
    CoreWorkerTask* task_slots = NULL;
    void** completion_slots = NULL;
    CoreQueueMutex completion_queue = {0};
    CoreWorkers workers = {0};
    size_t worker_count = 0u;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!pixel_buffer || !frame || !frame->valid || frame->width <= 0 || frame->height <= 0) {
        return false;
    }

    trace_scene = frame->traceScene ? frame->traceScene : &frame->scene;
    if (frame->traceScene &&
        RuntimeRay3D_CurrentTraceRoute() !=
            RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH &&
        !RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(trace_scene)) {
        return false;
    }

    occupancy_ok = RuntimeNative3DPrepareFrameTileOccupancy(
        frame, effective_tile_size);
    (void)occupancy_ok;

    runtime_native_3d_tile_scheduler_reset(&scheduler);
    scheduler.control = scheduler_control;
    if (scheduler_control && scheduler_control->cancelToken) {
        scheduler.cancelGeneration = scheduler_control->cancelToken->generation;
    }
    scheduler.firstFrameConservativeTileRender = frame->tileOccupancyConservativeAllTiles;
    if (!runtime_native_3d_tile_scheduler_build_jobs(&scheduler,
                                                     frame,
                                                     integrator_id,
                                                     effective_temporal_frames,
                                                     effective_tile_size)) {
        goto finalize;
    }
    if (scheduler.jobCount == 0u) {
        ok = true;
        goto finalize;
    }

    worker_count = RuntimeNative3DTileSchedulerResolveWorkerCountForCpuBudgeted(
        scheduler.jobCount,
        SDL_GetCPUCount(),
        tile_progress_callback != NULL,
        resource_budget);

    thread_slots = (pthread_t*)calloc(worker_count, sizeof(*thread_slots));
    task_slots = (CoreWorkerTask*)calloc(scheduler.jobCount, sizeof(*task_slots));
    completion_slots = (void**)calloc(scheduler.jobCount, sizeof(*completion_slots));
    if (!thread_slots || !task_slots || !completion_slots) {
        goto finalize;
    }

    if (!core_queue_mutex_init(&completion_queue, completion_slots, scheduler.jobCount)) {
        goto finalize;
    }
    scheduler.completionQueueOwnerCount = 1;
    if (!core_workers_init(&workers,
                           thread_slots,
                           worker_count,
                           task_slots,
                           scheduler.jobCount,
                           &completion_queue)) {
        core_queue_mutex_destroy(&completion_queue);
        scheduler.completionQueueOwnerCount = 0;
        goto finalize;
    }
    scheduler.workerPoolOwnerCount = 1;

    ok = true;
    for (int subpass = 0; ok && subpass < effective_temporal_frames; ++subpass) {
        ok = runtime_native_3d_tile_scheduler_dispatch_subpass(&scheduler,
                                                               &workers,
                                                               &completion_queue,
                                                               pixel_buffer,
                                                               frame->width,
                                                               subpass,
                                                               effective_temporal_frames,
                                                               progress_callback,
                                                               progress_user_data,
                                                               tile_progress_callback,
                                                               tile_progress_user_data);
        if (ok && scheduler.committedSubpasses == subpass) {
            break;
        }
    }

    if (scheduler.cancelRequested) {
        core_workers_shutdown_with_mode(&workers, CORE_WORKERS_SHUTDOWN_CANCEL);
        scheduler.workerCancelShutdownCount += 1;
    } else {
        core_workers_shutdown(&workers);
        scheduler.workerDrainShutdownCount += 1;
    }
    core_queue_mutex_destroy(&completion_queue);

    if (!ok) {
        goto finalize;
    }

    if (runtime_native_3d_tile_scheduler_cancel_requested(&scheduler)) {
        scheduler.cancelBeforeFinalResolveCount += 1;
        scheduler.finalResolveBlockedByCancelCount += 1;
        ok = false;
        goto finalize;
    }

    for (size_t i = 0; i < scheduler.jobCount; ++i) {
        RuntimeNative3DRenderStats resolve_stats = {0};
        const bool heatmap_enabled =
            RuntimeNative3DAdaptiveSampling_TemporalBudgetHeatmapEnabled();
        if (heatmap_enabled) {
            if (!RuntimeNative3DRenderUnit_ResolveTemporalBudgetHeatmapToPixels(
                    &scheduler.jobs[i].renderUnit,
                    pixel_buffer,
                    frame->width)) {
                ok = false;
                break;
            }
            resolve_stats.temporalAdaptiveBudgetHeatmapEnabled = 1;
        } else if (!RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(
                       &scheduler.jobs[i].renderUnit,
                       pixel_buffer,
                       frame->width,
                       &resolve_stats)) {
            ok = false;
            break;
        }
        if (!heatmap_enabled) {
            runtime_native_3d_tile_scheduler_capture_black_hit_pixels(
                &scheduler.jobs[i],
                pixel_buffer,
                frame->width,
                scheduler.jobs[i].renderUnit.committedSubpasses,
                "final_resolve");
        }
        RuntimeNative3DRenderStats_Accumulate(&scheduler.stats, &resolve_stats);
        RuntimeNative3DRenderUnit_RecordScratchStats(&scheduler.jobs[i].renderUnit,
                                                     &scheduler.stats);
    }
    if (!ok) {
        goto finalize;
    }
    scheduler.stats.temporalFinalFullResolveCount += 1;

    runtime_native_3d_tile_scheduler_collect_activity(&scheduler);
    runtime_native_3d_tile_scheduler_collect_adaptive_state(&scheduler);
    runtime_native_3d_tile_scheduler_collect_metrics(&scheduler);

finalize:
    timer_hud_record_duration_ms(
        "Tile Full Frame",
        runtime_native_3d_tile_scheduler_ticks_to_ms((uint64_t)SDL_GetPerformanceCounter() -
                                                     frame_start_ticks));
    if (ok && out_stats) {
        *out_stats = scheduler.stats;
        runtime_native_3d_tile_scheduler_record_lifetime_stats(&scheduler, out_stats);
        out_stats->temporalCommittedSubpasses = scheduler.committedSubpasses;
        out_stats->temporalActivePixelCount = scheduler.activePixelCount;
        out_stats->temporalActiveTileCount = scheduler.activeTileCount;
        out_stats->temporalInactiveTileCount = scheduler.inactiveTileCount;
        out_stats->temporalPlannedParentTileCount = scheduler.plannedParentTileCount;
        out_stats->temporalEmittedTileJobCount = (int)scheduler.jobCount;
        out_stats->temporalOccupancySkippedTileCount = scheduler.occupancySkippedTileCount;
        out_stats->temporalDispatchedTileJobCount = scheduler.dispatchedTileJobCount;
        out_stats->temporalCompletedTileJobCount = scheduler.completedTileJobCount;
        out_stats->temporalProgressDirtyBatchCount = scheduler.progressDirtyTileBatchCount;
        out_stats->temporalProgressDirtyTileCount = scheduler.progressDirtyTileCount;
        out_stats->temporalConservativeFirstFrameTileRender =
            scheduler.firstFrameConservativeTileRender ? 1 : 0;
    } else if (!ok && out_stats && scheduler.cancelRequested) {
        *out_stats = scheduler.stats;
        runtime_native_3d_tile_scheduler_record_lifetime_stats(&scheduler, out_stats);
        out_stats->temporalCommittedSubpasses = scheduler.committedSubpasses;
        out_stats->temporalPlannedParentTileCount = scheduler.plannedParentTileCount;
        out_stats->temporalEmittedTileJobCount = (int)scheduler.jobCount;
        out_stats->temporalOccupancySkippedTileCount = scheduler.occupancySkippedTileCount;
        out_stats->temporalDispatchedTileJobCount = scheduler.dispatchedTileJobCount;
        out_stats->temporalCompletedTileJobCount = scheduler.completedTileJobCount;
        out_stats->temporalProgressDirtyBatchCount = scheduler.progressDirtyTileBatchCount;
        out_stats->temporalProgressDirtyTileCount = scheduler.progressDirtyTileCount;
        out_stats->temporalConservativeFirstFrameTileRender =
            scheduler.firstFrameConservativeTileRender ? 1 : 0;
    }
    if (ok) {
        runtime_native_3d_tile_scheduler_update_adaptive_plan(&scheduler,
                                                              frame,
                                                              integrator_id,
                                                              effective_temporal_frames,
                                                              effective_tile_size);
    }
    runtime_native_3d_tile_scheduler_free(&scheduler);
    free(thread_slots);
    free(task_slots);
    free(completion_slots);
    return ok;
}
