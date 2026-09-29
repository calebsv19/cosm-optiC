#ifndef EVALUATED_CAMERA_ROUTE_H
#define EVALUATED_CAMERA_ROUTE_H
#include "app/preview_camera_sample.h"
/* Retain legacy orientation/lens while replacing translation with a route. */
bool EvaluatedCameraRouteSample(const Camera *camera, double z, const Path *path,
    const CameraPath3D *depth, double normalized_t, int width, int height,
    const TimelineFrameSnapshot *snapshot, PreviewCameraSample *out);
#endif
