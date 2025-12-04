#ifndef MATH_VEC2_H
#define MATH_VEC2_H

#include <math.h>

typedef struct {
    double x;
    double y;
} Vec2;

static inline Vec2 vec2(double x, double y) {
    Vec2 v = {x, y};
    return v;
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x, a.y - b.y);
}

static inline Vec2 vec2_scale(Vec2 v, double s) {
    return vec2(v.x * s, v.y * s);
}

static inline double vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline double vec2_length(Vec2 v) {
    return sqrt(vec2_dot(v, v));
}

static inline Vec2 vec2_normalize(Vec2 v) {
    double len = vec2_length(v);
    if (len <= 1e-9) return vec2(0.0, 0.0);
    return vec2_scale(v, 1.0 / len);
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, double t) {
    return vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

#endif // MATH_VEC2_H
