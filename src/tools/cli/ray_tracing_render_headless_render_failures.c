#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <time.h>

int ray_tracing_headless_note_render_frame_failed(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    const struct timespec *stage_started_at,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    if (!request || !preflight || !temporal_progress) return 9;
    preflight->render_trace_ms += ray_tracing_elapsed_ms_since(stage_started_at);
    preflight->render_frames_ms += ray_tracing_elapsed_ms_since(stage_started_at);
    snprintf(preflight->diagnostics,
             sizeof(preflight->diagnostics),
             "failed to render frame");
    RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight->bvh_trace_stats);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(
        &preflight->prepared_scene_cache_stats);
    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "failed",
        frame_index,
        preflight->frames_rendered,
        request->temporal_frames,
        0,
        request->temporal_frames,
        temporal_progress->completed_tiles_in_subpass,
        temporal_progress->total_tiles_in_subpass,
        ray_tracing_elapsed_seconds_since(&temporal_progress->frame_started_at),
        -1.0,
        "failed",
        preflight->diagnostics,
        job_status_path,
        job_id,
        request_path,
        9);
    return 9;
}

int ray_tracing_headless_note_bvh_flat_fallback_failed(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    if (!request || !preflight || !temporal_progress) return 9;
    snprintf(preflight->diagnostics,
             sizeof(preflight->diagnostics),
             "native 3D BVH flat fallback detected: calls=%llu overflow_calls=%llu",
             (unsigned long long)preflight->bvh_trace_stats.flatFallbackCalls,
             (unsigned long long)preflight->bvh_trace_stats.overflowFallbackCalls);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(
        &preflight->prepared_scene_cache_stats);
    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "bvh_flat_fallback_failed",
        frame_index,
        preflight->frames_rendered,
        request->temporal_frames,
        request->temporal_frames,
        request->temporal_frames,
        temporal_progress->completed_tiles_in_subpass,
        temporal_progress->total_tiles_in_subpass,
        ray_tracing_elapsed_seconds_since(&temporal_progress->frame_started_at),
        -1.0,
        "failed",
        preflight->diagnostics,
        job_status_path,
        job_id,
        request_path,
        9);
    return 9;
}
