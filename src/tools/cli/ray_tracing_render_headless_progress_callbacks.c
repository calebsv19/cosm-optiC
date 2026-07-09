#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <time.h>

double ray_tracing_elapsed_seconds_since(const struct timespec *start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec);
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

double ray_tracing_elapsed_ms_since(const struct timespec *start_time) {
    return ray_tracing_elapsed_seconds_since(start_time) * 1000.0;
}

static double ray_tracing_estimate_remaining_seconds(double elapsed_seconds,
                                                     int started_subpasses,
                                                     int completed_subpasses,
                                                     int total_subpasses,
                                                     size_t completed_tiles_in_subpass,
                                                     size_t total_tiles_in_subpass) {
    double progress = 0.0;
    if (elapsed_seconds <= 0.0 || total_subpasses <= 0) return -1.0;
    progress = (double)completed_subpasses / (double)total_subpasses;
    if (total_tiles_in_subpass > 0u && started_subpasses > completed_subpasses) {
        progress += ((double)completed_tiles_in_subpass / (double)total_tiles_in_subpass) /
                    (double)total_subpasses;
    }
    if (progress <= 0.001) return -1.0;
    if (progress >= 1.0) return 0.0;
    return (elapsed_seconds / progress) - elapsed_seconds;
}

void ray_tracing_temporal_progress_callback(int started_subpasses,
                                            int completed_subpasses,
                                            int total_subpasses,
                                            void *user_data) {
    RayTracingTemporalProgressContext *context =
        (RayTracingTemporalProgressContext *)user_data;
    char diagnostics[128];
    if (!context || !context->request) return;
    context->started_subpasses = started_subpasses;
    context->completed_subpasses = completed_subpasses;
    context->total_subpasses = total_subpasses;
    if (completed_subpasses < started_subpasses) {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d active)",
                 started_subpasses,
                 total_subpasses);
    } else {
        snprintf(diagnostics,
                 sizeof(diagnostics),
                 "rendering frame (subpass %d/%d committed)",
                 completed_subpasses,
                 total_subpasses);
    }
    (void)ray_tracing_render_headless_write_progress_and_job_status(context->request->progress_path,
                                        context->request,
                                        "rendering_frame",
                                        context->frame_index,
                                        context->frames_completed,
                                        started_subpasses,
                                        completed_subpasses,
                                        total_subpasses,
                                        context->completed_tiles_in_subpass,
                                        context->total_tiles_in_subpass,
                                        ray_tracing_elapsed_seconds_since(&context->frame_started_at),
                                        ray_tracing_estimate_remaining_seconds(
                                            ray_tracing_elapsed_seconds_since(&context->frame_started_at),
                                            started_subpasses,
                                            completed_subpasses,
                                            total_subpasses,
                                            context->completed_tiles_in_subpass,
                                            context->total_tiles_in_subpass),
                                        "running",
                                        diagnostics,
                                        context->job_status_path,
                                        context->job_id,
                                        context->request_path,
                                        -1);
}

void ray_tracing_tile_progress_callback(int started_subpasses,
                                        int completed_subpasses,
                                        int total_subpasses,
                                        size_t completed_tiles_in_subpass,
                                        size_t total_tiles_in_subpass,
                                        void *user_data) {
    RayTracingTemporalProgressContext *context =
        (RayTracingTemporalProgressContext *)user_data;
    char diagnostics[160];
    double elapsed_seconds = 0.0;
    double estimated_remaining_seconds = 0.0;
    if (!context || !context->request) return;
    context->started_subpasses = started_subpasses;
    context->completed_subpasses = completed_subpasses;
    context->total_subpasses = total_subpasses;
    context->completed_tiles_in_subpass = completed_tiles_in_subpass;
    context->total_tiles_in_subpass = total_tiles_in_subpass;
    elapsed_seconds = ray_tracing_elapsed_seconds_since(&context->frame_started_at);
    estimated_remaining_seconds = ray_tracing_estimate_remaining_seconds(
        elapsed_seconds,
        started_subpasses,
        completed_subpasses,
        total_subpasses,
        completed_tiles_in_subpass,
        total_tiles_in_subpass);
    snprintf(diagnostics,
             sizeof(diagnostics),
             "rendering frame (subpass %d/%d, tiles %zu/%zu)",
             started_subpasses > 0 ? started_subpasses : 1,
             total_subpasses > 0 ? total_subpasses : 1,
             completed_tiles_in_subpass,
             total_tiles_in_subpass);
    (void)ray_tracing_render_headless_write_progress_and_job_status(
        context->request->progress_path,
        context->request,
        "rendering_frame",
        context->frame_index,
        context->frames_completed,
        started_subpasses,
        completed_subpasses,
        total_subpasses,
        completed_tiles_in_subpass,
        total_tiles_in_subpass,
        elapsed_seconds,
        estimated_remaining_seconds,
        "running",
        diagnostics,
        context->job_status_path,
        context->job_id,
        context->request_path,
        -1);
}
