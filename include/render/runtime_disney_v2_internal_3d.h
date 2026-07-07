#ifndef RENDER_RUNTIME_DISNEY_V2_INTERNAL_3D_H
#define RENDER_RUNTIME_DISNEY_V2_INTERNAL_3D_H

#include "render/runtime_disney_v2_3d.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_disney_v2_estimator_3d.h"
#include "render/runtime_disney_v2_transport_3d.h"
#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_mirror_composition_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_specular_reflection_3d.h"

#define RUNTIME_DISNEY_V2_3D_EPSILON 1e-4
#define RUNTIME_DISNEY_V2_3D_MAX_DISTANCE 48.0

double runtime_disney_v2_3d_clamp(double value, double min_value, double max_value);
double runtime_disney_v2_3d_clamp01(double value);
double runtime_disney_v2_3d_peak(double r, double g, double b);
Vec3 runtime_disney_v2_3d_default_view_dir(const HitInfo3D* hit);
void runtime_disney_v2_3d_apply_transmittance(
    const RuntimeVisibility3DTransmittance* transmittance,
    RuntimeDisneyV2_3DResult* io_result);
void runtime_disney_v2_3d_refresh_peaks(RuntimeDisneyV2_3DResult* result);
void runtime_disney_v2_3d_apply_stochastic_transport(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* io_result);

#endif
