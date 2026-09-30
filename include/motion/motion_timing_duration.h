#ifndef MOTION_TIMING_DURATION_H
#define MOTION_TIMING_DURATION_H
#include "motion/motion_timing_plan.h"
#include <stddef.h>
#define MOTION_TIMING_PHASE_CAPACITY 8
/* Piecewise-linear velocity at fixed time; immutable after successful build. */
typedef struct MotionTimingFixedPlan {
    MotionTimingRequest request;
    double duration, stationary_seconds;
    size_t count;
    double times[MOTION_TIMING_PHASE_CAPACITY];
    double speeds[MOTION_TIMING_PHASE_CAPACITY];
    double distances[MOTION_TIMING_PHASE_CAPACITY];
    double accelerations[MOTION_TIMING_PHASE_CAPACITY];
} MotionTimingFixedPlan;
/* Maximum is +infinity when arbitrarily slow passage permits arbitrary delay.
 * Nonzero legs never insert an unrequested stationary interval.
 * All failure outputs unchanged. Bounds concern monotone straight-route motion. */
MotionTimingStatus MotionTimingDurationRange(const MotionTimingRequest *, double *minimum, double *maximum);
MotionTimingStatus MotionTimingFixedBuild(const MotionTimingRequest *, double seconds, MotionTimingFixedPlan *);
MotionTimingStatus MotionTimingFixedSample(const MotionTimingFixedPlan *, double seconds, MotionTimingSample *);
#endif
