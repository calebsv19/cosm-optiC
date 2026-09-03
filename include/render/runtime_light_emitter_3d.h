#ifndef RENDER_RUNTIME_LIGHT_EMITTER_3D_H
#define RENDER_RUNTIME_LIGHT_EMITTER_3D_H

#include <fisics/extensions.h>

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
                                        double t_min FISICS_DIM(length) FISICS_UNIT(meter),
                                        double t_max FISICS_DIM(length) FISICS_UNIT(meter),
                                        RuntimeLightEmitterHit3DResult* out_result);

bool RuntimeLightEmitter3D_ResolveFirstHit(const RuntimeScene3D* scene,
                                           const Ray3D* ray,
                                           double t_min FISICS_DIM(length) FISICS_UNIT(meter),
                                           double t_max FISICS_DIM(length) FISICS_UNIT(meter),
                                           RuntimeLightEmitterTrace3DResult* out_result);

#endif
