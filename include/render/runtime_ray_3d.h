#ifndef RENDER_RUNTIME_RAY_3D_H
#define RENDER_RUNTIME_RAY_3D_H

#include <stdbool.h>

#include "render/runtime_scene_3d.h"

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray3D;

typedef struct {
    double t;
    Vec3 position;
    Vec3 normal;
    int triangleIndex;
    int localTriangleIndex;
    int primitiveIndex;
    int sceneObjectIndex;
    RuntimePrimitive3DSourceRef source;
    double baryU;
    double baryV;
    double baryW;
} HitInfo3D;

Ray3D RuntimeRay3D_Make(Vec3 origin, Vec3 direction);
Ray3D RuntimeRay3D_MakeOffset(Vec3 origin, Vec3 normal, Vec3 direction, double epsilon);
void HitInfo3D_Reset(HitInfo3D* hit);

bool RuntimeRay3D_IntersectTriangle(const Ray3D* ray,
                                    const RuntimeTriangle3D* triangle,
                                    int triangle_index,
                                    double t_min,
                                    double t_max,
                                    HitInfo3D* out_hit);

bool RuntimeRay3D_TraceSceneFirstHit(const RuntimeScene3D* scene,
                                     const Ray3D* ray,
                                     double t_min,
                                     double t_max,
                                     HitInfo3D* out_hit);

#endif
