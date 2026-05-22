#ifndef RENDER_RUNTIME_VOLUME_3D_SCATTER_H
#define RENDER_RUNTIME_VOLUME_3D_SCATTER_H

#include <stdbool.h>

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
} RuntimeVolume3DScatterResult;

RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min,
    double t_max,
    const RuntimeNative3DSamplingContext* sampling);

void RuntimeVolume3DScatter_ResetTuning(void);
void RuntimeVolume3DScatter_SetStrengthGain(double gain);
void RuntimeVolume3DScatter_SetStepScale(double step_scale);
void RuntimeVolume3DScatter_SetTint(double r, double g, double b);

#endif
