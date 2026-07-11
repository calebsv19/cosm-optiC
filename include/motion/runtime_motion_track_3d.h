#ifndef RUNTIME_MOTION_TRACK_3D_H
#define RUNTIME_MOTION_TRACK_3D_H

#include <stdbool.h>
#include <stddef.h>

#include "camera/camera_path_3d.h"
#include "path/path_system.h"

#define RUNTIME_MOTION_TRACK_3D_MAX_TRACKS 64
#define RUNTIME_MOTION_TRACK_3D_MAX_ROTATION_KEYFRAMES 32
#define RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE 64
#define RUNTIME_MOTION_TRACK_3D_LABEL_SIZE 32
#define RUNTIME_MOTION_TRACK_3D_WRAP_SIZE 24
#define RUNTIME_MOTION_TRACK_3D_DIAGNOSTICS_SIZE 128

typedef enum RuntimeMotionTrack3DMode {
    RUNTIME_MOTION_TRACK_3D_MODE_UNKNOWN = 0,
    RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH = 1,
    RUNTIME_MOTION_TRACK_3D_MODE_PHYSICS = 2
} RuntimeMotionTrack3DMode;

typedef struct RuntimeMotionTrack3DRotationKeyframe {
    double t;
    double yaw_radians;
    double pitch_radians;
    double roll_radians;
} RuntimeMotionTrack3DRotationKeyframe;

typedef struct RuntimeMotionTrack3D {
    bool used;
    bool enabled;
    bool matched_object;
    bool duplicate_object;
    bool supported_mode;
    bool has_position_path;
    bool has_rotation_keyframes;
    RuntimeMotionTrack3DMode mode;
    char object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    char mode_label[RUNTIME_MOTION_TRACK_3D_LABEL_SIZE];
    char timing_domain[RUNTIME_MOTION_TRACK_3D_LABEL_SIZE];
    char wrap_label[RUNTIME_MOTION_TRACK_3D_WRAP_SIZE];
    Path position_path;
    CameraPath3D position_path_3d;
    int rotation_keyframe_count;
    RuntimeMotionTrack3DRotationKeyframe
        rotation_keyframes[RUNTIME_MOTION_TRACK_3D_MAX_ROTATION_KEYFRAMES];
} RuntimeMotionTrack3D;

typedef struct RuntimeMotionTrack3DSample {
    bool valid;
    bool has_position;
    bool has_rotation;
    double position_x;
    double position_y;
    double position_z;
    double yaw_radians;
    double pitch_radians;
    double roll_radians;
} RuntimeMotionTrack3DSample;

typedef struct RuntimeMotionTrack3DSummary {
    bool valid;
    bool has_object_motion_tracks;
    int total_tracks;
    int stored_tracks;
    int enabled_tracks;
    int disabled_tracks;
    int matched_tracks;
    int unmatched_tracks;
    int unsupported_tracks;
    int duplicate_tracks;
    int authored_path_tracks;
    int physics_tracks;
    int position_path_tracks;
    int rotation_keyframe_tracks;
    int sampled_tracks;
    bool has_executable_motion;
    char first_object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    char first_unmatched_object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    char first_unsupported_object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    char diagnostics[RUNTIME_MOTION_TRACK_3D_DIAGNOSTICS_SIZE];
    RuntimeMotionTrack3D tracks[RUNTIME_MOTION_TRACK_3D_MAX_TRACKS];
} RuntimeMotionTrack3DSummary;

RuntimeMotionTrack3DMode RuntimeMotionTrack3DModeFromLabel(const char *label);
const char *RuntimeMotionTrack3DModeLabel(RuntimeMotionTrack3DMode mode);
bool RuntimeMotionTrack3DModeSupported(RuntimeMotionTrack3DMode mode);
void RuntimeMotionTrack3DSummaryInit(RuntimeMotionTrack3DSummary *summary,
                                     const char *diagnostics);
void RuntimeMotionTrack3DSummarySetDiagnostics(RuntimeMotionTrack3DSummary *summary,
                                               const char *diagnostics);
void RuntimeMotionTrack3DCopyString(char *dst, size_t dst_size, const char *src);
bool RuntimeMotionTrack3DHasExecutableMotion(const RuntimeMotionTrack3D *track);
bool RuntimeMotionTrack3DSampleTrack(const RuntimeMotionTrack3D *track,
                                     double normalized_t,
                                     RuntimeMotionTrack3DSample *out_sample);

#endif
