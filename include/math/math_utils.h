#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>
#include <float.h>

static inline double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline double lerpd(double a, double b, double t) {
    return a + (b - a) * t;
}

static inline double is_near_zero(double v) {
    return fabs(v) < 1e-9;
}

#endif // MATH_UTILS_H
