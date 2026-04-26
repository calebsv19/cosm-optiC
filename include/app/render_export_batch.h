#ifndef RAY_TRACING_RENDER_EXPORT_BATCH_H
#define RAY_TRACING_RENDER_EXPORT_BATCH_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    RAY_TRACING_RENDER_EXPORT_OK = 0,
    RAY_TRACING_RENDER_EXPORT_NO_FRAMES = 1,
    RAY_TRACING_RENDER_EXPORT_FRAME_DIR_INVALID = 2,
    RAY_TRACING_RENDER_EXPORT_VIDEO_OUTPUT_INVALID = 3,
    RAY_TRACING_RENDER_EXPORT_VIDEO_TOOL_MISSING = 4,
    RAY_TRACING_RENDER_EXPORT_IO_ERROR = 5,
    RAY_TRACING_RENDER_EXPORT_VIDEO_BUILD_FAILED = 6
} RayTracingRenderExportCode;

typedef struct {
    RayTracingRenderExportCode code;
    size_t frame_count;
    size_t files_cleared;
    size_t progress_current_frame;
    size_t progress_total_frames;
    int progress_percent;
    int system_status;
    char frame_dir[PATH_MAX];
    char video_output_path[PATH_MAX];
    char message[128];
} RayTracingRenderExportStatus;

typedef void (*RayTracingRenderExportProgressCallback)(const RayTracingRenderExportStatus *status,
                                                       void *user_data);

void ray_tracing_render_export_status_reset(RayTracingRenderExportStatus *status);
bool ray_tracing_render_export_describe_active(RayTracingRenderExportStatus *status);
bool ray_tracing_render_export_count_active_frames(RayTracingRenderExportStatus *status);
bool ray_tracing_render_export_clear_active_frames(RayTracingRenderExportStatus *status);
bool ray_tracing_render_export_make_video(RayTracingRenderExportStatus *status);
bool ray_tracing_render_export_make_video_with_progress(
    RayTracingRenderExportStatus *status,
    RayTracingRenderExportProgressCallback progress_cb,
    void *user_data);

#endif
