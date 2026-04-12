#include "render/fluid_overlay.h"
#include "render/kit_viz_fluid_overlay_adapter.h"
#include "render/fluid/fluid_state.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "camera/camera.h"
#include "render/space_mode_adapter.h"
#include "kit_viz.h"
#include "vk_renderer.h"

static uint8_t *g_overlayScratchRgba = NULL;
static size_t g_overlayScratchRgbaCapacity = 0;
static float *g_overlayScratchVx = NULL;
static float *g_overlayScratchVy = NULL;
static size_t g_overlayScratchVCapacity = 0;
static KitVizVecSegment *g_overlayScratchSegments = NULL;
static size_t g_overlayScratchSegmentCapacity = 0;

static float clamp01f(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static bool ensure_overlay_scratch_capacity(size_t required_size) {
    if (required_size == 0) return false;
    if (g_overlayScratchRgbaCapacity >= required_size && g_overlayScratchRgba != NULL) {
        return true;
    }
    uint8_t *resized = (uint8_t *)realloc(g_overlayScratchRgba, required_size);
    if (!resized) return false;
    g_overlayScratchRgba = resized;
    g_overlayScratchRgbaCapacity = required_size;
    return true;
}

static bool ensure_overlay_vector_capacity(size_t required_count) {
    if (required_count == 0) return false;
    if (g_overlayScratchVCapacity >= required_count &&
        g_overlayScratchVx != NULL &&
        g_overlayScratchVy != NULL) {
        return true;
    }
    float *new_vx = (float *)realloc(g_overlayScratchVx, required_count * sizeof(float));
    if (!new_vx) return false;
    float *new_vy = (float *)realloc(g_overlayScratchVy, required_count * sizeof(float));
    if (!new_vy) {
        // Keep existing buffers valid on failure.
        return false;
    }
    g_overlayScratchVx = new_vx;
    g_overlayScratchVy = new_vy;
    g_overlayScratchVCapacity = required_count;
    return true;
}

static bool ensure_overlay_segment_capacity(size_t required_count) {
    if (required_count == 0) return false;
    if (g_overlayScratchSegmentCapacity >= required_count && g_overlayScratchSegments != NULL) {
        return true;
    }
    KitVizVecSegment *new_segments =
        (KitVizVecSegment *)realloc(g_overlayScratchSegments, required_count * sizeof(KitVizVecSegment));
    if (!new_segments) return false;
    g_overlayScratchSegments = new_segments;
    g_overlayScratchSegmentCapacity = required_count;
    return true;
}

static void build_density_overlay_legacy(const FluidFrame *frame, uint8_t *dst, int pitch) {
    size_t count = (size_t)frame->w * (size_t)frame->h;
    const float *density = frame->density;
    float max_d = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        if (density[i] > max_d) max_d = density[i];
    }
    float inv = (max_d > 1e-6f) ? (1.0f / max_d) : 1.0f;
    for (int y = 0; y < frame->h; ++y) {
        uint32_t *row = (uint32_t *)(dst + y * pitch);
        for (int x = 0; x < frame->w; ++x) {
            float d = density[(size_t)y * (size_t)frame->w + (size_t)x];
            float a = clamp01f(d * inv);
            uint8_t alpha = (uint8_t)lroundf(a * 180.0f);
            row[x] = ((uint32_t)alpha << 24) | 0x00FFFFFF; // white with alpha
        }
    }
}

