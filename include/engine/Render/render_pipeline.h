#ifndef ENGINE_RENDER_PIPELINE_H
#define ENGINE_RENDER_PIPELINE_H

#include "engine/Render/renderer_backend.h"
#include <stdbool.h>

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int width;
    int height;
    int logical_width;
    int logical_height;
    float dpi_scale_x;
    float dpi_scale_y;
#if USE_VULKAN
    VkCommandBuffer command_buffer;
    VkFramebuffer framebuffer;
    VkExtent2D extent;
#endif
} RenderContext;

RenderContext* getRenderContext(void);
void setRenderContext(SDL_Renderer* renderer,
                      SDL_Window* window,
                      int width,
                      int height);
void render_set_clear_color(SDL_Renderer* renderer,
                            Uint8 r,
                            Uint8 g,
                            Uint8 b,
                            Uint8 a);
bool render_begin_frame(void);
bool render_end_frame(void);
bool render_device_lost(void);

#endif
