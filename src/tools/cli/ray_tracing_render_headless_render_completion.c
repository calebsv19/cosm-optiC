#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <time.h>

#include "render/runtime_caustic_transport_debug_3d.h"
#include "tools/make_video.h"

void ray_tracing_headless_finalize_render_diagnostics(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request) {
    if (!preflight || !request) return;
    RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight->bvh_trace_stats);
    RuntimeRay3D_SnapshotRouteStats(&preflight->ray_trace_route_stats);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(
        &preflight->prepared_scene_cache_stats);
    RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
        &preflight->dynamic_water_cache_stats);
    RuntimeMeshBLASCache3D_SnapshotDiagnostics(&preflight->scene_acceleration_stats);
    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(
        &preflight->scene_acceleration_stats);
    preflight->rendered_frames = preflight->frames_rendered == request->frame_count;
    if (request->caustic_settings.debugExportEnabled) {
        (void)RuntimeCausticTransportDebug3D_WriteCameraStats(
            preflight->stats.causticVolumeScatterContributingSampleCount,
            preflight->stats.causticVolumeScatterContributingPixelCount,
            preflight->stats.totalCausticVolumeScatterPixelX,
            preflight->stats.totalCausticVolumeScatterPixelY,
            preflight->stats.causticVolumeScatterPixelMinX,
            preflight->stats.causticVolumeScatterPixelMinY,
            preflight->stats.causticVolumeScatterPixelMaxX,
            preflight->stats.causticVolumeScatterPixelMaxY,
            preflight->stats.totalCausticVolumeScatterSampledCacheRadiance,
            preflight->stats.maxCausticVolumeScatterSampledCacheRadiance);
    }
}

int ray_tracing_headless_encode_video_if_requested(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    struct timespec stage_started_at = {0};
    int video_code = 0;
    if (!request || !preflight) return 2;
    if (!preflight->rendered_frames || !request->video_enabled) return 0;

    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "encoding_video",
        request->start_frame + request->frame_count - 1,
        preflight->frames_rendered,
        request->temporal_frames,
        request->temporal_frames,
        request->temporal_frames,
        0u,
        0u,
        0.0,
        -1.0,
        "running",
        request->video_path,
        job_status_path,
        job_id,
        request_path,
        -1);
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    video_code = MakeVideoFromFrames(preflight->frame_dir,
                                     request->video_path,
                                     request->video_fps);
    preflight->video_encode_ms += ray_tracing_elapsed_ms_since(&stage_started_at);
    if (video_code != 0) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "failed to encode video: %s",
                 request->video_path);
        ray_tracing_render_headless_write_progress_and_job_status(
            request->progress_path,
            request,
            "failed",
            request->start_frame + request->frame_count - 1,
            preflight->frames_rendered,
            request->temporal_frames,
            request->temporal_frames,
            request->temporal_frames,
            0u,
            0u,
            0.0,
            -1.0,
            "failed",
            preflight->diagnostics,
            job_status_path,
            job_id,
            request_path,
            11);
        return 11;
    }
    return 0;
}

void ray_tracing_headless_write_completed_progress(
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    if (!request || !preflight) return;
    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "completed",
        request->start_frame + request->frame_count - 1,
        preflight->frames_rendered,
        request->temporal_frames,
        request->temporal_frames,
        request->temporal_frames,
        0u,
        0u,
        0.0,
        0.0,
        "completed",
        "render completed",
        job_status_path,
        job_id,
        request_path,
        0);
}
