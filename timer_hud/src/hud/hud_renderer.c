#include "hud_renderer.h"
#include "hud_snapshot.h"
#include "../core/session.h"

#include <stdio.h>
#include <string.h>

#define HUD_DEFAULT_PADDING 6
#define HUD_DEFAULT_SPACING 4
#define HUD_DEFAULT_BG_ALPHA 40
#define HUD_TEXT_COLOR (TimerHUDColor){255, 255, 255, 255}
#define HUD_GRAPH_COLOR (TimerHUDColor){64, 225, 255, 255}
#define HUD_GRAPH_BG_COLOR (TimerHUDColor){18, 18, 18, 120}
#define HUD_SCALE_TEXT_COLOR (TimerHUDColor){180, 180, 180, 255}
#define HUD_GRAPH_ROW_PADDING 3
#define HUD_GRAPH_TEXT_GAP 2

static int get_padding(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_padding > 0) return session->backend->hud_padding;
    return HUD_DEFAULT_PADDING;
}

static int get_spacing(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_spacing > 0) return session->backend->hud_spacing;
    return HUD_DEFAULT_SPACING;
}

static int get_bg_alpha(const TimerHUDSession* session) {
    if (session && session->backend && session->backend->hud_bg_alpha > 0) return session->backend->hud_bg_alpha;
    return HUD_DEFAULT_BG_ALPHA;
}

static int backend_get_line_height(const TimerHUDSession* session) {
    if (!session || !session->backend) return 0;
    if (session->backend->get_line_height) {
        return session->backend->get_line_height();
    }
    if (session->backend->measure_text) {
        int w = 0;
        int h = 0;
        if (session->backend->measure_text("Ag", &w, &h)) {
            return h;
        }
    }
    return 0;
}

static int backend_measure_text(const TimerHUDSession* session, const char* text, int* out_w, int* out_h) {
    if (session && session->backend && session->backend->measure_text) {
        return session->backend->measure_text(text, out_w, out_h);
    }
    if (out_w) *out_w = (int)strlen(text) * 8;
    if (out_h) *out_h = 14;
    return 0;
}

static int mode_has_graph(TimerHUDVisualMode mode) {
    return mode == TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH || mode == TIMER_HUD_VISUAL_MODE_HYBRID;
}

static int anchor_is_right_aligned(TimerHUDAnchor anchor) {
    return anchor == TIMER_HUD_ANCHOR_TOP_RIGHT || anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT;
}

static int anchor_is_bottom_aligned(TimerHUDAnchor anchor) {
    return anchor == TIMER_HUD_ANCHOR_BOTTOM_LEFT || anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT;
}

static void draw_graph(const TimerHUDSession* session,
                       const TimerHUDGraphSnapshot* graph,
                       int x,
                       int y,
                       int w,
                       int h) {
    if (!session || !graph || w <= 1 || h <= 1) {
        return;
    }

    session->backend->draw_rect(x, y, w, h, HUD_GRAPH_BG_COLOR);

    if (graph->sample_count == 0) {
        return;
    }

    int prev_x = 0;
    int prev_y = 0;
    int has_prev = 0;

    for (size_t i = 0; i < graph->sample_count; ++i) {
        double normalized = graph->samples[i] / graph->scale_max_ms;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;

        int px = x;
        if (graph->sample_count > 1) {
            px = x + (int)((double)(w - 1) * (double)i / (double)(graph->sample_count - 1));
        }

        int py = y + h - 1 - (int)((double)(h - 1) * normalized);

        if (session->backend->draw_line && has_prev) {
            session->backend->draw_line(prev_x, prev_y, px, py, HUD_GRAPH_COLOR);
        } else if (!session->backend->draw_line) {
            int bar_h = (y + h) - py;
            if (bar_h < 1) bar_h = 1;
            session->backend->draw_rect(px, py, 1, bar_h, HUD_GRAPH_COLOR);
        }

        prev_x = px;
        prev_y = py;
        has_prev = 1;
    }

    char scale_line[64];
    snprintf(scale_line, sizeof(scale_line), "%.2fms max", graph->scale_max_ms);
    int scale_x = x + w - 2;
    int scale_y = y + 1;
    session->backend->draw_text(scale_line,
                                scale_x,
                                scale_y,
                                TIMER_HUD_ALIGN_TOP | TIMER_HUD_ALIGN_RIGHT,
                                HUD_SCALE_TEXT_COLOR);
}

