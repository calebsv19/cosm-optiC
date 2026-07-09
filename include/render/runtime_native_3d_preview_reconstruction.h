#ifndef RUNTIME_NATIVE_3D_PREVIEW_RECONSTRUCTION_H
#define RUNTIME_NATIVE_3D_PREVIEW_RECONSTRUCTION_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "config/config_manager.h"

bool RuntimeNative3DPreviewReconstructABGRWithMode(const uint8_t* render_buffer,
                                                   int render_width,
                                                   int render_height,
                                                   uint8_t* host_buffer,
                                                   int host_width,
                                                   int host_height,
                                                   Runtime3DUpscaleMode upscale_mode);
bool RuntimeNative3DPreviewResolveDirtyHostRect(int render_x,
                                                int render_y,
                                                int render_rect_width,
                                                int render_rect_height,
                                                int render_width,
                                                int render_height,
                                                int host_width,
                                                int host_height,
                                                SDL_Rect* out_host_rect);
bool RuntimeNative3DPreviewReconstructABGRRectWithMode(const uint8_t* render_buffer,
                                                       int render_width,
                                                       int render_height,
                                                       uint8_t* host_buffer,
                                                       int host_width,
                                                       int host_height,
                                                       const SDL_Rect* host_rect,
                                                       Runtime3DUpscaleMode upscale_mode);

#endif
