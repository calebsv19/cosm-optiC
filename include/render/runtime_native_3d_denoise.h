#ifndef RENDER_RUNTIME_NATIVE_3D_DENOISE_H
#define RENDER_RUNTIME_NATIVE_3D_DENOISE_H

#include <stdbool.h>

#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_feature_buffer.h"

bool RuntimeNative3DDenoise_ShouldApply(RayTracing3DIntegratorId integrator_id,
                                        int temporal_frames,
                                        bool denoise_enabled);
bool RuntimeNative3DDenoise_Apply(float* radiance_buffer,
                                  int radiance_stride,
                                  const RuntimeNative3DFeatureBuffer* features);

#endif
