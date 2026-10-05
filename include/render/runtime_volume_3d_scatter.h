#ifndef RENDER_RUNTIME_VOLUME_3D_SCATTER_H
#define RENDER_RUNTIME_VOLUME_3D_SCATTER_H

#include <fisics/extensions.h>

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
    /* Raw work totals, not temporal averages or unique source counts. */
    uint64_t registrySourceEvaluations;
    uint64_t registryContributingSamples;
    uint64_t registryVisibilityRays;
    uint64_t registryUnsupportedSources;
    uint64_t compatibilityFallbackSamples;
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
    double causticSampledCacheRadianceSum;
    double causticSampledCacheRadianceMax;
    double causticSampledRawDensitySum;
    double causticSampledRawDensityMax;
    double causticSampledDensitySum;
    double causticSampledDensityMax;
    double causticScatterProbabilitySum;
    double causticScatterProbabilityMax;
    double causticCameraTransmittanceSum;
    double causticCameraTransmittanceMin;
    double causticCameraTransmittanceMax;
    double causticVisibilityTermSum;
    double causticVisibilityTermMax;
} RuntimeVolume3DScatterResult;

RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min FISICS_DIM(length) FISICS_UNIT(meter),
    double t_max FISICS_DIM(length) FISICS_UNIT(meter),
    const RuntimeNative3DSamplingContext* sampling);
RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min FISICS_DIM(length) FISICS_UNIT(meter),
    double t_max FISICS_DIM(length) FISICS_UNIT(meter),
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache);
RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateDensityDebugAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min FISICS_DIM(length) FISICS_UNIT(meter),
    double t_max FISICS_DIM(length) FISICS_UNIT(meter));

void RuntimeVolume3DScatter_ResetTuning(void);
typedef struct {double strength,caustic_strength,step_scale,tint[3];} RuntimeVolume3DScatterTuning;
RuntimeVolume3DScatterTuning RuntimeVolume3DScatter_GetTuning(void);
void RuntimeVolume3DScatter_RestoreTuning(RuntimeVolume3DScatterTuning tuning);
void RuntimeVolume3DScatter_SetStrengthGain(double gain);
void RuntimeVolume3DScatter_SetCausticStrengthGain(double gain);
void RuntimeVolume3DScatter_SetStepScale(double step_scale);
void RuntimeVolume3DScatter_SetTint(double r, double g, double b);

#endif
