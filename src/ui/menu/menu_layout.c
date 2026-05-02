#include "ui/menu_layout.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "ui/sdl_menu_render.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define MENU_ZONE_GAP 18
#define MENU_LEFT_PANEL_MIN_WIDTH 350
#define MENU_LEFT_PANEL_MAX_WIDTH 390
#define MENU_SLIDER_PANEL_WIDTH 304
#define MENU_ROUTE_STACK_WIDTH 430
#define MENU_ROUTE_STACK_BUTTON_HEIGHT 50
#define MENU_ROUTE_STACK_GAP 8
#define MENU_BOTTOM_ACTION_HEIGHT 64
#define MENU_CENTER_CONTROLS_MIN_HEIGHT 196
#define MENU_CENTER_CONTROLS_MAX_HEIGHT 232
#define MENU_MANIFEST_PANEL_MIN_HEIGHT 140
#define MENU_MANIFEST_PANEL_MAX_HEIGHT 260
#define MENU_MANIFEST_PANEL_GAP 6
#define MENU_LEFT_PANEL_CONTENT_INSET 18
#define MENU_PANEL_BOTTOM_GAP 18
#define MENU_BATCH_MIN_HEIGHT 150

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static int measure_button_width(TTF_Font* font,
                                const char* text,
                                int min_width,
                                int max_width) {
    int width = min_width;
    int text_w = 0;
    int text_h = 0;
    if (font && text && text[0] && TTF_SizeUTF8(font, text, &text_w, &text_h) == 0) {
        width = max_int(width, text_w + 24);
    }
    if (max_width > 0) {
        width = min_int(width, max_width);
    }
    return width;
}

void menu_layout_build_base(TTF_Font* font,
                            const MenuRuntimeState* state,
                            MenuScreenLayout* out_layout) {
    MenuScreenLayout layout;
    const RayTracingIntegratorMenuState integrator_menu =
        RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    const bool show_path_toggles = integrator_menu.showPathToggles;
    const int left_panel_w = min_int(MENU_LEFT_PANEL_MAX_WIDTH,
                                     max_int(MENU_LEFT_PANEL_MIN_WIDTH,
                                             max_int(measure_button_width(font,
                                                                          "Load Scene [Runtime]: ps4d_runtime_scene_visual_test.json (ray)",
                                                                          340,
                                                                          360),
                                                     140 + (56 * 3) + 12) + 20));
    const int bottom_row_y = MENU_HEIGHT - MENU_MARGIN_Y - MENU_BOTTOM_ACTION_HEIGHT;
    const int route_stack_h = MENU_ROUTE_STACK_BUTTON_HEIGHT * 5 + MENU_ROUTE_STACK_GAP * 4 + 20;
    const int route_stack_y = bottom_row_y - MENU_PANEL_BOTTOM_GAP - route_stack_h;
    const int slider_bottom = route_stack_y - MENU_PANEL_BOTTOM_GAP;
    const int center_left = MENU_MARGIN_X + left_panel_w + MENU_ZONE_GAP;
    const int slider_x = MENU_WIDTH - MENU_MARGIN_X - MENU_SLIDER_PANEL_WIDTH;
    int center_width = slider_x - MENU_ZONE_GAP - center_left;
    int center_controls_h = MENU_CENTER_CONTROLS_MIN_HEIGHT + (show_path_toggles ? 56 : 0);
    int center_batch_y = 0;
    int center_batch_h = 0;

    (void)state;

    if (center_controls_h > MENU_CENTER_CONTROLS_MAX_HEIGHT) {
        center_controls_h = MENU_CENTER_CONTROLS_MAX_HEIGHT;
    }
    if (center_width < 300) {
        center_width = 300;
    }

    memset(&layout, 0, sizeof(layout));
    layout.menuRect = (SDL_Rect){0, 0, MENU_WIDTH, MENU_HEIGHT};
    layout.leftPanelRect = (SDL_Rect){
        MENU_MARGIN_X,
        MENU_MARGIN_Y,
        left_panel_w,
        bottom_row_y - MENU_MARGIN_Y - MENU_PANEL_BOTTOM_GAP
    };
    layout.sliderPanelRect = (SDL_Rect){
        slider_x,
        MENU_MARGIN_Y,
        MENU_SLIDER_PANEL_WIDTH,
        slider_bottom - MENU_MARGIN_Y
    };
    layout.routeStackRect = (SDL_Rect){
        MENU_WIDTH - MENU_MARGIN_X - MENU_ROUTE_STACK_WIDTH,
        route_stack_y,
        MENU_ROUTE_STACK_WIDTH,
        route_stack_h
    };
    layout.bottomActionRowRect = (SDL_Rect){
        MENU_MARGIN_X,
        bottom_row_y,
        MENU_WIDTH - MENU_MARGIN_X * 2,
        MENU_BOTTOM_ACTION_HEIGHT
    };
    layout.centerControlsRect = (SDL_Rect){
        center_left,
        MENU_MARGIN_Y,
        center_width,
        center_controls_h
    };
    center_batch_y = layout.centerControlsRect.y + layout.centerControlsRect.h + MENU_ZONE_GAP;
    center_batch_h = layout.bottomActionRowRect.y - MENU_PANEL_BOTTOM_GAP - center_batch_y;
    if (center_batch_h < MENU_BATCH_MIN_HEIGHT) {
        center_batch_h = MENU_BATCH_MIN_HEIGHT;
    }
    layout.centerBatchRect = (SDL_Rect){
        center_left,
        center_batch_y,
        center_width,
        center_batch_h
    };

    if (out_layout) {
        *out_layout = layout;
    }
}

