#ifndef RAY_TRACING2_PREVIEW_H
#define RAY_TRACING2_PREVIEW_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

void RayTracingPreview_ApplySeparableBlur(Uint8* buffer, int width, int height, int radius);
void RayTracingPreview_CopyLuminanceRectToABGR(uint32_t* dstPixels,
                                               int width,
                                               int height,
                                               const Uint8* srcLuminance,
                                               const SDL_Rect* dirtyRect);
bool RayTracingPreview_ResetNative3DDirtyRect(SDL_Renderer* renderer, int width, int height);
bool RayTracingPreview_UpdateNative3DDirtyRect(SDL_Renderer* renderer,
                                               const Uint8* previewBuffer,
                                               int width,
                                               int height,
                                               const SDL_Rect* dirtyRect);
bool RayTracingPreview_UpdateNative3DDirtyRectABGR(SDL_Renderer* renderer,
                                                   const Uint8* previewBuffer,
                                                   int width,
                                                   int height,
                                                   const SDL_Rect* dirtyRect);
bool RayTracingPreview_DrawNative3DDirtyRect(SDL_Renderer* renderer, int width, int height);
bool RayTracingPreview_DrawNative3DPreviewBase(SDL_Renderer* renderer,
                                               const Uint8* previewBuffer,
                                               int width,
                                               int height,
                                               const SDL_Rect* dirtyRect,
                                               bool resetDirtyPreview);
bool RayTracingPreview_DrawNative3DPreviewBaseABGR(SDL_Renderer* renderer,
                                                   const Uint8* previewBuffer,
                                                   int width,
                                                   int height,
                                                   const SDL_Rect* dirtyRect,
                                                   bool resetDirtyPreview);
void RayTracingPreview_ApplySeparableBlurABGR(Uint8* buffer, int width, int height, int radius);
void RayTracingPreview_ShutdownNative3DDirtyRect(void);

#endif
