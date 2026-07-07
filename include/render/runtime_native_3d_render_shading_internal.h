#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_SHADING_INTERNAL_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_SHADING_INTERNAL_H

#include "render/runtime_native_3d_render_internal.h"

#include "render/runtime_disney_3d.h"
#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"

typedef struct RuntimeNative3DPrimaryTrace {
    RuntimePrimaryHit3DResult primary;
    RuntimeLightEmitterHit3DResult emitterHit;
    RuntimeMaterialPayload3D payload;
    bool payloadResolved;
    bool emitterWins;
} RuntimeNative3DPrimaryTrace;

RuntimeNative3DPrimaryTrace runtime_native_3d_render_trace_primary(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y);
void runtime_native_3d_render_record_disney_v2_emissive_area_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result);
void runtime_native_3d_render_record_disney_v2_mirror_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result);
void runtime_native_3d_render_apply_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const HitInfo3D* hit,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats);
void runtime_native_3d_render_apply_transmitted_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeDisneyV2_3DResult* result,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats);
bool runtime_native_3d_render_shade_direct_light(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DFeatureBuffer* feature_buffer,
    int feature_start_x,
    int feature_start_y,
    RuntimeNative3DRenderStats* out_stats);
bool runtime_native_3d_render_shade_diffuse_bounce(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DRenderStats* out_stats);
bool runtime_native_3d_render_shade_material(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DRenderStats* out_stats);
bool runtime_native_3d_render_shade_emission_transparency(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DRenderStats* out_stats);
bool runtime_native_3d_render_shade_disney(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DRenderStats* out_stats);
bool runtime_native_3d_render_shade_disney_v2(
    float* radiance_buffer,
    int radiance_stride,
    int width,
    int height,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeDisneyV2CausticSidecarProbe3D* caustic_probe,
    RuntimeNative3DFeatureBuffer* feature_buffer,
    int feature_start_x,
    int feature_start_y,
    RuntimeNative3DRenderStats* out_stats);

#endif
