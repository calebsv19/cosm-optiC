#include "render/pipeline/ray_tracing2_preview_present.h"

#include <stdlib.h>
#include <string.h>

#include "engine/Render/render_pipeline.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/ray_tracing2_preview.h"
#include "render/runtime_native_3d_progress_hud.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_preview_reconstruction.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/timer_hud_api.h"

#if USE_VULKAN
#include "vk_renderer.h"
#endif

#if USE_VULKAN
static SDL_Surface* g_luma_surface = NULL;
static int g_luma_w = 0;
static int g_luma_h = 0;
static SDL_Surface* g_abgr_surface = NULL;
static int g_abgr_w = 0;
static int g_abgr_h = 0;

static SDL_Surface* get_luma_surface(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    if (!g_luma_surface || g_luma_w != width || g_luma_h != height) {
        if (g_luma_surface) {
            SDL_FreeSurface(g_luma_surface);
        }
        g_luma_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
        if (!g_luma_surface) {
            g_luma_w = 0;
            g_luma_h = 0;
            return NULL;
        }
        g_luma_w = width;
        g_luma_h = height;
    }
    return g_luma_surface;
}

static SDL_Surface* get_abgr_surface(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    if (!g_abgr_surface || g_abgr_w != width || g_abgr_h != height) {
        if (g_abgr_surface) {
            SDL_FreeSurface(g_abgr_surface);
        }
        g_abgr_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
        if (!g_abgr_surface) {
            g_abgr_w = 0;
            g_abgr_h = 0;
            return NULL;
        }
        g_abgr_w = width;
        g_abgr_h = height;
    }
    return g_abgr_surface;
}
#endif

static Uint8* g_native3d_preview_history_buffer = NULL;
static size_t g_native3d_preview_history_capacity = 0u;
static int g_native3d_preview_history_width = 0;
static int g_native3d_preview_history_height = 0;
static bool g_native3d_preview_history_valid = false;

#define NATIVE3D_PREVIEW_HISTORY_DIM_NUMERATOR 1u
#define NATIVE3D_PREVIEW_HISTORY_DIM_DENOMINATOR 4u

