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
    double radiance; /* compatibility scalar; RGB is authoritative when present */
    bool directSampledRectangle; /* non-delta opaque receivers use direct sampling only */
    bool hasRadianceRGB;
    Vec3 radianceRGB;

} RuntimeLightEmitterHit3DResult;

typedef struct {
    bool geometryHit;
    bool emitterHit;
    bool emitterWins;
    HitInfo3D geometryHitInfo;
    RuntimeLightEmitterHit3DResult emitterHitInfo;
} RuntimeLightEmitterTrace3DResult;

double RuntimeLightEmitter3D_BsdfHitWeight(const RuntimeLightEmitterHit3DResult* hit,
                                            bool exclusive_path, double mis_weight);
double RuntimeLightEmitter3D_DirectEstimate(double total, double direct_only, double mis_weight);
Vec3 RuntimeLightEmitter3D_RadianceRGB(const RuntimeLightEmitterHit3DResult* hit);
void RuntimeLightEmitter3D_ApplyTransmittance(RuntimeLightEmitterHit3DResult* hit,
                                              Vec3 transmittance);

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

/* Camera queries omit legacy rectangular lighting helpers. Optical continuation
 * queries above retain them; calibrated radiance emitters remain visible. */
bool RuntimeLightEmitter3D_IntersectCameraRay(const RuntimeScene3D* scene, const Ray3D* ray,
                                              double t_min, double t_max,
                                              RuntimeLightEmitterHit3DResult* out_result);
bool RuntimeLightEmitter3D_ResolveCameraFirstHit(const RuntimeScene3D* scene, const Ray3D* ray,
                                                 double t_min, double t_max,
                                                 RuntimeLightEmitterTrace3DResult* out_result);

#endif
