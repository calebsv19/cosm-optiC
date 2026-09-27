#include "animation/timeline_camera_channels.h"

#include <string.h>

TimelineStatus TimelineCameraChannelsResolve(
    const TimelineFrameSnapshot* snapshot, const char* target_id,
    TimelineCameraChannels* out_channels) {
    TimelinePropertyRegistry registry;
    TimelineCameraChannels channels = {0};
    if (!snapshot || !target_id || !out_channels ||
        strnlen(target_id, TIMELINE_ID_CAPACITY) >= TIMELINE_ID_CAPACITY ||
        strncmp(target_id, "camera/", 7u) != 0 || !target_id[7])
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    TimelineStatus status = TimelinePropertyRegistryInitFoundationDefaults(&registry);
    if (status == TIMELINE_STATUS_OK)
        status = TimelineFrameSnapshotValidate(&registry, snapshot);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0; i < snapshot->property_count; ++i) {
        const TimelinePropertyEvaluationResult* property = &snapshot->properties[i];
        if (strcmp(property->track.target_id, target_id) != 0) continue;
        const char* id = property->track.property_id;
        if (strcmp(id, "camera/path_progress") == 0) {
            channels.has_progress = true;
            channels.progress = property->track.value.as.scalar;
        } else if (strcmp(id, "camera/position") == 0) {
            channels.has_position = true;
            channels.position = property->track.value.as.vec3;
        } else if (strcmp(id, "camera/yaw") == 0) {
            channels.has_yaw = true;
            channels.yaw = property->track.value.as.scalar;
        } else if (strcmp(id, "camera/pitch") == 0) {
            channels.has_pitch = true;
            channels.pitch = property->track.value.as.scalar;
        } else if (strcmp(id, "camera/fov_y") == 0) {
            channels.has_fov = true;
            channels.fov = property->track.value.as.scalar;
        } else {
            return TIMELINE_STATUS_INVALID_SNAPSHOT;
        }
    }
    if (channels.has_position && channels.has_progress)
        return TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
    *out_channels = channels;
    return TIMELINE_STATUS_OK;
}