static SDL_Rect resolve_full_window_destination(SDL_Renderer* renderer,
                                                int fallback_width,
                                                int fallback_height) {
    SDL_Rect dst = {0, 0, fallback_width, fallback_height};
    RenderContext* ctx = getRenderContext();
    if (ctx && ctx->renderer == renderer && ctx->logical_width > 0 && ctx->logical_height > 0) {
        dst.w = ctx->logical_width;
        dst.h = ctx->logical_height;
    }
    return dst;
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

static void SeedNative3DPreviewHistoryUnderlay(Uint8* host_buffer,
                                               size_t host_pixel_count,
                                               int host_width,
                                               int host_height) {
    if (!host_buffer || host_pixel_count == 0u || host_width <= 0 || host_height <= 0) {
        return;
    }

    if (!g_native3d_preview_history_valid ||
        !g_native3d_preview_history_buffer ||
        g_native3d_preview_history_width != host_width ||
        g_native3d_preview_history_height != host_height) {
        RuntimeNative3DFillPixelBufferEnvironment(host_buffer, host_pixel_count);
        return;
    }

    RayTracing2PreviewPresent_DimCopyABGR(g_native3d_preview_history_buffer,
                                          host_buffer,
                                          host_pixel_count,
                                          NATIVE3D_PREVIEW_HISTORY_DIM_NUMERATOR,
                                          NATIVE3D_PREVIEW_HISTORY_DIM_DENOMINATOR);
}

static bool PromoteNative3DPreviewHistory(const Uint8* host_buffer,
                                          size_t host_pixel_count,
                                          int host_width,
                                          int host_height) {
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

static bool PresentNative3DTilePreviewFrame(SDL_Renderer* renderer,
                                            const IntegratorContext* preview_ctx,
                                            const IntegratorTile* dirty_tile,
                                            const Uint8* preview_buffer,
                                            bool reset_dirty_preview) {
    if (!renderer || !preview_ctx || !preview_buffer) return false;

    SDL_Rect dirty_rect = {0};
    SDL_Rect* dirty_rect_ptr = NULL;
    if (dirty_tile) {
        dirty_rect.x = dirty_tile->originX;
        dirty_rect.y = dirty_tile->originY;
        dirty_rect.w = dirty_tile->width;
        dirty_rect.h = dirty_tile->height;
        dirty_rect_ptr = &dirty_rect;
    }

    if (!RayTracingPreview_DrawNative3DPreviewBaseABGR(renderer,
                                                       preview_buffer,
                                                       preview_ctx->width,
                                                       preview_ctx->height,
                                                       dirty_rect_ptr,
                                                       reset_dirty_preview)) {
        return false;
    }
    ts_session_render(timer_hud_session());
    RuntimeNative3DProgressHUD_Draw(renderer);
    render_end_frame();
    if (render_device_lost()) {
        return false;
    }
    return render_begin_frame();
}

static bool PresentNative3DTilePreviewFrameTimed(SDL_Renderer* renderer,
                                                 const IntegratorContext* preview_ctx,
                                                 const IntegratorTile* dirty_tile,
                                                 const Uint8* preview_buffer,
                                                 bool reset_dirty_preview) {
    ts_session_stop_timer(timer_hud_session(), "Buffer Calc");
    ts_session_start_timer(timer_hud_session(), "Tile Preview Present");
    bool ok = PresentNative3DTilePreviewFrame(renderer,
                                              preview_ctx,
                                              dirty_tile,
                                              preview_buffer,
                                              reset_dirty_preview);
    ts_session_stop_timer(timer_hud_session(), "Tile Preview Present");
    ts_session_start_timer(timer_hud_session(), "Buffer Calc");
    return ok;
}

static bool ResolveNative3DHostDirtyTile(const IntegratorTile* render_tile,
                                         int render_width,
                                         int render_height,
                                         int host_width,
                                         int host_height,
                                         IntegratorTile* out_host_tile) {
    SDL_Rect host_rect = {0};

    if (!render_tile || !out_host_tile) {
        return false;
    }
    if (!RuntimeNative3DPreviewResolveDirtyHostRect(render_tile->originX,
                                                    render_tile->originY,
                                                    render_tile->width,
                                                    render_tile->height,
                                                    render_width,
                                                    render_height,
                                                    host_width,
                                                    host_height,
                                                    &host_rect)) {
        return false;
    }
    out_host_tile->originX = host_rect.x;
    out_host_tile->originY = host_rect.y;
    out_host_tile->width = host_rect.w;
    out_host_tile->height = host_rect.h;
    out_host_tile->energy = NULL;
    return true;
}

static bool ResolveNative3DHostDirtyTileUnion(const IntegratorTile* render_tiles,
                                              size_t render_tile_count,
                                              int render_width,
                                              int render_height,
                                              int host_width,
                                              int host_height,
                                              IntegratorTile* out_host_tile) {
    bool seeded = false;

    if (!render_tiles || render_tile_count == 0u || !out_host_tile) {
        return false;
    }

    for (size_t i = 0; i < render_tile_count; ++i) {
        IntegratorTile host_tile = {0};
        if (!ResolveNative3DHostDirtyTile(&render_tiles[i],
                                          render_width,
                                          render_height,
                                          host_width,
                                          host_height,
                                          &host_tile)) {
            continue;
        }
        if (!seeded) {
            *out_host_tile = host_tile;
            seeded = true;
            continue;
        }

        const int min_x = (host_tile.originX < out_host_tile->originX)
                              ? host_tile.originX
                              : out_host_tile->originX;
        const int min_y = (host_tile.originY < out_host_tile->originY)
                              ? host_tile.originY
                              : out_host_tile->originY;
        const int max_x = ((host_tile.originX + host_tile.width) >
                           (out_host_tile->originX + out_host_tile->width))
                              ? (host_tile.originX + host_tile.width)
                              : (out_host_tile->originX + out_host_tile->width);
        const int max_y = ((host_tile.originY + host_tile.height) >
                           (out_host_tile->originY + out_host_tile->height))
                              ? (host_tile.originY + host_tile.height)
                              : (out_host_tile->originY + out_host_tile->height);
        out_host_tile->originX = min_x;
        out_host_tile->originY = min_y;
        out_host_tile->width = max_x - min_x;
        out_host_tile->height = max_y - min_y;
        out_host_tile->energy = NULL;
    }

    return seeded;
}

static void ClearNative3DPreviewHistoryForKnownEmptyTiles(
    Uint8* host_buffer,
    int host_width,
    int host_height,
    const Uint8* render_buffer,
    int render_width,
    int render_height,
    const TileGrid* grid,
    const RuntimeNative3DPreparedFrame* frame) {
    if (!host_buffer || !render_buffer || !grid || !grid->tiles || grid->count == 0u || !frame) {
        return;
    }

    for (size_t i = 0; i < grid->count; ++i) {
        const IntegratorTile* render_tile = &grid->tiles[i];
        IntegratorTile host_tile = {0};
        SDL_Rect host_rect = {0};
        if (RuntimeNative3DPreparedRegionMayContainGeometry(frame,
                                                            render_tile->originX,
                                                            render_tile->originY,
                                                            render_tile->originX + render_tile->width,
                                                            render_tile->originY + render_tile->height)) {
            continue;
        }
        if (!ResolveNative3DHostDirtyTile(render_tile,
                                          render_width,
                                          render_height,
                                          host_width,
                                          host_height,
                                          &host_tile)) {
            continue;
        }
        host_rect.x = host_tile.originX;
        host_rect.y = host_tile.originY;
        host_rect.w = host_tile.width;
        host_rect.h = host_tile.height;
        if (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_OFF) {
            RuntimeNative3DUpscaleNearestABGRRect(render_buffer,
                                                  render_width,
                                                  render_height,
                                                  host_buffer,
                                                  host_width,
                                                  host_height,
                                                  host_rect.x,
                                                  host_rect.y,
                                                  host_rect.w,
                                                  host_rect.h);
        } else {
            (void)RuntimeNative3DPreviewReconstructABGRRectWithMode(
                render_buffer,
                render_width,
                render_height,
                host_buffer,
                host_width,
                host_height,
                &host_rect,
                (Runtime3DUpscaleMode)animSettings.upscaleMode3D);
        }
    }
}

typedef struct Native3DPreviewTileProgressContext {
    SDL_Renderer* renderer;
    Uint8* hostBuffer;
    int hostWidth;
    int hostHeight;
    const Uint8* renderBuffer;
    int renderWidth;
    int renderHeight;
    IntegratorContext previewCtx;
} Native3DPreviewTileProgressContext;

static void PresentNative3DPreviewTemporalProgress(int started_subpasses,
                                                   int completed_subpasses,
                                                   int total_subpasses,
                                                   void* user_data) {
    (void)user_data;
    RuntimeNative3DProgressHUD_UpdateTemporal(started_subpasses,
                                              completed_subpasses,
                                              total_subpasses);
}

static bool PresentNative3DPreviewTileProgress(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    Native3DPreviewTileProgressContext* ctx =
        (Native3DPreviewTileProgressContext*)user_data;
    IntegratorTile host_dirty_tile = {0};
    const IntegratorTile* present_tile = NULL;
    SDL_Rect host_dirty_rect = {0};

    if (!progress || !ctx || !ctx->renderer || !ctx->hostBuffer || !ctx->renderBuffer) {
        return false;
    }
    RuntimeNative3DProgressHUD_UpdateTileProgress(progress);
    if (!ResolveNative3DHostDirtyTileUnion(progress->dirtyTiles,
                                           progress->dirtyTileCount,
                                           ctx->renderWidth,
                                           ctx->renderHeight,
                                           ctx->hostWidth,
                                           ctx->hostHeight,
                                           &host_dirty_tile)) {
        return true;
    }

    host_dirty_rect.x = host_dirty_tile.originX;
    host_dirty_rect.y = host_dirty_tile.originY;
    host_dirty_rect.w = host_dirty_tile.width;
    host_dirty_rect.h = host_dirty_tile.height;
    if (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_OFF) {
        RuntimeNative3DUpscaleNearestABGRRect(ctx->renderBuffer,
                                              ctx->renderWidth,
                                              ctx->renderHeight,
                                              ctx->hostBuffer,
                                              ctx->hostWidth,
                                              ctx->hostHeight,
                                              host_dirty_rect.x,
                                              host_dirty_rect.y,
                                              host_dirty_rect.w,
                                              host_dirty_rect.h);
    } else if (!RuntimeNative3DPreviewReconstructABGRRectWithMode(
                   ctx->renderBuffer,
                   ctx->renderWidth,
                   ctx->renderHeight,
                   ctx->hostBuffer,
                   ctx->hostWidth,
                   ctx->hostHeight,
                   &host_dirty_rect,
                   (Runtime3DUpscaleMode)animSettings.upscaleMode3D)) {
        return false;
    }
    present_tile = &host_dirty_tile;
    return PresentNative3DTilePreviewFrameTimed(ctx->renderer,
                                                &ctx->previewCtx,
                                                present_tile,
                                                ctx->hostBuffer,
                                                false);
}

static void DrawTilePreview(SDL_Renderer* renderer,
                            const IntegratorContext* ctx,
                            const IntegratorTile* tile,
                            const Uint8* preview_buffer) {
    if (!renderer || !ctx || !tile || !preview_buffer) return;
#if USE_VULKAN
    return;
#endif
    for (int y = 0; y < tile->height; y++) {
        int py = tile->originY + y;
        if (py < 0 || py >= ctx->height) continue;
        for (int x = 0; x < tile->width; x++) {
            int px = tile->originX + x;
            if (px < 0 || px >= ctx->width) continue;
            size_t idx = (size_t)py * (size_t)ctx->width + (size_t)px;
            Uint8 brightness = preview_buffer[idx];
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    }
}

static void DrawPreviewBuffer(SDL_Renderer* renderer,
                              const IntegratorContext* ctx,
                              const Uint8* preview_buffer) {
    if (!renderer || !ctx || !preview_buffer) return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect bg = resolve_full_window_destination(renderer, ctx->width, ctx->height);
    SDL_RenderFillRect(renderer, &bg);

#if USE_VULKAN
    RayTracing2PreviewPresent_DrawLuminanceBuffer(renderer,
                                                  preview_buffer,
                                                  ctx->width,
                                                  ctx->height);
    return;
#endif
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            Uint8 brightness = preview_buffer[(size_t)y * (size_t)ctx->width + (size_t)x];
            if (brightness == 0) continue;
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

void RayTracing2PreviewPresent_DrawLuminanceBuffer(SDL_Renderer* renderer,
                                                   const Uint8* buffer,
                                                   int width,
                                                   int height) {
#if USE_VULKAN
    if (!renderer || !buffer || width <= 0 || height <= 0) return;
    SDL_Surface* surface = get_luma_surface(width, height);
    if (!surface) return;

    uint8_t* dst = (uint8_t*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)(dst + y * pitch);
        size_t base = (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) {
            Uint8 b = buffer[base + (size_t)x];
            row[x] = ((uint32_t)0xFF << 24) | ((uint32_t)b << 16) |
                     ((uint32_t)b << 8) | (uint32_t)b;
        }
    }

    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer,
                                                   surface,
                                                   &texture,
                                                   VK_FILTER_NEAREST) != VK_SUCCESS) {
        return;
    }
    SDL_Rect dst_rect = resolve_full_window_destination(renderer, width, height);
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    (void)renderer;
    (void)buffer;
    (void)width;
    (void)height;
#endif
}

void RayTracing2PreviewPresent_DrawABGRBuffer(SDL_Renderer* renderer,
                                              const Uint8* buffer,
                                              int width,
                                              int height) {
    RayTracing2PreviewPresent_DrawABGRBufferToRect(renderer,
                                                   buffer,
                                                   width,
                                                   height,
                                                   resolve_full_window_destination(renderer, width, height));
}

void RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(SDL_Renderer* renderer,
                                                            const Uint8* buffer,
                                                            int width,
                                                            int height,
                                                            SDL_Rect dst_rect,
                                                            bool linear_filter) {
#if USE_VULKAN
    if (!renderer || !buffer || width <= 0 || height <= 0) return;
    if (dst_rect.w <= 0 || dst_rect.h <= 0) return;
    SDL_Surface* surface = get_abgr_surface(width, height);
    if (!surface) return;

    uint8_t* dst = (uint8_t*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)(dst + y * pitch);
        size_t base = (size_t)y * (size_t)width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = 0; x < width; x++) {
            size_t idx = base + (size_t)x * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            row[x] = ((uint32_t)buffer[idx + 3u] << 24) |
                     ((uint32_t)buffer[idx] << 16) |
                     ((uint32_t)buffer[idx + 1u] << 8) |
                     (uint32_t)buffer[idx + 2u];
        }
    }

    VkRendererTexture texture;
    VkFilter filter = linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer,
                                                   surface,
                                                   &texture,
                                                   filter) != VK_SUCCESS) {
        return;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    (void)renderer;
    (void)buffer;
    (void)width;
    (void)height;
    (void)dst_rect;
    (void)linear_filter;
#endif
}

