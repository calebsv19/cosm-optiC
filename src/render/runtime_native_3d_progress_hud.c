#include "render/runtime_native_3d_progress_hud.h"

#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_draw.h"

#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct RuntimeNative3DProgressHUDState {
    bool active;
    RayTracing3DIntegratorId integratorId;
    uint64_t frameStartCounter;
    uint64_t frameCompleteCounter;
    int startedSubpasses;
    int completedSubpasses;
    int totalSubpasses;
    size_t completedTilesInSubpass;
    size_t totalTilesInSubpass;
} RuntimeNative3DProgressHUDState;

static RuntimeNative3DProgressHUDState g_runtime_native_3d_progress_hud = {0};

static int runtime_native_3d_progress_hud_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static size_t runtime_native_3d_progress_hud_clamp_size(size_t value, size_t max_value) {
    if (value > max_value) return max_value;
    return value;
}

static double runtime_native_3d_progress_hud_progress01(void) {
    const RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    double completed = 0.0;
    double subpass_fraction = 0.0;

    if (!state->active || state->totalSubpasses <= 0) {
        return 0.0;
    }
    completed = (double)runtime_native_3d_progress_hud_clamp_int(state->completedSubpasses,
                                                                  0,
                                                                  state->totalSubpasses);
    if (state->totalTilesInSubpass > 0u && completed < (double)state->totalSubpasses) {
        subpass_fraction =
            (double)runtime_native_3d_progress_hud_clamp_size(state->completedTilesInSubpass,
                                                               state->totalTilesInSubpass) /
            (double)state->totalTilesInSubpass;
    }
    completed += subpass_fraction;
    if (completed < 0.0) completed = 0.0;
    if (completed > (double)state->totalSubpasses) completed = (double)state->totalSubpasses;
    return completed / (double)state->totalSubpasses;
}

static const char* runtime_native_3d_progress_hud_integrator_label(
    RayTracing3DIntegratorId integrator_id) {
    switch (integrator_id) {
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
            return "Direct";
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return "Diffuse";
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return "Material";
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return "Emission";
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
            return "Disney";
        default:
            return "Native 3D";
    }
}

static double runtime_native_3d_progress_hud_elapsed_seconds(void) {
    const RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    uint64_t end_counter = 0u;
    uint64_t frequency = 0u;

    if (!state->active || state->frameStartCounter == 0u) {
        return 0.0;
    }
    frequency = (uint64_t)SDL_GetPerformanceFrequency();
    if (frequency == 0u) {
        return 0.0;
    }
    end_counter = (state->frameCompleteCounter != 0u)
                      ? state->frameCompleteCounter
                      : (uint64_t)SDL_GetPerformanceCounter();
    if (end_counter <= state->frameStartCounter) {
        return 0.0;
    }
    return (double)(end_counter - state->frameStartCounter) / (double)frequency;
}

static void runtime_native_3d_progress_hud_format_duration(double seconds,
                                                           char* buffer,
                                                           size_t buffer_size) {
    int total_seconds = 0;
    int minutes = 0;
    int hours = 0;

    if (!buffer || buffer_size == 0u) {
        return;
    }
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    if (seconds < 100.0) {
        (void)snprintf(buffer, buffer_size, "%.1fs", seconds);
        return;
    }

    total_seconds = (int)(seconds + 0.5);
    minutes = total_seconds / 60;
    if (minutes < 60) {
        (void)snprintf(buffer, buffer_size, "%dm %02ds", minutes, total_seconds % 60);
        return;
    }

    hours = minutes / 60;
    (void)snprintf(buffer, buffer_size, "%dh %02dm", hours, minutes % 60);
}

static double runtime_native_3d_progress_hud_estimate_remaining_seconds(double elapsed_seconds,
                                                                        double progress01) {
    double estimated_total = 0.0;

    if (progress01 >= 1.0) {
        return 0.0;
    }
    if (progress01 <= 0.001 || elapsed_seconds <= 0.0) {
        return -1.0;
    }
    estimated_total = elapsed_seconds / progress01;
    if (estimated_total <= elapsed_seconds) {
        return 0.0;
    }
    return estimated_total - elapsed_seconds;
}

static bool runtime_native_3d_progress_hud_measure_line(SDL_Renderer* renderer,
                                                        TTF_Font* font,
                                                        const char* text,
                                                        int* out_w,
                                                        int* out_h) {
    if (!renderer || !font || !text) {
        return false;
    }
    return ray_tracing_text_measure_utf8(renderer, font, text, out_w, out_h) != 0;
}

static void runtime_native_3d_progress_hud_draw_line(SDL_Renderer* renderer,
                                                     TTF_Font* font,
                                                     const char* text,
                                                     int x,
                                                     int y,
                                                     SDL_Color color) {
    SDL_Rect rect = {x, y, 0, 0};
    if (!renderer || !font || !text) {
        return;
    }
    (void)ray_tracing_text_draw_utf8(renderer, font, text, color, &rect);
}

void RuntimeNative3DProgressHUD_Reset(void) {
    memset(&g_runtime_native_3d_progress_hud, 0, sizeof(g_runtime_native_3d_progress_hud));
}

