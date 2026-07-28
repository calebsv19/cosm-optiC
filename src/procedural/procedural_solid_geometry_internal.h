#ifndef PROCEDURAL_SOLID_GEOMETRY_INTERNAL_H
#define PROCEDURAL_SOLID_GEOMETRY_INTERNAL_H

#include "core_object.h"

#include <math.h>
#include <stdbool.h>

static inline CoreObjectVec3 psg_vec_add(
    CoreObjectVec3 a,
    CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline CoreObjectVec3 psg_vec_sub(
    CoreObjectVec3 a,
    CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline CoreObjectVec3 psg_vec_scale(
    CoreObjectVec3 value,
    double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static inline double psg_vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline CoreObjectVec3 psg_vec_cross(
    CoreObjectVec3 a,
    CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static inline double psg_vec_length(CoreObjectVec3 value) {
    return sqrt(psg_vec_dot(value, value));
}

static inline bool psg_vec_normalize(
    CoreObjectVec3 value,
    CoreObjectVec3 *out) {
    const double length = psg_vec_length(value);
    if (!out || !isfinite(length) || length <= 1.0e-14) return false;
    *out = psg_vec_scale(value, 1.0 / length);
    return true;
}

static inline double psg_clamp_unit(double value) {
    if (value < -1.0) return -1.0;
    if (value > 1.0) return 1.0;
    return value;
}

#endif
