#include "render/runtime_native_3d_tile_scheduler.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_queue.h"
#include "core_workers.h"
#include "render/integrators/integrator_common.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_render_unit.h"

typedef struct RuntimeNative3DTileSchedulerJob {
    RuntimeNative3DRenderUnit renderUnit;
    IntegratorTile tile;
    RuntimeNative3DRenderStats subpassStats;
    int subpassIndex;
    uint64_t totalRunTicks;
    uint64_t maxSubpassTicks;
    bool dispatched;
    bool ok;
} RuntimeNative3DTileSchedulerJob;

typedef struct RuntimeNative3DTileScheduler {
    RuntimeNative3DTileSchedulerJob* jobs;
    IntegratorTile* progressTiles;
    size_t jobCount;
    size_t completionCount;
    RuntimeNative3DRenderStats stats;
    int committedSubpasses;
    int activePixelCount;
    int activeTileCount;
    int inactiveTileCount;
} RuntimeNative3DTileScheduler;

static const size_t kRuntimeNative3DTileSchedulerMaxWorkers = 4u;
static const size_t kRuntimeNative3DTileSchedulerPreviewBatchDirtyTiles = 4u;

static void runtime_native_3d_tile_scheduler_reset(RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) return;
    memset(scheduler, 0, sizeof(*scheduler));
}

static void runtime_native_3d_tile_scheduler_free(RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) return;
    if (scheduler->jobs) {
        for (size_t i = 0; i < scheduler->jobCount; ++i) {
            RuntimeNative3DRenderUnit_Free(&scheduler->jobs[i].renderUnit);
        }
    }
    free(scheduler->jobs);
    free(scheduler->progressTiles);
    runtime_native_3d_tile_scheduler_reset(scheduler);
}

int RuntimeNative3DTileSchedulerResolveTileSize(int requested) {
    if (requested <= 0) {
        requested = 16;
    }
    return ClampTileSize(requested);
}

int RuntimeNative3DTileSchedulerResolveTileSizeForScale(int requested, int render_scale) {
    const int base_tile_size = RuntimeNative3DTileSchedulerResolveTileSize(requested);
    int clamped_scale = RuntimeNative3DClampRenderScale(render_scale);
    int adjusted = base_tile_size;

    if (clamped_scale == RUNTIME_3D_RENDER_SCALE_HIDPI) {
        clamped_scale = 1;
    }
    if (clamped_scale <= 1) {
        return base_tile_size;
    }

    adjusted = (int)lround((double)base_tile_size / sqrt((double)clamped_scale));
    if (adjusted > base_tile_size) {
        adjusted = base_tile_size;
    }
    return ClampTileSize(adjusted);
}

size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(size_t job_count,
                                                            int cpu_count,
                                                            bool interactive_preview) {
    size_t worker_count = 0u;

    if (job_count == 0u) {
        return 0u;
    }

    worker_count = (cpu_count > 0) ? (size_t)cpu_count : 1u;
    if (interactive_preview && worker_count > 1u) {
        worker_count -= 1u;
    }
    if (worker_count == 0u) {
        worker_count = 1u;
    }
    if (worker_count > job_count) {
        worker_count = job_count;
    }
    if (worker_count > kRuntimeNative3DTileSchedulerMaxWorkers) {
        worker_count = kRuntimeNative3DTileSchedulerMaxWorkers;
    }
    return worker_count;
}

size_t RuntimeNative3DTileSchedulerResolveWorkerCount(size_t job_count,
                                                      bool interactive_preview) {
    return RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(job_count,
                                                               SDL_GetCPUCount(),
                                                               interactive_preview);
}

