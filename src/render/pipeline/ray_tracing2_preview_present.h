#ifndef RAY_TRACING2_PREVIEW_PRESENT_H
#define RAY_TRACING2_PREVIEW_PRESENT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "render/integrators/integrator_common.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/runtime_native_3d_render.h"

typedef enum RayTracing2Native3DPresentationEventKind {
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_NONE = 0,
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_SEED = 1,
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_DIRTY_PROGRESS = 2,
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_RESOLVE = 3,
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_FINAL_PRESENT = 4,
    RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_PROMOTE = 5
} RayTracing2Native3DPresentationEventKind;

typedef struct RayTracing2Native3DPresentationEvent {
    RayTracing2Native3DPresentationEventKind kind;
    int sequence;
    int rendererAvailable;
    int renderWidth;
    int renderHeight;
    int hostWidth;
    int hostHeight;
    int dirtyTileCount;
    int startedSubpasses;
    int completedSubpasses;
    int totalSubpasses;
    int completedTilesInSubpass;
    int totalTilesInSubpass;
    int dirtyTileBoundsValid;
    int dirtyTileMinX;
    int dirtyTileMinY;
    int dirtyTileMaxX;
    int dirtyTileMaxY;
    int x;
    int y;
    int width;
    int height;
    int resetDirtyPreview;
    int success;
} RayTracing2Native3DPresentationEvent;

#define RAY_TRACING2_NATIVE3D_PRESENTATION_CAPTURE_MAX_EVENTS 128

typedef struct RayTracing2Native3DPresentationCapture {
    RayTracing2Native3DPresentationEvent events[RAY_TRACING2_NATIVE3D_PRESENTATION_CAPTURE_MAX_EVENTS];
    int eventCount;
    int droppedEventCount;
    int nextSequence;
    int historySeedCount;
    int dirtyProgressCount;
    int finalResolveCount;
    int finalPresentCount;
    int historyPromoteCount;
    int rendererPresentCount;
    int dirtyAfterFinalResolveCount;
    int finalResolveBeforeDirtyCount;
    int lastDirtyTileCount;
    SDL_Rect lastDirtyHostRect;
    SDL_Rect finalHostRect;
} RayTracing2Native3DPresentationCapture;

void RayTracing2PreviewPresent_ResetNative3DPresentationCapture(
    RayTracing2Native3DPresentationCapture* capture);
void RayTracing2PreviewPresent_SetNative3DPresentationCapture(
    RayTracing2Native3DPresentationCapture* capture);
const RayTracing2Native3DPresentationCapture*
RayTracing2PreviewPresent_GetNative3DPresentationCapture(void);

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
bool RayTracing2PreviewPresent_ReconstructNative3DHostTruth(
    const Uint8* render_buffer,
    int render_width,
    int render_height,
    Uint8* host_buffer,
    int host_width,
    int host_height,
    Runtime3DUpscaleMode upscale_mode,
    RuntimeNative3DRenderStats* stats);
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
