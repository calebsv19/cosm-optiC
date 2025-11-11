#include "hud_renderer.h"
#include "../core/timer_manager.h"
#include "../config/settings_loader.h"
#include "../hud/TextRender/text_render.h"
#include "../core/time_utils.h"  // new helper

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h" 

#include <stdio.h>
#include <string.h>

#define HUD_PADDING 6
#define HUD_SPACING 4
#define HUD_BG_ALPHA 180
#define HUD_TEXT_COLOR (SDL_Color){255, 255, 255, 255}
#define HUD_BG_COLOR (SDL_Color){0, 0, 0, HUD_BG_ALPHA}
#define HUD_UPDATE_INTERVAL_MS 500
#define MAX_HUD_LINES MAX_TIMERS

static uint64_t last_update_time_ns = 0;
static char cached_lines[MAX_HUD_LINES][256];
static int cached_line_count = 0;

void hud_init(void) {
    last_update_time_ns = 0;
    cached_line_count = 0;
    if (!initFontSystem()) {
        fprintf(stderr, "[TimerHUD] Failed to initialise font system.\n");
    }
    if (!Text_Init()) {
        fprintf(stderr, "[TimerHUD] Failed to initialise text renderer.\n");
    }
}

void ts_render(SDL_Renderer* renderer) {
    TimerManager* tm = &g_timer_manager;

    // --- 1. Throttled update ---
    if (has_interval_elapsed(&last_update_time_ns, HUD_UPDATE_INTERVAL_MS)) {
        cached_line_count = 0;
        for (int i = 0; i < tm->count && i < MAX_HUD_LINES; i++) {
            Timer* t = &tm->timers[i];
            snprintf(cached_lines[cached_line_count], sizeof(cached_lines[cached_line_count]),
                     "%s: %.2f ms (min %.2f / max %.2f / σ %.2f)",
                     t->name, t->avg, t->min, t->max, t->stddev);
            cached_line_count++;
        }
    }

    // --- 2. Pull screen size from RenderContext ---
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    int screenW = ctx->width;
    int screenH = ctx->height;

    TTF_Font* font = getActiveFont();
    int fontHeight = font ? TTF_FontHeight(font) : 14;  // fallback to 14px

    // --- 3. Compute block layout ---
    int totalHeight = 0;
    int maxWidth = 0;
    for (int i = 0; i < cached_line_count; i++) {
        int textW = getTextWidth(cached_lines[i]);
        int lineW = textW + HUD_PADDING * 2;
        if (lineW > maxWidth) maxWidth = lineW;
        totalHeight += fontHeight + HUD_PADDING * 2 + HUD_SPACING;
    }
    if (cached_line_count > 0) totalHeight -= HUD_SPACING;

    // --- 4. Determine corner anchor
    const char* pos = ts_settings.hud_position;
    int offsetX = ts_settings.hud_offset_x;
    int offsetY = ts_settings.hud_offset_y;

    int baseX = 0;
    int baseY = 0;
    bool rightAlign = false;

    if (strcmp(pos, "top-left") == 0) {
        baseX = offsetX;
        baseY = offsetY;
    } else if (strcmp(pos, "top-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = offsetY;
        rightAlign = true;
    } else if (strcmp(pos, "bottom-left") == 0) {
        baseX = offsetX;
        baseY = screenH - offsetY - totalHeight;
    } else if (strcmp(pos, "bottom-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = screenH - offsetY - totalHeight;
        rightAlign = true;
    } else {
        baseX = offsetX;
        baseY = offsetY;
    }

    // --- 5. Draw each line ---
    int y = baseY;
    for (int i = 0; i < cached_line_count; i++) {
        const char* line = cached_lines[i];
        int textW = getTextWidth(line);
        int bgW = textW + HUD_PADDING * 2;
        int bgH = fontHeight + HUD_PADDING * 2;

        int bgX = baseX;
        int textX = rightAlign ? (bgX + bgW - HUD_PADDING) : (bgX + HUD_PADDING);
        int align = ALIGN_TOP | (rightAlign ? ALIGN_RIGHT : ALIGN_LEFT);

        // Background box
        SDL_Rect bg = { bgX, y, bgW, bgH };
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
        SDL_SetRenderDrawColor(renderer, HUD_BG_COLOR.r, HUD_BG_COLOR.g, HUD_BG_COLOR.b, HUD_BG_COLOR.a);
        SDL_RenderFillRect(renderer, &bg);

        // Text
        Text_Draw(renderer, line, textX, y + HUD_PADDING, align, HUD_TEXT_COLOR);

        y += bgH + HUD_SPACING;
    }
}
