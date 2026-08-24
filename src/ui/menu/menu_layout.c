#include "ui/menu_layout.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>

#include "config/config_manager.h"
#include "ui/menu_panel_chrome.h"
#include "ui/sdl_menu_render.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define MENU_ROUTE_STACK_ROW_HEIGHT_MIN 30
#define MENU_ROUTE_STACK_ROW_HEIGHT_MAX 42
#define MENU_ROUTE_STACK_ROW_COUNT 3
#define MENU_ROUTE_STACK_GAP 5
#define MENU_ROUTE_STACK_CONTENT_INSET 10
#define MENU_BOTTOM_ACTION_HEIGHT 64
#define MENU_MANIFEST_PANEL_MIN_HEIGHT 140
#define MENU_MANIFEST_PANEL_MAX_HEIGHT 340
#define MENU_MANIFEST_PANEL_GAP 6
#define MENU_LEFT_PANEL_CONTENT_INSET 18
#define MENU_PANEL_BOTTOM_GAP 18
#define MENU_RENDER_INFO_GAP 6
#define MENU_RENDER_INFO_MIN_HEIGHT 96
#define MENU_EFFECTIVE_SLIDER_MIN_HEIGHT 220

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

bool menu_layout_build_runtime_route_actions(
    const SDL_Rect* route_stack_rect,
    MenuRuntimeRouteActionLayout* out_layout) {
    SDL_Rect content;
    int column_width;
    int row_height;
    int start_y;
    if (!route_stack_rect || !out_layout || route_stack_rect->w <= 0 ||
        route_stack_rect->h <= MENU_PANEL_CHROME_TITLE_BAND) {
        return false;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    content.x = route_stack_rect->x + MENU_ROUTE_STACK_CONTENT_INSET;
    content.y = route_stack_rect->y + MENU_PANEL_CHROME_TITLE_BAND +
                MENU_ROUTE_STACK_CONTENT_INSET;
    content.w = route_stack_rect->w - MENU_ROUTE_STACK_CONTENT_INSET * 2;
    content.h = route_stack_rect->h - MENU_PANEL_CHROME_TITLE_BAND -
                MENU_ROUTE_STACK_CONTENT_INSET * 2;
    if (content.w < 2 || content.h < MENU_ROUTE_STACK_ROW_COUNT) return false;

    row_height = (content.h - MENU_ROUTE_STACK_GAP *
                                  (MENU_ROUTE_STACK_ROW_COUNT - 1)) /
                 MENU_ROUTE_STACK_ROW_COUNT;
    if (row_height < 24) return false;
    column_width = (content.w - MENU_ROUTE_STACK_GAP) / 2;
    if (column_width < 1) return false;
    start_y = content.y + content.h -
              (row_height * MENU_ROUTE_STACK_ROW_COUNT +
               MENU_ROUTE_STACK_GAP * (MENU_ROUTE_STACK_ROW_COUNT - 1));

    out_layout->spaceModeRect =
        (SDL_Rect){content.x, start_y, column_width, row_height};
    out_layout->sceneModeRect =
        (SDL_Rect){content.x + column_width + MENU_ROUTE_STACK_GAP,
                   start_y,
                   content.w - column_width - MENU_ROUTE_STACK_GAP,
                   row_height};
    out_layout->sceneEditorRect =
        (SDL_Rect){content.x,
                   start_y + row_height + MENU_ROUTE_STACK_GAP,
                   column_width,
                   row_height};
    out_layout->previewRect =
        (SDL_Rect){out_layout->sceneModeRect.x,
                   out_layout->sceneEditorRect.y,
                   out_layout->sceneModeRect.w,
                   row_height};
    out_layout->startRect =
        (SDL_Rect){content.x,
                   out_layout->sceneEditorRect.y + row_height +
                       MENU_ROUTE_STACK_GAP,
                   content.w,
                   row_height};
    return true;
}

void menu_layout_build_base(TTF_Font* font,
                            MenuRuntimeState* state,
                            int window_width,
                            int window_height,
                            MenuScreenLayout* out_layout) {
    MenuScreenLayout layout;
    const int menu_width = (window_width > 0) ? window_width : MENU_WIDTH;
    const int menu_height = (window_height > 0) ? window_height : MENU_HEIGHT;
    const int bottom_row_y = menu_height - MENU_MARGIN_Y - MENU_BOTTOM_ACTION_HEIGHT;
    int text_line_height = font ? TTF_FontLineSkip(font) : 18;
    int route_row_height;
    int render_info_preferred_height;
    if (text_line_height < 12) text_line_height = 12;
    route_row_height = text_line_height + 12;
    if (route_row_height < MENU_ROUTE_STACK_ROW_HEIGHT_MIN) {
        route_row_height = MENU_ROUTE_STACK_ROW_HEIGHT_MIN;
    }
    if (route_row_height > MENU_ROUTE_STACK_ROW_HEIGHT_MAX) {
        route_row_height = MENU_ROUTE_STACK_ROW_HEIGHT_MAX;
    }
    render_info_preferred_height = MENU_PANEL_CHROME_TITLE_BAND + 8 +
                                   text_line_height * 5;
    const int route_stack_h = MENU_PANEL_CHROME_TITLE_BAND +
                              route_row_height * MENU_ROUTE_STACK_ROW_COUNT +
                              MENU_ROUTE_STACK_GAP * (MENU_ROUTE_STACK_ROW_COUNT - 1) +
                              MENU_ROUTE_STACK_CONTENT_INSET * 2;
    const int pane_bottom = bottom_row_y - MENU_PANEL_BOTTOM_GAP;
    const SDL_Rect pane_bounds = {
        MENU_MARGIN_X,
        MENU_MARGIN_Y,
        menu_width - MENU_MARGIN_X * 2,
        pane_bottom - MENU_MARGIN_Y
    };
    const RayTracingMenuPaneLayout* pane_layout = NULL;
    SDL_Rect scene_rect = {MENU_MARGIN_X, MENU_MARGIN_Y, 390, pane_bounds.h};
    SDL_Rect workspace_rect = {438, MENU_MARGIN_Y, 410, pane_bounds.h};
    SDL_Rect health_rect = {866, MENU_MARGIN_Y, 304, pane_bounds.h};
    int route_stack_y;
    int slider_bottom;
    int render_info_height;
    int effective_height;
    int center_left;
    MenuWorkspaceLayout workspace_layout;

    if (state) {
        if (!state->menuPaneHost.initialized) {
            (void)ray_tracing_menu_pane_host_init(&state->menuPaneHost, pane_bounds);
            ray_tracing_menu_pane_host_set_targets(&state->menuPaneHost,
                                                   animSettings.menuPaneSceneWidth,
                                                   animSettings.menuPaneHealthWidth);
            (void)ray_tracing_menu_pane_host_rebuild(&state->menuPaneHost, pane_bounds);
        } else {
            (void)ray_tracing_menu_pane_host_rebuild(&state->menuPaneHost, pane_bounds);
        }
        pane_layout = ray_tracing_menu_pane_host_layout(&state->menuPaneHost);
    }
    if (pane_layout) {
        scene_rect = pane_layout->scene_rect;
        workspace_rect = pane_layout->workspace_rect;
        health_rect = pane_layout->health_rect;
    }
    center_left = workspace_rect.x;
    route_stack_y = health_rect.y + health_rect.h - route_stack_h;
    slider_bottom = route_stack_y - MENU_PANEL_BOTTOM_GAP;
    effective_height = slider_bottom - health_rect.y;
    render_info_height = render_info_preferred_height;
    if (effective_height < MENU_EFFECTIVE_SLIDER_MIN_HEIGHT +
                               MENU_RENDER_INFO_GAP + render_info_height) {
        render_info_height = max_int(
            MENU_RENDER_INFO_MIN_HEIGHT,
            effective_height - MENU_EFFECTIVE_SLIDER_MIN_HEIGHT -
                MENU_RENDER_INFO_GAP);
    }
    if (render_info_height > effective_height - MENU_RENDER_INFO_GAP) {
        render_info_height = max_int(0, effective_height - MENU_RENDER_INFO_GAP);
    }

    menu_workspace_build_layout(workspace_rect, &workspace_layout);

    memset(&layout, 0, sizeof(layout));
    layout.menuRect = (SDL_Rect){0, 0, menu_width, menu_height};
    layout.leftPanelRect = (SDL_Rect){
        scene_rect.x,
        scene_rect.y,
        scene_rect.w,
        scene_rect.h
    };
    layout.workspace = workspace_layout;
    layout.sliderPanelRect = (SDL_Rect){
        health_rect.x,
        health_rect.y,
        health_rect.w,
        effective_height - render_info_height - MENU_RENDER_INFO_GAP
    };
    layout.renderInfoRect = (SDL_Rect){
        health_rect.x,
        slider_bottom - render_info_height,
        health_rect.w,
        render_info_height
    };
    layout.routeStackRect = (SDL_Rect){
        health_rect.x,
        route_stack_y,
        health_rect.w,
        route_stack_h
    };
    layout.bottomActionRowRect = (SDL_Rect){
        center_left,
        bottom_row_y,
        health_rect.x + health_rect.w - center_left,
        MENU_BOTTOM_ACTION_HEIGHT
    };
    layout.centerControlsRect = (SDL_Rect){
        workspace_layout.content_rect.x,
        workspace_layout.content_rect.y,
        workspace_layout.content_rect.w,
        workspace_layout.content_rect.h
    };
    layout.centerBatchRect = layout.centerControlsRect;
    layout.centerResumeRect = layout.centerControlsRect;

    if (out_layout) {
        *out_layout = layout;
    }
}

void menu_layout_finalize_with_buttons(MenuScreenLayout* layout,
                                       const MenuButtonLayout* buttons,
                                       const MenuRuntimeState* state) {
    if (!layout || !buttons) return;

    if (state && (state->manifestDropdownOpen ||
                  animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D)) {
        const int panel_x = buttons->loadSceneRect.x;
        const int panel_y = buttons->loadSceneRect.y + buttons->loadSceneRect.h + MENU_MANIFEST_PANEL_GAP;
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
}
