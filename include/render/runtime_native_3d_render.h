#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_scene_3d.h"
#include "render/ray_tracing_integrator_catalog.h"

typedef struct {
    int hitPixelCount;
    int visiblePixelCount;
    int bouncePixelCount;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
    double maxRadiance;
    double maxBounceRadiance;
    double totalBounceRadiance;
} RuntimeNative3DRenderStats;

typedef struct {
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector;
    RuntimeNative3DTileOccupancy tileOccupancy;
    RuntimeNative3DSamplingContext sampling;
    int width;
    int height;
    bool valid;
} RuntimeNative3DPreparedFrame;

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src);
bool RuntimeNative3DPrepareFrame(RuntimeNative3DPreparedFrame* out_frame,
                                 int width,
                                 int height,
                                 double normalized_t,
                                 double live_light_x,
                                 double live_light_y);
bool RuntimeNative3DPrepareFrameWithSampling(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             double live_light_x,
                                             double live_light_y,
                                             const RuntimeNative3DSamplingContext* sampling);
void RuntimeNative3DPreparedFrame_Free(RuntimeNative3DPreparedFrame* frame);
bool RuntimeNative3DRenderPreparedRegion(uint8_t* pixel_buffer,
                                         RayTracing3DIntegratorId integrator_id,
                                         const RuntimeNative3DPreparedFrame* frame,
                                         int start_x,
                                         int start_y,
                                         int end_x,
                                         int end_y,
                                         RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderPreparedRegionLuminance(float* luminance_buffer,
                                                  int luminance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DPrepareFrameTileOccupancy(RuntimeNative3DPreparedFrame* frame, int tile_size);
bool RuntimeNative3DPreparedRegionMayContainGeometry(const RuntimeNative3DPreparedFrame* frame,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y);
bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        RayTracing3DIntegratorId integrator_id,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSampling(uint8_t* pixel_buffer,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    int width,
                                                    int height,
                                                    double normalized_t,
                                                    double live_light_x,
                                                    double live_light_y,
                                                    const RuntimeNative3DSamplingContext* sampling,
                                                    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DRenderStats* out_stats);

#endif
