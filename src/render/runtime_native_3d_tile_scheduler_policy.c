#include "render/runtime_native_3d_tile_scheduler_internal.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <string.h>

#include "render/runtime_native_3d_resolution.h"

static RuntimeNative3DAdaptiveTilePlan s_runtime_native_3d_adaptive_tile_plan = {0};

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

int RuntimeNative3DTileSchedulerAdaptiveMinChildTileSize(void) {
    return kRuntimeNative3DTileSchedulerAdaptiveMinChildTileSize;
}

size_t RuntimeNative3DTileSchedulerAdaptiveMaxSplitParents(void) {
    return kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents;
}

bool RuntimeNative3DTileSchedulerTileEligibleForAdaptiveSplit(int tile_width,
                                                              int tile_height,
                                                              int min_child_tile_size) {
    return tile_width >= (min_child_tile_size * 2) && tile_height >= (min_child_tile_size * 2);
}

bool RuntimeNative3DTileSchedulerTileShouldAdaptiveSplit(int tile_width,
                                                         int tile_height,
                                                         double total_tile_ms,
                                                         double average_tile_ms) {
    if (!RuntimeNative3DTileSchedulerTileEligibleForAdaptiveSplit(
            tile_width, tile_height, RuntimeNative3DTileSchedulerAdaptiveMinChildTileSize())) {
        return false;
    }
    if (total_tile_ms < kRuntimeNative3DTileSchedulerAdaptiveAbsoluteSplitMs) {
        return false;
    }
    if (average_tile_ms <= 0.0) {
        return false;
    }
    return total_tile_ms >= (average_tile_ms * kRuntimeNative3DTileSchedulerAdaptiveRelativeSplitScale);
}

void RuntimeNative3DTileSchedulerResetAdaptivePlan(void) {
    memset(&s_runtime_native_3d_adaptive_tile_plan, 0, sizeof(s_runtime_native_3d_adaptive_tile_plan));
}