void RuntimeNative3DProgressHUD_BeginFrame(RayTracing3DIntegratorId integrator_id,
                                           int total_subpasses) {
    RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    memset(state, 0, sizeof(*state));
    state->active = true;
    state->integratorId = integrator_id;
    state->frameStartCounter = (uint64_t)SDL_GetPerformanceCounter();
    state->totalSubpasses = (total_subpasses <= 0) ? 1 : total_subpasses;
}

void RuntimeNative3DProgressHUD_UpdateTemporal(int started_subpasses,
                                               int completed_subpasses,
                                               int total_subpasses) {
    RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    if (!state->active) {
        return;
    }
    if (total_subpasses > 0) {
        state->totalSubpasses = total_subpasses;
    }
    state->startedSubpasses =
        runtime_native_3d_progress_hud_clamp_int(started_subpasses, 0, state->totalSubpasses);
    state->completedSubpasses =
        runtime_native_3d_progress_hud_clamp_int(completed_subpasses, 0, state->totalSubpasses);
    if (state->completedSubpasses >= state->startedSubpasses) {
        state->completedTilesInSubpass = 0u;
        state->totalTilesInSubpass = 0u;
    }
}

void RuntimeNative3DProgressHUD_UpdateTileProgress(
    const RuntimeNative3DTileSchedulerProgress* progress) {
    RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    if (!state->active || !progress) {
        return;
    }
    if (progress->totalSubpasses > 0) {
        state->totalSubpasses = progress->totalSubpasses;
    }
    state->startedSubpasses =
        runtime_native_3d_progress_hud_clamp_int(progress->startedSubpasses,
                                                 0,
                                                 state->totalSubpasses);
    state->completedSubpasses =
        runtime_native_3d_progress_hud_clamp_int(progress->completedSubpasses,
                                                 0,
                                                 state->totalSubpasses);
    state->totalTilesInSubpass = progress->totalTilesInSubpass;
    state->completedTilesInSubpass =
        runtime_native_3d_progress_hud_clamp_size(progress->completedTilesInSubpass,
                                                  state->totalTilesInSubpass);
}

void RuntimeNative3DProgressHUD_CompleteFrame(void) {
    RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    if (!state->active) {
        return;
    }
    state->startedSubpasses = state->totalSubpasses;
    state->completedSubpasses = state->totalSubpasses;
    state->completedTilesInSubpass = 0u;
    state->totalTilesInSubpass = 0u;
    if (state->frameCompleteCounter == 0u) {
        state->frameCompleteCounter = (uint64_t)SDL_GetPerformanceCounter();
    }
}

