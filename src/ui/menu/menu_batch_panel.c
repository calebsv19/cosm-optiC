#include "ui/menu_batch_panel.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "ui/menu_panel_chrome.h"
#include "ui/sdl_menu_render.h"
#include "ui/shared_theme_font_adapter.h"

#define BATCH_PANEL_MIN_HEIGHT 150
#define BATCH_PANEL_MAX_HEIGHT 228
#define BATCH_PANEL_MIN_WIDTH 340
#define BATCH_PANEL_TARGET_WIDTH 432
#define BATCH_PANEL_INSET 12
#define BATCH_PANEL_ROW_HEIGHT 34
#define BATCH_PANEL_ROW_GAP 8
#define BATCH_PANEL_CTRL_BUTTON_W 56
#define BATCH_PANEL_ACTION_HEIGHT 34
#define BATCH_PANEL_ACTION_W 118
#define BATCH_PANEL_HEADER_TEXT_OFFSET_Y 4
#define BATCH_PANEL_HEADER_DIVIDER_GAP 20
#define BATCH_PANEL_HEADER_TO_ROWS_GAP 8

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static bool pick_folder_macos(const char *prompt, char *out_path, size_t out_cap) {
#if defined(__APPLE__)
    FILE *pipe = NULL;
    char cmd[320];
    char line[512];
    if (!prompt || !out_path || out_cap == 0u) return false;
    snprintf(cmd,
             sizeof(cmd),
             "/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"%s\")'",
             prompt);
    pipe = popen(cmd, "r");
    if (!pipe) return false;
    if (!fgets(line, sizeof(line), pipe)) {
        (void)pclose(pipe);
        return false;
    }
    (void)pclose(pipe);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') return false;
    snprintf(out_path, out_cap, "%s", line);
    return true;
#else
    (void)prompt;
    (void)out_path;
    (void)out_cap;
    return false;
#endif
}

static void set_status(MenuRuntimeState *state,
                       const char *label,
                       SDL_Color color,
                       Uint32 ttl_ms) {
    if (!state || !label) return;
    snprintf(state->statusLabel, sizeof(state->statusLabel), "%s", label);
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = color;
    state->statusExpireMs = SDL_GetTicks() + ttl_ms;
}

static void clear_batch_edit_flags(MenuRuntimeState *state) {
    if (!state) return;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
}

static void begin_frame_dir_edit(MenuRuntimeState *state) {
    if (!state) return;
    clear_batch_edit_flags(state);
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = true;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.frameDir);
}

static void begin_video_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    clear_batch_edit_flags(state);
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->editingVideoOutputRoot = true;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.videoOutputRoot);
}

static void apply_frame_dir(MenuRuntimeState *state, const char *path) {
    if (!state || !path || !path[0]) return;
    snprintf(animSettings.frameDir, sizeof(animSettings.frameDir), "%s", path);
    menu_batch_panel_refresh(state);
    set_status(state, "Frames root set", (SDL_Color){120, 220, 180, 255}, 1800);
}