void RayTracing2PreviewPresent_DrawABGRBufferToRect(SDL_Renderer* renderer,
                                                    const Uint8* buffer,
                                                    int width,
                                                    int height,
                                                    SDL_Rect dst_rect) {
    const bool linear_filter = !(dst_rect.w == width && dst_rect.h == height);
    RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(renderer,
                                                           buffer,
                                                           width,
                                                           height,
                                                           dst_rect,
                                                           linear_filter);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    IntegratorContext preview_ctx = {
        .width = host_width,
        .height = host_height
    };
    Native3DPreviewTileProgressContext progress_ctx = {
        .renderer = renderer,
        .hostBuffer = host_buffer,
        .hostWidth = host_width,
        .hostHeight = host_height,
        .renderBuffer = render_buffer,
        .renderWidth = render_width,
        .renderHeight = render_height,
        .previewCtx = {
            .width = host_width,
            .height = host_height
        }
    };
    size_t total = (size_t)render_width * (size_t)render_height;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    (void)disney_denoise_enabled;
    if (!host_buffer || !render_buffer || host_width <= 0 || host_height <= 0 ||
        render_width <= 0 || render_height <= 0 ||
        !grid || !grid->tiles || grid->count == 0) {
        return false;
    }

    RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
    if (!RuntimeNative3DPrepareFrameWithSampling(&frame,
                                                 render_width,
                                                 render_height,
                                                 normalized_t,
                                                 light_x,
                                                 light_y,
                                                 sampling)) {
        return false;
    }
    (void)RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid->tileSize);

    if (present_progress && renderer) {
        SeedNative3DPreviewHistoryUnderlay(host_buffer,
                                           (size_t)host_width * (size_t)host_height,
                                           host_width,
                                           host_height);
        ClearNative3DPreviewHistoryForKnownEmptyTiles(host_buffer,
                                                      host_width,
                                                      host_height,
                                                      render_buffer,
                                                      render_width,
                                                      render_height,
                                                      grid,
                                                      &frame);
        if (!PresentNative3DTilePreviewFrameTimed(renderer,
                                                  &preview_ctx,
                                                  NULL,
                                                  host_buffer,
                                                  true)) {
            RuntimeNative3DPreparedFrame_Free(&frame);
            return false;
        }
    }
    ts_session_start_timer(timer_hud_session(), "Tile Frame Calc");
    if (!RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(
            render_buffer,
            integrator_id,
            &frame,
            temporal_frames,
            PresentNative3DPreviewTemporalProgress,
            NULL,
            (present_progress && renderer) ? PresentNative3DPreviewTileProgress : NULL,
            (present_progress && renderer) ? &progress_ctx : NULL,
            &stats)) {
        ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
        RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return false;
    }
    ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");

    if (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_OFF) {
        RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                          render_width,
                                          render_height,
                                          host_buffer,
                                          host_width,
                                          host_height);
    } else if (!RuntimeNative3DPreviewReconstructABGRWithMode(
                   render_buffer,
                   render_width,
                   render_height,
                   host_buffer,
                   host_width,
                   host_height,
                   (Runtime3DUpscaleMode)animSettings.upscaleMode3D)) {
        RuntimeNative3DPreparedFrame_Free(&frame);
        return false;
    }
    (void)PromoteNative3DPreviewHistory(host_buffer,
                                        (size_t)host_width * (size_t)host_height,
                                        host_width,
                                        host_height);
    RuntimeNative3DPreparedFrame_Free(&frame);
    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

