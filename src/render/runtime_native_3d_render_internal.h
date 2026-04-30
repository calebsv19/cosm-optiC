#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_H

#include <stdbool.h>

#include "render/runtime_native_3d_render.h"

bool runtime_native_3d_render_dispatch_integrator(float* radiance_buffer,
                                                  int radiance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats);

#endif
