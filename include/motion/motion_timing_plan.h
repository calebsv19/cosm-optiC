#ifndef MOTION_TIMING_PLAN_H
#define MOTION_TIMING_PLAN_H

/* Detached M5 straight-route timing, in world units and seconds. No scene
 * mutation, allocation, clock, serialization, or alternative position owner. */
typedef enum MotionTimingStatus {
    MOTION_TIMING_OK,
    MOTION_TIMING_INVALID_INPUT,
    MOTION_TIMING_INFEASIBLE,
    MOTION_TIMING_NUMERIC_RANGE,
    MOTION_TIMING_TIME_OUT_OF_RANGE
} MotionTimingStatus;

typedef struct MotionTimingRequest {
    double displacement; /* Signed route distance; origin is zero. */
    double max_speed, acceleration, braking;
    double start_speed, end_speed; /* Nonnegative magnitudes along travel. */
} MotionTimingRequest;

typedef struct MotionTimingPlan {
    MotionTimingRequest request;
    double duration, peak_speed;
    double accelerate_time, cruise_time, brake_time;
    double accelerate_distance, cruise_distance;
} MotionTimingPlan;

typedef struct MotionTimingSample {
    double displacement, velocity, acceleration;
} MotionTimingSample;

const char *MotionTimingStatusLabel(MotionTimingStatus status);
/* Minimum-duration monotone motion. Outputs are unchanged on any failure.
 * Positive finite limits, endpoint speeds <= max_speed. No internal reversal.
 * Zero distance is valid only at rest and has zero duration. */
MotionTimingStatus MotionTimingPlanBuild(const MotionTimingRequest *request,
                                        MotionTimingPlan *out);
/* Read-only, seek-order independent. Accepts only [0,duration]. At interior
 * switches acceleration is right-sided; at the endpoint it is left-sided.
 * Acceleration can jump. No continuous-acceleration or jerk guarantee.
 * Pass an unmodified successful PlanBuild result. */
MotionTimingStatus MotionTimingPlanSample(const MotionTimingPlan *plan,
                                         double seconds,
                                         MotionTimingSample *out);
#endif
