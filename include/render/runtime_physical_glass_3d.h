#ifndef RUNTIME_PHYSICAL_GLASS_3D_H
#define RUNTIME_PHYSICAL_GLASS_3D_H
#include "render/runtime_camera_dielectric_3d.h"
#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_light_emitter_3d.h"
typedef struct {
    bool valid, reflected, delta;
    bool hasMarginalPdf; /* false for microsurface random-walk estimators */
    unsigned scatteringOrder;
    Vec3 direction;
    double pdf, weight;
    RuntimeDielectricTransport3D interface; /* delta/single-scatter details only */
} RuntimePhysicalGlass3DSample;
typedef struct {
    RuntimeCameraMedium3D medium;
    int depth;
} RuntimePhysicalGlass3DContext;
const RuntimePhysicalGlass3DContext *
RuntimePhysicalGlass3D_SetContext(const RuntimePhysicalGlass3DContext *);
void RuntimePhysicalGlass3D_CurrentMedium(RuntimeCameraMedium3D *);
typedef struct {
    bool hit, emitterWins, scattered, deterministicStraight;
    RuntimeLightEmitterHit3DResult emitter;
    HitInfo3D hitInfo;
    Ray3D ray;
    RuntimeMaterialPayload3D payload;
    RuntimeCameraMedium3D medium;
    Vec3 throughput, radiance;
    int depth;
} RuntimePhysicalGlass3DContinuation;
bool RuntimePhysicalGlass3D_Advance(const RuntimeScene3D *, const Ray3D *, const HitInfo3D *,
                                    const RuntimeMaterialPayload3D *,
                                    const RuntimeNative3DSamplingContext *,
                                    const RuntimeCameraMedium3D *, int depth,
                                    RuntimePhysicalGlass3DContinuation *);
double RuntimePhysicalGlass3D_Fresnel(double cosine, double eta_from, double eta_to);
bool RuntimePhysicalGlass3D_EvaluateSingleScatter(const RuntimeMaterialPayload3D *, Vec3 normal, Vec3 incoming,
                                     Vec3 outgoing, double from, double to, double *f_cos,
                                     double *pdf);
/* Single-scattering reference only; not the total rough dielectric BSDF. */
bool RuntimePhysicalGlass3D_SampleSingleScatter(const RuntimeMaterialPayload3D *, Vec3, Vec3,
                                               double, double, double, double, double,
                                               RuntimePhysicalGlass3DSample *);
bool RuntimePhysicalGlass3D_SampleMicrosurface(const RuntimeMaterialPayload3D *, Vec3, Vec3,
                                              double, double, double, double, double,
                                              RuntimePhysicalGlass3DSample *);
bool RuntimePhysicalGlass3D_Sample(const RuntimeMaterialPayload3D *payload, Vec3 boundary_normal,
                                   Vec3 incoming, double eta_from, double eta_to, double u,
                                   double v, double choice, RuntimePhysicalGlass3DSample *out);
bool RuntimePhysicalGlass3D_Shade(const RuntimeScene3D *scene, const Ray3D *ray,
                                  const HitInfo3D *hit, const RuntimeMaterialPayload3D *payload,
                                  const RuntimeNative3DSamplingContext *sampling, int x, int y,
                                  int width, int height, RuntimeDisneyV2_3DResult *out);
#endif
