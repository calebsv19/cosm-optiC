#include "render/runtime_camera_3d_rays.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double kRuntimeCameraProjectorDefaultFovYDegrees = 55.0;
static const double kRuntimeCameraProjectorMinimumZoom = 0.01;

bool RuntimeCameraProjector3D_Build(const RuntimeCamera3D* camera,
                                    int viewport_width,
                                    int viewport_height,
                                    RuntimeCameraProjector3D* out_projector) {
    RuntimeCameraProjector3D projector = {0};
    Vec3 world_up = vec3(0.0, 0.0, 1.0);
    double yaw = 0.0;
    double pitch = 0.0;
    double aspect = 1.0;
    double zoom = 1.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double near_plane = 0.1;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double near_plane_epsilon = 1e-6;

    if (!camera || !out_projector) return false;
    if (viewport_width <= 0 || viewport_height <= 0) return false;

    yaw = camera->rotation;
    pitch = camera->lookPitch;
    aspect = (double)viewport_width / (double)viewport_height;
    zoom = camera->zoom;
    if (zoom < kRuntimeCameraProjectorMinimumZoom) {
        zoom = 1.0;
    }

    projector.viewportWidth = viewport_width;
    projector.viewportHeight = viewport_height;
    projector.origin = camera->position;
    projector.forward = vec3(sin(yaw) * cos(pitch),
                             -cos(yaw) * cos(pitch),
                             sin(pitch));
    projector.forward = vec3_normalize(projector.forward);
    projector.right = vec3_cross(world_up, projector.forward);
    if (vec3_length(projector.right) <= 1e-9) {
        projector.right = vec3(cos(yaw), sin(yaw), 0.0);
    }
    projector.right = vec3_normalize(projector.right);
    projector.up = vec3_normalize(vec3_cross(projector.forward, projector.right));
    near_plane = camera->nearPlane;
    if (!(near_plane > near_plane_epsilon)) {
        near_plane = 0.1;
    }
    projector.nearPlane = near_plane;
    projector.tanHalfFovY =
        tan((kRuntimeCameraProjectorDefaultFovYDegrees * M_PI / 180.0) * 0.5) / zoom;
    projector.tanHalfFovX = projector.tanHalfFovY * aspect;

    if (!(projector.tanHalfFovY > 1e-6) || !(projector.tanHalfFovX > 1e-6)) {
        return false;
    }

    *out_projector = projector;
    return true;
}

Ray3D RuntimeCameraProjector3D_MakePrimaryRay(const RuntimeCameraProjector3D* projector,
                                              double pixel_x,
                                              double pixel_y) {
    double ndc_x = 0.0;
    double ndc_y = 0.0;
    Vec3 direction = vec3(0.0, -1.0, 0.0);
    if (!projector || projector->viewportWidth <= 0 || projector->viewportHeight <= 0) {
        return RuntimeRay3D_Make(vec3(0.0, 0.0, 0.0), direction);
    }

    ndc_x = (((pixel_x + 0.5) / (double)projector->viewportWidth) * 2.0) - 1.0;
    ndc_y = 1.0 - (((pixel_y + 0.5) / (double)projector->viewportHeight) * 2.0);
    direction =
        vec3_add(projector->forward,
                 vec3_add(vec3_scale(projector->right, ndc_x * projector->tanHalfFovX),
                          vec3_scale(projector->up, ndc_y * projector->tanHalfFovY)));
    return RuntimeRay3D_Make(projector->origin, direction);
}

bool RuntimeCameraProjector3D_ProjectPoint(const RuntimeCameraProjector3D* projector,
                                           Vec3 world_point,
                                           double* out_screen_x,
                                           double* out_screen_y,
                                           [[fisics::dim(length)]] [[fisics::unit(meter)]] double* out_camera_depth,
                                           bool* out_inside_viewport) {
    Vec3 offset = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double camera_x = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double camera_y = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double camera_z = 0.0;
    double ndc_x = 0.0;
    double ndc_y = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double near_plane = 0.0;

    if (out_screen_x) *out_screen_x = 0.0;
    if (out_screen_y) *out_screen_y = 0.0;
    if (out_camera_depth) *out_camera_depth = 0.0;
    if (out_inside_viewport) *out_inside_viewport = false;
    if (!projector || projector->viewportWidth <= 0 || projector->viewportHeight <= 0) return false;

    offset = vec3_sub(world_point, projector->origin);
    camera_x = vec3_dot(offset, projector->right);
    camera_y = vec3_dot(offset, projector->up);
    camera_z = vec3_dot(offset, projector->forward);
    near_plane = projector->nearPlane;
    if (out_camera_depth) {
        *out_camera_depth = camera_z;
    }
    if (camera_z <= near_plane) {
        return false;
    }

    ndc_x = camera_x / (camera_z * projector->tanHalfFovX);
    ndc_y = camera_y / (camera_z * projector->tanHalfFovY);

    if (out_screen_x) {
        *out_screen_x = (ndc_x + 1.0) * 0.5 * (double)projector->viewportWidth;
    }
    if (out_screen_y) {
        *out_screen_y = (1.0 - (ndc_y + 1.0) * 0.5) * (double)projector->viewportHeight;
    }
    if (out_inside_viewport) {
        *out_inside_viewport = (ndc_x >= -1.0 && ndc_x <= 1.0 &&
                                ndc_y >= -1.0 && ndc_y <= 1.0);
    }
    return true;
}
