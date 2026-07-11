#include "render/pipeline/ray_tracing2_preview_present_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Uint8* g_native3d_preview_history_buffer = NULL;
static size_t g_native3d_preview_history_capacity = 0u;
static int g_native3d_preview_history_width = 0;
static int g_native3d_preview_history_height = 0;
static bool g_native3d_preview_history_valid = false;
#define NATIVE3D_PREVIEW_HISTORY_DIM_NUMERATOR 1u
#define NATIVE3D_PREVIEW_HISTORY_DIM_DENOMINATOR 4u

uint64_t Native3DHostMovementBytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0u;
    return (uint64_t)width *
           (uint64_t)height *
           (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
}

void RayTracing2PreviewPresent_DimCopyABGR(const Uint8* src,
                                           Uint8* dst,
                                           size_t pixel_count,
                                           unsigned int numerator,
                                           unsigned int denominator) {
    if (!src || !dst || pixel_count == 0u || denominator == 0u) {
        return;
    }

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        dst[base] = (Uint8)(((unsigned int)src[base] * numerator) / denominator);
        dst[base + 1u] = (Uint8)(((unsigned int)src[base + 1u] * numerator) / denominator);
        dst[base + 2u] = (Uint8)(((unsigned int)src[base + 2u] * numerator) / denominator);
        dst[base + 3u] = 0xFFu;
    }
}

static void InvalidateNative3DPreviewHistory(void) {
    g_native3d_preview_history_valid = false;
}

static bool EnsureNative3DPreviewHistoryBuffer(size_t host_pixel_count,
                                               int host_width,
                                               int host_height) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;

    if (host_pixel_count == 0u || host_width <= 0 || host_height <= 0) {
        InvalidateNative3DPreviewHistory();
        return false;
    }

    if (g_native3d_preview_history_width != host_width ||
        g_native3d_preview_history_height != host_height) {
        InvalidateNative3DPreviewHistory();
    }

    if (g_native3d_preview_history_buffer &&
        g_native3d_preview_history_capacity >= host_pixel_count &&
        g_native3d_preview_history_width == host_width &&
        g_native3d_preview_history_height == host_height) {
        return true;
    }

    byte_count = host_pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(g_native3d_preview_history_buffer, byte_count);
    if (!resized) {
        InvalidateNative3DPreviewHistory();
        return false;
    }
    g_native3d_preview_history_buffer = resized;
    g_native3d_preview_history_capacity = host_pixel_count;
    g_native3d_preview_history_width = host_width;
    g_native3d_preview_history_height = host_height;
    return true;
}

void SeedNative3DPreviewHistoryUnderlay(Uint8* host_buffer,
                                        size_t host_pixel_count,
                                        int host_width,
                                        int host_height,
                                        const RuntimeScene3D* scene,
                                        const RuntimeCameraProjector3D* projector,
                                        RuntimeNative3DRenderStats* stats) {
    if (!host_buffer || host_pixel_count == 0u || host_width <= 0 || host_height <= 0) {
        return;
    }

    if (!g_native3d_preview_history_valid ||
        !g_native3d_preview_history_buffer ||
        g_native3d_preview_history_width != host_width ||
        g_native3d_preview_history_height != host_height) {
        RuntimeNative3DFillPixelBufferBackground(host_buffer,
                                                 host_width,
                                                 host_height,
                                                 scene,
                                                 projector);
        if (stats) {
            stats->temporalHistorySeedHostBytes +=
                Native3DHostMovementBytes(host_width, host_height);
        }
        RecordNative3DPresentationEvent(RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_SEED,
                                        false,
                                        0,
                                        0,
                                        host_width,
                                        host_height,
                                        0,
                                        NULL,
                                        NULL,
                                        false,
                                        true);
        return;
    }

    RayTracing2PreviewPresent_DimCopyABGR(g_native3d_preview_history_buffer,
                                          host_buffer,
                                          host_pixel_count,
                                          NATIVE3D_PREVIEW_HISTORY_DIM_NUMERATOR,
                                          NATIVE3D_PREVIEW_HISTORY_DIM_DENOMINATOR);
    if (stats) {
        stats->temporalHistorySeedHostBytes +=
            Native3DHostMovementBytes(host_width, host_height);
    }
    RecordNative3DPresentationEvent(RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_SEED,
                                    false,
                                    0,
                                    0,
                                    host_width,
                                    host_height,
                                    0,
                                    NULL,
                                    NULL,
                                    false,
                                    true);
}

bool PromoteNative3DPreviewHistory(const Uint8* host_buffer,
                                   size_t host_pixel_count,
                                   int host_width,
                                   int host_height,
                                   RuntimeNative3DRenderStats* stats) {
    const size_t byte_count =
        host_pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    if (!host_buffer || host_pixel_count == 0u || host_width <= 0 || host_height <= 0) {
        InvalidateNative3DPreviewHistory();
        return false;
    }
    if (!EnsureNative3DPreviewHistoryBuffer(host_pixel_count, host_width, host_height)) {
        return false;
    }
    memcpy(g_native3d_preview_history_buffer, host_buffer, byte_count);
    g_native3d_preview_history_valid = true;
    if (stats) {
        stats->temporalHistoryPromoteCount += 1;
        stats->temporalHistoryPromoteHostBytes += byte_count;
    }
    RecordNative3DPresentationEvent(RAY_TRACING2_NATIVE3D_PRESENT_EVENT_HISTORY_PROMOTE,
                                    false,
                                    0,
                                    0,
                                    host_width,
                                    host_height,
                                    0,
                                    NULL,
                                    NULL,
                                    false,
                                    true);
    return true;
}

bool RayTracing2PreviewPresent_LoadNative3DPreviewHistoryFromBMP(const char* path) {
    SDL_Surface* loaded = NULL;
    SDL_Surface* converted = NULL;

    if (!path || !path[0]) {
        InvalidateNative3DPreviewHistory();
        return false;
    }

    loaded = SDL_LoadBMP(path);
    if (!loaded) {
        InvalidateNative3DPreviewHistory();
        return false;
    }
    converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(loaded);
    loaded = NULL;
    if (!converted) {
        InvalidateNative3DPreviewHistory();
        return false;
    }
    if (!EnsureNative3DPreviewHistoryBuffer((size_t)converted->w * (size_t)converted->h,
                                            converted->w,
                                            converted->h)) {
        SDL_FreeSurface(converted);
        return false;
    }

    for (int y = 0; y < converted->h; ++y) {
        const uint32_t* row =
            (const uint32_t*)((const uint8_t*)converted->pixels + ((size_t)y * (size_t)converted->pitch));
        for (int x = 0; x < converted->w; ++x) {
            Uint8 r = 0u;
            Uint8 g = 0u;
            Uint8 b = 0u;
            Uint8 a = 0u;
            const size_t base =
                ((size_t)y * (size_t)converted->w + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            SDL_GetRGBA(row[x], converted->format, &r, &g, &b, &a);
            g_native3d_preview_history_buffer[base] = b;
            g_native3d_preview_history_buffer[base + 1u] = g;
            g_native3d_preview_history_buffer[base + 2u] = r;
            g_native3d_preview_history_buffer[base + 3u] = a;
        }
    }

    SDL_FreeSurface(converted);
    g_native3d_preview_history_valid = true;
    return true;
}