static bool runtime_native_3d_tile_scheduler_build_jobs(RuntimeNative3DTileScheduler* scheduler,
                                                        const RuntimeNative3DPreparedFrame* frame,
                                                        RayTracing3DIntegratorId integrator_id,
                                                        int temporal_frames,
                                                        int tile_size) {
    const int tiles_x = (frame->width + tile_size - 1) / tile_size;
    const int tiles_y = (frame->height + tile_size - 1) / tile_size;
    const size_t max_tiles = (size_t)tiles_x * (size_t)tiles_y;

    scheduler->jobs = (RuntimeNative3DTileSchedulerJob*)calloc(max_tiles, sizeof(*scheduler->jobs));
    if (!scheduler->jobs) {
        return false;
    }
    scheduler->progressTiles = (IntegratorTile*)calloc(max_tiles, sizeof(*scheduler->progressTiles));
    if (!scheduler->progressTiles) {
        return false;
    }

    for (int tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles_x; ++tile_x) {
            RuntimeNative3DTileSchedulerJob* job = NULL;
            const int start_x = tile_x * tile_size;
            const int start_y = tile_y * tile_size;
            const int end_x = (start_x + tile_size < frame->width) ? (start_x + tile_size)
                                                                    : frame->width;
            const int end_y = (start_y + tile_size < frame->height) ? (start_y + tile_size)
                                                                     : frame->height;

            if (!RuntimeNative3DPreparedRegionMayContainGeometry(frame,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y)) {
                continue;
            }

            job = &scheduler->jobs[scheduler->jobCount++];
            job->tile.originX = start_x;
            job->tile.originY = start_y;
            job->tile.width = end_x - start_x;
            job->tile.height = end_y - start_y;
            job->tile.energy = NULL;
            RuntimeNative3DRenderUnit_Init(&job->renderUnit);
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
    const IntegratorTile* dirty_tiles,
    size_t dirty_count,
    int subpass_index,
    int temporal_frames,
    RuntimeNative3DTileSchedulerProgressCallback tile_progress_callback,
    void* tile_progress_user_data) {
    RuntimeNative3DTileSchedulerProgress progress = {0};

    if (!tile_progress_callback || !dirty_tiles || dirty_count == 0u) {
        return true;
    }
    progress.dirtyTiles = dirty_tiles;
    progress.dirtyTileCount = dirty_count;
    progress.startedSubpasses = subpass_index + 1;
    progress.completedSubpasses = subpass_index + 1;
    progress.totalSubpasses = temporal_frames;
    return tile_progress_callback(&progress, tile_progress_user_data);
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
        if (!core_queue_mutex_timed_pop(completion_queue, &completion, 100u)) {
            continue;
        }
        RuntimeNative3DTileSchedulerJob* job = (RuntimeNative3DTileSchedulerJob*)completion;
        if (!job || !job->ok) {
            return false;
        }
        RuntimeNative3DRenderStats_Accumulate(&scheduler->stats, &job->subpassStats);
        if (tile_progress_callback && pixel_buffer && pixel_width > 0) {
            if (!RuntimeNative3DRenderUnit_ResolveCurrentToPixels(&job->renderUnit,
                                                                  pixel_buffer,
                                                                  pixel_width)) {
                return false;
            }
            scheduler->progressTiles[dirty_count++] = job->tile;
            if (dirty_count >= kRuntimeNative3DTileSchedulerPreviewBatchDirtyTiles) {
                if (!runtime_native_3d_tile_scheduler_flush_progress_tiles(
                        scheduler->progressTiles,
                        dirty_count,
                        subpass_index,
                        temporal_frames,
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
        if (!runtime_native_3d_tile_scheduler_flush_progress_tiles(scheduler->progressTiles,
                                                                   dirty_count,
                                                                   subpass_index,
                                                                   temporal_frames,
                                                                   tile_progress_callback,
                                                                   tile_progress_user_data)) {
            return false;
        }
    }
    scheduler->completionCount += completions;
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

    if (dispatched == 0u) {
        return true;
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

static void runtime_native_3d_tile_scheduler_collect_activity(
    RuntimeNative3DTileScheduler* scheduler) {
    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        int active_pixels = 0;
        int active_tiles = 0;
        int inactive_tiles = 0;
        RuntimeNative3DRenderUnit_GetActivityCounts(&scheduler->jobs[i].renderUnit,
                                                    &active_pixels,
                                                    &active_tiles,
                                                    &inactive_tiles);
        scheduler->activePixelCount += active_pixels;
        scheduler->activeTileCount += active_tiles;
        scheduler->inactiveTileCount += inactive_tiles;
    }
}

static void runtime_native_3d_tile_scheduler_collect_metrics(
    RuntimeNative3DTileScheduler* scheduler) {
    const uint64_t frequency = (uint64_t)SDL_GetPerformanceFrequency();

    if (!scheduler || frequency == 0u) {
        return;
    }

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        const RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        const double total_tile_ms =
            ((double)job->totalRunTicks * 1000.0) / (double)frequency;
        const double max_subpass_ms =
            ((double)job->maxSubpassTicks * 1000.0) / (double)frequency;

        scheduler->stats.temporalMeasuredTileJobs += 1;
        scheduler->stats.temporalTotalTileMs += total_tile_ms;
        if (total_tile_ms > scheduler->stats.temporalMaxTileMs) {
            scheduler->stats.temporalMaxTileMs = total_tile_ms;
            scheduler->stats.temporalSlowTileOriginX = job->tile.originX;
            scheduler->stats.temporalSlowTileOriginY = job->tile.originY;
            scheduler->stats.temporalSlowTileWidth = job->tile.width;
            scheduler->stats.temporalSlowTileHeight = job->tile.height;
        }
        if (max_subpass_ms > scheduler->stats.temporalMaxTileSubpassMs) {
            scheduler->stats.temporalMaxTileSubpassMs = max_subpass_ms;
        }
    }

    if (scheduler->stats.temporalMeasuredTileJobs > 0) {
        scheduler->stats.temporalAverageTileMs =
            scheduler->stats.temporalTotalTileMs /
            (double)scheduler->stats.temporalMeasuredTileJobs;
    }
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
    RuntimeNative3DTileScheduler scheduler = {0};
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

    occupancy_ok = RuntimeNative3DPrepareFrameTileOccupancy(
        frame, effective_tile_size);
    (void)occupancy_ok;

    runtime_native_3d_tile_scheduler_reset(&scheduler);
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

    worker_count = RuntimeNative3DTileSchedulerResolveWorkerCount(
        scheduler.jobCount, tile_progress_callback != NULL);

    thread_slots = (pthread_t*)calloc(worker_count, sizeof(*thread_slots));
    task_slots = (CoreWorkerTask*)calloc(scheduler.jobCount, sizeof(*task_slots));
    completion_slots = (void**)calloc(scheduler.jobCount, sizeof(*completion_slots));
    if (!thread_slots || !task_slots || !completion_slots) {
        goto finalize;
    }

    if (!core_queue_mutex_init(&completion_queue, completion_slots, scheduler.jobCount)) {
        goto finalize;
    }
    if (!core_workers_init(&workers,
                           thread_slots,
                           worker_count,
                           task_slots,
                           scheduler.jobCount,
                           &completion_queue)) {
        core_queue_mutex_destroy(&completion_queue);
        goto finalize;
    }

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

    core_workers_shutdown(&workers);
    core_queue_mutex_destroy(&completion_queue);

    if (!ok) {
        goto finalize;
    }

    for (size_t i = 0; i < scheduler.jobCount; ++i) {
        if (!RuntimeNative3DRenderUnit_ResolveCurrentToPixels(&scheduler.jobs[i].renderUnit,
                                                              pixel_buffer,
                                                              frame->width)) {
            ok = false;
            break;
        }
    }
    if (!ok) {
        goto finalize;
    }

    runtime_native_3d_tile_scheduler_collect_activity(&scheduler);
    runtime_native_3d_tile_scheduler_collect_metrics(&scheduler);

finalize:
    if (ok && out_stats) {
        *out_stats = scheduler.stats;
        out_stats->temporalCommittedSubpasses = scheduler.committedSubpasses;
        out_stats->temporalActivePixelCount = scheduler.activePixelCount;
        out_stats->temporalActiveTileCount = scheduler.activeTileCount;
        out_stats->temporalInactiveTileCount = scheduler.inactiveTileCount;
    }
    runtime_native_3d_tile_scheduler_free(&scheduler);
    free(thread_slots);
    free(task_slots);
    free(completion_slots);
    return ok;
}
