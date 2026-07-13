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
#include "engine/Render/render_pipeline.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "import/runtime_scene_bridge.h"
#include "platform/ray_tracing_folder_picker.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_resume_panel.h"
#include "ui/scene_source_ui_labels.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/text_zoom_shortcuts.h"
#include "ui/volume_source_ui_labels.h"

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static bool path_edit_active(const MenuRuntimeState *state) {
    if (!state) return false;
    return state->editingInputRoot ||
           state->editingMeshAssetRoot ||
           state->editingOutputRoot ||
           menu_batch_panel_edit_active(state);
}

static bool scene_manifest_list_visible(const MenuRuntimeState *state) {
    return state &&
           (state->manifestDropdownOpen ||
            animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D);
}

static void begin_input_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingInputRoot = true;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.inputRoot);
}

static void begin_mesh_asset_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingMeshAssetRoot = true;
    state->editingInputRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.meshAssetRoot);
}

static void begin_output_root_edit(MenuRuntimeState *state) {
    if (!state) return;
    state->editingOutputRoot = true;
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    snprintf(state->pathInputBuffer, sizeof(state->pathInputBuffer), "%s", animSettings.outputRoot);
}

static void apply_input_root(MenuRuntimeState *state, const char *path) {
    if (!state || !path || !path[0]) return;
    snprintf(animSettings.inputRoot, sizeof(animSettings.inputRoot), "%s", path);
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    menu_state_refresh_manifest_options(state);
    menu_state_refresh_volume_options(state);
    SaveAnimationConfig();
    snprintf(state->statusLabel, sizeof(state->statusLabel), "Input root set");
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = (SDL_Color){120, 220, 180, 255};
    state->statusExpireMs = SDL_GetTicks() + 1800;
}

static void apply_mesh_asset_root(MenuRuntimeState *state, const char *path) {
    RuntimeSceneBridgePreflight summary = {0};
    if (!state || !path) return;
    snprintf(animSettings.meshAssetRoot, sizeof(animSettings.meshAssetRoot), "%s", path);
    if (animSettings.meshAssetRoot[0]) {
        (void)setenv("RAY_TRACING_MESH_ASSET_ROOT", animSettings.meshAssetRoot, 1);
    } else {
        (void)unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    SaveAnimationConfig();
    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0] != '\0') {
        (void)runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &summary);
    }
    snprintf(state->statusLabel, sizeof(state->statusLabel), "Mesh asset root set");
    state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
    state->statusColor = (SDL_Color){150, 210, 255, 255};
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
    if (menu_batch_panel_edit_active(state)) {
        menu_batch_panel_finish_edit(state, apply);
        return;
    }
    if (apply && state->pathInputBuffer[0]) {
        if (state->editingInputRoot) {
            apply_input_root(state, state->pathInputBuffer);
        } else if (state->editingMeshAssetRoot) {
            apply_mesh_asset_root(state, state->pathInputBuffer);
        } else if (state->editingOutputRoot) {
            apply_output_root(state, state->pathInputBuffer);
        }
    }
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->pathInputBuffer[0] = '\0';
}

static void finish_start_frame_edit(MenuRuntimeState *state, bool apply) {
    if (!state) return;
    if (apply && state->inputBuffer[0]) {
        int newValue = atoi(state->inputBuffer);
        if (newValue < 0) newValue = 0;
        animSettings.startFrameIndex = newValue;
    }
    state->editingStartFrame = false;
    state->inputBuffer[0] = '\0';
}