static bool build_density_overlay_adapter(const FluidFrame *frame, uint8_t *dst, int pitch) {
    KitVizFluidOverlayConfig cfg;
    cfg.alpha = 180u;
    cfg.auto_range = true;
    cfg.min_density = 0.0f;
    cfg.max_density = 1.0f;
    cfg.colormap = KIT_VIZ_COLORMAP_HEAT;

    if (frame->w > 0 && (size_t)frame->w > SIZE_MAX / 4u) return false;
    const size_t row_bytes = (size_t)frame->w * 4u;
    if (frame->h > 0 && (size_t)frame->h > SIZE_MAX / row_bytes) return false;
    const size_t packed_bytes = row_bytes * (size_t)frame->h;
    if ((size_t)pitch == row_bytes) {
        return kit_viz_fluid_overlay_build_density_rgba(frame, &cfg, dst, packed_bytes);
    }

    if (!ensure_overlay_scratch_capacity(packed_bytes)) return false;

    bool ok = kit_viz_fluid_overlay_build_density_rgba(frame, &cfg, g_overlayScratchRgba, packed_bytes);
    if (ok) {
        for (int y = 0; y < frame->h; ++y) {
            memcpy(dst + y * pitch, g_overlayScratchRgba + (size_t)y * row_bytes, row_bytes);
        }
    }
    return ok;
}

static bool build_velocity_heatmap_overlay_adapter(const FluidFrame *frame, uint8_t *dst, int pitch) {
    if (!frame || !frame->velX || !frame->velY || frame->w <= 0 || frame->h <= 0) return false;
    if ((size_t)frame->w > SIZE_MAX / (size_t)frame->h) return false;

    size_t cell_count = (size_t)frame->w * (size_t)frame->h;
    if (!ensure_overlay_vector_capacity(cell_count)) return false;

    float max_mag = 0.0f;
    for (size_t i = 0; i < cell_count; ++i) {
        float vx = frame->velX[i];
        float vy = frame->velY[i];
        if (!isfinite(vx) || !isfinite(vy)) {
            g_overlayScratchVx[i] = 0.0f;
            continue;
        }
        float mag = sqrtf(vx * vx + vy * vy);
        g_overlayScratchVx[i] = mag;
        if (mag > max_mag) max_mag = mag;
    }
    if (max_mag <= 1e-8f) return false;

    if (frame->w > 0 && (size_t)frame->w > SIZE_MAX / 4u) return false;
    const size_t row_bytes = (size_t)frame->w * 4u;
    if (frame->h > 0 && (size_t)frame->h > SIZE_MAX / row_bytes) return false;
    const size_t packed_bytes = row_bytes * (size_t)frame->h;

    uint8_t *rgba_target = dst;
    if ((size_t)pitch != row_bytes) {
        if (!ensure_overlay_scratch_capacity(packed_bytes)) return false;
        rgba_target = g_overlayScratchRgba;
    }

    CoreResult heat_r = kit_viz_build_heatmap_rgba(g_overlayScratchVx,
                                                   (uint32_t)frame->w,
                                                   (uint32_t)frame->h,
                                                   0.0f,
                                                   max_mag,
                                                   KIT_VIZ_COLORMAP_HEAT,
                                                   rgba_target,
                                                   packed_bytes);
    if (heat_r.code != CORE_OK) return false;

    for (size_t i = 0; i < cell_count; ++i) {
        rgba_target[i * 4u + 3u] = 185u;
    }

    if ((size_t)pitch != row_bytes) {
        for (int y = 0; y < frame->h; ++y) {
            memcpy(dst + y * pitch, rgba_target + (size_t)y * row_bytes, row_bytes);
        }
    }
    return true;
}