void RuntimeNative3DTileSchedulerSnapshotAdaptivePlan(
    RuntimeNative3DAdaptiveTilePlanSnapshot* out_snapshot) {
    if (!out_snapshot) {
        return;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->valid = s_runtime_native_3d_adaptive_tile_plan.valid;
    out_snapshot->frameWidth = s_runtime_native_3d_adaptive_tile_plan.frameWidth;
    out_snapshot->frameHeight = s_runtime_native_3d_adaptive_tile_plan.frameHeight;
    out_snapshot->tileSize = s_runtime_native_3d_adaptive_tile_plan.tileSize;
    out_snapshot->temporalFrames = s_runtime_native_3d_adaptive_tile_plan.temporalFrames;
    out_snapshot->integratorId = s_runtime_native_3d_adaptive_tile_plan.integratorId;
    out_snapshot->splitEntryCount = s_runtime_native_3d_adaptive_tile_plan.splitEntryCount;
    if (out_snapshot->splitEntryCount > kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents) {
        out_snapshot->splitEntryCount = kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents;
    }
    for (size_t i = 0; i < out_snapshot->splitEntryCount; ++i) {
        out_snapshot->splitEntries[i] =
            s_runtime_native_3d_adaptive_tile_plan.splitEntries[i].parentTile;
    }
}

static size_t runtime_native_3d_tile_scheduler_cpu_percent_cap(int cpu_count,
                                                               int cpu_percent) {
    double scaled = 0.0;
    if (cpu_count <= 0 || cpu_percent <= 0) {
        return 0u;
    }
    if (cpu_percent > 100) {
        cpu_percent = 100;
    }
    scaled = ((double)cpu_count * (double)cpu_percent) / 100.0;
    if (scaled < 1.0) {
        return 1u;
    }
    return (size_t)ceil(scaled);
}

size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpuBudgeted(
    size_t job_count,
    int cpu_count,
    bool interactive_preview,
    const RuntimeNative3DResourceBudget* resource_budget) {
    size_t worker_count = 0u;
    size_t cap = 0u;
    int effective_cpu_count = cpu_count;

    if (job_count == 0u) {
        return 0u;
    }

    if (resource_budget && resource_budget->reserveCpuCount > 0 && effective_cpu_count > 1) {
        effective_cpu_count -= resource_budget->reserveCpuCount;
        if (effective_cpu_count < 1) {
            effective_cpu_count = 1;
        }
    }

    worker_count = (effective_cpu_count > 0) ? (size_t)effective_cpu_count : 1u;
    if (interactive_preview && worker_count > 1u) {
        worker_count -= 1u;
    }
    if (worker_count == 0u) {
        worker_count = 1u;
    }

    if (resource_budget && resource_budget->cpuPercent > 0) {
        cap = runtime_native_3d_tile_scheduler_cpu_percent_cap(effective_cpu_count,
                                                               resource_budget->cpuPercent);
        if (cap > 0u && worker_count > cap) {
            worker_count = cap;
        }
    }
    if (resource_budget && resource_budget->maxWorkerThreads > 0) {
        cap = (size_t)resource_budget->maxWorkerThreads;
        if (worker_count > cap) {
            worker_count = cap;
        }
    }
    if (worker_count > job_count) {
        worker_count = job_count;
    }
    if (worker_count > kRuntimeNative3DTileSchedulerMaxWorkers) {
        worker_count = kRuntimeNative3DTileSchedulerMaxWorkers;
    }
    if (worker_count == 0u) {
        worker_count = 1u;
    }
    return worker_count;
}

size_t RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(size_t job_count,
                                                            int cpu_count,
                                                            bool interactive_preview) {
    return RuntimeNative3DTileSchedulerResolveWorkerCountForCpuBudgeted(job_count,
                                                                        cpu_count,
                                                                        interactive_preview,
                                                                        NULL);
}

size_t RuntimeNative3DTileSchedulerResolveWorkerCount(size_t job_count,
                                                      bool interactive_preview) {
    return RuntimeNative3DTileSchedulerResolveWorkerCountForCpu(job_count,
                                                               SDL_GetCPUCount(),
                                                               interactive_preview);
}

static bool runtime_native_3d_tile_scheduler_plan_matches(
    const RuntimeNative3DAdaptiveTilePlan* plan,
    const RuntimeNative3DPreparedFrame* frame,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    int tile_size) {
    return plan && plan->valid && plan->frameWidth == frame->width && plan->frameHeight == frame->height &&
           plan->integratorId == integrator_id && plan->temporalFrames == temporal_frames &&
           plan->tileSize == tile_size;
}

static bool runtime_native_3d_tile_scheduler_parent_tile_matches(const IntegratorTile* a,
                                                                 const IntegratorTile* b) {
    return a && b && a->originX == b->originX && a->originY == b->originY && a->width == b->width &&
           a->height == b->height;
}

int runtime_native_3d_tile_scheduler_compare_dispatch_priority(const void* lhs,
                                                               const void* rhs) {
    const RuntimeNative3DTileSchedulerJob* a = (const RuntimeNative3DTileSchedulerJob*)lhs;
    const RuntimeNative3DTileSchedulerJob* b = (const RuntimeNative3DTileSchedulerJob*)rhs;
    int diff = 0;

    if (!a || !b) {
        return 0;
    }
    diff = b->activePixelCount - a->activePixelCount;
    if (diff != 0) {
        return diff;
    }
    diff = b->activeTileCount - a->activeTileCount;
    if (diff != 0) {
        return diff;
    }
    diff = b->tile.height * b->tile.width - a->tile.height * a->tile.width;
    if (diff != 0) {
        return diff;
    }
    diff = a->tile.originY - b->tile.originY;
    if (diff != 0) {
        return diff;
    }
    return a->tile.originX - b->tile.originX;
}

int runtime_native_3d_tile_scheduler_compare_first_subpass_priority(const void* lhs,
                                                                    const void* rhs) {
    const RuntimeNative3DTileSchedulerJob* a = (const RuntimeNative3DTileSchedulerJob*)lhs;
    const RuntimeNative3DTileSchedulerJob* b = (const RuntimeNative3DTileSchedulerJob*)rhs;
    int diff = 0;

    if (!a || !b) {
        return 0;
    }
    diff = (int)b->occupancyLikely - (int)a->occupancyLikely;
    if (diff != 0) {
        return diff;
    }
    diff = b->tile.height * b->tile.width - a->tile.height * a->tile.width;
    if (diff != 0) {
        return diff;
    }
    diff = a->tile.originY - b->tile.originY;
    if (diff != 0) {
        return diff;
    }
    return a->tile.originX - b->tile.originX;
}

bool runtime_native_3d_tile_scheduler_parent_tile_should_split(
    const RuntimeNative3DAdaptiveTilePlan* plan,
    const IntegratorTile* parent_tile) {
    if (!plan || !plan->valid || !parent_tile) {
        return false;
    }
    for (size_t i = 0; i < plan->splitEntryCount; ++i) {
        if (runtime_native_3d_tile_scheduler_parent_tile_matches(&plan->splitEntries[i].parentTile,
                                                                 parent_tile)) {
            return true;
        }
    }
    return false;
}

static int runtime_native_3d_tile_scheduler_compare_slow_parent_metrics(const void* lhs,
                                                                        const void* rhs) {
    const RuntimeNative3DTileSchedulerParentMetric* const* a =
        (const RuntimeNative3DTileSchedulerParentMetric* const*)lhs;
    const RuntimeNative3DTileSchedulerParentMetric* const* b =
        (const RuntimeNative3DTileSchedulerParentMetric* const*)rhs;
    if ((*a)->totalRunTicks < (*b)->totalRunTicks) return 1;
    if ((*a)->totalRunTicks > (*b)->totalRunTicks) return -1;
    return 0;
}

void runtime_native_3d_tile_scheduler_update_adaptive_plan(
    const RuntimeNative3DTileScheduler* scheduler,
    const RuntimeNative3DPreparedFrame* frame,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    int tile_size) {
    RuntimeNative3DTileSchedulerParentMetric* eligible[
        kRuntimeNative3DTileSchedulerAdaptiveEligibleCapacity] = {0};
    size_t eligible_count = 0u;
    double average_tile_ms = 0.0;
    RuntimeNative3DAdaptiveTilePlan next_plan = {0};
    const uint64_t frequency = (uint64_t)SDL_GetPerformanceFrequency();

    if (!scheduler || !frame || frequency == 0u || scheduler->parentMetricCount == 0u) {
        RuntimeNative3DTileSchedulerResetAdaptivePlan();
        return;
    }

    if (scheduler->stats.temporalMeasuredTileJobs > 0) {
        average_tile_ms = scheduler->stats.temporalAverageTileMs;
    }
    if (average_tile_ms <= 0.0) {
        RuntimeNative3DTileSchedulerResetAdaptivePlan();
        return;
    }

    for (size_t i = 0; i < scheduler->parentMetricCount; ++i) {
        RuntimeNative3DTileSchedulerParentMetric* parent_metric =
            &scheduler->parentMetrics[i];
        const double total_tile_ms =
            ((double)parent_metric->totalRunTicks * 1000.0) / (double)frequency;

        if (!RuntimeNative3DTileSchedulerTileShouldAdaptiveSplit(parent_metric->tile.width,
                                                                 parent_metric->tile.height,
                                                                 total_tile_ms,
                                                                 average_tile_ms)) {
            continue;
        }
        if (eligible_count < SDL_arraysize(eligible)) {
            eligible[eligible_count++] = parent_metric;
        }
    }

    if (eligible_count == 0u) {
        RuntimeNative3DTileSchedulerResetAdaptivePlan();
        return;
    }

    qsort(eligible,
          eligible_count,
          sizeof(eligible[0]),
          runtime_native_3d_tile_scheduler_compare_slow_parent_metrics);

    next_plan.valid = true;
    next_plan.frameWidth = frame->width;
    next_plan.frameHeight = frame->height;
    next_plan.tileSize = tile_size;
    next_plan.temporalFrames = temporal_frames;
    next_plan.integratorId = integrator_id;
    next_plan.splitEntryCount = eligible_count;
    if (next_plan.splitEntryCount > kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents) {
        next_plan.splitEntryCount = kRuntimeNative3DTileSchedulerAdaptiveMaxSplitParents;
    }
    for (size_t i = 0; i < next_plan.splitEntryCount; ++i) {
        next_plan.splitEntries[i].parentTile = eligible[i]->tile;
    }

    s_runtime_native_3d_adaptive_tile_plan = next_plan;
}


const RuntimeNative3DAdaptiveTilePlan* runtime_native_3d_tile_scheduler_active_plan(
    const RuntimeNative3DPreparedFrame* frame,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    int tile_size) {
    if (runtime_native_3d_tile_scheduler_plan_matches(&s_runtime_native_3d_adaptive_tile_plan,
                                                      frame,
                                                      integrator_id,
                                                      temporal_frames,
                                                      tile_size)) {
        return &s_runtime_native_3d_adaptive_tile_plan;
    }
    return NULL;
}
