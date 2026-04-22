#include "app/preview_camera_projector.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void preview_camera_projector_normalize(double* x, double* y, double* z) {
    double len = 0.0;
    if (!x || !y || !z) return;
    len = sqrt((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    if (len <= 1e-9) return;
    *x /= len;
    *y /= len;
    *z /= len;
}

static void preview_camera_projector_cross(double ax,
                                           double ay,
                                           double az,
                                           double bx,
                                           double by,
                                           double bz,
                                           double* out_x,
                                           double* out_y,
                                           double* out_z) {
    if (!out_x || !out_y || !out_z) return;
    *out_x = ay * bz - az * by;
    *out_y = az * bx - ax * bz;
    *out_z = ax * by - ay * bx;
}

bool PreviewCameraProjectorBuild(const PreviewCameraSample* sample,
                                 SDL_Rect viewport,
                                 PreviewCameraProjector* out_projector) {
    PreviewCameraProjector projector = {0};
    double yaw = 0.0;
    double pitch = 0.0;
    double fov_y_degrees = 0.0;
    double aspect = 1.0;
    double world_up_x = 0.0;
    double world_up_y = 0.0;
    double world_up_z = 1.0;

    if (!sample || !out_projector) return false;
    if (!sample->valid) return false;
    if (viewport.w <= 0 || viewport.h <= 0) return false;

    yaw = sample->yaw_radians;
    pitch = sample->pitch_radians;
    fov_y_degrees = sample->fov_y_degrees;
    if (!(fov_y_degrees > 1.0) || !isfinite(fov_y_degrees)) {
        fov_y_degrees = PREVIEW_CAMERA_SAMPLE_DEFAULT_FOV_Y_DEGREES;
    }
    if (fov_y_degrees < 10.0) fov_y_degrees = 10.0;
    if (fov_y_degrees > 140.0) fov_y_degrees = 140.0;

    aspect = sample->aspect_ratio;
    if (!(aspect > 0.01) || !isfinite(aspect)) {
        aspect = (double)viewport.w / (double)viewport.h;
    }
    if (!(aspect > 0.01) || !isfinite(aspect)) {
        aspect = 1.0;
    }

    projector.viewport = viewport;
    projector.origin_x = sample->position_x;
    projector.origin_y = sample->position_y;
    projector.origin_z = sample->position_z;
    projector.forward_x = sin(yaw) * cos(pitch);
    projector.forward_y = -cos(yaw) * cos(pitch);
    projector.forward_z = sin(pitch);
    preview_camera_projector_normalize(&projector.forward_x,
                                       &projector.forward_y,
                                       &projector.forward_z);

    preview_camera_projector_cross(projector.forward_x,
                                   projector.forward_y,
                                   projector.forward_z,
                                   world_up_x,
                                   world_up_y,
                                   world_up_z,
                                   &projector.right_x,
                                   &projector.right_y,
                                   &projector.right_z);
    if (fabs(projector.right_x) <= 1e-9 &&
        fabs(projector.right_y) <= 1e-9 &&
        fabs(projector.right_z) <= 1e-9) {
        projector.right_x = cos(yaw);
        projector.right_y = sin(yaw);
        projector.right_z = 0.0;
    }
    preview_camera_projector_normalize(&projector.right_x,
                                       &projector.right_y,
                                       &projector.right_z);

    preview_camera_projector_cross(projector.right_x,
                                   projector.right_y,
                                   projector.right_z,
                                   projector.forward_x,
                                   projector.forward_y,
                                   projector.forward_z,
                                   &projector.up_x,
                                   &projector.up_y,
                                   &projector.up_z);
    preview_camera_projector_normalize(&projector.up_x,
                                       &projector.up_y,
                                       &projector.up_z);

    projector.near_plane = 0.05;
    projector.tan_half_fov_y = tan((fov_y_degrees * M_PI / 180.0) * 0.5);
    if (!(projector.tan_half_fov_y > 1e-6) || !isfinite(projector.tan_half_fov_y)) {
        return false;
    }
    projector.tan_half_fov_x = projector.tan_half_fov_y * aspect;
    if (!(projector.tan_half_fov_x > 1e-6) || !isfinite(projector.tan_half_fov_x)) {
        return false;
    }

    *out_projector = projector;
    return true;
}

void PreviewCameraProjectorWorldToCamera(const PreviewCameraProjector* projector,
                                         double world_x,
                                         double world_y,
                                         double world_z,
                                         double* out_camera_x,
                                         double* out_camera_y,
                                         double* out_camera_z) {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    if (out_camera_x) *out_camera_x = 0.0;
    if (out_camera_y) *out_camera_y = 0.0;
    if (out_camera_z) *out_camera_z = 0.0;
    if (!projector) return;

    dx = world_x - projector->origin_x;
    dy = world_y - projector->origin_y;
    dz = world_z - projector->origin_z;

    if (out_camera_x) {
        *out_camera_x = dx * projector->right_x +
                        dy * projector->right_y +
                        dz * projector->right_z;
    }
    if (out_camera_y) {
        *out_camera_y = dx * projector->up_x +
                        dy * projector->up_y +
                        dz * projector->up_z;
    }
    if (out_camera_z) {
        *out_camera_z = dx * projector->forward_x +
                        dy * projector->forward_y +
                        dz * projector->forward_z;
    }
}

bool PreviewCameraProjectorProjectPoint(const PreviewCameraProjector* projector,
                                        double world_x,
                                        double world_y,
                                        double world_z,
                                        double* out_screen_x,
                                        double* out_screen_y,
                                        double* out_camera_depth,
                                        bool* out_inside_viewport) {
    double camera_x = 0.0;
    double camera_y = 0.0;
    double camera_z = 0.0;
    double ndc_x = 0.0;
    double ndc_y = 0.0;

    if (out_screen_x) *out_screen_x = 0.0;
    if (out_screen_y) *out_screen_y = 0.0;
    if (out_camera_depth) *out_camera_depth = 0.0;
    if (out_inside_viewport) *out_inside_viewport = false;
    if (!projector) return false;

    PreviewCameraProjectorWorldToCamera(projector,
                                        world_x,
                                        world_y,
                                        world_z,
                                        &camera_x,
                                        &camera_y,
                                        &camera_z);
    if (out_camera_depth) {
        *out_camera_depth = camera_z;
    }
    if (camera_z <= projector->near_plane) {
        return false;
    }

    ndc_x = camera_x / (camera_z * projector->tan_half_fov_x);
    ndc_y = camera_y / (camera_z * projector->tan_half_fov_y);

    if (out_screen_x) {
        *out_screen_x = (double)projector->viewport.x +
                        (ndc_x + 1.0) * 0.5 * (double)projector->viewport.w;
    }
    if (out_screen_y) {
        *out_screen_y = (double)projector->viewport.y +
                        (1.0 - (ndc_y + 1.0) * 0.5) * (double)projector->viewport.h;
    }
    if (out_inside_viewport) {
        *out_inside_viewport = (ndc_x >= -1.0 && ndc_x <= 1.0 &&
                                ndc_y >= -1.0 && ndc_y <= 1.0);
    }
    return true;
}
