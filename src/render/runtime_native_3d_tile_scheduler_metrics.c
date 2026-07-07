#include "render/runtime_native_3d_tile_scheduler_internal.h"

#include "render/timer_hud_adapter.h"

void runtime_native_3d_tile_scheduler_collect_activity(
    RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) {
        return;
    }
    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        RuntimeNative3DTileSchedulerParentMetric* parent_metric = NULL;

        scheduler->activePixelCount += job->activePixelCount;
        scheduler->activeTileCount += job->activeTileCount;
        scheduler->inactiveTileCount += job->inactiveTileCount;
        if (job->parentMetricIndex >= scheduler->parentMetricCount) {
            continue;
        }
        parent_metric = &scheduler->parentMetrics[job->parentMetricIndex];
        parent_metric->activePixelCount += job->activePixelCount;
        parent_metric->activeTileCount += job->activeTileCount;
        parent_metric->inactiveTileCount += job->inactiveTileCount;
    }
}

static void runtime_native_3d_tile_scheduler_record_adaptive_state_summary(
    RuntimeNative3DRenderStats* stats,
    const RuntimeNative3DAdaptivePixelStateSummary* summary) {
    if (!stats || !summary) return;
    stats->temporalAdaptiveStateMeasuredPixels += summary->measuredPixelCount;
    stats->temporalAdaptiveStateStablePixels += summary->stablePixelCount;
    stats->temporalAdaptiveStateActivePixels += summary->activePixelCount;
    stats->temporalAdaptiveStateProbePixels += summary->probePixelCount;
    stats->temporalAdaptiveStateHighRiskPixels += summary->highRiskPixelCount;
    stats->temporalAdaptiveStateStableTiles += summary->stableTileCount;
    stats->temporalAdaptiveStateActiveTiles += summary->activeTileCount;
    stats->temporalAdaptiveStateProbeTiles += summary->probeTileCount;
    stats->temporalAdaptiveStateHighRiskTiles += summary->highRiskTileCount;
    stats->temporalAdaptiveStateActivityRiskPixels += summary->activityRiskPixelCount;
    stats->temporalAdaptiveStateMaterialRiskPixels += summary->materialRiskPixelCount;
    stats->temporalAdaptiveStateTransparentRiskPixels += summary->transparentRiskPixelCount;
    stats->temporalAdaptiveStateGlossyRiskPixels += summary->glossyRiskPixelCount;
    stats->temporalAdaptiveStateGeometryEdgeRiskPixels += summary->geometryEdgeRiskPixelCount;
    stats->temporalAdaptiveStateDirectLightNoTracePixels +=
        summary->directLightNoTracePixelCount;
    stats->temporalAdaptiveStateDirectLightClearVisiblePixels +=
        summary->directLightClearVisiblePixelCount;
    stats->temporalAdaptiveStateDirectLightClearBlockedPixels +=
        summary->directLightClearBlockedPixelCount;
    stats->temporalAdaptiveStateDirectLightStablePartialPixels +=
        summary->directLightStablePartialPixelCount;
    stats->temporalAdaptiveStateDirectLightMixedPartialPixels +=
        summary->directLightMixedPartialPixelCount;
    stats->temporalAdaptiveStateDirectLightBoundaryRiskPixels +=
        summary->directLightBoundaryRiskPixelCount;
    stats->temporalAdaptiveStateMixedRiskTiles += summary->mixedRiskTileCount;
    stats->temporalAdaptiveStateRiskSum += summary->riskSum;
    if (summary->riskMax > stats->temporalAdaptiveStateRiskMax) {
        stats->temporalAdaptiveStateRiskMax = summary->riskMax;
    }
    if (summary->minSampleFloor > stats->temporalAdaptiveStateMinSampleFloor) {
        stats->temporalAdaptiveStateMinSampleFloor = summary->minSampleFloor;
    }
}

void runtime_native_3d_tile_scheduler_collect_adaptive_state(
    RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        RuntimeNative3DAdaptivePixelStateSummary summary = {0};
        RuntimeNative3DRenderUnit_GetAdaptiveStateSummary(&scheduler->jobs[i].renderUnit,
                                                          &summary);
        runtime_native_3d_tile_scheduler_record_adaptive_state_summary(&scheduler->stats,
                                                                       &summary);
    }
}

void runtime_native_3d_tile_scheduler_collect_metrics(
    RuntimeNative3DTileScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    for (size_t i = 0; i < scheduler->jobCount; ++i) {
        const RuntimeNative3DTileSchedulerJob* job = &scheduler->jobs[i];
        RuntimeNative3DTileSchedulerParentMetric* parent_metric = NULL;
        if (job->parentMetricIndex >= scheduler->parentMetricCount) {
            continue;
        }
        parent_metric = &scheduler->parentMetrics[job->parentMetricIndex];
        parent_metric->totalRunTicks += job->totalRunTicks;
        if (job->maxSubpassTicks > parent_metric->maxSubpassTicks) {
            parent_metric->maxSubpassTicks = job->maxSubpassTicks;
        }
    }

    for (size_t i = 0; i < scheduler->parentMetricCount; ++i) {
        const RuntimeNative3DTileSchedulerParentMetric* parent_metric =
            &scheduler->parentMetrics[i];
        const double total_tile_ms =
            runtime_native_3d_tile_scheduler_ticks_to_ms(parent_metric->totalRunTicks);
        const double max_subpass_ms =
            runtime_native_3d_tile_scheduler_ticks_to_ms(parent_metric->maxSubpassTicks);

        scheduler->stats.temporalMeasuredTileJobs += 1;
        scheduler->stats.temporalTotalTileMs += total_tile_ms;
        timer_hud_record_duration_ms("Tile Single", total_tile_ms);
        if (parent_metric->splitApplied) {
            scheduler->stats.temporalAdaptiveSplitParentCount += 1;
            scheduler->stats.temporalAdaptiveChildTileCount += parent_metric->childTileCount;
        }
        if (total_tile_ms > scheduler->stats.temporalMaxTileMs) {
            scheduler->stats.temporalMaxTileMs = total_tile_ms;
            scheduler->stats.temporalSlowTileOriginX = parent_metric->tile.originX;
            scheduler->stats.temporalSlowTileOriginY = parent_metric->tile.originY;
            scheduler->stats.temporalSlowTileWidth = parent_metric->tile.width;
            scheduler->stats.temporalSlowTileHeight = parent_metric->tile.height;
        }
        if (max_subpass_ms > scheduler->stats.temporalMaxTileSubpassMs) {
            scheduler->stats.temporalMaxTileSubpassMs = max_subpass_ms;
        }
    }

    if (scheduler->stats.temporalMeasuredTileJobs > 0) {
        scheduler->stats.temporalAverageTileMs =
            scheduler->stats.temporalTotalTileMs /
            (double)scheduler->stats.temporalMeasuredTileJobs;
        timer_hud_record_duration_ms("Tile Average", scheduler->stats.temporalAverageTileMs);
        timer_hud_record_duration_ms("Tile Slowest", scheduler->stats.temporalMaxTileMs);
    }

}
