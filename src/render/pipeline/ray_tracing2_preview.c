#include "render/ray_tracing2_preview.h"

#include "engine/Render/build_config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/integrator_common.h"
#include "render/runtime_native_3d_render.h"

#if USE_VULKAN
#include "vk_renderer.h"

typedef struct {
    uint32_t* pixels;
    size_t pixelCount;
    int width;
    int height;
    SDL_Renderer* ownerRenderer;
    VkDevice deviceHandle;
    VkDescriptorPool descriptorPool;
    VkRendererTexture texture;
    bool textureValid;
} Native3DDirtyRectCache;

static Native3DDirtyRectCache s_native3DDirtyRectCache = {0};

static uint32_t PackLuminanceToABGR(Uint8 value) {
    return ((uint32_t)0xFFu << 24) |
           ((uint32_t)value << 16) |
           ((uint32_t)value << 8) |
           (uint32_t)value;
}

static void CopyABGRRectToABGR(uint32_t* dstPixels,
                               int width,
                               int height,
                               const Uint8* srcABGR,
                               const SDL_Rect* dirtyRect) {
    SDL_Rect rect = {0, 0, width, height};
    if (!dstPixels || !srcABGR || width <= 0 || height <= 0) return;
    if (dirtyRect) {
        rect = *dirtyRect;
        if (rect.x < 0 || rect.y < 0 ||
            rect.w <= 0 || rect.h <= 0 ||
            rect.x + rect.w > width ||
            rect.y + rect.h > height) {
            return;
        }
    }

    for (int row = 0; row < rect.h; ++row) {
        int py = rect.y + row;
        size_t srcBase =
            ((size_t)py * (size_t)width + (size_t)rect.x) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        size_t dstBase = (size_t)py * (size_t)width + (size_t)rect.x;
        for (int col = 0; col < rect.w; ++col) {
            size_t srcIndex = srcBase + (size_t)col * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            dstPixels[dstBase + (size_t)col] =
                ((uint32_t)srcABGR[srcIndex + 3u] << 24) |
                ((uint32_t)srcABGR[srcIndex + 2u] << 16) |
                ((uint32_t)srcABGR[srcIndex + 1u] << 8) |
                (uint32_t)srcABGR[srcIndex];
        }
    }
}

void RayTracingPreview_CopyLuminanceRectToABGR(uint32_t* dstPixels,
                                               int width,
                                               int height,
                                               const Uint8* srcLuminance,
                                               const SDL_Rect* dirtyRect) {
    SDL_Rect rect = {0, 0, width, height};
    if (!dstPixels || !srcLuminance || width <= 0 || height <= 0) return;
    if (dirtyRect) {
        rect = *dirtyRect;
        if (rect.x < 0 || rect.y < 0 ||
            rect.w <= 0 || rect.h <= 0 ||
            rect.x + rect.w > width ||
            rect.y + rect.h > height) {
            return;
        }
    }

    for (int row = 0; row < rect.h; ++row) {
        int py = rect.y + row;
        size_t srcBase = (size_t)py * (size_t)width + (size_t)rect.x;
        size_t dstBase = (size_t)py * (size_t)width + (size_t)rect.x;
        for (int col = 0; col < rect.w; ++col) {
            dstPixels[dstBase + (size_t)col] =
                PackLuminanceToABGR(srcLuminance[srcBase + (size_t)col]);
        }
    }
}

static void Native3DDirtyRectCache_Destroy(void) {
    if (s_native3DDirtyRectCache.textureValid && s_native3DDirtyRectCache.ownerRenderer) {
        vk_renderer_texture_destroy((VkRenderer*)s_native3DDirtyRectCache.ownerRenderer,
                                    &s_native3DDirtyRectCache.texture);
    }
    free(s_native3DDirtyRectCache.pixels);
    memset(&s_native3DDirtyRectCache, 0, sizeof(s_native3DDirtyRectCache));
}

static bool Native3DDirtyRectCache_Matches(SDL_Renderer* renderer, int width, int height) {
    VkRenderer* vk = (VkRenderer*)renderer;
    if (!renderer || !vk->context.device) return false;
    if (!s_native3DDirtyRectCache.textureValid || !s_native3DDirtyRectCache.pixels) return false;
    return s_native3DDirtyRectCache.ownerRenderer == renderer &&
           s_native3DDirtyRectCache.width == width &&
           s_native3DDirtyRectCache.height == height &&
           s_native3DDirtyRectCache.deviceHandle == vk->context.device->device &&
           s_native3DDirtyRectCache.descriptorPool == vk->descriptor_pool;
}

