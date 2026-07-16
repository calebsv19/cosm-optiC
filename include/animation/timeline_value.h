#ifndef RAY_TRACING_TIMELINE_VALUE_H
#define RAY_TRACING_TIMELINE_VALUE_H

#include <stdbool.h>

#include "animation/timeline_clock.h"

typedef enum TimelineValueType {
    TIMELINE_VALUE_NONE = 0,
    TIMELINE_VALUE_SCALAR,
    TIMELINE_VALUE_VEC3,
    TIMELINE_VALUE_ROTATION_RESERVED
} TimelineValueType;

typedef struct TimelineVec3 {
    double x;
    double y;
    double z;
} TimelineVec3;

typedef struct TimelineValue {
    TimelineValueType type;
    union {
        double scalar;
        TimelineVec3 vec3;
    } as;
} TimelineValue;

const char* TimelineValueTypeLabel(TimelineValueType type);
TimelineValue TimelineValueScalar(double value);
TimelineValue TimelineValueVec3(double x, double y, double z);
bool TimelineValueIsFinite(TimelineValue value);
TimelineStatus TimelineValueInterpolateLinear(TimelineValue a,
                                              TimelineValue b,
                                              double alpha,
                                              TimelineValue* out_value);

#endif
