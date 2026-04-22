#include "ui/sdl_menu_input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor.h"
#include "engine/Render/render_font.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/text_zoom_shortcuts.h"

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

static void begin_input_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingInputRoot = true;
    state->editingOutputRoot = false;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.inputRoot);
}

static void begin_output_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingOutputRoot = true;
    state->editingInputRoot = false;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.outputRoot);
}

static void apply_input_root(MenuRuntimeState *state, const char *path) {
    if (!state || !path || !path[0]) return;
    snprintf(animSettings.inputRoot, sizeof(animSettings.inputRoot), "%s", path);
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    menu_state_refresh_manifest_options(state);
    snprintf(state->statusLabel, sizeof(state->statusLabel), "Input root set");
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = (SDL_Color){120, 220, 180, 255};
    state->statusExpireMs = SDL_GetTicks() + 1800;
}

static void apply_output_root(MenuRuntimeState *state, const char *path) {
    if (!state || !path || !path[0]) return;
    snprintf(animSettings.outputRoot, sizeof(animSettings.outputRoot), "%s", path);
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    snprintf(state->statusLabel, sizeof(state->statusLabel), "Output root set");
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = (SDL_Color){120, 200, 240, 255};
    state->statusExpireMs = SDL_GetTicks() + 1800;
}

static void finish_root_edit(MenuRuntimeState *state, bool apply) {
    if (!state) return;
    if (apply && state->pathInputBuffer[0]) {
        if (state->editingInputRoot) {
            apply_input_root(state, state->pathInputBuffer);
        } else if (state->editingOutputRoot) {
            apply_output_root(state, state->pathInputBuffer);
        }
    }
    state->editingInputRoot = false;
    state->editingOutputRoot = false;
    state->pathInputBuffer[0] = '\0';
}

static void handle_slider_click(SDL_Event* event,
                                const SliderLayout* layout,
                                MenuRuntimeState* state) {
    if (!event || !layout || !state) return;
    int x = event->button.x;
    int y = event->button.y;
    if (!point_in_rect(&layout->panelRect, x, y)) return;
    for (size_t i = 0; i < layout->count; i++) {
        const MenuSlider* slider = &layout->items[i];
        if (point_in_rect(&slider->hitRect, x, y)) {
            state->draggingSlider = true;
            state->selectedSlider = slider->value;
            state->selectedSliderMin = slider->min;
            state->selectedSliderMax = slider->max;
            state->sliderStartX = slider->trackRect.x;
            state->sliderWidth = slider->trackRect.w;

            bool adjustCamera = (state->selectedSlider == &sceneSettings.windowWidth ||
                                 state->selectedSlider == &sceneSettings.windowHeight);
            int prevWidth = sceneSettings.windowWidth;
            int prevHeight = sceneSettings.windowHeight;

            float percent = (float)(x - slider->trackRect.x) / slider->trackRect.w;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            int newValue = slider->min + percent * (slider->max - slider->min);

            if (newValue < slider->min) newValue = slider->min;
            if (newValue > slider->max) newValue = slider->max;

            *state->selectedSlider = newValue;
            menu_state_apply_special_slider_rules(state, state->selectedSlider);
            if (adjustCamera &&
                (prevWidth != sceneSettings.windowWidth || prevHeight != sceneSettings.windowHeight)) {
                menu_state_reanchor_camera_after_resize(prevWidth, prevHeight);
                state->oldWindowWidth = sceneSettings.windowWidth;
                state->oldWindowHeight = sceneSettings.windowHeight;
            }
            return;
        }
    }
}