static bool Native3DDirtyRectCache_Ensure(SDL_Renderer* renderer, int width, int height) {
    VkRenderer* vk = (VkRenderer*)renderer;
    size_t pixelCount = (size_t)width * (size_t)height;

    if (!renderer || width <= 0 || height <= 0 || !vk->context.device) return false;
    if (Native3DDirtyRectCache_Matches(renderer, width, height)) return true;

    Native3DDirtyRectCache_Destroy();

    s_native3DDirtyRectCache.pixels =
        (uint32_t*)malloc(pixelCount * sizeof(uint32_t));
    if (!s_native3DDirtyRectCache.pixels) {
        return false;
    }
    s_native3DDirtyRectCache.pixelCount = pixelCount;
    s_native3DDirtyRectCache.width = width;
    s_native3DDirtyRectCache.height = height;
    s_native3DDirtyRectCache.ownerRenderer = renderer;
    s_native3DDirtyRectCache.deviceHandle = vk->context.device->device;
    s_native3DDirtyRectCache.descriptorPool = vk->descriptor_pool;

    for (size_t i = 0; i < pixelCount; ++i) {
        s_native3DDirtyRectCache.pixels[i] = PackLuminanceToABGR(0);
    }

    if (vk_renderer_texture_create_from_rgba(
            vk,
            s_native3DDirtyRectCache.pixels,
            (uint32_t)width,
            (uint32_t)height,
            VK_FILTER_NEAREST,
            &s_native3DDirtyRectCache.texture) != VK_SUCCESS) {
        Native3DDirtyRectCache_Destroy();
        return false;
    }

    s_native3DDirtyRectCache.textureValid = true;
    return true;
}
#endif

static void BuildGaussianKernel(float* kernel, int radius) {
    float sigma = (float)radius * 0.5f + 0.5f;
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        float value = expf(-(i * i) / (2.0f * sigma * sigma));
        kernel[i + radius] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (int i = 0; i < (2 * radius + 1); i++) {
            kernel[i] /= sum;
        }
    }
}

void RayTracingPreview_ApplySeparableBlur(Uint8* buffer, int width, int height, int radius) {
    if (radius <= 0 || !buffer) return;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc((size_t)kernelSize * sizeof(float));
    if (!kernel) return;
    BuildGaussianKernel(kernel, radius);

    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    float* output = (float*)malloc(total * sizeof(float));
    if (!temp || !output) {
        free(kernel);
        free(temp);
        free(output);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sx = x + k;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                accum += kernel[k + radius] * buffer[y * width + sx];
            }
            temp[y * width + x] = accum;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sy = y + k;
                if (sy < 0) sy = 0;
                if (sy >= height) sy = height - 1;
                accum += kernel[k + radius] * temp[sy * width + x];
            }
            output[y * width + x] = accum;
        }
    }

    for (size_t i = 0; i < total; i++) {
        buffer[i] = (Uint8)Clamp(output[i], 0, 255);
    }

    free(kernel);
    free(temp);
    free(output);
}

