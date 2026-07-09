#ifndef RAY_TRACING_TEXT_UPLOAD_POLICY_H
#define RAY_TRACING_TEXT_UPLOAD_POLICY_H

#include <SDL2/SDL.h>

#include "vk_renderer.h"

float ray_tracing_text_raster_scale(SDL_Renderer* renderer);
VkFilter ray_tracing_text_upload_filter(SDL_Renderer* renderer);
int ray_tracing_text_raster_point_size(SDL_Renderer* renderer,
                                       int base_point_size,
                                       int min_point_size);
int ray_tracing_text_logical_pixels(SDL_Renderer* renderer, int raster_pixels);

#endif
