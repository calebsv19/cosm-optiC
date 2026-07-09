#include "app/preview_camera_sample.h"

#include <string.h>

static double preview_camera_sample_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double preview_camera_sample_pitch_for_t(const Path* camera_path,
                                                const CameraPath3D* camera_path3d,
                                                double normalized_t) {
    int segment = 0;
    int next = 0;
    double local_t = 0.0;
    double pitch0 = 0.0;
    double pitch1 = 0.0;

    if (!camera_path3d || !camera_path || camera_path->numPoints <= 0) {
        return 0.0;
    }
    if (camera_path->numPoints == 1) {
        return camera_path3d->point_pitch[0];
    }

    PathMapNormalizedT(camera_path, normalized_t, &segment, &local_t);
    if (segment < 0) segment = 0;
    if (segment >= camera_path->numPoints) segment = camera_path->numPoints - 1;
    next = (segment + 1 < camera_path->numPoints) ? (segment + 1) : segment;
    pitch0 = camera_path3d->point_pitch[segment];
    pitch1 = camera_path3d->point_pitch[next];
    return pitch0 + (pitch1 - pitch0) * local_t;
}

bool PreviewCameraSampleEvaluate(const Camera* base_camera,
                                 double base_camera_z,
                                 const Path* camera_path,
                                 const CameraPath3D* camera_path3d,
                                 double normalized_t,
                                 int viewport_width,
                                 int viewport_height,
                                 PreviewCameraSample* out_sample) {
    PreviewCameraSample sample = {0};

    if (!base_camera || !out_sample) {
        return false;
    }

    sample.valid = true;
    sample.uses_authored_path = (camera_path && camera_path->numPoints > 0);
    sample.position_x = base_camera->x;
    sample.position_y = base_camera->y;
    sample.position_z = base_camera_z;
    sample.yaw_radians = base_camera->rotation;
    sample.pitch_radians = 0.0;
    sample.fov_y_degrees = PREVIEW_CAMERA_SAMPLE_DEFAULT_FOV_Y_DEGREES;
    sample.aspect_ratio = 1.0;
    if (viewport_width > 0 && viewport_height > 0) {
        sample.aspect_ratio = (double)viewport_width / (double)viewport_height;
    }

    if (sample.uses_authored_path) {
        if (camera_path->numPoints >= 2) {
            Point cam_point =
                GetPositionAlongPathNormalized((Path*)camera_path,
                                               preview_camera_sample_clamp01(normalized_t));
            sample.position_x = cam_point.x;
            sample.position_y = cam_point.y;
            sample.yaw_radians =
                GetRotationAlongPathNormalized((Path*)camera_path,
                                               preview_camera_sample_clamp01(normalized_t));
            if (camera_path3d) {
                sample.position_z =
                    CameraPath3D_GetPositionZNormalized(camera_path,
                                                        camera_path3d,
                                                        preview_camera_sample_clamp01(normalized_t));
            }
        } else {
            sample.position_x = camera_path->points[0].x;
            sample.position_y = camera_path->points[0].y;
            sample.yaw_radians = camera_path->rotations[0];
            if (camera_path3d) {
                sample.position_z = camera_path3d->point_z[0];
            }
        }

        if (camera_path3d) {
            sample.pitch_radians =
                preview_camera_sample_pitch_for_t(camera_path,
                                                  camera_path3d,
                                                  preview_camera_sample_clamp01(normalized_t));
        }
    }

    *out_sample = sample;
    return true;
}