void RayTracingPreview_ApplySeparableBlurABGR(Uint8* buffer, int width, int height, int radius) {
    if (radius <= 0 || !buffer) return;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc((size_t)kernelSize * sizeof(float));
    if (!kernel) return;
    BuildGaussianKernel(kernel, radius);

    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES * sizeof(float));
    float* output = (float*)malloc(total * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES * sizeof(float));
    if (!temp || !output) {
        free(kernel);
        free(temp);
        free(output);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int channel = 0; channel < 3; ++channel) {
                float accum = 0.0f;
                for (int k = -radius; k <= radius; k++) {
                    int sx = x + k;
                    if (sx < 0) sx = 0;
                    if (sx >= width) sx = width - 1;
                    size_t srcIndex =
                        ((size_t)y * (size_t)width + (size_t)sx) *
                            RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
                        (size_t)channel;
                    accum += kernel[k + radius] * buffer[srcIndex];
                }
                temp[(((size_t)y * (size_t)width + (size_t)x) *
                      RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES) +
                     (size_t)channel] = accum;
            }
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int channel = 0; channel < 3; ++channel) {
                float accum = 0.0f;
                for (int k = -radius; k <= radius; k++) {
                    int sy = y + k;
                    if (sy < 0) sy = 0;
                    if (sy >= height) sy = height - 1;
                    size_t srcIndex =
                        ((size_t)sy * (size_t)width + (size_t)x) *
                            RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
                        (size_t)channel;
                    accum += kernel[k + radius] * temp[srcIndex];
                }
                output[(((size_t)y * (size_t)width + (size_t)x) *
                        RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES) +
                       (size_t)channel] = accum;
            }
        }
    }

    for (size_t i = 0; i < total; ++i) {
        const size_t base = i * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        buffer[base] = (Uint8)Clamp(output[base], 0, 255);
        buffer[base + 1u] = (Uint8)Clamp(output[base + 1u], 0, 255);
        buffer[base + 2u] = (Uint8)Clamp(output[base + 2u], 0, 255);
        buffer[base + 3u] = 0xFFu;
    }

    free(kernel);
    free(temp);
    free(output);
}

bool RayTracingPreview_ResetNative3DDirtyRect(SDL_Renderer* renderer, int width, int height) {
#if USE_VULKAN
    size_t byteStride = (size_t)width * sizeof(uint32_t);

    if (!Native3DDirtyRectCache_Ensure(renderer, width, height)) {
        return false;
    }
    for (size_t i = 0; i < s_native3DDirtyRectCache.pixelCount; ++i) {
        s_native3DDirtyRectCache.pixels[i] = PackLuminanceToABGR(0);
    }

    return vk_renderer_texture_update_rgba_subrect(
               (VkRenderer*)renderer,
               &s_native3DDirtyRectCache.texture,
               s_native3DDirtyRectCache.pixels,
               byteStride,
               0,
               0,
               (uint32_t)width,
               (uint32_t)height) == VK_SUCCESS;
#else
    (void)renderer;
    (void)width;
    (void)height;
    return true;
#endif
}

bool RayTracingPreview_UpdateNative3DDirtyRect(SDL_Renderer* renderer,
                                               const Uint8* previewBuffer,
                                               int width,
                                               int height,
                                               const SDL_Rect* dirtyRect) {
#if USE_VULKAN
    SDL_Rect fullRect = {0, 0, width, height};
    const SDL_Rect* uploadRect = dirtyRect ? dirtyRect : &fullRect;

    if (!previewBuffer || uploadRect->w <= 0 || uploadRect->h <= 0) {
        return false;
    }
    if (!Native3DDirtyRectCache_Ensure(renderer, width, height)) {
        return false;
    }
    if (uploadRect->x < 0 || uploadRect->y < 0 ||
        uploadRect->x + uploadRect->w > width ||
        uploadRect->y + uploadRect->h > height) {
        return false;
    }

    RayTracingPreview_CopyLuminanceRectToABGR(s_native3DDirtyRectCache.pixels,
                                              width,
                                              height,
                                              previewBuffer,
                                              uploadRect);

    return vk_renderer_texture_update_rgba_subrect(
               (VkRenderer*)renderer,
               &s_native3DDirtyRectCache.texture,
               s_native3DDirtyRectCache.pixels +
                   ((size_t)uploadRect->y * (size_t)width + (size_t)uploadRect->x),
               (size_t)width * sizeof(uint32_t),
               (uint32_t)uploadRect->x,
               (uint32_t)uploadRect->y,
               (uint32_t)uploadRect->w,
               (uint32_t)uploadRect->h) == VK_SUCCESS;
#else
    (void)renderer;
    (void)previewBuffer;
    (void)width;
    (void)height;
    (void)dirtyRect;
    return true;
#endif
}

