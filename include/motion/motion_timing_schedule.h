#ifndef MOTION_TIMING_SCHEDULE_H
#define MOTION_TIMING_SCHEDULE_H
#include "motion/motion_timing_duration.h"
#include <stdbool.h>
#define MOTION_TIMING_WAYPOINT_CAPACITY 32
/* Coordinates are world distance along one straight axis, not curve parameters. */
typedef struct MotionTimingWaypoint {
    double position, speed, hold;
    bool fixed_arrival;
    double arrival; /* seconds on schedule clock; ignored unless fixed */
} MotionTimingWaypoint;
typedef struct MotionTimingScheduleRequest {
    double start_time, max_speed, acceleration, braking;
    size_t count;
    MotionTimingWaypoint points[MOTION_TIMING_WAYPOINT_CAPACITY];
} MotionTimingScheduleRequest;
typedef enum MotionTimingConflictKind {
    MOTION_CONFLICT_NONE, MOTION_CONFLICT_INPUT, MOTION_CONFLICT_MOVING_HOLD,
    MOTION_CONFLICT_MOVING_REVERSAL, MOTION_CONFLICT_ENDPOINT_SPEED,
    MOTION_CONFLICT_ARRIVAL_EARLY, MOTION_CONFLICT_ARRIVAL_LATE,
    MOTION_CONFLICT_NUMERIC
} MotionTimingConflictKind;
typedef struct MotionTimingConflict {
    MotionTimingConflictKind kind;
    size_t waypoint; /* conflicting destination, or offending point */
    double requested_arrival, earliest_arrival, latest_arrival;
} MotionTimingConflict;
typedef struct MotionTimingSchedule {
    MotionTimingScheduleRequest request;
    double arrivals[MOTION_TIMING_WAYPOINT_CAPACITY];
    double departures[MOTION_TIMING_WAYPOINT_CAPACITY];
    MotionTimingFixedPlan legs[MOTION_TIMING_WAYPOINT_CAPACITY-1];
    double end_time;
} MotionTimingSchedule;
typedef struct MotionTimingScheduleSample {
    double position, velocity, acceleration;
    size_t waypoint; /* departure point while travelling */
    bool stationary; /* includes declared holds and zero-distance legs */
} MotionTimingScheduleSample;
/* Waypoint speeds and holds are hard requirements. Between fixed anchors,
 * allocate slack to later legs first within each leg's feasible time range.
 * Unanchored suffix uses minimum durations. Point zero arrives at start_time.
 * Output unchanged on failure; optional conflict contains the reason/time range. */
MotionTimingStatus MotionTimingScheduleBuild(const MotionTimingScheduleRequest *, MotionTimingSchedule *, MotionTimingConflict *);
/* Immutable successful schedule; arbitrary/reverse seeks. Right-sided at
 * internal events, left-sided at final arrival when there is no final hold. */
MotionTimingStatus MotionTimingScheduleSampleAt(const MotionTimingSchedule *, double seconds, MotionTimingScheduleSample *);
const char *MotionTimingConflictLabel(MotionTimingConflictKind);
#endif
