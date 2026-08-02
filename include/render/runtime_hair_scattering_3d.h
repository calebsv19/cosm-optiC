#ifndef RENDER_RUNTIME_HAIR_SCATTERING_3D_H
#define RENDER_RUNTIME_HAIR_SCATTERING_3D_H

#include <stdbool.h>

#include "render/runtime_scene_3d.h"

typedef struct RuntimeHairOptics3D {
    bool enabled;
    double absorptionR;
    double absorptionG;
    double absorptionB;
    double longitudinalRoughness;
    double azimuthalRoughness;
    double ior;
    double cuticleTiltDegrees;
} RuntimeHairOptics3D;

typedef struct RuntimeHairScattering3DResult {
    bool valid;
    double r;
    double g;
    double b;
    double lobeR;
    double lobeTT;
    double lobeTRT;
    double lobeHigherOrder;
    double longitudinal;
    double azimuthal;
} RuntimeHairScattering3DResult;

RuntimeHairOptics3D RuntimeHairOptics3D_Default(void);
RuntimeHairOptics3D RuntimeHairOptics3D_Normalize(
    const RuntimeHairOptics3D* optics);
bool RuntimeHairScattering3D_ShouldApply(
    bool has_curve_tangent,
    const RuntimeHairOptics3D* optics);

bool RuntimeHairScattering3D_EvaluateSingleFiber(
    const RuntimeHairOptics3D* optics,
    Vec3 tangent,
    Vec3 geometric_normal,
    Vec3 light_direction,
    Vec3 view_direction,
    RuntimeHairScattering3DResult* out_result);

#endif
