#include "motion/runtime_motion_track_3d.h"

#include <stdio.h>
#include <string.h>

RuntimeMotionTrack3DMode RuntimeMotionTrack3DModeFromLabel(const char *label) {
    if (!label || !label[0]) {
        return RUNTIME_MOTION_TRACK_3D_MODE_UNKNOWN;
    }
    if (strcmp(label, "authored_path") == 0) {
        return RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH;
    }
    if (strcmp(label, "physics") == 0) {
        return RUNTIME_MOTION_TRACK_3D_MODE_PHYSICS;
    }
    return RUNTIME_MOTION_TRACK_3D_MODE_UNKNOWN;
}

const char *RuntimeMotionTrack3DModeLabel(RuntimeMotionTrack3DMode mode) {
    switch (mode) {
        case RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH:
            return "authored_path";
        case RUNTIME_MOTION_TRACK_3D_MODE_PHYSICS:
            return "physics";
        case RUNTIME_MOTION_TRACK_3D_MODE_UNKNOWN:
        default:
            return "unknown";
    }
}

bool RuntimeMotionTrack3DModeSupported(RuntimeMotionTrack3DMode mode) {
    return mode == RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH;
}

void RuntimeMotionTrack3DCopyString(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src ? src : "");
}

void RuntimeMotionTrack3DSummarySetDiagnostics(RuntimeMotionTrack3DSummary *summary,
                                               const char *diagnostics) {
    if (!summary) {
        return;
    }
    RuntimeMotionTrack3DCopyString(summary->diagnostics,
                                   sizeof(summary->diagnostics),
                                   diagnostics ? diagnostics : "ok");
}

void RuntimeMotionTrack3DSummaryInit(RuntimeMotionTrack3DSummary *summary,
                                     const char *diagnostics) {
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->valid = true;
    RuntimeMotionTrack3DSummarySetDiagnostics(summary, diagnostics);
}