void RuntimeNative3DProgressHUD_Draw(SDL_Renderer* renderer) {
    RuntimeNative3DProgressHUDState* state = &g_runtime_native_3d_progress_hud;
    RenderContext* ctx = getRenderContext();
    TTF_Font* font = getActiveFont();
    SDL_Rect panel = {0};
    SDL_Rect bar_bg = {0};
    SDL_Rect bar_fill = {0};
    SDL_Color title_color = {236, 239, 244, 255};
    SDL_Color detail_color = {164, 175, 192, 255};
    SDL_Color accent_color = {113, 214, 182, 255};
    char title[64] = {0};
    char subpass_line[64] = {0};
    char tile_line[64] = {0};
    char elapsed_value[32] = {0};
    char estimate_value[32] = {0};
    char elapsed_line[40] = {0};
    char estimate_line[40] = {0};
    int title_w = 0;
    int title_h = 0;
    int subpass_w = 0;
    int subpass_h = 0;
    int tile_w = 0;
    int tile_h = 0;
    int elapsed_w = 0;
    int elapsed_h = 0;
    int estimate_w = 0;
    int estimate_h = 0;
    int left_col_w = 0;
    int right_col_w = 0;
    int column_gap = 18;
    int right_col_x = 0;
    int max_text_w = 0;
    int panel_w = 0;
    int panel_h = 0;
    int screen_w = 0;
    int screen_h = 0;
    int line_gap = 4;
    int padding = 10;
    int bar_h = 8;
    int bar_gap = 8;
    double progress01 = 0.0;
    double elapsed_seconds = 0.0;
    double estimated_remaining_seconds = 0.0;
    int progress_pct = 0;

    if (!state->active || !renderer || !ctx || ctx->renderer != renderer ||
        !fontSystemReady() || !font) {
        return;
    }

    screen_w = (ctx->logical_width > 0) ? ctx->logical_width : ctx->width;
    screen_h = (ctx->logical_height > 0) ? ctx->logical_height : ctx->height;
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }

    progress01 = runtime_native_3d_progress_hud_progress01();
    progress_pct = (int)(progress01 * 100.0 + 0.5);
    if (progress_pct < 0) progress_pct = 0;
    if (progress_pct > 100) progress_pct = 100;
    elapsed_seconds = runtime_native_3d_progress_hud_elapsed_seconds();
    estimated_remaining_seconds =
        runtime_native_3d_progress_hud_estimate_remaining_seconds(elapsed_seconds, progress01);
    runtime_native_3d_progress_hud_format_duration(elapsed_seconds,
                                                   elapsed_value,
                                                   sizeof(elapsed_value));
    if (estimated_remaining_seconds < 0.0) {
        (void)snprintf(estimate_value, sizeof(estimate_value), "--");
    } else {
        runtime_native_3d_progress_hud_format_duration(estimated_remaining_seconds,
                                                       estimate_value,
                                                       sizeof(estimate_value));
    }

    (void)snprintf(title,
                   sizeof(title),
                   "%s %d%%",
                   runtime_native_3d_progress_hud_integrator_label(state->integratorId),
                   progress_pct);
    (void)snprintf(subpass_line,
                   sizeof(subpass_line),
                   "subpass %d/%d",
                   state->startedSubpasses > 0 ? state->startedSubpasses : 1,
                   state->totalSubpasses > 0 ? state->totalSubpasses : 1);
    if (state->totalTilesInSubpass > 0u && state->startedSubpasses > state->completedSubpasses) {
        (void)snprintf(tile_line,
                       sizeof(tile_line),
                       "tiles %zu/%zu",
                       state->completedTilesInSubpass,
                       state->totalTilesInSubpass);
    } else if (state->completedSubpasses >= state->totalSubpasses) {
        (void)snprintf(tile_line, sizeof(tile_line), "frame complete");
    } else {
        (void)snprintf(tile_line, sizeof(tile_line), "starting");
    }
    (void)snprintf(elapsed_line, sizeof(elapsed_line), "T=%s", elapsed_value);
    (void)snprintf(estimate_line, sizeof(estimate_line), "est=%s", estimate_value);

    if (!runtime_native_3d_progress_hud_measure_line(renderer, font, title, &title_w, &title_h) ||
        !runtime_native_3d_progress_hud_measure_line(renderer,
                                                     font,
                                                     subpass_line,
                                                     &subpass_w,
                                                     &subpass_h) ||
        !runtime_native_3d_progress_hud_measure_line(renderer, font, tile_line, &tile_w, &tile_h) ||
        !runtime_native_3d_progress_hud_measure_line(renderer,
                                                     font,
                                                     elapsed_line,
                                                     &elapsed_w,
                                                     &elapsed_h) ||
        !runtime_native_3d_progress_hud_measure_line(renderer,
                                                     font,
                                                     estimate_line,
                                                     &estimate_w,
                                                     &estimate_h)) {
        return;
    }

    left_col_w = subpass_w > tile_w ? subpass_w : tile_w;
    right_col_w = elapsed_w > estimate_w ? elapsed_w : estimate_w;
    max_text_w = title_w;
    if (left_col_w + column_gap + right_col_w > max_text_w) {
        max_text_w = left_col_w + column_gap + right_col_w;
    }

    panel_w = max_text_w + (padding * 2);
    if (panel_w < 270) panel_w = 270;
    panel_h = padding + title_h + line_gap + bar_h + bar_gap + subpass_h + line_gap + tile_h + padding;
    panel.x = screen_w - panel_w - 12;
    panel.y = screen_h - panel_h - 12;
    panel.w = panel_w;
    panel.h = panel_h;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, 10, 14, 18, 190);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 84, 95, 112, 220);
    SDL_RenderDrawRect(renderer, &panel);

    bar_bg.x = panel.x + padding;
    bar_bg.y = panel.y + padding + title_h + line_gap;
    bar_bg.w = panel_w - (padding * 2);
    bar_bg.h = bar_h;
    SDL_SetRenderDrawColor(renderer, 34, 40, 48, 255);
    SDL_RenderFillRect(renderer, &bar_bg);

    bar_fill = bar_bg;
    bar_fill.w = (int)(progress01 * (double)bar_bg.w);
    if (bar_fill.w > 0) {
        SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 255);
        SDL_RenderFillRect(renderer, &bar_fill);
    }

    runtime_native_3d_progress_hud_draw_line(renderer,
                                             font,
                                             title,
                                             panel.x + padding,
                                             panel.y + padding,
                                             title_color);
    runtime_native_3d_progress_hud_draw_line(renderer,
                                             font,
                                             subpass_line,
                                             panel.x + padding,
                                             bar_bg.y + bar_h + bar_gap,
                                             detail_color);
    right_col_x = panel.x + panel_w - padding - right_col_w;
    runtime_native_3d_progress_hud_draw_line(renderer,
                                             font,
                                             elapsed_line,
                                             right_col_x,
                                             bar_bg.y + bar_h + bar_gap,
                                             detail_color);
    runtime_native_3d_progress_hud_draw_line(renderer,
                                             font,
                                             tile_line,
                                             panel.x + padding,
                                             bar_bg.y + bar_h + bar_gap + subpass_h + line_gap,
                                             detail_color);
    runtime_native_3d_progress_hud_draw_line(renderer,
                                             font,
                                             estimate_line,
                                             right_col_x,
                                             bar_bg.y + bar_h + bar_gap + subpass_h + line_gap,
                                             detail_color);
}
