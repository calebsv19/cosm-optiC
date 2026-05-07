#include "editor/material_editor_knob_control.h"

#include <math.h>
#include <stdio.h>

#include "render/render_helper.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double material_editor_knob_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static void material_editor_knob_draw_disk(SDL_Renderer* renderer,
                                           int cx,
                                           int cy,
                                           int radius,
                                           SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -radius; y <= radius; ++y) {
        int span = (int)sqrt((double)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
}

double MaterialEditorKnobValueFromDrag(double start_value, int start_y, int current_y) {
    double delta = (double)(start_y - current_y) * 0.006;
    return material_editor_knob_clamp01(start_value + delta);
}

void MaterialEditorKnobDraw(SDL_Renderer* renderer,
                            SDL_Rect bounds,
                            const char* label,
                            double value,
                            RayTracingThemePalette palette) {
    int diameter = 0;
    int radius = 0;
    int cx = 0;
    int cy = 0;
    double angle = 0.0;
    int ix = 0;
    int iy = 0;
    char value_text[16];
    SDL_Color knob_fill = palette.panel_fill;
    SDL_Color knob_border = palette.panel_border;
    SDL_Color cell_fill = palette.background_fill;
    SDL_Rect cell = {bounds.x, bounds.y, bounds.w, bounds.h};
    if (!renderer || !label || bounds.w <= 0 || bounds.h <= 0) return;
    value = material_editor_knob_clamp01(value);
    diameter = bounds.w - 12;
    if (bounds.h - 32 < diameter) diameter = bounds.h - 32;
    if (diameter > 26) diameter = 26;
    if (diameter < 16) diameter = 16;
    radius = diameter / 2;
    cx = bounds.x + bounds.w / 2;
    cy = bounds.y + bounds.h - radius - 6;
    knob_fill.a = 255;
    knob_border.a = 255;
    cell_fill.a = 120;
    SDL_SetRenderDrawColor(renderer, cell_fill.r, cell_fill.g, cell_fill.b, cell_fill.a);
    SDL_RenderFillRect(renderer, &cell);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           150);
    SDL_RenderDrawRect(renderer, &cell);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x + 4, bounds.y + 3, bounds.w - 8, 13},
                        label,
                        palette.text_primary);
    snprintf(value_text, sizeof(value_text), "%.2f", value);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x + 4, bounds.y + 17, bounds.w - 8, 12},
                        value_text,
                        palette.text_muted);
    material_editor_knob_draw_disk(renderer, cx, cy, radius, knob_fill);
    SDL_SetRenderDrawColor(renderer, knob_border.r, knob_border.g, knob_border.b, 255);
    for (int r = radius - 1; r <= radius; ++r) {
        for (int a = 0; a < 360; a += 8) {
            double t = (double)a * M_PI / 180.0;
            int x = cx + (int)lround(cos(t) * (double)r);
            int y = cy + (int)lround(sin(t) * (double)r);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    angle = (-135.0 + value * 270.0) * M_PI / 180.0;
    ix = cx + (int)lround(cos(angle) * (double)(radius - 4));
    iy = cy + (int)lround(sin(angle) * (double)(radius - 4));
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_primary.r,
                           palette.accent_primary.g,
                           palette.accent_primary.b,
                           255);
    SDL_RenderDrawLine(renderer, cx, cy, ix, iy);
}
