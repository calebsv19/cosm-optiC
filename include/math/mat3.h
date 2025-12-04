#ifndef MATH_MAT3_H
#define MATH_MAT3_H

#include "math/vec2.h"

typedef struct {
    double m[3][3];
} Mat3;

static inline Mat3 mat3_identity(void) {
    Mat3 m = {{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    }};
    return m;
}

static inline Mat3 mat3_translate(double tx, double ty) {
    Mat3 m = mat3_identity();
    m.m[0][2] = tx;
    m.m[1][2] = ty;
    return m;
}

static inline Mat3 mat3_scale(double sx, double sy) {
    Mat3 m = mat3_identity();
    m.m[0][0] = sx;
    m.m[1][1] = sy;
    return m;
}

static inline Mat3 mat3_mul(Mat3 a, Mat3 b) {
    Mat3 r = {{{0}}};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] +
                        a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j];
        }
    }
    return r;
}

static inline Vec2 mat3_mul_vec2(Mat3 m, Vec2 v) {
    double x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2];
    double y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2];
    double w = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2];
    if (w == 0.0) w = 1.0;
    return vec2(x / w, y / w);
}

#endif // MATH_MAT3_H
