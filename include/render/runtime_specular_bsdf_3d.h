#ifndef RUNTIME_SPECULAR_BSDF_3D_H
#define RUNTIME_SPECULAR_BSDF_3D_H
#include "render/runtime_principled_bsdf_3d.h"
#include "render/runtime_ray_3d.h"

typedef struct {
    bool valid;
    Vec3 direction;
    double pdf; /* Directional PDF including lobe selection probability. */
    double cosTheta;
    double throughputR, throughputG, throughputB;
} RuntimeSpecularBSDF3DSample;

/* Rejected below-surface samples retain zero weight; never redirect/resample them. */
RuntimeSpecularBSDF3DSample RuntimeSpecularBSDF3D_Sample(
    const RuntimePrincipledBSDF3D *bsdf, const HitInfo3D *hit, Vec3 view,
    double lobe_probability, double u, double v);
double RuntimeSpecularBSDF3D_Pdf(const RuntimePrincipledBSDF3D *bsdf,
    const HitInfo3D *hit, Vec3 view, Vec3 direction);
#endif