void RayTracing2PreviewPresent_RenderHybridTilesPreview(
    SDL_Renderer* renderer,
    IntegratorContext* ctx,
    const LightSource* light,
    const CameraIntegratorSettings* settings,
    double cam_x,
    double cam_y,
    Uint8* preview_buffer) {
    if (!renderer || !ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) return;
    if (!settings || !light || !preview_buffer) return;

    IntegratorDirectContext dctx = {
        .width = ctx->width,
        .height = ctx->height,
        .grid = (UniformGrid*)ctx->uniformGrid,
        .pixelBuffer = ctx->pixelBuffer,
        .energyBuffer = ctx->energyBuffer,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid
    };

    IntegratorIndirectContext ictx = {
        .width = ctx->width,
        .height = ctx->height,
        .grid = (UniformGrid*)ctx->uniformGrid,
        .cache = ctx->cache,
        .energyBuffer = ctx->energyBuffer,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid,
        .objects = ctx->objects,
        .objectCount = ctx->objectCount,
        .materials = (MaterialBSDF*)ctx->materials,
        .materialCount = ctx->materialCount
    };

    TonemapContext tctx = {
        .width = ctx->width,
        .height = ctx->height,
        .useTiles = ctx->useTiles,
        .tiles = ctx->tileGrid,
        .energyBuffer = ctx->energyBuffer,
        .pixelBuffer = preview_buffer
    };

    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    memset(preview_buffer, 0, total * sizeof(Uint8));

    const int tiles_per_present = 4;
    const Uint32 present_interval_ms = 200;
    Uint32 last_present = SDL_GetTicks();
    int tiles_since_present = 0;

    for (size_t ti = 0; ti < ctx->tileGrid->count; ti++) {
        IntegratorTile* tile = &ctx->tileGrid->tiles[ti];
        if (!tile->energy) continue;

        int start_x = tile->originX;
        int start_y = tile->originY;
        int end_x = tile->originX + tile->width;
        int end_y = tile->originY + tile->height;

        DirectLightingPassRegion(&dctx,
                                 light,
                                 cam_x,
                                 cam_y,
                                 settings->directIntensityScale,
                                 start_x, start_y, end_x, end_y);
        IndirectLightingPassRegion(&ictx,
                                   light,
                                   settings->indirectVariance,
                                   settings->indirectHaloRadius,
                                   settings->directIntensityScale,
                                   start_x, start_y, end_x, end_y);
        TonemapTile(&tctx, tile);
        DrawTilePreview(renderer, ctx, tile, preview_buffer);

        tiles_since_present++;
        Uint32 now = SDL_GetTicks();
        if (tiles_since_present >= tiles_per_present ||
            (now - last_present) >= present_interval_ms) {
            DrawPreviewBuffer(renderer, ctx, preview_buffer);
            render_end_frame();
            if (render_device_lost()) {
                return;
            }
            if (!render_begin_frame()) {
                return;
            }
            last_present = now;
            tiles_since_present = 0;
        }
    }

    DrawPreviewBuffer(renderer, ctx, preview_buffer);

    if (preview_buffer != ctx->pixelBuffer) {
        tctx.pixelBuffer = ctx->pixelBuffer;
        TonemapTiles(&tctx);
    }
}
