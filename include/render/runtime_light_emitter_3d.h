#ifndef RENDER_RUNTIME_LIGHT_EMITTER_3D_H
#define RENDER_RUNTIME_LIGHT_EMITTER_3D_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"

typedef struct {
    bool hit;
    double t;
    Vec3 position;
    Vec3 normal;
    double radialFalloff;
    double attenuation;
    double radiance;
} RuntimeLightEmitterHit3DResult;

typedef struct {
    bool geometryHit;
    bool emitterHit;
    bool emitterWins;
    HitInfo3D geometryHitInfo;
    RuntimeLightEmitterHit3DResult emitterHitInfo;
} RuntimeLightEmitterTrace3DResult;

bool RuntimeLightEmitter3D_IntersectRay(const RuntimeScene3D* scene,
                                        const Ray3D* ray,
                                        double t_min,
                                        double t_max,
                                        RuntimeLightEmitterHit3DResult* out_result);

bool RuntimeLightEmitter3D_ResolveFirstHit(const RuntimeScene3D* scene,
                                           const Ray3D* ray,
                                           double t_min,
                                           double t_max,
                                           RuntimeLightEmitterTrace3DResult* out_result);

#endif
