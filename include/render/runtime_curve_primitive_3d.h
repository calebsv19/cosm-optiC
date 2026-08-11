#ifndef RENDER_RUNTIME_CURVE_PRIMITIVE_3D_H
#define RENDER_RUNTIME_CURVE_PRIMITIVE_3D_H

#include <stdbool.h>
#include <stddef.h>

#include "render/runtime_ray_3d.h"

typedef struct RuntimeCurveBLAS3D RuntimeCurveBLAS3D;

typedef struct RuntimeCurvePrimitive3D {
    Vec3 p0;
    Vec3 p1;
    Vec3 tangent0;
    Vec3 tangent1;
    double radius0;
    double radius1;
    int strandIndex;
    int segmentIndex;
    bool hasRootCap;
    bool hasTipCap;
} RuntimeCurvePrimitive3D;

typedef struct RuntimeCurveAsset3D {
    RuntimeCurvePrimitive3D *primitives;
    size_t primitiveCount;
    RuntimeCurveBLAS3D *blas;
    bool blasDirty;
} RuntimeCurveAsset3D;

void RuntimeCurveAsset3D_Init(RuntimeCurveAsset3D *asset);
void RuntimeCurveAsset3D_Free(RuntimeCurveAsset3D *asset);
bool RuntimeCurveAsset3D_CopyFrom(RuntimeCurveAsset3D *dst,
                                  const RuntimeCurveAsset3D *src);

bool RuntimeCurveAsset3D_BuildFromPolylineStrands(
    RuntimeCurveAsset3D *asset,
    const Vec3 *points,
    const double *radii,
    size_t strand_count,
    size_t points_per_strand);

bool RuntimeRay3D_IntersectCurvePrimitive(
    const Ray3D *ray,
    const RuntimeCurvePrimitive3D *primitive,
    int primitive_index,
    double t_min,
    double t_max,
    HitInfo3D *out_hit);

#endif
