#include "engine/Render/render_pipeline.h"

#include <stdio.h>

#if USE_VULKAN
#include <SDL2/SDL_vulkan.h>
#include "render/vk_shared_device.h"
#include "vk_renderer.h"
#endif

static RenderContext globalContext = {0};
static bool s_device_lost = false;
#if USE_VULKAN
static int s_logged_extent_mismatch = 0;
static int s_logged_begin_out_of_date = 0;
static int s_logged_begin_failure = 0;
static int s_logged_end_failure = 0;
static int s_logged_device_lost = 0;
#endif

RenderContext* getRenderContext(void) {
    if (!globalContext.renderer) {
        return NULL;
    }
    return &globalContext;
}

void setRenderContext(SDL_Renderer* renderer,
                      SDL_Window* window,
                      int width,
                      int height) {
    globalContext.renderer = renderer;
    globalContext.window = window;
    globalContext.width = width;
    globalContext.height = height;
}

void render_set_clear_color(SDL_Renderer* renderer,
                            Uint8 r,
                            Uint8 g,
                            Uint8 b,
                            Uint8 a) {
    if (!renderer) return;
#if USE_VULKAN
    VkRenderer* vk_renderer = (VkRenderer*)renderer;
    vk_renderer->config.clear_color[0] = r / 255.0f;
    vk_renderer->config.clear_color[1] = g / 255.0f;
    vk_renderer->config.clear_color[2] = b / 255.0f;
    vk_renderer->config.clear_color[3] = a / 255.0f;
#else
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
#endif
}

bool render_begin_frame(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer || !ctx->window) return false;

    int winW = 0;
    int winH = 0;
    int drawableW = 0;
    int drawableH = 0;
    SDL_GetWindowSize(ctx->window, &winW, &winH);
    drawableW = winW;
    drawableH = winH;

#if USE_VULKAN
    SDL_Vulkan_GetDrawableSize(ctx->window, &drawableW, &drawableH);
    vk_renderer_set_logical_size((VkRenderer*)ctx->renderer, (float)winW, (float)winH);

    VkRenderer* vk_renderer = (VkRenderer*)ctx->renderer;
    if (vk_renderer && drawableW > 0 && drawableH > 0) {
        VkExtent2D swapExtent = vk_renderer->context.swapchain.extent;
        if (swapExtent.width != (uint32_t)drawableW ||
            swapExtent.height != (uint32_t)drawableH) {
            if (!s_logged_extent_mismatch) {
                fprintf(stderr,
                        "[Render] Drawable size (%dx%d) differs from swapchain extent (%ux%u); "
                        "forcing swapchain recreate.\n",
                        drawableW, drawableH,
                        swapExtent.width, swapExtent.height);
                s_logged_extent_mismatch = 1;
            }
            vk_renderer_recreate_swapchain(vk_renderer, ctx->window);
            s_logged_begin_out_of_date = 0;
            s_logged_begin_failure = 0;
            s_logged_end_failure = 0;
            return false;
        }
        s_logged_extent_mismatch = 0;
    }
#else
    SDL_GL_GetDrawableSize(ctx->window, &drawableW, &drawableH);
    float scaleX = (float)drawableW / (float)winW;
    float scaleY = (float)drawableH / (float)winH;
    SDL_RenderSetScale(ctx->renderer, scaleX, scaleY);
#endif

    ctx->width = drawableW;
    ctx->height = drawableH;

#if USE_VULKAN
    VkResult frameResult = vk_renderer_begin_frame((VkRenderer*)ctx->renderer,
                                                   &ctx->command_buffer,
                                                   &ctx->framebuffer,
                                                   &ctx->extent);
    if (frameResult == VK_ERROR_OUT_OF_DATE_KHR ||
        frameResult == VK_SUBOPTIMAL_KHR) {
        if (!s_logged_begin_out_of_date) {
            fprintf(stderr,
                    "[Render] vk_renderer_begin_frame reported %s (win=%dx%d drawable=%dx%d); "
                    "triggering swapchain recreate.\n",
                    (frameResult == VK_SUBOPTIMAL_KHR) ? "SUBOPTIMAL" : "OUT_OF_DATE",
                    winW, winH, drawableW, drawableH);
            s_logged_begin_out_of_date = 1;
        }
        vk_renderer_recreate_swapchain((VkRenderer*)ctx->renderer, ctx->window);
        s_logged_begin_failure = 0;
        s_logged_end_failure = 0;
        return false;
    } else if (frameResult == VK_ERROR_DEVICE_LOST) {
        if (!s_logged_device_lost) {
            fprintf(stderr, "[Render] vk_renderer_begin_frame failed: device lost.\n");
            s_logged_device_lost = 1;
        }
        s_device_lost = true;
        vk_shared_device_mark_lost();
        return false;
    } else if (frameResult != VK_SUCCESS) {
        if (!s_logged_begin_failure) {
            fprintf(stderr, "[Render] vk_renderer_begin_frame failed: %d\n", frameResult);
            s_logged_begin_failure = 1;
        }
        return false;
    }
    s_logged_begin_out_of_date = 0;
    s_logged_begin_failure = 0;
#endif

    return true;
}

void render_end_frame(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
#if USE_VULKAN
    if (s_device_lost) return;
    VkResult endResult =
        vk_renderer_end_frame((VkRenderer*)ctx->renderer, ctx->command_buffer);
    if (endResult == VK_ERROR_OUT_OF_DATE_KHR || endResult == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain((VkRenderer*)ctx->renderer, ctx->window);
        s_logged_end_failure = 0;
    } else if (endResult == VK_ERROR_DEVICE_LOST) {
        if (!s_logged_device_lost) {
            fprintf(stderr, "[Render] vk_renderer_end_frame failed: device lost.\n");
            s_logged_device_lost = 1;
        }
        s_device_lost = true;
        vk_shared_device_mark_lost();
    } else if (endResult != VK_SUCCESS) {
        if (!s_logged_end_failure) {
            fprintf(stderr, "[Render] vk_renderer_end_frame failed: %d\n", endResult);
            s_logged_end_failure = 1;
        }
    } else {
        s_logged_end_failure = 0;
    }
#else
    SDL_RenderPresent(ctx->renderer);
#endif
}

bool render_device_lost(void) {
    return s_device_lost;
}