bool RayTracingPreview_UpdateNative3DDirtyRectABGR(SDL_Renderer* renderer,
                                                   const Uint8* previewBuffer,
                                                   int width,
                                                   int height,
                                                   const SDL_Rect* dirtyRect) {
#if USE_VULKAN
    SDL_Rect fullRect = {0, 0, width, height};
    const SDL_Rect* uploadRect = dirtyRect ? dirtyRect : &fullRect;

    if (!previewBuffer || uploadRect->w <= 0 || uploadRect->h <= 0) {
        return false;
    }
    if (!Native3DDirtyRectCache_Ensure(renderer, width, height)) {
        return false;
    }
    if (uploadRect->x < 0 || uploadRect->y < 0 ||
        uploadRect->x + uploadRect->w > width ||
        uploadRect->y + uploadRect->h > height) {
        return false;
    }

    CopyABGRRectToABGR(s_native3DDirtyRectCache.pixels,
                       width,
                       height,
                       previewBuffer,
                       uploadRect);

    return vk_renderer_texture_update_rgba_subrect(
               (VkRenderer*)renderer,
               &s_native3DDirtyRectCache.texture,
               s_native3DDirtyRectCache.pixels +
                   ((size_t)uploadRect->y * (size_t)width + (size_t)uploadRect->x),
               (size_t)width * sizeof(uint32_t),
               (uint32_t)uploadRect->x,
               (uint32_t)uploadRect->y,
               (uint32_t)uploadRect->w,
               (uint32_t)uploadRect->h) == VK_SUCCESS;
#else
    (void)renderer;
    (void)previewBuffer;
    (void)width;
    (void)height;
    (void)dirtyRect;
    return true;
#endif
}

bool RayTracingPreview_DrawNative3DDirtyRect(SDL_Renderer* renderer, int width, int height) {
#if USE_VULKAN
    SDL_Rect dstRect = {0, 0, width, height};

    if (!renderer || !Native3DDirtyRectCache_Matches(renderer, width, height)) {
        return false;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer,
                             &s_native3DDirtyRectCache.texture,
                             NULL,
                             &dstRect);
    return true;
#else
    (void)renderer;
    (void)width;
    (void)height;
    return false;
#endif
}

bool RayTracingPreview_DrawNative3DPreviewBase(SDL_Renderer* renderer,
                                               const Uint8* previewBuffer,
                                               int width,
                                               int height,
                                               const SDL_Rect* dirtyRect,
                                               bool resetDirtyPreview) {
    if (!renderer || !previewBuffer || width <= 0 || height <= 0) {
        return false;
    }

#if USE_VULKAN
    if (resetDirtyPreview) {
        if (!RayTracingPreview_ResetNative3DDirtyRect(renderer, width, height)) {
            return false;
        }
    } else if (dirtyRect) {
        if (!RayTracingPreview_UpdateNative3DDirtyRect(renderer,
                                                       previewBuffer,
                                                       width,
                                                       height,
                                                       dirtyRect)) {
            return false;
        }
    }
    return RayTracingPreview_DrawNative3DDirtyRect(renderer, width, height);
#else
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect bg = {0, 0, width, height};
    SDL_RenderFillRect(renderer, &bg);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Uint8 brightness = previewBuffer[(size_t)y * (size_t)width + (size_t)x];
            if (brightness == 0) continue;
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    return true;
#endif
}

bool RayTracingPreview_DrawNative3DPreviewBaseABGR(SDL_Renderer* renderer,
                                                   const Uint8* previewBuffer,
                                                   int width,
                                                   int height,
                                                   const SDL_Rect* dirtyRect,
                                                   bool resetDirtyPreview) {
#if USE_VULKAN
    bool ok = false;
    if (!renderer || !previewBuffer || width <= 0 || height <= 0) return false;
    if (resetDirtyPreview) {
        ok = RayTracingPreview_ResetNative3DDirtyRect(renderer, width, height);
        if (!ok) return false;
    }
    ok = RayTracingPreview_UpdateNative3DDirtyRectABGR(renderer,
                                                       previewBuffer,
                                                       width,
                                                       height,
                                                       dirtyRect);
    if (!ok) return false;
    return RayTracingPreview_DrawNative3DDirtyRect(renderer, width, height);
#else
    (void)renderer;
    (void)previewBuffer;
    (void)width;
    (void)height;
    (void)dirtyRect;
    (void)resetDirtyPreview;
    return true;
#endif
}

void RayTracingPreview_ShutdownNative3DDirtyRect(void) {
#if USE_VULKAN
    Native3DDirtyRectCache_Destroy();
#endif
}
