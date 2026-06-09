#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_scene_3d.h"
#include "render/ray_tracing_integrator_catalog.h"

#define RUNTIME_NATIVE_3D_RADIANCE_COLOR_CHANNELS 3
#define RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL 3
#define RUNTIME_NATIVE_3D_RADIANCE_CHANNELS 4
#define RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES 4

typedef struct {
    int hitPixelCount;
    int visiblePixelCount;
    int bouncePixelCount;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
    int temporalCommittedSubpasses;
    int temporalPixelsRendered;
    int temporalPixelsSkipped;
    int temporalActivePixelCount;
    int temporalActiveTileCount;
    int temporalInactiveTileCount;
    int temporalMeasuredTileJobs;
    int temporalAdaptiveSplitParentCount;
    int temporalAdaptiveChildTileCount;
    int temporalSlowTileOriginX;
    int temporalSlowTileOriginY;
    int temporalSlowTileWidth;
    int temporalSlowTileHeight;
    double maxRadiance;
    double maxBounceRadiance;
    double totalBounceRadiance;
    double temporalTotalTileMs;
    double temporalMaxTileMs;
    double temporalAverageTileMs;
    double temporalMaxTileSubpassMs;
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

typedef void (*RuntimeNative3DTemporalProgressCallback)(int started_subpasses,
                                                        int completed_subpasses,
                                                        int total_subpasses,
                                                        void* user_data);
typedef void (*RuntimeNative3DTemporalTileProgressCallback)(
    int started_subpasses,
    int completed_subpasses,
    int total_subpasses,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    void* user_data);

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src);
void RuntimeNative3DRender_ResetInspectionCameraOverrides(void);
void RuntimeNative3DRender_SetInspectionCameraPosition(Vec3 position);
void RuntimeNative3DRender_SetInspectionCameraLookAt(Vec3 target);
const char* RuntimeNative3DPrepareFrameLastDiagnostics(void);
bool RuntimeNative3DPrepareFrame(RuntimeNative3DPreparedFrame* out_frame,
                                 int width,
                                 int height,
                                 double normalized_t,
                                 double live_light_x,
                                 double live_light_y);
bool RuntimeNative3DPrepareFrameAtFrameIndex(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             int frame_index,
                                             double live_light_x,
                                             double live_light_y);
bool RuntimeNative3DPrepareFrameWithSampling(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             double live_light_x,
                                             double live_light_y,
                                             const RuntimeNative3DSamplingContext* sampling);
bool RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(
    RuntimeNative3DPreparedFrame* out_frame,
    int width,
    int height,
    double normalized_t,
    int frame_index,
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
bool RuntimeNative3DRenderPreparedRegionRadianceRGB(float* radiance_buffer,
                                                    int radiance_stride,
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
void RuntimeNative3DResolveRadianceRegionToPixels(uint8_t* pixel_buffer,
                                                  int pixel_width,
                                                  const float* radiance_buffer,
                                                  int radiance_stride,
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
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgress(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTemporalTileProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
uint8_t RuntimeNative3DResolveEnvironmentByte(void);
void RuntimeNative3DFillPixelBufferEnvironment(uint8_t* pixel_buffer, size_t pixel_count);

#endif