void menu_layout_finalize_with_buttons(MenuScreenLayout* layout,
                                       const MenuButtonLayout* buttons,
                                       const MenuRuntimeState* state) {
    int batch_top = 0;
    int batch_bottom = 0;
    if (!layout || !buttons) return;

    batch_top = layout->centerControlsRect.y + layout->centerControlsRect.h + MENU_ZONE_GAP;
    batch_bottom = layout->bottomActionRowRect.y - MENU_PANEL_BOTTOM_GAP;

    if (state && (state->manifestDropdownOpen || state->volumeDropdownOpen)) {
        const bool use_volume_anchor = state->volumeDropdownOpen;
        const int panel_x = use_volume_anchor ? buttons->attachVolumeRect.x : buttons->loadSceneRect.x;
        const int panel_y = use_volume_anchor
                                ? (buttons->volumeClearRect.y + buttons->volumeClearRect.h + MENU_MANIFEST_PANEL_GAP)
                                : (buttons->inputRootValueRect.y + buttons->inputRootValueRect.h + MENU_MANIFEST_PANEL_GAP);
        const int panel_right_limit = layout->leftPanelRect.x + layout->leftPanelRect.w - MENU_LEFT_PANEL_CONTENT_INSET;
        const int control_right = buttons->inputRootApplyRect.x + buttons->inputRootApplyRect.w;
        const int panel_bottom_limit = layout->leftPanelRect.y + layout->leftPanelRect.h - MENU_LEFT_PANEL_CONTENT_INSET;
        const int panel_w = min_int(panel_right_limit, control_right) - panel_x;
        int panel_h = panel_bottom_limit - panel_y;

        if (panel_h > MENU_MANIFEST_PANEL_MAX_HEIGHT) {
            panel_h = MENU_MANIFEST_PANEL_MAX_HEIGHT;
        }
        if (panel_h < MENU_MANIFEST_PANEL_MIN_HEIGHT &&
            panel_bottom_limit - panel_y >= MENU_MANIFEST_PANEL_MIN_HEIGHT) {
            panel_h = MENU_MANIFEST_PANEL_MIN_HEIGHT;
        }

        if (panel_w > 0 && panel_h > 0) {
            layout->manifestReserveRect = (SDL_Rect){panel_x, panel_y, panel_w, panel_h};
        } else {
            layout->manifestReserveRect = (SDL_Rect){0, 0, 0, 0};
        }
    } else {
        layout->manifestReserveRect = (SDL_Rect){0, 0, 0, 0};
    }

    if (batch_bottom - batch_top < MENU_BATCH_MIN_HEIGHT) {
        batch_top = batch_bottom - MENU_BATCH_MIN_HEIGHT;
    }
    if (batch_top < layout->centerControlsRect.y + layout->centerControlsRect.h + MENU_ZONE_GAP) {
        batch_top = layout->centerControlsRect.y + layout->centerControlsRect.h + MENU_ZONE_GAP;
    }
    if (batch_bottom < batch_top) {
        batch_bottom = batch_top;
    }

    layout->centerBatchRect = (SDL_Rect){
        layout->centerBatchRect.x,
        batch_top,
        layout->centerBatchRect.w,
        batch_bottom - batch_top
    };
}