static bool build_velocity_overlay_segments(const FluidFrame *frame,
                                            uint32_t stride,
                                            size_t *out_segment_count) {
    if (!frame || !frame->velX || !frame->velY || !out_segment_count) return false;
    if (stride == 0 || frame->w <= 0 || frame->h <= 0) return false;
    if ((size_t)frame->w > SIZE_MAX / (size_t)frame->h) return false;

    size_t cell_count = (size_t)frame->w * (size_t)frame->h;
    if (!ensure_overlay_vector_capacity(cell_count)) return false;

    float max_mag = 0.0f;
    for (size_t i = 0; i < cell_count; ++i) {
        float vx = frame->velX[i];
        float vy = frame->velY[i];
        if (!isfinite(vx) || !isfinite(vy)) continue;
        float mag = sqrtf(vx * vx + vy * vy);
        if (mag > max_mag) max_mag = mag;
    }
    if (max_mag <= 1e-8f) {
        *out_segment_count = 0;
        return true;
    }

    float cap_mag = max_mag;
    float denom = log1pf(cap_mag);
    if (denom <= 1e-6f) denom = 1.0f;
    float max_len_cells = (float)stride * 0.85f;

    for (size_t i = 0; i < cell_count; ++i) {
        float vx = frame->velX[i];
        float vy = frame->velY[i];
        if (!isfinite(vx) || !isfinite(vy)) {
            g_overlayScratchVx[i] = 0.0f;
            g_overlayScratchVy[i] = 0.0f;
            continue;
        }

        float mag = sqrtf(vx * vx + vy * vy);
        if (mag <= 1e-8f) {
            g_overlayScratchVx[i] = 0.0f;
            g_overlayScratchVy[i] = 0.0f;
            continue;
        }

        float capped_mag = fminf(mag, cap_mag);
        float shaped = log1pf(capped_mag) / denom;
        float target_len = shaped * max_len_cells;
        float scale = target_len / mag;
        g_overlayScratchVx[i] = vx * scale;
        g_overlayScratchVy[i] = vy * scale;
    }

    uint32_t cols = ((uint32_t)frame->w + stride - 1u) / stride;
    uint32_t rows = ((uint32_t)frame->h + stride - 1u) / stride;
    if ((size_t)cols > SIZE_MAX / (size_t)rows) return false;
    size_t max_segments = (size_t)cols * (size_t)rows;
    if (!ensure_overlay_segment_capacity(max_segments)) return false;

    CoreResult seg_r = kit_viz_build_vector_segments(g_overlayScratchVx,
                                                     g_overlayScratchVy,
                                                     (uint32_t)frame->w,
                                                     (uint32_t)frame->h,
                                                     stride,
                                                     1.0f,
                                                     g_overlayScratchSegments,
                                                     g_overlayScratchSegmentCapacity,
                                                     out_segment_count);
    return seg_r.code == CORE_OK;
}

static void draw_velocity_overlay(SDL_Renderer *renderer,
                                  const FluidFrame *frame,
                                  const Camera *camera,
                                  int screen_w,
                                  int screen_h) {
    if (!renderer || !frame || !camera) return;
    if (g_fluidOverlayMode != FLUID_OVERLAY_MODE_DENSITY_VELOCITY &&
        g_fluidOverlayMode != FLUID_OVERLAY_MODE_VELOCITY_HEATMAP) return;

    const uint32_t stride = 4u;
    size_t segment_count = 0;
    if (!build_velocity_overlay_segments(frame, stride, &segment_count) || segment_count == 0) return;

    float origin_x = frame->meta.origin_x;
    float origin_y = frame->meta.origin_y;
    float cell = frame->meta.cell_size > 0.0f ? frame->meta.cell_size : 1.0f;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, 72, 255, 210, 220);
    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(camera, screen_w, screen_h);

    for (size_t i = 0; i < segment_count; ++i) {
        const KitVizVecSegment *seg = &g_overlayScratchSegments[i];

        float wx0 = origin_x + seg->x0 * cell;
        float wy0 = origin_y + seg->y0 * cell;
        float wx1 = origin_x + seg->x1 * cell;
        float wy1 = origin_y + seg->y1 * cell;

        CameraPoint p0 = SpaceModeAdapter_WorldToScreen(&view_ctx, wx0, wy0);
        CameraPoint p1 = SpaceModeAdapter_WorldToScreen(&view_ctx, wx1, wy1);

        SDL_RenderDrawLine(renderer,
                           (int)lroundf((float)p0.x),
                           (int)lroundf((float)p0.y),
                           (int)lroundf((float)p1.x),
                           (int)lroundf((float)p1.y));

        float dx = (float)p1.x - (float)p0.x;
        float dy = (float)p1.y - (float)p0.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len <= 3.0f) continue;

        float ux = dx / len;
        float uy = dy / len;
        float head_len = 5.0f;
        float wing = 2.5f;
        float lx = (float)p1.x - ux * head_len + uy * wing;
        float ly = (float)p1.y - uy * head_len - ux * wing;
        float rx = (float)p1.x - ux * head_len - uy * wing;
        float ry = (float)p1.y - uy * head_len + ux * wing;

        SDL_RenderDrawLine(renderer,
                           (int)lroundf((float)p1.x),
                           (int)lroundf((float)p1.y),
                           (int)lroundf(lx),
                           (int)lroundf(ly));
        SDL_RenderDrawLine(renderer,
                           (int)lroundf((float)p1.x),
                           (int)lroundf((float)p1.y),
                           (int)lroundf(rx),
                           (int)lroundf(ry));
    }
}

