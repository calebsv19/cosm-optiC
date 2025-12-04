#ifndef MATH_VEC3_H
#define MATH_VEC3_H

#include <math.h>

typedef struct {
    double x;
    double y;
    double z;
} Vec3;

static inline Vec3 vec3(double x, double y, double z) {
    Vec3 v = {x, y, z};
    return v;
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline Vec3 vec3_scale(Vec3 v, double s) {
    return vec3(v.x * s, v.y * s, v.z * s);
}

static inline double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

static inline double vec3_length(Vec3 v) {
    return sqrt(vec3_dot(v, v));
}

static inline Vec3 vec3_normalize(Vec3 v) {
    double len = vec3_length(v);
    if (len <= 1e-9) return vec3(0.0, 0.0, 0.0);
    return vec3_scale(v, 1.0 / len);
}

static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, double t) {
    return vec3(a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t);
}

#endif // MATH_VEC3_H