void menu_input_handle_key(SDL_Event* event,
                           bool* running,
                           TTF_Font** font,
                           MenuRuntimeState* state) {
    if (!event || !running || !font || !state) return;
    SDL_Keymod mod = event->key.keysym.mod;
    bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    bool shift = (mod & KMOD_SHIFT) != 0;
    bool zoom_changed = false;
    int zoom_step = 0;
    int zoom_percent = 100;

    if (ctrl_or_cmd && shift) {
        if (event->key.keysym.sym == SDLK_b) {
            char selected[PATH_MAX];
            if (pick_folder_macos("Choose optiC Output Root", selected, sizeof(selected))) {
                apply_output_root(state, selected);
            }
            return;
        }
        if (event->key.keysym.sym == SDLK_i) {
            begin_input_root_edit(state);
            return;
        }
        if (event->key.keysym.sym == SDLK_o) {
            begin_output_root_edit(state);
            return;
        }
        if (event->key.keysym.sym == SDLK_t) {
            ray_tracing_shared_theme_cycle_next();
            ray_tracing_shared_theme_save_persisted();
            menu_state_reload_font(font);
            return;
        }
        if (event->key.keysym.sym == SDLK_y) {
            ray_tracing_shared_theme_cycle_prev();
            ray_tracing_shared_theme_save_persisted();
            menu_state_reload_font(font);
            return;
        }
    }
    if (ctrl_or_cmd && event->key.keysym.sym == SDLK_b) {
        char selected[PATH_MAX];
        if (pick_folder_macos("Choose optiC Input Root", selected, sizeof(selected))) {
            apply_input_root(state, selected);
        }
        return;
    }
    if (ray_tracing_text_zoom_apply_shortcut(event->key.keysym.sym,
                                             mod,
                                             &zoom_changed,
                                             &zoom_step,
                                             &zoom_percent)) {
        if (zoom_changed) {
            menu_state_reload_font(font);
        }
        snprintf(state->statusLabel, sizeof(state->statusLabel), "Text %d%%", zoom_percent);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){120, 210, 240, 255};
        state->statusExpireMs = SDL_GetTicks() + 1700;
        (void)zoom_step;
        return;
    }

    switch (event->key.keysym.sym) {
        case SDLK_BACKSPACE:
            if ((state->editingInputRoot || state->editingOutputRoot) &&
                strlen(state->pathInputBuffer) > 0) {
                state->pathInputBuffer[strlen(state->pathInputBuffer) - 1] = '\0';
                break;
            }
            if ((state->editingBounce || state->editingFrame) && strlen(state->inputBuffer) > 0) {
                state->inputBuffer[strlen(state->inputBuffer) - 1] = '\0';
            }
            break;
        case SDLK_RETURN:
            if (state->editingInputRoot || state->editingOutputRoot) {
                finish_root_edit(state, true);
                break;
            }
            if (strlen(state->inputBuffer) > 0) {
                int newValue = atoi(state->inputBuffer);
                if (state->editingBounce) animSettings.bounceLimit = newValue;
                if (state->editingFrame) animSettings.frameLimit = newValue;
            }
            state->editingBounce = false;
            state->editingFrame = false;
            state->inputBuffer[0] = '\0';
            break;
        case SDLK_ESCAPE:
            if (state->editingInputRoot || state->editingOutputRoot) {
                finish_root_edit(state, false);
                break;
            }
            *running = false;
            break;
        case SDLK_i:
            animSettings.interactiveMode = true;
            animSettings.deepRenderMode = false;
            break;
        case SDLK_d:
            animSettings.deepRenderMode = true;
            animSettings.interactiveMode = false;
            break;
        case SDLK_b:
            if (animSettings.deepRenderMode) animSettings.bounceMode = !animSettings.bounceMode;
            break;
        case SDLK_m:
            if (animSettings.deepRenderMode) animSettings.autoMP4 = !animSettings.autoMP4;
            break;
        case SDLK_p:
            if (animSettings.deepRenderMode) {
                state->activeView = MENU_VIEW_SCENE_EDITOR;
            }
            break;
        case SDLK_r:
            menu_state_reset_defaults(state);
            (void)refreshActiveFontFromAnimationConfig();
            menu_state_reload_font(font);
            break;
        default:
            break;
    }
}

