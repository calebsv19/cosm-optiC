#include "app/ray_tracing_frame_recovery.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app/ray_tracing_durable_io.h"

bool ray_tracing_frame_recovery_scan(const char *output_root,
                                     int requested_start_frame,
                                     int requested_frame_count,
                                     int expected_width,
                                     int expected_height,
                                     RayTracingFrameRecoveryScan *out_scan) {
    char frame_path[PATH_MAX];
    bool gap_seen = false;
    if (!output_root || !output_root[0] || requested_start_frame < 0 ||
        requested_frame_count <= 0 || !out_scan) {
        return false;
    }
    memset(out_scan, 0, sizeof(*out_scan));
    out_scan->requested_start_frame = requested_start_frame;
    out_scan->requested_frame_count = requested_frame_count;
    out_scan->resume_from_frame = requested_start_frame;
    out_scan->first_invalid_frame = -1;
    out_scan->first_noncontiguous_frame = -1;

    for (int offset = 0; offset < requested_frame_count; ++offset) {
        struct stat status;
        const int frame_index = requested_start_frame + offset;
        if (snprintf(frame_path,
                     sizeof(frame_path),
                     "%s/frames/frame_%04d.bmp",
                     output_root,
                     frame_index) >= (int)sizeof(frame_path)) {
            return false;
        }
        if (lstat(frame_path, &status) != 0) {
            gap_seen = true;
            continue;
        }
        out_scan->any_existing_frame = true;
        if (!S_ISREG(status.st_mode) ||
            !ray_tracing_durable_validate_bmp(frame_path,
                                              expected_width,
                                              expected_height)) {
            if (out_scan->first_invalid_frame < 0) {
                out_scan->first_invalid_frame = frame_index;
            }
            gap_seen = true;
            continue;
        }
        if (gap_seen) {
            if (out_scan->first_noncontiguous_frame < 0) {
                out_scan->first_noncontiguous_frame = frame_index;
            }
            continue;
        }
        out_scan->durable_prefix_count += 1;
        out_scan->resume_from_frame = frame_index + 1;
    }
    return true;
}
