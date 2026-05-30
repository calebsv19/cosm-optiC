#ifndef RAY_TRACING2_NATIVE3D_OVERLAY_H
#define RAY_TRACING2_NATIVE3D_OVERLAY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

bool RayTracing2Native3DOverlay_ExportFrameBMP(const char* filename,
                                               int width,
                                               int height,
                                               const Uint8* native3d_preview_buffer,
                                               const Uint8* luminance_buffer);
const char* RayTracing2Native3DOverlay_ResolveUpscaleModeLabel(int upscale_mode);

#endif
