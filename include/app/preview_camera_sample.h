#ifndef PREVIEW_CAMERA_SAMPLE_H
#define PREVIEW_CAMERA_SAMPLE_H

#include <stdbool.h>

#include "camera/camera.h"
#include "camera/camera_path_3d.h"
#include "path/path_system.h"
#include "animation/timeline_camera_channels.h"

#define PREVIEW_CAMERA_SAMPLE_DEFAULT_FOV_Y_DEGREES 55.0

typedef struct PreviewCameraSample {
    bool valid;
    bool uses_authored_path;
    double position_x;
    double position_y;
    double position_z;
    double yaw_radians;
    double pitch_radians;
    double fov_y_degrees;
    double aspect_ratio;
} PreviewCameraSample;

bool PreviewCameraSampleEvaluate(const Camera* base_camera,
                                 double base_camera_z,
                                 const Path* camera_path,
                                 const CameraPath3D* camera_path3d,
                                 double normalized_t,
                                 int viewport_width,
                                 int viewport_height,
                                 PreviewCameraSample* out_sample);

/* Exact-frame adapter. Explicit position replaces path-driven pose; explicit
 * orientation/lens channels override the sampled base. Output is transactional. */
bool PreviewCameraSampleEvaluateTimeline(const Camera* base_camera,
    double base_camera_z, const Path* camera_path,
    const CameraPath3D* camera_path3d, double legacy_normalized_t,
    int viewport_width, int viewport_height,
    const TimelineFrameSnapshot* snapshot, const char* camera_target_id,
    PreviewCameraSample* out_sample);

#endif