bool fluid_overlay_draw(SDL_Renderer *renderer,
                        const FluidFrame *frame,
                        const Camera *camera,
                        int screen_w,
                        int screen_h) {
    if (!renderer || !frame || !camera || frame->w <= 0 || frame->h <= 0) return false;
#if USE_VULKAN
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, frame->w, frame->h, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;
    uint8_t *dst = (uint8_t *)surface->pixels;
    int pitch = surface->pitch;
#else
    SDL_Texture *tex = SDL_CreateTexture(renderer,
                                         SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         frame->w,
                                         frame->h);
    if (!tex) return false;

    // Build a small texture where alpha encodes density.
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0) {
        SDL_DestroyTexture(tex);
        return false;
    }
    uint8_t *dst = (uint8_t *)pixels;
#endif
    bool built = false;
    if (g_fluidOverlayMode == FLUID_OVERLAY_MODE_VELOCITY_HEATMAP) {
        built = build_velocity_heatmap_overlay_adapter(frame, dst, pitch);
    } else {
        built = build_density_overlay_adapter(frame, dst, pitch);
    }
    if (!built) {
        if (!build_density_overlay_adapter(frame, dst, pitch)) {
            build_density_overlay_legacy(frame, dst, pitch);
        }
    }
#if !USE_VULKAN
    SDL_UnlockTexture(tex);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
#endif

    // Compute screen-space rectangle for the fluid grid
    float origin_x = frame->meta.origin_x;
    float origin_y = frame->meta.origin_y;
    float cell = frame->meta.cell_size > 0.0f ? frame->meta.cell_size : 1.0f;
    float world_min_x = origin_x;
    float world_min_y = origin_y;
    float world_max_x = origin_x + cell * (float)(frame->w);
    float world_max_y = origin_y + cell * (float)(frame->h);

    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(camera, screen_w, screen_h);
    CameraPoint p0 = SpaceModeAdapter_WorldToScreen(&view_ctx, world_min_x, world_min_y);
    CameraPoint p1 = SpaceModeAdapter_WorldToScreen(&view_ctx, world_max_x, world_max_y);

    SDL_Rect dst_rect = {
        (int)lroundf(fminf((float)p0.x, (float)p1.x)),
        (int)lroundf(fminf((float)p0.y, (float)p1.y)),
        (int)lroundf(fabsf((float)p1.x - (float)p0.x)),
        (int)lroundf(fabsf((float)p1.y - (float)p0.y))
    };
    if (dst_rect.w < 1) dst_rect.w = 1;
    if (dst_rect.h < 1) dst_rect.h = 1;

#if USE_VULKAN
    VkRendererTexture texture;
    VkResult result = vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                                 VK_FILTER_LINEAR);
    SDL_FreeSurface(surface);
    if (result != VK_SUCCESS) {
        return false;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    SDL_RenderCopy(renderer, tex, NULL, &dst_rect);
    SDL_DestroyTexture(tex);
#endif
    draw_velocity_overlay(renderer, frame, camera, screen_w, screen_h);
    return true;
}