static void apply_video_root(MenuRuntimeState *state, const char *path) {
    if (!state || !path || !path[0]) return;
    snprintf(animSettings.videoOutputRoot, sizeof(animSettings.videoOutputRoot), "%s", path);
    (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    menu_batch_panel_refresh(state);
    set_status(state, "Video root set", (SDL_Color){120, 200, 240, 255}, 1800);
}

static void set_export_action_status(MenuRuntimeState *state,
                                     const RayTracingRenderExportStatus *status,
                                     bool ok) {
    SDL_Color color = ok ? (SDL_Color){120, 220, 180, 255}
                         : (SDL_Color){255, 170, 140, 255};
    if (!state || !status) return;
    set_status(state,
               status->message[0] ? status->message : (ok ? "Done" : "Batch action failed"),
               color,
               ok ? 2200u : 2600u);
}

typedef struct {
    SDL_Renderer *renderer;
    TTF_Font *font;
    MenuRuntimeState *state;
} MenuBatchVideoProgressContext;

static void menu_batch_panel_progress_repaint(const RayTracingRenderExportStatus *status,
                                              void *user_data) {
    MenuBatchVideoProgressContext *ctx = (MenuBatchVideoProgressContext *)user_data;
    if (!ctx || !ctx->renderer || !ctx->font || !ctx->state || !status) return;
    ctx->state->exportBatchStatus = *status;
    set_status(ctx->state,
               status->message,
               (SDL_Color){160, 210, 255, 255},
               60000u);
    SDL_PumpEvents();
    menu_render_frame(ctx->renderer, ctx->font, ctx->state, NULL);
}

static const char* path_basename(const char *path) {
    const char *cursor = NULL;
    if (!path || !path[0]) return "";
    cursor = strrchr(path, '/');
    return cursor ? cursor + 1 : path;
}

void menu_batch_panel_build_layout(TTF_Font* font,
                                   const MenuRuntimeState* state,
                                   const MenuScreenLayout* screen_layout,
                                   MenuBatchPanelLayout* out_layout) {
    MenuBatchPanelLayout layout;
    int value_width = 0;
    int row_y = 0;
    int footer_y = 0;
    (void)font;
    (void)state;

    memset(&layout, 0, sizeof(layout));
    if (!screen_layout || !out_layout) return;

    layout.panelRect = screen_layout->centerBatchRect;
    if (layout.panelRect.w > BATCH_PANEL_TARGET_WIDTH) {
        layout.panelRect.x += (layout.panelRect.w - BATCH_PANEL_TARGET_WIDTH) / 2;
        layout.panelRect.w = BATCH_PANEL_TARGET_WIDTH;
    }
    if (layout.panelRect.w < BATCH_PANEL_MIN_WIDTH) {
        layout.panelRect.w = screen_layout->centerBatchRect.w;
    }
    if (layout.panelRect.h > BATCH_PANEL_MAX_HEIGHT) {
        layout.panelRect.h = BATCH_PANEL_MAX_HEIGHT;
    }
    if (layout.panelRect.h < BATCH_PANEL_MIN_HEIGHT) {
        layout.panelRect.h = BATCH_PANEL_MIN_HEIGHT;
    }

    value_width = layout.panelRect.w - (BATCH_PANEL_CTRL_BUTTON_W * 3) - 22 - BATCH_PANEL_INSET * 2;
    if (value_width < 180) value_width = 180;

    layout.videoFileLabelRect = (SDL_Rect){
        layout.panelRect.x + BATCH_PANEL_INSET,
        layout.panelRect.y + MENU_PANEL_CHROME_TITLE_BAND + BATCH_PANEL_HEADER_TEXT_OFFSET_Y,
        layout.panelRect.w - BATCH_PANEL_INSET * 2,
        16
    };
    layout.headerDividerRect = (SDL_Rect){
        layout.panelRect.x + BATCH_PANEL_INSET,
        layout.videoFileLabelRect.y + BATCH_PANEL_HEADER_DIVIDER_GAP,
        layout.panelRect.w - BATCH_PANEL_INSET * 2,
        1
    };

    row_y = layout.headerDividerRect.y + BATCH_PANEL_HEADER_TO_ROWS_GAP;
    layout.frameDirValueRect = (SDL_Rect){layout.panelRect.x + BATCH_PANEL_INSET,
                                          row_y,
                                          value_width,
                                          BATCH_PANEL_ROW_HEIGHT};
    layout.frameDirEditRect = (SDL_Rect){layout.frameDirValueRect.x + layout.frameDirValueRect.w + 4,
                                         row_y,
                                         BATCH_PANEL_CTRL_BUTTON_W,
                                         BATCH_PANEL_ROW_HEIGHT};
    layout.frameDirFolderRect = (SDL_Rect){layout.frameDirEditRect.x + BATCH_PANEL_CTRL_BUTTON_W + 3,
                                           row_y,
                                           BATCH_PANEL_CTRL_BUTTON_W,
                                           BATCH_PANEL_ROW_HEIGHT};
    layout.frameDirApplyRect = (SDL_Rect){layout.frameDirFolderRect.x + BATCH_PANEL_CTRL_BUTTON_W + 3,
                                          row_y,
                                          BATCH_PANEL_CTRL_BUTTON_W,
                                          BATCH_PANEL_ROW_HEIGHT};

    row_y += BATCH_PANEL_ROW_HEIGHT + BATCH_PANEL_ROW_GAP;
    layout.videoRootValueRect = (SDL_Rect){layout.panelRect.x + BATCH_PANEL_INSET,
                                           row_y,
                                           value_width,
                                           BATCH_PANEL_ROW_HEIGHT};
    layout.videoRootEditRect = (SDL_Rect){layout.videoRootValueRect.x + layout.videoRootValueRect.w + 4,
                                          row_y,
                                          BATCH_PANEL_CTRL_BUTTON_W,
                                          BATCH_PANEL_ROW_HEIGHT};
    layout.videoRootFolderRect = (SDL_Rect){layout.videoRootEditRect.x + BATCH_PANEL_CTRL_BUTTON_W + 3,
                                            row_y,
                                            BATCH_PANEL_CTRL_BUTTON_W,
                                            BATCH_PANEL_ROW_HEIGHT};
    layout.videoRootApplyRect = (SDL_Rect){layout.videoRootFolderRect.x + BATCH_PANEL_CTRL_BUTTON_W + 3,
                                           row_y,
                                           BATCH_PANEL_CTRL_BUTTON_W,
                                           BATCH_PANEL_ROW_HEIGHT};

    footer_y = layout.panelRect.y + layout.panelRect.h - BATCH_PANEL_ACTION_HEIGHT - BATCH_PANEL_INSET;
    layout.frameCountValueRect = (SDL_Rect){layout.panelRect.x + BATCH_PANEL_INSET,
                                            footer_y,
                                            150,
                                            BATCH_PANEL_ACTION_HEIGHT};
    layout.fpsValueRect = (SDL_Rect){layout.frameCountValueRect.x + layout.frameCountValueRect.w + 8,
                                     footer_y,
                                     92,
                                     BATCH_PANEL_ACTION_HEIGHT};
    layout.makeVideoRect = (SDL_Rect){layout.panelRect.x + layout.panelRect.w - BATCH_PANEL_INSET - BATCH_PANEL_ACTION_W,
                                      footer_y,
                                      BATCH_PANEL_ACTION_W,
                                      BATCH_PANEL_ACTION_HEIGHT};
    layout.clearFramesRect = (SDL_Rect){layout.makeVideoRect.x - 8 - BATCH_PANEL_ACTION_W,
                                        footer_y,
                                        BATCH_PANEL_ACTION_W,
                                        BATCH_PANEL_ACTION_HEIGHT};

    *out_layout = layout;
}

void menu_batch_panel_refresh(MenuRuntimeState* state) {
    RayTracingRenderExportStatus status = {0};
    if (!state) return;
    if (!ray_tracing_render_export_describe_active(&status)) {
        state->exportBatchStatus = status;
        return;
    }
    state->exportBatchStatus = status;
}

bool menu_batch_panel_edit_active(const MenuRuntimeState* state) {
    if (!state) return false;
    return state->editingFrameDir || state->editingVideoOutputRoot;
}

void menu_batch_panel_finish_edit(MenuRuntimeState* state, bool apply) {
    if (!state) return;
    if (apply && state->pathInputBuffer[0]) {
        if (state->editingFrameDir) {
            apply_frame_dir(state, state->pathInputBuffer);
        } else if (state->editingVideoOutputRoot) {
            apply_video_root(state, state->pathInputBuffer);
        }
    }
    clear_batch_edit_flags(state);
    state->pathInputBuffer[0] = '\0';
}

void menu_batch_panel_backspace_edit(MenuRuntimeState* state) {
    size_t len = 0;
    if (!menu_batch_panel_edit_active(state)) return;
    len = strlen(state->pathInputBuffer);
    if (len > 0u) {
        state->pathInputBuffer[len - 1u] = '\0';
    }
}

bool menu_batch_panel_append_text(MenuRuntimeState* state, const char* text) {
    if (!state || !text || !text[0] || !menu_batch_panel_edit_active(state)) return false;
    if (strlen(state->pathInputBuffer) + strlen(text) >= sizeof(state->pathInputBuffer)) {
        return false;
    }
    strcat(state->pathInputBuffer, text);
    return true;
}

void menu_batch_panel_build_frame_count_label(const MenuRuntimeState* state,
                                              char* out,
                                              size_t out_size) {
    if (!out || out_size == 0u) return;
    if (!state) {
        snprintf(out, out_size, "Frames: ?");
        return;
    }
    if (state->exportBatchStatus.code == RAY_TRACING_RENDER_EXPORT_FRAME_DIR_INVALID) {
        snprintf(out, out_size, "Frames: ?");
        return;
    }
    snprintf(out, out_size, "Frames: %zu", state->exportBatchStatus.frame_count);
}

bool menu_batch_panel_handle_click(const SDL_Event* event,
                                   SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   MenuRuntimeState* state,
                                   const MenuBatchPanelLayout* layout) {
    int x = 0;
    int y = 0;
    char selected[PATH_MAX];
    RayTracingRenderExportStatus status = {0};
    MenuBatchVideoProgressContext progress_ctx = {0};
    bool ok = false;
    if (!event || !state || !layout) return false;

    x = event->button.x;
    y = event->button.y;
    progress_ctx.renderer = renderer;
    progress_ctx.font = font;
    progress_ctx.state = state;

    if (point_in_rect(&layout->frameDirValueRect, x, y)) {
        if (event->button.clicks >= 2 &&
            pick_folder_macos("Choose render frames root", selected, sizeof(selected))) {
            apply_frame_dir(state, selected);
        } else {
            begin_frame_dir_edit(state);
        }
        return true;
    }
    if (point_in_rect(&layout->videoRootValueRect, x, y)) {
        if (event->button.clicks >= 2 &&
            pick_folder_macos("Choose video output root", selected, sizeof(selected))) {
            apply_video_root(state, selected);
        } else {
            begin_video_root_edit(state);
        }
        return true;
    }
    if (point_in_rect(&layout->frameDirEditRect, x, y)) {
        begin_frame_dir_edit(state);
        return true;
    }
    if (point_in_rect(&layout->videoRootEditRect, x, y)) {
        begin_video_root_edit(state);
        return true;
    }
    if (point_in_rect(&layout->frameDirFolderRect, x, y)) {
        if (pick_folder_macos("Choose render frames root", selected, sizeof(selected))) {
            apply_frame_dir(state, selected);
        }
        return true;
    }
    if (point_in_rect(&layout->videoRootFolderRect, x, y)) {
        if (pick_folder_macos("Choose video output root", selected, sizeof(selected))) {
            apply_video_root(state, selected);
        }
        return true;
    }
    if (point_in_rect(&layout->frameDirApplyRect, x, y)) {
        if (state->editingFrameDir) {
            menu_batch_panel_finish_edit(state, true);
        } else {
            apply_frame_dir(state, animSettings.frameDir);
        }
        return true;
    }
    if (point_in_rect(&layout->videoRootApplyRect, x, y)) {
        if (state->editingVideoOutputRoot) {
            menu_batch_panel_finish_edit(state, true);
        } else {
            apply_video_root(state, animSettings.videoOutputRoot);
        }
        return true;
    }
    if (point_in_rect(&layout->clearFramesRect, x, y)) {
        if (menu_batch_panel_edit_active(state)) {
            menu_batch_panel_finish_edit(state, true);
        }
        ok = ray_tracing_render_export_clear_active_frames(&status);
        menu_batch_panel_refresh(state);
        set_export_action_status(state, &status, ok);
        return true;
    }
    if (point_in_rect(&layout->makeVideoRect, x, y)) {
        if (menu_batch_panel_edit_active(state)) {
            menu_batch_panel_finish_edit(state, true);
        }
        ok = ray_tracing_render_export_make_video_with_progress(&status,
                                                                menu_batch_panel_progress_repaint,
                                                                &progress_ctx);
        menu_batch_panel_refresh(state);
        set_export_action_status(state, &status, ok);
        return true;
    }
    return false;
}

void menu_batch_panel_render(SDL_Renderer* renderer,
                             TTF_Font* font,
                             MenuRuntimeState* state,
                             const MenuBatchPanelLayout* layout) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    char frame_count[64];
    char fps_label[32];
    char video_file[80];
    SDL_Rect divider;
    SDL_Color info_color;
    if (!renderer || !font || !state || !layout) return;

    menu_panel_chrome_draw(renderer, font, &layout->panelRect, "Data I/O + Batch", true);

    info_color = has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 180, 255};
    snprintf(video_file,
             sizeof(video_file),
             "Video file: %s",
             path_basename(state->exportBatchStatus.video_output_path[0]
                               ? state->exportBatchStatus.video_output_path
                               : "output.mp4"));
    menu_render_fit_text_to_width(font,
                                  video_file,
                                  layout->panelRect.w - BATCH_PANEL_INSET * 2,
                                  video_file,
                                  sizeof(video_file));
    menu_render_draw_text_color(renderer,
                                font,
                                layout->videoFileLabelRect.x,
                                layout->videoFileLabelRect.y,
                                info_color,
                                video_file);
    divider = layout->headerDividerRect;
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, 220);
    } else {
        SDL_SetRenderDrawColor(renderer, 92, 96, 108, 255);
    }
    SDL_RenderFillRect(renderer, &divider);

    menu_render_draw_root_row(renderer,
                              font,
                              &layout->frameDirValueRect,
                              "Render Frames Root",
                              state->editingFrameDir ? state->pathInputBuffer : animSettings.frameDir,
                              state->editingFrameDir);
    menu_render_draw_button_rect(renderer, font, &layout->frameDirEditRect, "Edit", state->editingFrameDir);
    menu_render_draw_button_rect(renderer, font, &layout->frameDirFolderRect, "Folder", false);
    menu_render_draw_button_rect(renderer, font, &layout->frameDirApplyRect, "Apply", false);

    menu_render_draw_root_row(renderer,
                              font,
                              &layout->videoRootValueRect,
                              "Video Output Root",
                              state->editingVideoOutputRoot ? state->pathInputBuffer : animSettings.videoOutputRoot,
                              state->editingVideoOutputRoot);
    menu_render_draw_button_rect(renderer, font, &layout->videoRootEditRect, "Edit", state->editingVideoOutputRoot);
    menu_render_draw_button_rect(renderer, font, &layout->videoRootFolderRect, "Folder", false);
    menu_render_draw_button_rect(renderer, font, &layout->videoRootApplyRect, "Apply", false);

    menu_batch_panel_build_frame_count_label(state, frame_count, sizeof(frame_count));
    menu_render_draw_root_row(renderer,
                              font,
                              &layout->frameCountValueRect,
                              "Frames",
                              frame_count + strlen("Frames: "),
                              false);
    snprintf(fps_label, sizeof(fps_label), "FPS: %d", animSettings.fps);
    menu_render_draw_button_rect(renderer, font, &layout->fpsValueRect, fps_label, false);
    menu_render_draw_button_rect(renderer, font, &layout->clearFramesRect, "Clear Frames", false);
    menu_render_draw_button_rect(renderer, font, &layout->makeVideoRect, "Make Video", true);
}