void menu_input_handle_mouse_motion(SDL_Event* event, MenuRuntimeState* state) {
    if (!event || !state) return;
    if (state->manifestScrollbarDragging && state->manifestDropdownOpen && state->manifestScrollbarVisible) {
        float trackRange = state->manifestTrackHeight - state->manifestThumbHeight;
        if (trackRange < 1.0f) trackRange = 1.0f;
        int deltaY = event->motion.y - state->manifestDragStartY;
        float newScroll = state->manifestScrollStart + ((float)deltaY * state->manifestMaxScroll / trackRange);
        state->manifestScroll = newScroll;
        menu_state_manifest_clamp_scroll(state);
    }

    if (!state->draggingSlider || !state->selectedSlider) return;

    int x = event->motion.x;

    bool adjustCamera = (state->selectedSlider == &sceneSettings.windowWidth ||
                         state->selectedSlider == &sceneSettings.windowHeight);
    int prevWidth = sceneSettings.windowWidth;
    int prevHeight = sceneSettings.windowHeight;

    float percent = (float)(x - state->sliderStartX) / state->sliderWidth;
    int newValue = state->selectedSliderMin + percent * (state->selectedSliderMax - state->selectedSliderMin);

    if (newValue < state->selectedSliderMin) newValue = state->selectedSliderMin;
    if (newValue > state->selectedSliderMax) newValue = state->selectedSliderMax;

    *state->selectedSlider = newValue;
    menu_state_apply_special_slider_rules(state, state->selectedSlider);
    if (adjustCamera &&
        (prevWidth != sceneSettings.windowWidth || prevHeight != sceneSettings.windowHeight)) {
        menu_state_reanchor_camera_after_resize(prevWidth, prevHeight);
        state->oldWindowWidth = sceneSettings.windowWidth;
        state->oldWindowHeight = sceneSettings.windowHeight;
    }
}

void menu_input_handle_mouse_wheel(SDL_Event *event, MenuRuntimeState* state) {
    if (!event || !state) return;
    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);
    if (state->manifestDropdownOpen && point_in_rect(&state->manifestPanelRect, mx, my)) {
        float delta = (float)event->wheel.y * (float)(SDL_MENU_MANIFEST_ITEM_HEIGHT * 2);
        menu_state_manifest_scroll_by(state, -delta);
        return;
    }
    if (point_in_rect(&state->sliderPanelRect, mx, my) && state->sliderMaxScroll > 0.5f) {
        float delta = (float)event->wheel.y * 28.0f;
        state->sliderScroll = menu_state_slider_clamp_scroll(state->sliderScroll - delta, state->sliderMaxScroll);
    }
}

