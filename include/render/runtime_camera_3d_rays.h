#ifndef RENDER_RUNTIME_CAMERA_3D_RAYS_H
#define RENDER_RUNTIME_CAMERA_3D_RAYS_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"

typedef struct {
    int viewportWidth;
    int viewportHeight;
    Vec3 origin;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    double nearPlane;
    double tanHalfFovX;
    double tanHalfFovY;
} RuntimeCameraProjector3D;

bool RuntimeCameraProjector3D_Build(const RuntimeCamera3D* camera,
                                    int viewport_width,
                                    int viewport_height,
                                    RuntimeCameraProjector3D* out_projector);

Ray3D RuntimeCameraProjector3D_MakePrimaryRay(const RuntimeCameraProjector3D* projector,
                                              double pixel_x,
                                              double pixel_y);

bool RuntimeCameraProjector3D_ProjectPoint(const RuntimeCameraProjector3D* projector,
                                           Vec3 world_point,
                                           double* out_screen_x,
                                           double* out_screen_y,
                                           double* out_camera_depth,
                                           bool* out_inside_viewport);

#endif
