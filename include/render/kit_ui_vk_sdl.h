#pragma once

#include "vk_renderer_sdl.h"
#include <math.h>

/* kit_ui_sdl requests SDL_BLENDMODE_BLEND. The native pipeline always uses
 * source-alpha blending, so this mode already holds for a valid renderer. */
static inline int optic_kit_ui_blend_mode(VkRenderer *renderer, SDL_BlendMode mode) {
    return renderer && mode == SDL_BLENDMODE_BLEND ? 0 : -1;
}
#define SDL_SetRenderDrawBlendMode(renderer, mode) optic_kit_ui_blend_mode((renderer), (mode))

static inline int optic_kit_ui_get_blend(VkRenderer *renderer, SDL_BlendMode *mode) {
    if (!renderer || !mode) return -1;
    *mode = SDL_BLENDMODE_BLEND;
    return 0;
}
static inline int optic_kit_ui_get_color(VkRenderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    if (!renderer) return -1;
    const float *color = renderer->draw_state.current_color;
    if (r) *r = (Uint8)lroundf(255 * color[0]);
    if (g) *g = (Uint8)lroundf(255 * color[1]);
    if (b) *b = (Uint8)lroundf(255 * color[2]);
    if (a) *a = (Uint8)lroundf(255 * color[3]);
    return 0;
}
static inline int optic_kit_ui_fill_rect(VkRenderer *renderer, const SDL_FRect *rect) {
    if (!renderer || !rect) return -1;
    SDL_Rect pixels = {(int)floorf(rect->x), (int)floorf(rect->y), 0, 0};
    pixels.w = (int)ceilf(rect->x + rect->w) - pixels.x;
    pixels.h = (int)ceilf(rect->y + rect->h) - pixels.y;
    vk_renderer_fill_rect(renderer, &pixels);
    return 0;
}
#define SDL_GetRenderDrawBlendMode(r, m) optic_kit_ui_get_blend((r), (m))
#define SDL_GetRenderDrawColor(r, a, b, c, d) optic_kit_ui_get_color((r), (a), (b), (c), (d))
#define SDL_RenderFillRectF(r, rect) optic_kit_ui_fill_rect((r), (rect))
