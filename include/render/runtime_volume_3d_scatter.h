#ifndef RENDER_RUNTIME_VOLUME_3D_SCATTER_H
#define RENDER_RUNTIME_VOLUME_3D_SCATTER_H

#include <stdbool.h>

#include "render/runtime_caustic_volume_cache_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    bool active;
    int sampleCount;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    int directSampleCount;
    double directRadiance;
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    int causticSampleCount;
    int causticContributingSampleCount;
    double causticRadiance;
    double causticRadianceR;
    double causticRadianceG;
    double causticRadianceB;
} RuntimeVolume3DScatterResult;

RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
    const RuntimeNative3DSamplingContext* sampling);
RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache);
RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateDensityDebugAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]]);

void RuntimeVolume3DScatter_ResetTuning(void);
void RuntimeVolume3DScatter_SetStrengthGain(double gain);
void RuntimeVolume3DScatter_SetCausticStrengthGain(double gain);
void RuntimeVolume3DScatter_SetStepScale(double step_scale);
void RuntimeVolume3DScatter_SetTint(double r, double g, double b);

#endif
