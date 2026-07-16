#include "animation/timeline_value.h"

#include <math.h>
#include <string.h>

const char* TimelineValueTypeLabel(TimelineValueType type) {
    switch (type) {
        case TIMELINE_VALUE_SCALAR: return "scalar";
        case TIMELINE_VALUE_VEC3: return "vec3";
        case TIMELINE_VALUE_ROTATION_RESERVED: return "rotation_reserved";
        case TIMELINE_VALUE_NONE:
        default: return "none";
    }
}

TimelineValue TimelineValueScalar(double value) {
    TimelineValue result;
    memset(&result, 0, sizeof(result));
    result.type = TIMELINE_VALUE_SCALAR;
    result.as.scalar = value;
    return result;
}

TimelineValue TimelineValueVec3(double x, double y, double z) {
    TimelineValue result;
    memset(&result, 0, sizeof(result));
    result.type = TIMELINE_VALUE_VEC3;
    result.as.vec3.x = x;
    result.as.vec3.y = y;
    result.as.vec3.z = z;
    return result;
}

bool TimelineValueIsFinite(TimelineValue value) {
    switch (value.type) {
        case TIMELINE_VALUE_SCALAR:
            return isfinite(value.as.scalar);
        case TIMELINE_VALUE_VEC3:
            return isfinite(value.as.vec3.x) && isfinite(value.as.vec3.y) &&
                   isfinite(value.as.vec3.z);
        case TIMELINE_VALUE_NONE:
        case TIMELINE_VALUE_ROTATION_RESERVED:
        default:
            return false;
    }
}

TimelineStatus TimelineValueInterpolateLinear(TimelineValue a,
                                              TimelineValue b,
                                              double alpha,
                                              TimelineValue* out_value) {
    TimelineValue result;
    if (!out_value || !isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (a.type != b.type) return TIMELINE_STATUS_TYPE_MISMATCH;
    if (!TimelineValueIsFinite(a) || !TimelineValueIsFinite(b)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    memset(&result, 0, sizeof(result));
    result.type = a.type;
    switch (a.type) {
        case TIMELINE_VALUE_SCALAR:
            result.as.scalar = a.as.scalar + (b.as.scalar - a.as.scalar) * alpha;
            break;
        case TIMELINE_VALUE_VEC3:
            result.as.vec3.x = a.as.vec3.x + (b.as.vec3.x - a.as.vec3.x) * alpha;
            result.as.vec3.y = a.as.vec3.y + (b.as.vec3.y - a.as.vec3.y) * alpha;
            result.as.vec3.z = a.as.vec3.z + (b.as.vec3.z - a.as.vec3.z) * alpha;
            break;
        case TIMELINE_VALUE_NONE:
        case TIMELINE_VALUE_ROTATION_RESERVED:
        default:
            return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
    }
    if (!TimelineValueIsFinite(result)) return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    *out_value = result;
    return TIMELINE_STATUS_OK;
}
