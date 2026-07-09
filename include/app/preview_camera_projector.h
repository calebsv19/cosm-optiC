#ifndef PREVIEW_CAMERA_PROJECTOR_H
#define PREVIEW_CAMERA_PROJECTOR_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "app/preview_camera_sample.h"

typedef struct PreviewCameraProjector {
    SDL_Rect viewport;
    double origin_x;
    double origin_y;
    double origin_z;
    double forward_x;
    double forward_y;
    double forward_z;
    double right_x;
    double right_y;
    double right_z;
    double up_x;
    double up_y;
    double up_z;
    double near_plane;
    double tan_half_fov_x;
    double tan_half_fov_y;
} PreviewCameraProjector;

bool PreviewCameraProjectorBuild(const PreviewCameraSample* sample,
                                 SDL_Rect viewport,
                                 PreviewCameraProjector* out_projector);

void PreviewCameraProjectorWorldToCamera(const PreviewCameraProjector* projector,
                                         double world_x,
                                         double world_y,
                                         double world_z,
                                         double* out_camera_x,
                                         double* out_camera_y,
                                         double* out_camera_z);

bool PreviewCameraProjectorProjectPoint(const PreviewCameraProjector* projector,
                                        double world_x,
                                        double world_y,
                                        double world_z,
                                        double* out_screen_x,
                                        double* out_screen_y,
                                        double* out_camera_depth,
                                        bool* out_inside_viewport);

#endif
