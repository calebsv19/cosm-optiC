#ifndef RAY_TYPES_H
#define RAY_TYPES_H

typedef struct {
    double ox;
    double oy;
    double dx;
    double dy;
} Ray2D;

typedef struct {
    double t;
    double px;
    double py;
    double nx;
    double ny;
    int objectIndex;
    int triangleIndex;
    double baryU;
    double baryV;
    double baryW;
} HitInfo2D;

#endif // RAY_TYPES_H
