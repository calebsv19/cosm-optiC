#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_HOST_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_HOST_H

#include "render/runtime_native_3d_render.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render_internal.h"
#include "render/runtime_native_3d_render_unit.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_scene_3d_samples.h"
#include "render/runtime_volume_3d_integrate.h"

double runtime_native_3d_render_clamp_unit(double value);
void runtime_native_3d_render_stats_normalize_temporal(
    RuntimeNative3DRenderStats* stats,
    int committed_subpasses);
void runtime_native_3d_render_stats_record_adaptive_state_summary(
    RuntimeNative3DRenderStats* stats,
    const RuntimeNative3DAdaptivePixelStateSummary* summary);

#endif
