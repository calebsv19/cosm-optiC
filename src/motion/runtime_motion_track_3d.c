#include "motion/runtime_motion_track_3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static double runtime_motion_track_3d_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double runtime_motion_track_3d_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

static double runtime_motion_track_3d_lerp_angle(double a, double b, double t) {
    double delta = atan2(sin(b - a), cos(b - a));
    return a + delta * t;
}

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

bool RuntimeMotionTrack3DHasExecutableMotion(const RuntimeMotionTrack3D *track) {
    if (!track || !track->used || !track->enabled || !track->matched_object ||
        track->duplicate_object || !track->supported_mode ||
        track->mode != RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH) {
        return false;
    }
    return track->has_position_path || track->has_rotation_keyframes;
}

bool RuntimeMotionTrack3DSampleTrack(const RuntimeMotionTrack3D *track,
                                     double normalized_t,
                                     RuntimeMotionTrack3DSample *out_sample) {
    RuntimeMotionTrack3DSample sample = {0};
    double t = runtime_motion_track_3d_clamp01(normalized_t);

    if (!out_sample) {
        return false;
    }
    *out_sample = sample;
    if (!RuntimeMotionTrack3DHasExecutableMotion(track)) {
        return false;
    }

    sample.valid = true;
    if (track->has_position_path && track->position_path.numPoints > 0) {
        Point position = track->position_path.points[0];
        if (track->position_path.numPoints >= 2) {
            position = GetPositionAlongPathNormalized((Path *)&track->position_path, t);
        }
        sample.has_position = true;
        sample.position_x = position.x;
        sample.position_y = position.y;
        sample.position_z = CameraPath3D_GetPositionZNormalized(&track->position_path,
                                                                &track->position_path_3d,
                                                                t);
    }

    if (track->has_rotation_keyframes && track->rotation_keyframe_count > 0) {
        const RuntimeMotionTrack3DRotationKeyframe *first =
            &track->rotation_keyframes[0];
        const RuntimeMotionTrack3DRotationKeyframe *last =
            &track->rotation_keyframes[track->rotation_keyframe_count - 1];
        sample.has_rotation = true;
        sample.yaw_radians = first->yaw_radians;
        sample.pitch_radians = first->pitch_radians;
        sample.roll_radians = first->roll_radians;
        if (t >= last->t) {
            sample.yaw_radians = last->yaw_radians;
            sample.pitch_radians = last->pitch_radians;
            sample.roll_radians = last->roll_radians;
        } else {
            for (int i = 0; i < track->rotation_keyframe_count - 1; ++i) {
                const RuntimeMotionTrack3DRotationKeyframe *a =
                    &track->rotation_keyframes[i];
                const RuntimeMotionTrack3DRotationKeyframe *b =
                    &track->rotation_keyframes[i + 1];
                if (t < a->t) {
                    break;
                }
                if (t <= b->t) {
                    double local_t = (b->t - a->t) > 1e-12
                                         ? (t - a->t) / (b->t - a->t)
                                         : 0.0;
                    sample.yaw_radians =
                        runtime_motion_track_3d_lerp_angle(a->yaw_radians,
                                                            b->yaw_radians,
                                                            local_t);
                    sample.pitch_radians =
                        runtime_motion_track_3d_lerp(a->pitch_radians,
                                                     b->pitch_radians,
                                                     local_t);
                    sample.roll_radians =
                        runtime_motion_track_3d_lerp_angle(a->roll_radians,
                                                            b->roll_radians,
                                                            local_t);
                    break;
                }
            }
        }
    }

    *out_sample = sample;
    return sample.has_position || sample.has_rotation;
}
