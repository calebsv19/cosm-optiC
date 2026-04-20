#include "editor/scene_editor_pane_host.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    SCENE_EDITOR_MIN_WIDTH = 780,
    SCENE_EDITOR_MIN_HEIGHT = 420,
    SCENE_EDITOR_MIN_LEFT_WIDTH = 220,
    SCENE_EDITOR_MAX_LEFT_WIDTH = 420,
    SCENE_EDITOR_MIN_RIGHT_WIDTH = 240,
    SCENE_EDITOR_MAX_RIGHT_WIDTH = 460,
    SCENE_EDITOR_MIN_CENTER_WIDTH = 360,
    SCENE_EDITOR_MIN_PANE_HEIGHT = 200,
    SCENE_EDITOR_SHELL_PADDING = 0,
    SCENE_EDITOR_CONTENT_PADDING = 10,
    SCENE_EDITOR_PANE_HEADER_HEIGHT = 28,
    SCENE_EDITOR_MODE_ROUTER_HEIGHT = 44,
    SCENE_EDITOR_VIEWPORT_TOP_GAP = 8
};

static int pane_host_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static SDL_Rect pane_host_inset_rect(SDL_Rect rect, int inset) {
    SDL_Rect out = rect;
    out.x += inset;
    out.y += inset;
    out.w -= inset * 2;
    out.h -= inset * 2;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Rect pane_host_reserve_top_space(SDL_Rect rect, int top) {
    SDL_Rect out = rect;
    if (top <= 0) return out;
    if (out.h <= 0) return out;
    if (top > out.h) top = out.h;
    out.y += top;
    out.h -= top;
    return out;
}

static void pane_host_set_error(SceneEditorPaneHost* host, const char* fmt, ...) {
    va_list args;
    if (!host || !fmt) return;
    host->last_error[0] = '\0';
    va_start(args, fmt);
    (void)vsnprintf(host->last_error, sizeof(host->last_error), fmt, args);
    va_end(args);
}

bool scene_editor_pane_host_rebuild(SceneEditorPaneHost* host, int width, int height) {
    int shell_w = 0;
    int shell_h = 0;
    int left_w = 0;
    int right_w = 0;
    int center_w = 0;
    int x = 0;
    int y = 0;
    int mode_h = SCENE_EDITOR_MODE_ROUTER_HEIGHT;
    SDL_Rect viewport = {0};

    if (!host) return false;
    if (width < SCENE_EDITOR_MIN_WIDTH || height < SCENE_EDITOR_MIN_HEIGHT) {
        pane_host_set_error(host, "invalid editor bounds %dx%d", width, height);
        return false;
    }

    shell_w = width - SCENE_EDITOR_SHELL_PADDING * 2;
    shell_h = height - SCENE_EDITOR_SHELL_PADDING * 2;
    if (shell_h < SCENE_EDITOR_MIN_PANE_HEIGHT) {
        pane_host_set_error(host, "insufficient editor height %d", shell_h);
        return false;
    }

    left_w = host->target_left_width > 0 ? host->target_left_width : 286;
    left_w = pane_host_clamp_int(left_w, SCENE_EDITOR_MIN_LEFT_WIDTH, SCENE_EDITOR_MAX_LEFT_WIDTH);
    right_w = host->target_right_width > 0 ? host->target_right_width : 312;
    right_w = pane_host_clamp_int(right_w, SCENE_EDITOR_MIN_RIGHT_WIDTH, SCENE_EDITOR_MAX_RIGHT_WIDTH);

    center_w = shell_w - left_w - right_w - SCENE_EDITOR_SHELL_PADDING * 2;
    if (center_w < SCENE_EDITOR_MIN_CENTER_WIDTH) {
        int deficit = SCENE_EDITOR_MIN_CENTER_WIDTH - center_w;
        int trim_right = deficit / 2;
        int trim_left = deficit - trim_right;
        right_w -= trim_right;
        left_w -= trim_left;
        right_w = pane_host_clamp_int(right_w, SCENE_EDITOR_MIN_RIGHT_WIDTH, SCENE_EDITOR_MAX_RIGHT_WIDTH);
        left_w = pane_host_clamp_int(left_w, SCENE_EDITOR_MIN_LEFT_WIDTH, SCENE_EDITOR_MAX_LEFT_WIDTH);
        center_w = shell_w - left_w - right_w - SCENE_EDITOR_SHELL_PADDING * 2;
        if (center_w < SCENE_EDITOR_MIN_CENTER_WIDTH) {
            pane_host_set_error(host, "cannot satisfy pane minimums in %dx%d", width, height);
            return false;
        }
    }

    x = SCENE_EDITOR_SHELL_PADDING;
    y = SCENE_EDITOR_SHELL_PADDING;
    host->layout.left_pane_rect = (SDL_Rect){x, y, left_w, shell_h};
    x += left_w + SCENE_EDITOR_SHELL_PADDING;
    host->layout.center_pane_rect = (SDL_Rect){x, y, center_w, shell_h};
    x += center_w + SCENE_EDITOR_SHELL_PADDING;
    host->layout.right_pane_rect = (SDL_Rect){x, y, right_w, shell_h};

    host->layout.left_content_rect = pane_host_inset_rect(host->layout.left_pane_rect,
                                                          SCENE_EDITOR_CONTENT_PADDING);
    host->layout.center_content_rect = pane_host_inset_rect(host->layout.center_pane_rect,
                                                            SCENE_EDITOR_CONTENT_PADDING);
    host->layout.right_content_rect = pane_host_inset_rect(host->layout.right_pane_rect,
                                                           SCENE_EDITOR_CONTENT_PADDING);
    host->layout.left_content_rect = pane_host_reserve_top_space(host->layout.left_content_rect,
                                                                  SCENE_EDITOR_PANE_HEADER_HEIGHT);
    host->layout.center_content_rect = pane_host_reserve_top_space(host->layout.center_content_rect,
                                                                    SCENE_EDITOR_PANE_HEADER_HEIGHT);
    host->layout.right_content_rect = pane_host_reserve_top_space(host->layout.right_content_rect,
                                                                   SCENE_EDITOR_PANE_HEADER_HEIGHT);

    mode_h = pane_host_clamp_int(mode_h, 32, host->layout.center_content_rect.h / 3);
    host->layout.mode_router_rect = (SDL_Rect){
        host->layout.center_content_rect.x,
        host->layout.center_content_rect.y,
        host->layout.center_content_rect.w,
        mode_h
    };

    viewport = host->layout.center_content_rect;
    viewport.y += mode_h + SCENE_EDITOR_VIEWPORT_TOP_GAP;
    viewport.h -= mode_h + SCENE_EDITOR_VIEWPORT_TOP_GAP;
    if (viewport.h < 0) viewport.h = 0;
    host->layout.viewport_rect = viewport;

    host->initialized = true;
    host->last_error[0] = '\0';
    return true;
}

bool scene_editor_pane_host_init(SceneEditorPaneHost* host, int width, int height) {
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    host->target_left_width = 286;
    host->target_right_width = 312;
    return scene_editor_pane_host_rebuild(host, width, height);
}

void scene_editor_pane_host_set_targets(SceneEditorPaneHost* host,
                                        int left_width,
                                        int right_width) {
    if (!host) return;
    if (left_width > 0) host->target_left_width = left_width;
    if (right_width > 0) host->target_right_width = right_width;
}

const SceneEditorPaneLayout* scene_editor_pane_host_layout(const SceneEditorPaneHost* host) {
    if (!host || !host->initialized) return NULL;
    return &host->layout;
}

const char* scene_editor_pane_host_last_error(const SceneEditorPaneHost* host) {
    if (!host || host->last_error[0] == '\0') return "";
    return host->last_error;
}
