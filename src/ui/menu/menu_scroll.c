#include "ui/menu_scroll.h"
#include <math.h>
#include "ui/shared_theme_font_adapter.h"

static void set_offset(MenuScroll *scroll, float offset) {
    if (offset < 0) offset = 0;
    if (offset > scroll->maximum) offset = scroll->maximum;
    *scroll->offset = offset;
    kit_ui_sdl_scrollbar_layout(&scroll->viewport,
        scroll->viewport.h + (int)ceilf(scroll->maximum), (int)offset, &scroll->layout);
}

void menu_scroll_setup(MenuScroll *scroll, SDL_Rect viewport, int content_height, float *offset) {
    scroll->viewport = viewport;
    scroll->offset = offset;
    scroll->maximum = fmaxf(0, content_height - viewport.h);
    scroll->visible = viewport.w > 0 && viewport.h > 0;
    if (!scroll->visible || scroll->maximum == 0) scroll->dragging = false;
    set_offset(scroll, *offset);
}

void menu_scroll_draw(SDL_Renderer *renderer, const MenuScroll *scroll) {
    if (!scroll->visible) return;
    RayTracingThemePalette palette;
    KitRenderColor track = {65,68,75,90}, thumb = {180,185,195,210};
    if (ray_tracing_shared_theme_resolve_palette(&palette)) {
        track = (KitRenderColor){palette.panel_border.r, palette.panel_border.g, palette.panel_border.b, 90};
        thumb = (KitRenderColor){palette.text_muted.r, palette.text_muted.g, palette.text_muted.b, 210};
    }
    kit_ui_sdl_draw_scrollbar(renderer, &scroll->layout,
        track, thumb);
}

bool menu_scroll_event(MenuScroll *scroll, const SDL_Event *event) {
    SDL_Point point;
    if ((event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
        bool dragging = scroll->dragging;
        scroll->dragging = false;
        return dragging;
    }
    if (!scroll->visible || !scroll->offset || scroll->maximum <= 0) return false;
    if (event->type == SDL_MOUSEMOTION && scroll->dragging) {
        int travel = scroll->layout.track.h - scroll->layout.thumb.h;
        if (travel > 0) set_offset(scroll, scroll->dragOffset +
            (event->motion.y - scroll->dragY) * scroll->maximum / travel);
        return true;
    }
    if (event->type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&point.x, &point.y);
        if (!SDL_PointInRect(&point, &scroll->viewport)) return false;
        float amount = (float)event->wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (event->wheel.preciseY != 0) amount = event->wheel.preciseY;
#endif
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) amount = -amount;
        set_offset(scroll, *scroll->offset - amount * 32.0f);
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        point = (SDL_Point){event->button.x, event->button.y};
        SDL_Rect hit = scroll->layout.track;
        hit.x -= 3; hit.w += 6;
        if (!SDL_PointInRect(&point, &hit)) return false;
        if (point.y >= scroll->layout.thumb.y && point.y < scroll->layout.thumb.y + scroll->layout.thumb.h) {
            scroll->dragging = true;
            scroll->dragY = point.y;
            scroll->dragOffset = *scroll->offset;
        } else {
            set_offset(scroll, *scroll->offset +
                (point.y < scroll->layout.thumb.y ? -1 : 1) * scroll->viewport.h * 0.9f);
        }
        return true;
    }
    return false;
}