void hud_set_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    if (!session) {
        return;
    }
    session->backend = backend;
}

void hud_init(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    memset(session->display_max_ms, 0, sizeof(session->display_max_ms));
    if (session->backend && session->backend->init) {
        session->backend->init();
    }
}

void hud_shutdown(TimerHUDSession* session) {
    if (session && session->backend && session->backend->shutdown) {
        session->backend->shutdown();
    }
}

void hud_render(TimerHUDSession* session) {
    TimerHUDRenderSnapshot snapshot;

    if (!session || !session->backend || !session->backend->draw_text || !session->backend->draw_rect) {
        return;
    }
    if (!hud_snapshot_build(session, &snapshot)) {
        return;
    }
    if (!snapshot.hud_enabled || snapshot.row_count <= 0) {
        return;
    }

    int graph_enabled = mode_has_graph(snapshot.visual_mode);
    int padding = get_padding(session);
    int graph_padding = graph_enabled ? HUD_GRAPH_ROW_PADDING : padding;
    int spacing = get_spacing(session);

    int screenW = 0;
    int screenH = 0;
    if (!session->backend->get_screen_size || !session->backend->get_screen_size(&screenW, &screenH)) {
        screenW = 800;
        screenH = 600;
    }

    int fontHeight = backend_get_line_height(session);
    if (fontHeight <= 0) fontHeight = 14;

    int row_h[MAX_TIMERS];

    int maxWidth = 0;
    int totalHeight = 0;

    for (int i = 0; i < snapshot.row_count; ++i) {
        int textW = 0;
        int textH = 0;
        backend_measure_text(session, snapshot.rows[i].text, &textW, &textH);
        int content_w = textW;
        if (graph_enabled && snapshot.graph_width > content_w) {
            content_w = snapshot.graph_width;
        }

        int h = fontHeight + graph_padding * 2;
        if (graph_enabled) {
            h += HUD_GRAPH_TEXT_GAP + snapshot.graph_height;
        }
        row_h[i] = h;

        int row_w = content_w + graph_padding * 2;
        if (row_w > maxWidth) maxWidth = row_w;

        totalHeight += h + spacing;
    }
    totalHeight -= spacing;

    int offsetX = snapshot.offset_x;
    int offsetY = snapshot.offset_y;

    int baseX = 0;
    int baseY = 0;
    int rightAlign = anchor_is_right_aligned(snapshot.anchor);

    if (snapshot.anchor == TIMER_HUD_ANCHOR_TOP_LEFT) {
        baseX = offsetX;
        baseY = offsetY;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_TOP_RIGHT) {
        baseX = screenW - offsetX - maxWidth;
        baseY = offsetY;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_BOTTOM_LEFT) {
        baseX = offsetX;
        baseY = screenH - offsetY - totalHeight;
    } else if (snapshot.anchor == TIMER_HUD_ANCHOR_BOTTOM_RIGHT) {
        baseX = screenW - offsetX - maxWidth;
        baseY = screenH - offsetY - totalHeight;
    } else {
        baseX = offsetX;
        baseY = offsetY;
    }

    if (anchor_is_bottom_aligned(snapshot.anchor)) {
        baseY = screenH - offsetY - totalHeight;
    }

    int y = baseY;
    TimerHUDColor bg = {0, 0, 0, (unsigned char)get_bg_alpha(session)};

    for (int i = 0; i < snapshot.row_count; ++i) {
        int card_x = baseX;
        int card_w = maxWidth;
        int card_h = row_h[i];

        session->backend->draw_rect(card_x, y, card_w, card_h, bg);

        int text_x = rightAlign ? (card_x + card_w - graph_padding) : (card_x + graph_padding);
        int text_align = TIMER_HUD_ALIGN_TOP | (rightAlign ? TIMER_HUD_ALIGN_RIGHT : TIMER_HUD_ALIGN_LEFT);
        session->backend->draw_text(snapshot.rows[i].text, text_x, y + graph_padding, text_align, HUD_TEXT_COLOR);

        if (snapshot.rows[i].has_graph) {
            int graph_w = snapshot.graph_width;
            int graph_x = rightAlign ? (card_x + card_w - graph_padding - graph_w) : (card_x + graph_padding);
            int graph_y = y + graph_padding + fontHeight + HUD_GRAPH_TEXT_GAP;

            draw_graph(session, &snapshot.rows[i].graph, graph_x, graph_y, graph_w, snapshot.graph_height);
        }

        y += card_h + spacing;
    }
}
