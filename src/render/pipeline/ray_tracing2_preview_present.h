#ifndef RAY_TRACING2_PREVIEW_PRESENT_H
#define RAY_TRACING2_PREVIEW_PRESENT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "render/integrators/integrator_common.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/runtime_native_3d_render.h"

void RayTracing2PreviewPresent_DrawLuminanceBuffer(SDL_Renderer* renderer,
                                                   const Uint8* buffer,
                                                   int width,
                                                   int height);
void RayTracing2PreviewPresent_DrawABGRBuffer(SDL_Renderer* renderer,
                                              const Uint8* buffer,
                                              int width,
                                              int height);
void RayTracing2PreviewPresent_DrawABGRBufferToRect(SDL_Renderer* renderer,
                                                    const Uint8* buffer,
                                                    int width,
                                                    int height,
                                                    SDL_Rect dst_rect);
void RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(SDL_Renderer* renderer,
                                                            const Uint8* buffer,
                                                            int width,
                                                            int height,
                                                            SDL_Rect dst_rect,
                                                            bool linear_filter);
void RayTracing2PreviewPresent_DimCopyABGR(const Uint8* src,
                                           Uint8* dst,
                                           size_t pixel_count,
                                           unsigned int numerator,
                                           unsigned int denominator);
bool RayTracing2PreviewPresent_LoadNative3DPreviewHistoryFromBMP(const char* path);
bool RayTracing2PreviewPresent_RenderNative3DTilesPreview(
    SDL_Renderer* renderer,
    Uint8* host_buffer,
    int host_width,
    int host_height,
    Uint8* render_buffer,
    int render_width,
    int render_height,
    TileGrid* grid,
    RayTracing3DIntegratorId integrator_id,
    double normalized_t,
    double light_x,
    double light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    bool disney_denoise_enabled,
    bool present_progress,
    RuntimeNative3DRenderStats* out_stats);
void RayTracing2PreviewPresent_RenderHybridTilesPreview(
    SDL_Renderer* renderer,
    IntegratorContext* ctx,
    const LightSource* light,
    const CameraIntegratorSettings* settings,
    double cam_x,
    double cam_y,
    Uint8* preview_buffer);

#endif
