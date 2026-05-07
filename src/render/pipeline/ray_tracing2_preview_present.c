#include "render/pipeline/ray_tracing2_preview_present.h"

#include <stdlib.h>
#include <string.h>

#include "engine/Render/render_pipeline.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/ray_tracing2_preview.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_resolution.h"
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

static RuntimeNative3DSamplingContext ResolveNative3DSubpassSampling(
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t subpass_index,
    int total_subpasses) {
    RuntimeNative3DSamplingContext resolved = {0};
    if (sampling) {
        resolved = *sampling;
    }
    resolved.sampleSequence += subpass_index;
    if (resolved.sampleSequence == 0U) {
        resolved.sampleSequence = subpass_index + 1U;
    }
    resolved.temporalSubpassIndex = (uint16_t)((subpass_index > 65535u) ? 65535u : subpass_index);
    resolved.temporalSubpassCount =
        (uint16_t)((total_subpasses <= 1) ? 1 : ((total_subpasses > 65535) ? 65535 : total_subpasses));
    return resolved;
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

static bool ShouldPresentNative3DTileSubpassPreview(int subpass,
                                                    int temporal_frames) {
    int completed_subpasses = 0;
    int preview_stride = 1;

    if (subpass < 0 || temporal_frames <= 0) {
        return false;
    }

    completed_subpasses = subpass + 1;
    if (temporal_frames > 16) {
        preview_stride = 8;
    } else if (temporal_frames > 8) {
        preview_stride = 4;
    } else if (temporal_frames > 4) {
        preview_stride = 2;
    }

    return completed_subpasses == 1 ||
           completed_subpasses == temporal_frames ||
           (completed_subpasses % preview_stride) == 0;
}

static bool ResolveNative3DHostDirtyTile(const IntegratorTile* render_tile,
                                         int render_width,
                                         int render_height,
                                         int host_width,
                                         int host_height,
                                         IntegratorTile* out_host_tile) {
    if (!render_tile || !out_host_tile) {
        return false;
    }
    if (!RuntimeNative3DResolveUpscaledRect(render_tile->originX,
                                            render_tile->originY,
                                            render_tile->width,
                                            render_tile->height,
                                            render_width,
                                            render_height,
                                            host_width,
                                            host_height,
                                            &out_host_tile->originX,
                                            &out_host_tile->originY,
                                            &out_host_tile->width,
                                            &out_host_tile->height)) {
        return false;
    }
    out_host_tile->energy = NULL;
    return true;
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

void RayTracing2PreviewPresent_DrawABGRBufferToRect(SDL_Renderer* renderer,
                                                    const Uint8* buffer,
                                                    int width,
                                                    int height,
                                                    SDL_Rect dst_rect) {
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
    VkFilter filter = (dst_rect.w == width && dst_rect.h == height) ? VK_FILTER_NEAREST
                                                                    : VK_FILTER_LINEAR;
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
#endif
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
    RuntimeNative3DTemporalAccumulation tile_accumulation = {0};
    RuntimeNative3DAdaptiveSamplingMask adaptive_mask = {0};
    RuntimeNative3DFeatureBuffer tile_features = {0};
    IntegratorContext preview_ctx = {
        .width = host_width,
        .height = host_height
    };
    float* tile_radiance = NULL;
    float* tile_resolved_radiance = NULL;
    size_t total = (size_t)render_width * (size_t)render_height;
    int max_tile_pixels = 0;
    const bool use_adaptive_sampling =
        RuntimeNative3DAdaptiveSampling_ShouldUse(integrator_id, temporal_frames);
    const bool use_denoise =
        RuntimeNative3DDenoise_ShouldApply(integrator_id,
                                           temporal_frames,
                                           disney_denoise_enabled);

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!host_buffer || !render_buffer || host_width <= 0 || host_height <= 0 ||
        render_width <= 0 || render_height <= 0 ||
        !grid || !grid->tiles || grid->count == 0) {
        return false;
    }

    RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
    RuntimeNative3DTemporalAccumulation_Init(&tile_accumulation);
    RuntimeNative3DAdaptiveSamplingMask_Init(&adaptive_mask);
    RuntimeNative3DFeatureBuffer_Init(&tile_features);
    if (!RuntimeNative3DPrepareFrameWithSampling(&frame,
                                                 render_width,
                                                 render_height,
                                                 normalized_t,
                                                 light_x,
                                                 light_y,
                                                 sampling)) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
        RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
        RuntimeNative3DFeatureBuffer_Free(&tile_features);
        return false;
    }
    (void)RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid->tileSize);
    max_tile_pixels = grid->tileSize * grid->tileSize;
    if (max_tile_pixels <= 0) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
        RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DFeatureBuffer_Free(&tile_features);
        return false;
    }
    tile_radiance = (float*)calloc((size_t)max_tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                   sizeof(*tile_radiance));
    tile_resolved_radiance =
        (float*)calloc((size_t)max_tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                       sizeof(*tile_resolved_radiance));
    if (!tile_radiance || !tile_resolved_radiance) {
        free(tile_radiance);
        free(tile_resolved_radiance);
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
        RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DFeatureBuffer_Free(&tile_features);
        return false;
    }

    if (present_progress && renderer) {
        RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                          render_width,
                                          render_height,
                                          host_buffer,
                                          host_width,
                                          host_height);
        if (!PresentNative3DTilePreviewFrameTimed(renderer,
                                                  &preview_ctx,
                                                  NULL,
                                                  host_buffer,
                                                  true)) {
            free(tile_radiance);
            free(tile_resolved_radiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
            RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tile_features);
            return false;
        }
    }

    for (size_t ti = 0; ti < grid->count; ++ti) {
        const IntegratorTile* tile = &grid->tiles[ti];
        IntegratorTile host_dirty_tile = {0};
        const IntegratorTile* present_tile = NULL;
        const int tile_stride = tile->width;
        const int tile_pixels = tile->width * tile->height;
        bool tile_timer_active = false;

        if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                             tile->originX,
                                                             tile->originY,
                                                             tile->originX + tile->width,
                                                             tile->originY + tile->height)) {
            continue;
        }

        if (!RuntimeNative3DTemporalAccumulation_Ensure(&tile_accumulation, tile->width, tile->height)) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tile_radiance);
            free(tile_resolved_radiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
            RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tile_features);
            return false;
        }
        RuntimeNative3DTemporalAccumulation_Clear(&tile_accumulation);
        if (use_denoise &&
            (!RuntimeNative3DFeatureBuffer_Ensure(&tile_features, tile->width, tile->height) ||
             !RuntimeNative3DFeatureBuffer_RenderRegion(&tile_features,
                                                       &frame.scene,
                                                       &frame.projector,
                                                       tile->originX,
                                                       tile->originY,
                                                       tile->originX + tile->width,
                                                       tile->originY + tile->height))) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tile_radiance);
            free(tile_resolved_radiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
            RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tile_features);
            return false;
        }
        if (use_adaptive_sampling &&
            (!RuntimeNative3DAdaptiveSamplingMask_Ensure(&adaptive_mask, tile->width, tile->height) ||
             !RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(&adaptive_mask,
                                                                    &frame.scene,
                                                                    &frame.projector,
                                                                    tile->originX,
                                                                    tile->originY,
                                                                    tile->originX + tile->width,
                                                                    tile->originY + tile->height))) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tile_radiance);
            free(tile_resolved_radiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
            RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tile_features);
            return false;
        }
        if (present_progress && renderer &&
            ResolveNative3DHostDirtyTile(tile,
                                         render_width,
                                         render_height,
                                         host_width,
                                         host_height,
                                         &host_dirty_tile)) {
            present_tile = &host_dirty_tile;
        }

        ts_session_start_timer(timer_hud_session(), "Tile Frame Calc");
        tile_timer_active = true;
        for (int subpass = 0; subpass < temporal_frames; ++subpass) {
            RuntimeNative3DPreparedFrame subpass_frame = frame;
            RuntimeNative3DRenderStats subpass_stats = {0};
            RuntimeNative3DSamplingContext subpass_sampling =
                ResolveNative3DSubpassSampling(sampling, (uint32_t)subpass, temporal_frames);
            const uint8_t* active_mask =
                (use_adaptive_sampling && subpass > 0) ? adaptive_mask.activeSampleMask : NULL;
            const int active_mask_stride =
                (use_adaptive_sampling && subpass > 0) ? adaptive_mask.width : 0;
            if (active_mask && !RuntimeNative3DAdaptiveSampling_HasActiveSamples(&adaptive_mask)) {
                break;
            }
            subpass_frame.sampling = subpass_sampling;
            memset(tile_radiance,
                   0,
                   (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                       sizeof(*tile_radiance));
            if (!RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
                    tile_radiance,
                    tile_stride,
                    integrator_id,
                    &subpass_frame,
                    tile->originX,
                    tile->originY,
                    tile->originX + tile->width,
                    tile->originY + tile->height,
                    active_mask,
                    active_mask_stride,
                    &subpass_stats) ||
                !RuntimeNative3DTemporalAccumulation_AddRegionSamples(&tile_accumulation,
                                                                      tile_radiance,
                                                                      tile_stride,
                                                                      0,
                                                                      0,
                                                                      tile->width,
                                                                      tile->height,
                                                                      active_mask,
                                                                      active_mask_stride)) {
                ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
                RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
                free(tile_radiance);
                free(tile_resolved_radiance);
                RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
                RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
                RuntimeNative3DPreparedFrame_Free(&frame);
                RuntimeNative3DFeatureBuffer_Free(&tile_features);
                return false;
            }
            RuntimeNative3DTemporalAccumulation_CommitSubpass(&tile_accumulation);
            RuntimeNative3DRenderStats_Accumulate(&stats, &subpass_stats);

            if (present_progress && renderer &&
                ShouldPresentNative3DTileSubpassPreview(subpass, temporal_frames)) {
                if (use_denoise) {
                    memset(tile_resolved_radiance,
                           0,
                           (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                               sizeof(*tile_resolved_radiance));
                    if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
                            &tile_accumulation,
                            tile_resolved_radiance,
                            tile_stride,
                            0,
                            0,
                            tile->width,
                            tile->height) ||
                        !RuntimeNative3DDenoise_Apply(tile_resolved_radiance,
                                                      tile_stride,
                                                      &tile_features)) {
                        ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
                        free(tile_radiance);
                        free(tile_resolved_radiance);
                        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
                        RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
                        RuntimeNative3DPreparedFrame_Free(&frame);
                        RuntimeNative3DFeatureBuffer_Free(&tile_features);
                        return false;
                    }
                    RuntimeNative3DResolveRadianceRegionToPixels(render_buffer,
                                                                 render_width,
                                                                 tile_resolved_radiance,
                                                                 tile_stride,
                                                                 tile->originX,
                                                                 tile->originY,
                                                                 tile->originX + tile->width,
                                                                 tile->originY + tile->height);
                } else {
                    RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&tile_accumulation,
                                                                                     render_buffer,
                                                                                     render_width,
                                                                                     tile->originX,
                                                                                     tile->originY);
                }
                RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                                  render_width,
                                                  render_height,
                                                  host_buffer,
                                                  host_width,
                                                  host_height);
                if (!PresentNative3DTilePreviewFrameTimed(renderer,
                                                          &preview_ctx,
                                                          present_tile,
                                                          host_buffer,
                                                          false)) {
                    ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
                    free(tile_radiance);
                    free(tile_resolved_radiance);
                    RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
                    RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
                    RuntimeNative3DPreparedFrame_Free(&frame);
                    RuntimeNative3DFeatureBuffer_Free(&tile_features);
                    return false;
                }
            }
        }

        if (use_denoise) {
            memset(tile_resolved_radiance,
                   0,
                   (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                       sizeof(*tile_resolved_radiance));
            if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(&tile_accumulation,
                                                                                   tile_resolved_radiance,
                                                                                   tile_stride,
                                                                                   0,
                                                                                   0,
                                                                                   tile->width,
                                                                                   tile->height) ||
                !RuntimeNative3DDenoise_Apply(tile_resolved_radiance, tile_stride, &tile_features)) {
                if (tile_timer_active) {
                    ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
                }
                free(tile_radiance);
                free(tile_resolved_radiance);
                RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
                RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
                RuntimeNative3DPreparedFrame_Free(&frame);
                RuntimeNative3DFeatureBuffer_Free(&tile_features);
                return false;
            }
            RuntimeNative3DResolveRadianceRegionToPixels(render_buffer,
                                                         render_width,
                                                         tile_resolved_radiance,
                                                         tile_stride,
                                                         tile->originX,
                                                         tile->originY,
                                                         tile->originX + tile->width,
                                                         tile->originY + tile->height);
        } else {
            RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&tile_accumulation,
                                                                             render_buffer,
                                                                             render_width,
                                                                             tile->originX,
                                                                             tile->originY);
        }
        if (tile_timer_active) {
            ts_session_stop_timer(timer_hud_session(), "Tile Frame Calc");
        }
    }

    RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                      render_width,
                                      render_height,
                                      host_buffer,
                                      host_width,
                                      host_height);
    free(tile_radiance);
    free(tile_resolved_radiance);
    RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
    RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
    RuntimeNative3DPreparedFrame_Free(&frame);
    RuntimeNative3DFeatureBuffer_Free(&tile_features);
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
