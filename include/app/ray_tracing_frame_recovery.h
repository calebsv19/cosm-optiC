#ifndef RAY_TRACING_FRAME_RECOVERY_H
#define RAY_TRACING_FRAME_RECOVERY_H

#include <stdbool.h>

typedef struct RayTracingFrameRecoveryScan {
    int requested_start_frame;
    int requested_frame_count;
    int durable_prefix_count;
    int resume_from_frame;
    int first_invalid_frame;
    int first_noncontiguous_frame;
    bool any_existing_frame;
} RayTracingFrameRecoveryScan;

bool ray_tracing_frame_recovery_scan(const char *output_root,
                                     int requested_start_frame,
                                     int requested_frame_count,
                                     int expected_width,
                                     int expected_height,
                                     RayTracingFrameRecoveryScan *out_scan);

#endif
