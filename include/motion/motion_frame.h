#ifndef MOTION_FRAME_H
#define MOTION_FRAME_H
#include <stdbool.h>
/* Orthonormal, right-handed path frame. No playback state or UI dependencies. */
typedef struct MotionFrame { double forward[3], up[3], right[3]; } MotionFrame;
bool MotionFrameSeed(MotionFrame *out, const double forward[3], const double up[3]);
bool MotionFrameTransport(const MotionFrame *from, const double forward[3], MotionFrame *out);
void MotionFrameRoll(MotionFrame *frame, double radians);
void MotionFrameModelRotation(const MotionFrame *frame, int axis, const double offset_degrees[3], double euler[3]);
#endif
