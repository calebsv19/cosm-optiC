#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "render/pipeline/ray_tracing2_native3d_overlay.h"

int ray_tracing_headless_prepare_frame_output(
    char *frame_path,
    size_t frame_path_size,
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    if (!frame_path || frame_path_size == 0u || !request || !preflight) return 8;
    if (snprintf(frame_path,
                 frame_path_size,
                 "%s/frame_%04d.bmp",
                 preflight->frame_dir,
                 frame_index) >= (int)frame_path_size) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "frame path too long");
        ray_tracing_render_headless_write_progress_and_job_status(
            request->progress_path,
            request,
            "failed",
            frame_index,
            preflight->frames_rendered,
            0,
            0,
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
            8);
        return 8;
    }
    if (!request->overwrite && access(frame_path, F_OK) == 0) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "frame exists; set output.overwrite=true");
        ray_tracing_render_headless_write_progress_and_job_status(
            request->progress_path,
            request,
            "failed",
            frame_index,
            preflight->frames_rendered,
            0,
            0,
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
            8);
        return 8;
    }
    return 0;
}

void ray_tracing_headless_note_rendering_frame_started(
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    if (!request || !preflight) return;
    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "rendering_frame",
        frame_index,
        preflight->frames_rendered,
        0,
        0,
        request->temporal_frames,
        0u,
        0u,
        0.0,
        -1.0,
        "running",
        "rendering frame",
        job_status_path,
        job_id,
        request_path,
        -1);
}

int ray_tracing_headless_write_rendered_frame_output(
    const char *frame_path,
    const uint8_t *pixels,
    int local_frame,
    int frame_index,
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    const char *job_status_path,
    const char *job_id,
    const char *request_path) {
    struct timespec stage_started_at = {0};
    uint8_t frame_max_r = 0u;
    uint8_t frame_max_g = 0u;
    uint8_t frame_max_b = 0u;
    size_t frame_nonzero_pixels = 0u;
    if (!frame_path || !pixels || !request || !preflight || !temporal_progress) return 10;

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    frame_nonzero_pixels = ray_tracing_headless_count_nonzero_pixels(pixels,
                                                                     request->width,
                                                                     request->height,
                                                                     &frame_max_r,
                                                                     &frame_max_g,
                                                                     &frame_max_b);
    preflight->nonzero_pixels += frame_nonzero_pixels;
    if (frame_max_r > preflight->max_r) preflight->max_r = frame_max_r;
    if (frame_max_g > preflight->max_g) preflight->max_g = frame_max_g;
    if (frame_max_b > preflight->max_b) preflight->max_b = frame_max_b;
    preflight->frame_analysis_ms += ray_tracing_elapsed_ms_since(&stage_started_at);

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    if (!RayTracing2Native3DOverlay_ExportFrameBMP(frame_path,
                                                   request->width,
                                                   request->height,
                                                   pixels,
                                                   NULL)) {
        preflight->frame_write_ms += ray_tracing_elapsed_ms_since(&stage_started_at);
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "failed to write frame bmp");
        RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight->bvh_trace_stats);
        ray_tracing_render_headless_write_progress_and_job_status(
            request->progress_path,
            request,
            "failed",
            frame_index,
            preflight->frames_rendered,
            request->temporal_frames,
            request->temporal_frames,
            request->temporal_frames,
            temporal_progress->total_tiles_in_subpass,
            temporal_progress->total_tiles_in_subpass,
            ray_tracing_elapsed_seconds_since(&temporal_progress->frame_started_at),
            0.0,
            "failed",
            preflight->diagnostics,
            job_status_path,
            job_id,
            request_path,
            10);
        return 10;
    }
    preflight->frame_write_ms += ray_tracing_elapsed_ms_since(&stage_started_at);
    if (local_frame == 0) {
        snprintf(preflight->first_frame_path,
                 sizeof(preflight->first_frame_path),
                 "%s",
                 frame_path);
    }
    snprintf(preflight->last_frame_path,
             sizeof(preflight->last_frame_path),
             "%s",
             frame_path);
    preflight->frames_rendered += 1;
    ray_tracing_render_headless_write_progress_and_job_status(
        request->progress_path,
        request,
        "writing_frame",
        frame_index,
        preflight->frames_rendered,
        request->temporal_frames,
        request->temporal_frames,
        request->temporal_frames,
        temporal_progress->total_tiles_in_subpass,
        temporal_progress->total_tiles_in_subpass,
        ray_tracing_elapsed_seconds_since(&temporal_progress->frame_started_at),
        0.0,
        "running",
        frame_path,
        job_status_path,
        job_id,
        request_path,
        -1);
    return 0;
}
