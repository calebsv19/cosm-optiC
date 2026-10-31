#ifndef EVALUATED_CAMERA_ORIENTATION_H
#define EVALUATED_CAMERA_ORIENTATION_H
#include "app/preview_camera_sample.h"
#include "motion/scene_motion_paths.h"
bool EvaluatedCameraOrientation(const MotionPathBinding *binding,
                                double progress, PreviewCameraSample *sample);
bool EvaluatedCameraOrientationTarget(const char *target_id, const MotionPathBinding *binding,
                                double progress, PreviewCameraSample *sample);
#endif
