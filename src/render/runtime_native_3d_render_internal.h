#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "render/runtime_native_3d_render.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_volume_3d_scatter.h"

bool runtime_native_3d_render_dispatch_integrator(float* radiance_buffer,
                                                  int radiance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats);
void runtime_native_3d_prepare_frame_set_diag(const char* message);
void runtime_native_3d_prepare_frame_set_diagf(const char* format, ...);
bool runtime_native_3d_render_trace_visible_emitter(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    RuntimeLightEmitterHit3DResult* out_emitter_hit);
void runtime_native_3d_render_write_radiance_rgb(float* radiance_buffer,
                                                 size_t pixel_index,
                                                 double radiance_r,
                                                 double radiance_g,
                                                 double radiance_b,
                                                 double background_floor);
double runtime_native_3d_render_peak_rgb(double radiance_r,
                                         double radiance_g,
                                         double radiance_b);
double runtime_native_3d_render_clamp01(double value);
void runtime_native_3d_render_resolve_hit_tint(const HitInfo3D* hit,
                                               double* out_r,
                                               double* out_g,
                                               double* out_b);
void runtime_native_3d_render_apply_ambient_hit_lighting(const RuntimeScene3D* scene,
                                                         const HitInfo3D* hit,
                                                         double* io_radiance_r,
                                                         double* io_radiance_g,
                                                         double* io_radiance_b,
                                                         double* io_peak_radiance,
                                                         bool* io_visible);
void runtime_native_3d_render_background_rgb(const RuntimeScene3D* scene,
                                             const Ray3D* primary_ray,
                                             double* out_r,
                                             double* out_g,
                                             double* out_b);
RuntimeVolume3DScatterResult runtime_native_3d_render_primary_scatter(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    double t_max,
    const RuntimeNative3DSamplingContext* sampling);
void runtime_native_3d_render_apply_scatter_rgb(
    double* io_radiance_r,
    double* io_radiance_g,
    double* io_radiance_b,
    double* io_peak_radiance,
    bool* io_visible,
    const RuntimeVolume3DScatterResult* scatter);
void runtime_native_3d_render_write_background_radiance(float* radiance_buffer,
                                                        size_t pixel_index,
                                                        const RuntimeScene3D* scene,
                                                        const RuntimeCameraProjector3D* projector,
                                                        const Ray3D* primary_ray,
                                                        const RuntimeVolume3DScatterResult* scatter);
void runtime_native_3d_render_write_emitter_radiance_with_scatter(
    float* radiance_buffer,
    size_t pixel_index,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    const RuntimeLightEmitterHit3DResult* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeNative3DRenderStats* io_stats);

#endif
