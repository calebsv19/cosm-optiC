#ifndef RENDER_RUNTIME_DIRECT_LIGHT_INTERNAL_3D_H
#define RENDER_RUNTIME_DIRECT_LIGHT_INTERNAL_3D_H

#include "render/runtime_direct_light_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

double runtime_direct_light_3d_peak(double r, double g, double b);

void runtime_direct_light_3d_accumulate_source(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSource3D* source,
    bool receiver_is_transparent,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* io_result,
    double* io_light_r,
    double* io_light_g,
    double* io_light_b,
    bool* io_any_light_sample_visible);

#endif