void menu_input_handle_mouse_click(SDL_Event* event,
                                   bool* running,
                                   bool* menuExitedNormally,
                                   SDL_Renderer* renderer,
                                   TTF_Font** font,
                                   MenuRuntimeState* state) {
    (void)renderer;
    if (!event || !running || !menuExitedNormally || !font || !state || !*font) return;

    MenuButtonLayout buttons;
    SliderLayout layout;
    menu_render_build_button_layout(*font, state, &buttons);
    menu_render_build_slider_layout(*font, state, &buttons, &layout);
    handle_slider_click(event, &layout, state);

    int x = event->button.x;
    int y = event->button.y;

    if (state->manifestDropdownOpen) {
        if (point_in_rect(&state->manifestPanelRect, x, y)) {
            if (state->manifestScrollbarVisible && point_in_rect(&state->manifestScrollbarRect, x, y)) {
                state->manifestScrollbarDragging = true;
                state->manifestDragStartY = y;
                state->manifestScrollStart = state->manifestScroll;
                return;
            }
            if (state->manifestOptionCount > 0 && point_in_rect(&state->manifestListRect, x, y)) {
                int relativeY = y - state->manifestListRect.y + (int)state->manifestScroll;
                int visible_indices[SDL_MENU_MAX_MANIFEST_OPTIONS];
                int visible_count = 0;
                int row_idx = 0;
                int idx = -1;
                for (int i = 0; i < (int)state->manifestOptionCount; ++i) {
                    if (menu_state_manifest_option_visible(state, &state->manifestOptions[i])) {
                        visible_indices[visible_count++] = i;
                    }
                }
                row_idx = relativeY / SDL_MENU_MANIFEST_ITEM_HEIGHT;
                if (row_idx >= 0 && row_idx < visible_count) {
                    idx = visible_indices[row_idx];
                }
                if (idx >= 0 && idx < (int)state->manifestOptionCount) {
                    int source = animation_config_scene_source_clamp(state->manifestOptions[idx].source);
                    bool ok = AnimationSelectSceneSource(source,
                                                         state->manifestOptions[idx].path,
                                                         true);
                    if (ok) {
                        if (source == SCENE_SOURCE_RUNTIME_SCENE) {
                            strncpy(state->statusLabel, "Runtime scene set", sizeof(state->statusLabel) - 1);
                        } else if (source == SCENE_SOURCE_FLUID_MANIFEST) {
                            strncpy(state->statusLabel, "Scene set", sizeof(state->statusLabel) - 1);
                        } else {
                            strncpy(state->statusLabel, "2D config active", sizeof(state->statusLabel) - 1);
                        }
                        SaveAnimationConfig();
                        state->statusColor = (SDL_Color){140, 220, 200, 255};
                    } else {
                        strncpy(state->statusLabel, "Scene apply failed", sizeof(state->statusLabel) - 1);
                        state->statusColor = (SDL_Color){240, 120, 120, 255};
                    }
                    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
                    state->statusExpireMs = SDL_GetTicks() + 1800;
                    menu_state_sync_from_anim(state);
                    return;
                }
            }
            return;
        }
    }

    if (point_in_rect(&buttons.loadSceneRect, x, y)) {
        menu_state_set_load_scene_enabled(state, !state->manifestDropdownOpen);
        return;
    }

    if (point_in_rect(&buttons.inputRootFolderRect, x, y)) {
        char selected[PATH_MAX];
        if (pick_folder_macos("Choose optiC Input Root", selected, sizeof(selected))) {
            apply_input_root(state, selected);
        }
        return;
    }
    if (point_in_rect(&buttons.outputRootFolderRect, x, y)) {
        char selected[PATH_MAX];
        if (pick_folder_macos("Choose optiC Output Root", selected, sizeof(selected))) {
            apply_output_root(state, selected);
        }
        return;
    }
    if (point_in_rect(&buttons.inputRootEditRect, x, y) || point_in_rect(&buttons.inputRootValueRect, x, y)) {
        begin_input_root_edit(state);
        return;
    }
    if (point_in_rect(&buttons.outputRootEditRect, x, y) || point_in_rect(&buttons.outputRootValueRect, x, y)) {
        begin_output_root_edit(state);
        return;
    }
    if (point_in_rect(&buttons.inputRootApplyRect, x, y)) {
        if (state->editingInputRoot) {
            finish_root_edit(state, true);
        } else {
            apply_input_root(state, animSettings.inputRoot);
        }
        return;
    }
    if (point_in_rect(&buttons.outputRootApplyRect, x, y)) {
        if (state->editingOutputRoot) {
            finish_root_edit(state, true);
        } else {
            apply_output_root(state, animSettings.outputRoot);
        }
        return;
    }

    if (point_in_rect(&buttons.interactiveRect, x, y)) {
        animSettings.interactiveMode = true;
        animSettings.deepRenderMode = false;
        return;
    }

    if (point_in_rect(&buttons.deepRenderRect, x, y)) {
        animSettings.deepRenderMode = true;
        animSettings.interactiveMode = false;
        return;
    }

    if (animSettings.deepRenderMode) {
        if (point_in_rect(&buttons.bounceRect, x, y)) {
            animSettings.bounceMode = !animSettings.bounceMode;
            return;
        }

        if (point_in_rect(&buttons.autoMp4Rect, x, y)) {
            animSettings.autoMP4 = !animSettings.autoMP4;
            return;
        }

        if (point_in_rect(&buttons.sceneEditorRect, x, y)) {
            state->activeView = MENU_VIEW_SCENE_EDITOR;
            return;
        }

        if (point_in_rect(&buttons.sceneModeRect, x, y)) {
            animSettings.editorMode = EditorModeRouter_NextEditorMode(animSettings.editorMode,
                                                                       false,
                                                                       AnimationUseFluidScene());
            const char* newModeText = (animSettings.editorMode == 0) ? "Path" :
                                      (animSettings.editorMode == 1) ? "Scene" : "Camera";
            printf("Scene Editor Mode Toggled: %s\n", newModeText);
            return;
        }
    }

    if (point_in_rect(&buttons.spaceModeRect, x, y)) {
        const char* mode_status = NULL;
        animSettings.spaceMode = (animSettings.spaceMode == SPACE_MODE_3D)
                                     ? SPACE_MODE_2D
                                     : SPACE_MODE_3D;
        menu_state_sync_from_anim(state);
        menu_state_refresh_manifest_options(state);
        if (state->manifestDropdownOpen) {
            state->manifestScroll = 0.0f;
            state->manifestScrollbarDragging = false;
        }
        mode_status = (animSettings.spaceMode == SPACE_MODE_3D)
                          ? (EditorModeRouter_IsControlled3D()
                                 ? "3D compat fallback active (2D backend)"
                                 : "Space: 3D active")
                          : "Space: 2D active";
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "%s",
                 mode_status);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){255, 220, 140, 255};
        state->statusExpireMs = SDL_GetTicks() + 2200;
        printf("Space Mode Toggled: %s\n", menu_space_mode_button_label());
        return;
    }

    if (point_in_rect(&buttons.falloffRect, x, y)) {
        animSettings.forwardFalloffMode = (animSettings.forwardFalloffMode + 1) % 3;
        return;
    }

    if (point_in_rect(&buttons.tileRect, x, y)) {
        animSettings.useTiledRenderer = !animSettings.useTiledRenderer;
        return;
    }
    if (point_in_rect(&buttons.tilePreviewRect, x, y)) {
        animSettings.tilePreviewEnabled = !animSettings.tilePreviewEnabled;
        return;
    }

    if (buttons.showLightHeight && point_in_rect(&buttons.lightHeightRect, x, y)) {
        double options[] = {2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0};
        int count = (int)(sizeof(options) / sizeof(options[0]));
        int idx = 0;
        double current = animSettings.lightHeight;
        for (int i = 0; i < count; i++) {
            if (fabs(options[i] - current) < 1e-3) { idx = i; break; }
            if (options[i] > current) { idx = i; break; }
            idx = i;
        }
        idx = (idx + 1) % count;
        animSettings.lightHeight = options[idx];
        return;
    }

    if (point_in_rect(&buttons.integratorRect, x, y)) {
        animSettings.integratorMode = (animSettings.integratorMode + 1) % 3;
        menu_state_sync_from_anim(state);
        return;
    }

    if (buttons.showPathToggles) {
        if (point_in_rect(&buttons.pathRouletteRect, x, y)) {
            animSettings.pathRussianRoulette = !animSettings.pathRussianRoulette;
            return;
        }
        if (point_in_rect(&buttons.pathBsdfRect, x, y)) {
            animSettings.bsdfModel = (animSettings.bsdfModel == 0) ? 1 : 0;
            menu_state_sync_from_anim(state);
            return;
        }
    }

    if (point_in_rect(&buttons.saveRect, x, y)) {
        if (state->editingInputRoot || state->editingOutputRoot) {
            finish_root_edit(state, true);
        }
        SaveAllSettings();
        strncpy(state->statusLabel, "Saved", sizeof(state->statusLabel) - 1);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){120, 220, 120, 255};
        state->statusExpireMs = SDL_GetTicks() + 2000;
    }

    if (point_in_rect(&buttons.restoreRect, x, y)) {
        menu_state_reset_defaults(state);
        (void)refreshActiveFontFromAnimationConfig();
        menu_state_reload_font(font);
        strncpy(state->statusLabel, "Restored", sizeof(state->statusLabel) - 1);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){200, 180, 120, 255};
        state->statusExpireMs = SDL_GetTicks() + 2000;
    }

    if (point_in_rect(&buttons.previewRect, x, y)) {
        if (state->editingInputRoot || state->editingOutputRoot) {
            finish_root_edit(state, true);
        }
        menu_state_sync_from_anim(state);
        animSettings.previewMode = false;
        SaveAllSettings();
        SceneEditorSessionRequestPreviewOnBegin();
        state->activeView = MENU_VIEW_SCENE_EDITOR;
        return;
    }

    if (point_in_rect(&buttons.exitRect, x, y)) {
        *running = false;
    }

    if (point_in_rect(&buttons.startRect, x, y)) {
        if (state->editingInputRoot || state->editingOutputRoot) {
            finish_root_edit(state, true);
        }
        menu_state_sync_from_anim(state);
        printf("[Menu] Start pressed: integrator=%d falloffMode=%d decay=%.2f softness=%.2f intensity=%.2f\n",
               animSettings.integratorMode,
               animSettings.forwardFalloffMode,
               animSettings.forwardDecay,
               animSettings.lightDecaySoftness,
               animSettings.lightIntensity);
        SaveAllSettings();
        animSettings.previewMode = false;
        *menuExitedNormally = true;
        *running = false;
    }
}
