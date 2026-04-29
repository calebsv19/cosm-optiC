#include "app/render_export_batch.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/data_paths.h"
#include "config/config_manager.h"
#include "tools/make_video.h"

static bool is_frame_dump_name(const char *name) {
    size_t len = 0;
    if (!name) return false;
    if (strncmp(name, "frame_", 6) != 0) return false;
    len = strlen(name);
    if (len < 11) return false;
    return strcmp(name + len - 4, ".bmp") == 0;
}

static bool parse_frame_dump_index(const char *name, int *out_index) {
    const char *digits = NULL;
    const char *ext = NULL;
    char *end = NULL;
    long parsed = 0;
    if (out_index) *out_index = -1;
    if (!is_frame_dump_name(name)) return false;
    digits = name + 6;
    ext = strrchr(name, '.');
    if (!digits || !ext || ext <= digits) return false;
    parsed = strtol(digits, &end, 10);
    if (end != ext || parsed < 0 || parsed > INT_MAX) return false;
    if (out_index) *out_index = (int)parsed;
    return true;
}

static bool ffmpeg_available(void) {
    char ffmpeg_bin[PATH_MAX];
    return ResolveFFmpegBinary(ffmpeg_bin, sizeof(ffmpeg_bin));
}

typedef struct {
    RayTracingRenderExportStatus *status;
    RayTracingRenderExportProgressCallback callback;
    void *user_data;
} RenderExportProgressBridge;

static bool resolve_active_paths(RayTracingRenderExportStatus *status) {
    if (!status) return false;
    if (!ray_tracing_resolve_frame_output_dir(animSettings.frameDir,
                                              status->frame_dir,
                                              sizeof(status->frame_dir))) {
        status->code = RAY_TRACING_RENDER_EXPORT_FRAME_DIR_INVALID;
        snprintf(status->message, sizeof(status->message), "Frame directory unavailable");
        return false;
    }
    if (!ray_tracing_resolve_video_output_path(animSettings.videoOutputRoot,
                                               status->video_output_path,
                                               sizeof(status->video_output_path))) {
        status->code = RAY_TRACING_RENDER_EXPORT_VIDEO_OUTPUT_INVALID;
        snprintf(status->message, sizeof(status->message), "Video output path unavailable");
        return false;
    }
    return true;
}

static size_t summarize_frames_in_dir(const char *dir_path,
                                      bool *had_io_error,
                                      int *out_highest_frame_index) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    size_t count = 0;
    int highest = -1;
    if (had_io_error) *had_io_error = false;
    if (out_highest_frame_index) *out_highest_frame_index = -1;
    if (!dir_path || !dir_path[0]) {
        if (had_io_error) *had_io_error = true;
        return 0;
    }
    dir = opendir(dir_path);
    if (!dir) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        int frame_index = -1;
        if (!parse_frame_dump_index(entry->d_name, &frame_index)) continue;
        count += 1u;
        if (frame_index > highest) {
            highest = frame_index;
        }
    }
    closedir(dir);
    if (out_highest_frame_index) {
        *out_highest_frame_index = highest;
    }
    return count;
}

static bool clear_frames_in_dir(const char *dir_path,
                                size_t *removed_count,
                                bool *had_io_error) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    size_t removed = 0;
    bool ok = true;
    if (removed_count) *removed_count = 0;
    if (had_io_error) *had_io_error = false;
    if (!dir_path || !dir_path[0]) {
        if (had_io_error) *had_io_error = true;
        return false;
    }
    dir = opendir(dir_path);
    if (!dir) {
        return true;
    }
    while ((entry = readdir(dir)) != NULL) {
        char file_path[PATH_MAX];
        if (!is_frame_dump_name(entry->d_name)) continue;
        if (snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(file_path)) {
            ok = false;
            continue;
        }
        if (unlink(file_path) != 0) {
            ok = false;
            if (had_io_error) *had_io_error = true;
            continue;
        }
        removed += 1u;
    }
    closedir(dir);
    if (removed_count) *removed_count = removed;
    return ok;
}

void ray_tracing_render_export_status_reset(RayTracingRenderExportStatus *status) {
    if (!status) return;
    memset(status, 0, sizeof(*status));
    status->code = RAY_TRACING_RENDER_EXPORT_OK;
    status->highest_frame_index = -1;
    status->next_frame_index = 0;
}

