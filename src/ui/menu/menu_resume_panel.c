#include "ui/menu_resume_panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_panel_chrome.h"
#include "ui/sdl_menu_render.h"

#define RESUME_PANEL_MIN_WIDTH 340
#define RESUME_PANEL_TARGET_WIDTH 432
#define RESUME_PANEL_INSET 12
#define RESUME_PANEL_ROW_HEIGHT 34
#define RESUME_PANEL_ROW_GAP 8
#define RESUME_PANEL_STATUS_HEIGHT 28

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void begin_resume_start_frame_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingStartFrame = true;
    state->editingBounce = false;
    state->editingFrame = false;
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    state->inputBuffer[0] = '\0';
    snprintf(state->inputBuffer, sizeof(state->inputBuffer), "%d", animSettings.startFrameIndex);
}

static void set_resume_status(MenuRuntimeState *state, const char *label) {
    if (!state || !label) return;
    snprintf(state->statusLabel, sizeof(state->statusLabel), "%s", label);
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = (SDL_Color){160, 210, 255, 255};
    state->statusExpireMs = SDL_GetTicks() + 1800;
}

void menu_resume_panel_build_layout(TTF_Font* font,
                                    const MenuRuntimeState* state,
                                    const MenuScreenLayout* screen_layout,
                                    MenuResumePanelLayout* out_layout) {
    MenuResumePanelLayout layout;
    int content_x;
    int content_y;
    int content_w;
    int half_w;

    (void)font;
    (void)state;
    memset(&layout, 0, sizeof(layout));
    if (!screen_layout || screen_layout->centerResumeRect.w <= 0 ||
        screen_layout->centerResumeRect.h <= 0) {
        if (out_layout) *out_layout = layout;
        return;
    }

    layout.panelRect = screen_layout->centerResumeRect;
    if (layout.panelRect.w > RESUME_PANEL_TARGET_WIDTH) {
        layout.panelRect.x += (layout.panelRect.w - RESUME_PANEL_TARGET_WIDTH) / 2;
        layout.panelRect.w = RESUME_PANEL_TARGET_WIDTH;
    }
    if (layout.panelRect.w < RESUME_PANEL_MIN_WIDTH) {
        layout.panelRect.w = screen_layout->centerResumeRect.w;
    }

    content_x = layout.panelRect.x + RESUME_PANEL_INSET;
    content_y = layout.panelRect.y + MENU_PANEL_CHROME_TITLE_BAND + 10;
    content_w = layout.panelRect.w - RESUME_PANEL_INSET * 2;
    half_w = (content_w - RESUME_PANEL_ROW_GAP) / 2;
    if (half_w < 120) half_w = 120;

    layout.statusRect = (SDL_Rect){content_x, content_y, content_w, RESUME_PANEL_STATUS_HEIGHT};
    content_y += RESUME_PANEL_STATUS_HEIGHT + RESUME_PANEL_ROW_GAP;
    layout.resumeToggleRect = (SDL_Rect){content_x, content_y, half_w, RESUME_PANEL_ROW_HEIGHT};
    layout.startFrameRect = (SDL_Rect){
        content_x + content_w - half_w,
        content_y,
        half_w,
        RESUME_PANEL_ROW_HEIGHT
    };
    content_y += RESUME_PANEL_ROW_HEIGHT + RESUME_PANEL_ROW_GAP;
    layout.nextExistingRect = (SDL_Rect){content_x, content_y, content_w, RESUME_PANEL_ROW_HEIGHT};

    if (out_layout) {
        *out_layout = layout;
    }
}

bool menu_resume_panel_handle_click(const SDL_Event* event,
                                    MenuRuntimeState* state,
                                    const MenuResumePanelLayout* layout) {
    int x;
    int y;
    char status[64];

    if (!event || !state || !layout || event->type != SDL_MOUSEBUTTONDOWN) return false;
    x = event->button.x;
    y = event->button.y;
    if (!point_in_rect(&layout->panelRect, x, y)) return false;

    if (point_in_rect(&layout->resumeToggleRect, x, y)) {
        animSettings.resumeFromExistingFrames = !animSettings.resumeFromExistingFrames;
        state->editingStartFrame = false;
        state->inputBuffer[0] = '\0';
        menu_batch_panel_refresh(state);
        return true;
    }

    if (point_in_rect(&layout->startFrameRect, x, y)) {
        animSettings.resumeFromExistingFrames = false;
        begin_resume_start_frame_edit(state);
        return true;
    }

    if (point_in_rect(&layout->nextExistingRect, x, y)) {
        animSettings.resumeFromExistingFrames = false;
        animSettings.startFrameIndex = state->exportBatchStatus.next_frame_index;
        state->editingStartFrame = false;
        state->inputBuffer[0] = '\0';
        snprintf(status, sizeof(status), "Start frame set to %d", animSettings.startFrameIndex);
        set_resume_status(state, status);
        return true;
    }

    return false;
}

void menu_resume_panel_render(SDL_Renderer* renderer,
                              TTF_Font* font,
                              MenuRuntimeState* state,
                              const MenuResumePanelLayout* layout) {
    char status_value[96];
    char resume_label[64];
    char start_label[64];
    char next_label[64];

    if (!renderer || !font || !state || !layout || layout->panelRect.w <= 0 ||
        layout->panelRect.h <= 0) {
        return;
    }

    menu_panel_chrome_draw(renderer, font, &layout->panelRect, "Frame Resume", true);

    snprintf(status_value,
             sizeof(status_value),
             "Saved: %zu  Next: %d",
             state->exportBatchStatus.frame_count,
             state->exportBatchStatus.next_frame_index);
    menu_render_draw_root_row(renderer,
                              font,
                              &layout->statusRect,
                              "Existing Frames",
                              status_value,
                              false);

    snprintf(resume_label,
             sizeof(resume_label),
             "Resume: %s",
             animSettings.resumeFromExistingFrames ? "ON" : "OFF");
    if (state->editingStartFrame && state->inputBuffer[0] != '\0') {
        snprintf(start_label, sizeof(start_label), "Start: %s", state->inputBuffer);
    } else {
        snprintf(start_label, sizeof(start_label), "Start: %d", animSettings.startFrameIndex);
    }
    snprintf(next_label,
             sizeof(next_label),
             "Use Next Existing: %d",
             state->exportBatchStatus.next_frame_index);

    menu_render_draw_button_rect(renderer,
                                 font,
                                 &layout->resumeToggleRect,
                                 resume_label,
                                 animSettings.resumeFromExistingFrames);
    menu_render_draw_button_rect(renderer,
                                 font,
                                 &layout->startFrameRect,
                                 start_label,
                                 state->editingStartFrame || !animSettings.resumeFromExistingFrames);
    menu_render_draw_button_rect(renderer, font, &layout->nextExistingRect, next_label, false);
}
