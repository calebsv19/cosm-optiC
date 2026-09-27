#ifndef TIMELINE_CAMERA_CHANNELS_H
#define TIMELINE_CAMERA_CHANNELS_H

#include "animation/timeline_frame_snapshot.h"

/* Resolved channels for one stable camera target. Geometry/path sampling stays
 * in the camera adapter; this module has no dependency on UI or global state. */
typedef struct TimelineCameraChannels {
    bool has_progress, has_position, has_yaw, has_pitch, has_fov;
    double progress, yaw, pitch, fov;
    TimelineVec3 position;
} TimelineCameraChannels;

/* Validates the whole snapshot before resolving the selected camera. Refusal
 * leaves output unchanged. No matching channels is a valid legacy fallback. */
TimelineStatus TimelineCameraChannelsResolve(
    const TimelineFrameSnapshot* snapshot, const char* target_id,
    TimelineCameraChannels* out_channels);

#endif