static bool handle_slider_click(SDL_Event* event,
                                const SliderLayout* layout,
                                MenuRuntimeState* state) {
    if (!event || !layout || !state) return false;
    int x = event->button.x;
    int y = event->button.y;
    if (!point_in_rect(&layout->panelRect, x, y)) return false;
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
            return true;
        }
    }
    return false;
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
            if (RayTracing_FolderPicker_Select("Choose optiC Output Root", animSettings.outputRoot, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED) {
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
        if (RayTracing_FolderPicker_Select("Choose optiC Input Root", animSettings.inputRoot, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED) {
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
            if (menu_batch_panel_edit_active(state)) {
                menu_batch_panel_backspace_edit(state);
                break;
            }
            if ((state->editingInputRoot ||
                 state->editingMeshAssetRoot ||
                 state->editingOutputRoot) &&
                strlen(state->pathInputBuffer) > 0) {
                state->pathInputBuffer[strlen(state->pathInputBuffer) - 1] = '\0';
                break;
            }
            if ((state->editingBounce || state->editingFrame || state->editingStartFrame) &&
                strlen(state->inputBuffer) > 0) {
                state->inputBuffer[strlen(state->inputBuffer) - 1] = '\0';
            }
            break;
        case SDLK_RETURN:
            if (path_edit_active(state)) {
                finish_root_edit(state, true);
                break;
            }
            if (state->editingStartFrame) {
                finish_start_frame_edit(state, true);
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
            if (path_edit_active(state)) {
                finish_root_edit(state, false);
                break;
            }
            if (state->editingStartFrame) {
                finish_start_frame_edit(state, false);
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
    if (ray_tracing_menu_pane_host_splitter_drag_active(&state->menuPaneHost)) {
        if (ray_tracing_menu_pane_host_update_splitter_drag(&state->menuPaneHost,
                                                            (float)event->motion.x,
                                                            (float)event->motion.y)) {
            animSettings.menuPaneSceneWidth = state->menuPaneHost.target_scene_width;
            animSettings.menuPaneHealthWidth = state->menuPaneHost.target_health_width;
        }
        return;
    }
    if (!state->draggingSlider && !state->manifestScrollbarDragging &&
        !state->volumeScrollbarDragging) {
        ray_tracing_menu_pane_host_update_pointer(&state->menuPaneHost,
                                                  (float)event->motion.x,
                                                  (float)event->motion.y);
    }
    if (state->manifestScrollbarDragging && scene_manifest_list_visible(state) && state->manifestScrollbarVisible) {
        float trackRange = state->manifestTrackHeight - state->manifestThumbHeight;
        if (trackRange < 1.0f) trackRange = 1.0f;
        int deltaY = event->motion.y - state->manifestDragStartY;
        float newScroll = state->manifestScrollStart + ((float)deltaY * state->manifestMaxScroll / trackRange);
        state->manifestScroll = newScroll;
        menu_state_manifest_clamp_scroll(state);
    }
    if (state->volumeScrollbarDragging && state->volumeDropdownOpen && state->volumeScrollbarVisible) {
        float trackRange = state->volumeTrackHeight - state->volumeThumbHeight;
        if (trackRange < 1.0f) trackRange = 1.0f;
        int deltaY = event->motion.y - state->volumeDragStartY;
        float newScroll = state->volumeScrollStart + ((float)deltaY * state->volumeMaxScroll / trackRange);
        state->volumeScroll = newScroll;
        menu_state_volume_clamp_scroll(state);
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
    if (scene_manifest_list_visible(state) && point_in_rect(&state->manifestPanelRect, mx, my)) {
        float delta = (float)event->wheel.y * (float)(SDL_MENU_MANIFEST_ITEM_HEIGHT * 2);
        menu_state_manifest_scroll_by(state, -delta);
        return;
    }
    if (state->volumeDropdownOpen && point_in_rect(&state->volumePanelRect, mx, my)) {
        float delta = (float)event->wheel.y * (float)(SDL_MENU_MANIFEST_ITEM_HEIGHT * 2);
        menu_state_volume_scroll_by(state, -delta);
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
    MenuScreenLayout screenLayout;
    SliderLayout layout;
    MenuBatchPanelLayout batchLayout;
    MenuResumePanelLayout resumeLayout;
    RenderContext* render_ctx = getRenderContext();
    int menu_width = 1200;
    int menu_height = 900;
    if (render_ctx && render_ctx->window) {
        SDL_GetWindowSize(render_ctx->window, &menu_width, &menu_height);
    }
    menu_layout_build_base(*font, state, menu_width, menu_height, &screenLayout);
    if (ray_tracing_menu_pane_host_begin_splitter_drag(&state->menuPaneHost,
                                                       (float)event->button.x,
                                                       (float)event->button.y)) {
        state->draggingSlider = false;
        state->selectedSlider = NULL;
        return;
    }
    menu_render_build_button_layout(*font, state, &screenLayout, &buttons);
    menu_layout_finalize_with_buttons(&screenLayout, &buttons, state);
    menu_render_build_slider_layout(*font, state, &screenLayout, &layout);
    menu_batch_panel_build_layout(*font, state, &screenLayout, &batchLayout);
    menu_resume_panel_build_layout(*font, state, &screenLayout, &resumeLayout);
    int x = event->button.x;
    int y = event->button.y;
    {
        int workspace_tab = menu_workspace_tab_at_point(&screenLayout.workspace, x, y);
        if (workspace_tab >= 0) {
            (void)menu_workspace_host_select(&state->menuWorkspaceHost,
                                             (MenuWorkspaceModule)workspace_tab);
            animSettings.menuWorkspaceModule =
                (int)state->menuWorkspaceHost.active_module;
            state->draggingSlider = false;
            state->selectedSlider = NULL;
            return;
        }
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        handle_slider_click(event, &buttons.rendererControlSliders, state)) {
        return;
    }
    if (handle_slider_click(event, &layout, state)) {
        return;
    }

    if (scene_manifest_list_visible(state)) {
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
                                                         false);
                    if (ok) {
                        scene_source_ui_format_scene_select_status(source,
                                                                   state->manifestOptions[idx].path,
                                                                   state->statusLabel,
                                                                   sizeof(state->statusLabel));
                        SaveAnimationConfig();
                        state->statusColor = (SDL_Color){140, 220, 200, 255};
                    } else {
                        strncpy(state->statusLabel, "Scene selection failed", sizeof(state->statusLabel) - 1);
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
    if (state->volumeDropdownOpen) {
        if (point_in_rect(&state->volumePanelRect, x, y)) {
            if (state->volumeScrollbarVisible && point_in_rect(&state->volumeScrollbarRect, x, y)) {
                state->volumeScrollbarDragging = true;
                state->volumeDragStartY = y;
                state->volumeScrollStart = state->volumeScroll;
                return;
            }
            if (state->volumeOptionCount > 0 && point_in_rect(&state->volumeListRect, x, y)) {
                int relativeY = y - state->volumeListRect.y + (int)state->volumeScroll;
                int idx = relativeY / SDL_MENU_MANIFEST_ITEM_HEIGHT;
                if (idx >= 0 && idx < (int)state->volumeOptionCount) {
                    int kind = animation_config_volume_source_kind_clamp(state->volumeOptions[idx].kind);
                    bool ok = AnimationSelectVolumeSource(kind,
                                                          state->volumeOptions[idx].path,
                                                          true);
                    if (ok) {
                        volume_source_ui_format_attach_status(kind,
                                                              state->volumeOptions[idx].path,
                                                              true,
                                                              state->statusLabel,
                                                              sizeof(state->statusLabel));
                        SaveAnimationConfig();
                        state->statusColor = (SDL_Color){140, 220, 200, 255};
                    } else {
                        strncpy(state->statusLabel, "Volume attach failed", sizeof(state->statusLabel) - 1);
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

    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        point_in_rect(&buttons.rendererLightingTabRect, x, y)) {
        state->rendererControlsTab = MENU_RENDERER_CONTROLS_LIGHTING;
        state->draggingSlider = false;
        state->selectedSlider = NULL;
        return;
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        point_in_rect(&buttons.rendererPerformanceTabRect, x, y)) {
        state->rendererControlsTab = MENU_RENDERER_CONTROLS_PERFORMANCE;
        state->draggingSlider = false;
        state->selectedSlider = NULL;
        return;
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        point_in_rect(&buttons.rendererCausticsTabRect, x, y)) {
        state->rendererControlsTab = MENU_RENDERER_CONTROLS_CAUSTICS;
        state->draggingSlider = false;
        state->selectedSlider = NULL;
        return;
    }

    if (point_in_rect(&buttons.loadSceneRect, x, y)) {
        if (animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D) {
            menu_state_refresh_manifest_options(state);
            state->manifestScroll = 0.0f;
            state->manifestScrollbarDragging = false;
            return;
        }
        menu_state_set_load_scene_enabled(state, !state->manifestDropdownOpen);
        return;
    }
    if (buttons.attachVolumeRect.w > 0 && point_in_rect(&buttons.attachVolumeRect, x, y)) {
        menu_state_set_volume_load_enabled(state, !state->volumeDropdownOpen);
        return;
    }
    if (buttons.volumeToggleRect.w > 0 && point_in_rect(&buttons.volumeToggleRect, x, y)) {
        if (animSettings.volumeSourcePath[0] &&
            animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind) != VOLUME_SOURCE_NONE) {
            animSettings.volumeInteractionEnabled = !animSettings.volumeInteractionEnabled;
            menu_state_sync_from_anim(state);
            SaveAnimationConfig();
            snprintf(state->statusLabel,
                     sizeof(state->statusLabel),
                     "Atmosphere: %s",
                     animSettings.volumeInteractionEnabled ? "ON" : "OFF");
            state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
            state->statusColor = (SDL_Color){160, 210, 255, 255};
            state->statusExpireMs = SDL_GetTicks() + 1800;
        } else {
            strncpy(state->statusLabel, "No volume selected", sizeof(state->statusLabel) - 1);
            state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
            state->statusColor = (SDL_Color){240, 180, 120, 255};
            state->statusExpireMs = SDL_GetTicks() + 1800;
        }
        return;
    }
    if (buttons.volumeClearRect.w > 0 && point_in_rect(&buttons.volumeClearRect, x, y)) {
        AnimationClearVolumeSource();
        menu_state_sync_from_anim(state);
        menu_state_refresh_volume_options(state);
        SaveAnimationConfig();
        strncpy(state->statusLabel, "Volume cleared", sizeof(state->statusLabel) - 1);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){200, 180, 120, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
        return;
    }

    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_OUTPUT &&
        menu_batch_panel_handle_click(event, renderer, *font, state, &batchLayout)) {
        return;
    }

    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RUN &&
        menu_resume_panel_handle_click(event, state, &resumeLayout)) {
        return;
    }

    if (point_in_rect(&buttons.inputRootFolderRect, x, y)) {
        char selected[PATH_MAX];
        if (RayTracing_FolderPicker_Select("Choose optiC Input Root", animSettings.inputRoot, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED) {
            apply_input_root(state, selected);
        }
        return;
    }
    if (point_in_rect(&buttons.inputRootEditRect, x, y) || point_in_rect(&buttons.inputRootValueRect, x, y)) {
        begin_input_root_edit(state);
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
    if (point_in_rect(&buttons.meshAssetRootFolderRect, x, y)) {
        char selected[PATH_MAX];
        if (RayTracing_FolderPicker_Select("Choose optiC Mesh Asset Root", animSettings.meshAssetRoot, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED) {
            apply_mesh_asset_root(state, selected);
        }
        return;
    }
    if (point_in_rect(&buttons.meshAssetRootEditRect, x, y) ||
        point_in_rect(&buttons.meshAssetRootValueRect, x, y)) {
        begin_mesh_asset_root_edit(state);
        return;
    }
    if (point_in_rect(&buttons.meshAssetRootApplyRect, x, y)) {
        if (state->editingMeshAssetRoot) {
            finish_root_edit(state, true);
        } else {
            apply_mesh_asset_root(state, animSettings.meshAssetRoot);
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
            const char* newModeText = (animSettings.editorMode == EDITOR_MODE_PATH) ? "Path" :
                                      (animSettings.editorMode == EDITOR_MODE_OBJECT) ? "Scene" :
                                      (animSettings.editorMode == EDITOR_MODE_CAMERA) ? "Camera" :
                                      "Material";
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
        menu_state_refresh_volume_options(state);
        if (state->manifestDropdownOpen) {
            state->manifestScroll = 0.0f;
            state->manifestScrollbarDragging = false;
        }
        if (animSettings.spaceMode != SPACE_MODE_3D) {
            state->volumeDropdownOpen = false;
        }
        if (state->volumeDropdownOpen) {
            state->volumeScroll = 0.0f;
            state->volumeScrollbarDragging = false;
        }
        mode_status = EditorModeRouter_SpaceButtonLabel();
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

    if (point_in_rect(&buttons.causticModeRect, x, y)) {
        state->causticSettings.mode =
            (RuntimeCausticMode3D)(((int)state->causticSettings.mode + 1) % 4);
        if (state->causticSettings.mode == RUNTIME_CAUSTIC_MODE_OFF ||
            state->causticSettings.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC) {
            state->causticSettings.surfaceCacheEnabled = false;
            state->causticSettings.volumeCacheEnabled = false;
        } else if (!state->causticSettings.surfaceCacheEnabled &&
                   !state->causticSettings.volumeCacheEnabled) {
            state->causticSettings.surfaceCacheEnabled = true;
        }
        return;
    }
    if (point_in_rect(&buttons.causticEngineRect, x, y)) {
        state->causticSettings.transportEngine =
            state->causticSettings.transportEngine ==
                    RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP
                ? RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT
                : RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP;
        return;
    }
    if (point_in_rect(&buttons.causticSurfaceRect, x, y)) {
        state->causticSettings.surfaceCacheEnabled =
            !state->causticSettings.surfaceCacheEnabled;
        return;
    }
    if (point_in_rect(&buttons.causticVolumeRect, x, y)) {
        state->causticSettings.volumeCacheEnabled =
            !state->causticSettings.volumeCacheEnabled;
        return;
    }
    if (point_in_rect(&buttons.causticDebugSummaryRect, x, y)) {
        state->causticSettings.debugSummaryEnabled =
            !state->causticSettings.debugSummaryEnabled;
        return;
    }
    if (point_in_rect(&buttons.causticDebugExportRect, x, y)) {
        state->causticSettings.debugExportEnabled =
            !state->causticSettings.debugExportEnabled;
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
    if (point_in_rect(&buttons.denoiseRect, x, y)) {
        animSettings.disneyDenoiseEnabled = !animSettings.disneyDenoiseEnabled;
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "Disney Denoise: %s",
                 animSettings.disneyDenoiseEnabled ? "ON" : "OFF");
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){160, 210, 255, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
        return;
    }
    if (point_in_rect(&buttons.topFillRect, x, y)) {
        animSettings.environmentLightMode =
            (animation_config_environment_light_mode_clamp(animSettings.environmentLightMode) + 1) %
            (ENVIRONMENT_LIGHT_MODE_AMBIENT + 1);
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "Env Light: %s",
                 (animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL)
                     ? "Top Fill"
                     : ((animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT)
                            ? "Ambient"
                            : "Off"));
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){160, 210, 255, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
        return;
    }
    if (point_in_rect(&buttons.environmentPresetRect, x, y)) {
        const char* preset_label = "Sky";
        animSettings.environmentPreset =
            (animation_config_environment_preset_clamp(animSettings.environmentPreset) + 1) %
            (ENVIRONMENT_PRESET_WARM_SKY + 1);
        animSettings.environmentBackgroundLightingAuthored = true;
        if (animSettings.environmentPreset == ENVIRONMENT_PRESET_NEUTRAL) {
            preset_label = "Neutral";
        } else if (animSettings.environmentPreset == ENVIRONMENT_PRESET_WARM_SKY) {
            preset_label = "Warm";
        }
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "Env Preset: %s",
                 preset_label);
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){160, 210, 255, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
        return;
    }
    if (point_in_rect(&buttons.environmentBackgroundModeRect, x, y)) {
        animSettings.environmentBackgroundLightingAuthored = true;
        animSettings.environmentBackgroundBrightnessAuto =
            !animSettings.environmentBackgroundBrightnessAuto;
        if (!animSettings.environmentBackgroundBrightnessAuto) {
            animSettings.environmentBackgroundBrightness =
                state->environmentBackgroundBrightnessSliderValue / 100.0;
        }
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "BG Brightness: %s",
                 animSettings.environmentBackgroundBrightnessAuto ? "Auto" : "Manual");
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){160, 210, 255, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
        return;
    }
    if (point_in_rect(&buttons.upscaleModeRect, x, y)) {
        if (animSettings.upscaleMode3D < RUNTIME_3D_UPSCALE_MODE_MIN ||
            animSettings.upscaleMode3D > RUNTIME_3D_UPSCALE_MODE_MAX) {
            animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_DEFAULT;
        } else {
            animSettings.upscaleMode3D =
                (animSettings.upscaleMode3D + 1) % (RUNTIME_3D_UPSCALE_MODE_MAX + 1);
        }
        snprintf(state->statusLabel,
                 sizeof(state->statusLabel),
                 "Upscale: %s",
                 (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_NEAREST)
                     ? "Nearest"
                     : (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_BILINEAR)
                           ? "Bilinear"
                           : "OFF");
        state->statusLabel[sizeof(state->statusLabel) - 1] = '\0';
        state->statusColor = (SDL_Color){160, 210, 255, 255};
        state->statusExpireMs = SDL_GetTicks() + 1800;
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
        RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
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
        if (path_edit_active(state)) {
            finish_root_edit(state, true);
        }
        if (state->editingStartFrame) {
            finish_start_frame_edit(state, true);
        }
        menu_state_apply_effective_render_recipe(state);
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
        if (path_edit_active(state)) {
            finish_root_edit(state, true);
        }
        if (state->editingStartFrame) {
            finish_start_frame_edit(state, true);
        }
        menu_state_sync_from_anim(state);
        menu_state_apply_effective_render_recipe(state);
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
        if (path_edit_active(state)) {
            finish_root_edit(state, true);
        }
        if (state->editingStartFrame) {
            finish_start_frame_edit(state, true);
        }
        menu_state_sync_from_anim(state);
        menu_state_apply_effective_render_recipe(state);
        printf("[Menu] Start pressed: spaceMode=%d integrator2D=%d integrator3D=%d falloffMode=%d decay=%.2f softness=%.2f intensity=%.2f\n",
               animSettings.spaceMode,
               animSettings.integratorMode,
               animSettings.integratorMode3D,
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