static void render_export_progress_bridge_emit(size_t current_frame,
                                               size_t total_frames,
                                               int percent_complete,
                                               void *user_data) {
    RenderExportProgressBridge *bridge = (RenderExportProgressBridge *)user_data;
    if (!bridge || !bridge->status) return;
    bridge->status->progress_current_frame = current_frame;
    bridge->status->progress_total_frames = total_frames;
    bridge->status->progress_percent = percent_complete;
    snprintf(bridge->status->message,
             sizeof(bridge->status->message),
             "Making video: %d%% (%zu/%zu)",
             percent_complete,
             current_frame,
             total_frames);
    if (bridge->callback) {
        bridge->callback(bridge->status, bridge->user_data);
    }
}

bool ray_tracing_render_export_describe_active(RayTracingRenderExportStatus *status) {
    bool had_io_error = false;
    if (!status) return false;
    ray_tracing_render_export_status_reset(status);
    if (!resolve_active_paths(status)) return false;
    status->frame_count = summarize_frames_in_dir(status->frame_dir,
                                                  &had_io_error,
                                                  &status->highest_frame_index);
    status->next_frame_index = (status->highest_frame_index >= 0)
                                   ? (status->highest_frame_index + 1)
                                   : 0;
    snprintf(status->message,
             sizeof(status->message),
             "Frames ready: %zu",
             status->frame_count);
    return !had_io_error;
}

bool ray_tracing_render_export_count_active_frames(RayTracingRenderExportStatus *status) {
    return ray_tracing_render_export_describe_active(status);
}

bool ray_tracing_render_export_clear_active_frames(RayTracingRenderExportStatus *status) {
    bool had_io_error = false;
    if (!status) return false;
    ray_tracing_render_export_status_reset(status);
    if (!resolve_active_paths(status)) return false;
    if (!clear_frames_in_dir(status->frame_dir, &status->files_cleared, &had_io_error)) {
        status->code = RAY_TRACING_RENDER_EXPORT_IO_ERROR;
        snprintf(status->message, sizeof(status->message), "Failed clearing frames");
        return false;
    }
    status->frame_count = 0;
    status->highest_frame_index = -1;
    status->next_frame_index = 0;
    if (had_io_error) {
        status->code = RAY_TRACING_RENDER_EXPORT_IO_ERROR;
        snprintf(status->message, sizeof(status->message), "Failed clearing frames");
        return false;
    }
    snprintf(status->message,
             sizeof(status->message),
             "Cleared %zu frame%s",
             status->files_cleared,
             (status->files_cleared == 1u) ? "" : "s");
    return true;
}

bool ray_tracing_render_export_make_video(RayTracingRenderExportStatus *status) {
    return ray_tracing_render_export_make_video_with_progress(status, NULL, NULL);
}

bool ray_tracing_render_export_make_video_with_progress(
    RayTracingRenderExportStatus *status,
    RayTracingRenderExportProgressCallback progress_cb,
    void *user_data) {
    bool had_io_error = false;
    RenderExportProgressBridge progress_bridge = {0};
    if (!status) return false;
    ray_tracing_render_export_status_reset(status);
    if (!resolve_active_paths(status)) return false;
    status->frame_count = summarize_frames_in_dir(status->frame_dir,
                                                  &had_io_error,
                                                  &status->highest_frame_index);
    status->next_frame_index = (status->highest_frame_index >= 0)
                                   ? (status->highest_frame_index + 1)
                                   : 0;
    if (had_io_error || status->frame_count == 0u) {
        status->code = RAY_TRACING_RENDER_EXPORT_NO_FRAMES;
        snprintf(status->message, sizeof(status->message), "No frames found");
        return false;
    }
    if (!ffmpeg_available()) {
        status->code = RAY_TRACING_RENDER_EXPORT_VIDEO_TOOL_MISSING;
        snprintf(status->message, sizeof(status->message), "ffmpeg missing");
        return false;
    }
    progress_bridge.status = status;
    progress_bridge.callback = progress_cb;
    progress_bridge.user_data = user_data;
    status->progress_current_frame = 0u;
    status->progress_total_frames = status->frame_count;
    status->progress_percent = 0;
    if (progress_cb) {
        render_export_progress_bridge_emit(0u, status->frame_count, 0, &progress_bridge);
    }
    status->system_status = MakeVideoFromFramesWithProgress(status->frame_dir,
                                                            status->video_output_path,
                                                            animSettings.fps,
                                                            progress_cb ? render_export_progress_bridge_emit : NULL,
                                                            progress_cb ? &progress_bridge : NULL);
    if (status->system_status != 0) {
        status->code = RAY_TRACING_RENDER_EXPORT_VIDEO_BUILD_FAILED;
        snprintf(status->message, sizeof(status->message), "Video build failed");
        return false;
    }
    status->progress_current_frame = status->frame_count;
    status->progress_total_frames = status->frame_count;
    status->progress_percent = 100;
    snprintf(status->message, sizeof(status->message), "Video ready");
    return true;
}
