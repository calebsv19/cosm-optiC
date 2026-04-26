#include "ui/sdl_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "app/scene_loop_diag.h"
#include "app/scene_loop_policy.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_draw.h"
#include "render/text_font_cache.h"
#include "render/vk_shared_device.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/menu_batch_panel.h"
#include "ui/sdl_menu_input.h"
#include "ui/sdl_menu_render.h"
#include "ui/sdl_menu_state.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_IDLE_HEARTBEAT_MS 250u

#if USE_VULKAN
static VkRenderer g_menu_renderer_storage;
#endif

static bool initialize_menu(SDL_Window** window,
                            SDL_Renderer** renderer,
                            TTF_Font** font,
                            MenuRuntimeState* state) {
    if (!window || !renderer || !font || !state) return false;

    ray_tracing_shared_theme_load_persisted();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Initialization Failed: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() == -1) {
        printf("TTF Initialization Failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    *window = SDL_CreateWindow("RayTracing Menu",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               MENU_WIDTH,
                               MENU_HEIGHT,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!*window) {
        printf("Window Creation Failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(*window, &cfg)) {
        printf("vk_shared_device_init failed.\n");
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        printf("vk_shared_device_get failed.\n");
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    VkResult init = vk_renderer_init_with_device(&g_menu_renderer_storage, shared_device, *window, &cfg);
    if (init != VK_SUCCESS) {
        printf("vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    *renderer = (SDL_Renderer*)&g_menu_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)*renderer, (float)MENU_WIDTH, (float)MENU_HEIGHT);
#else
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        printf("Renderer Creation Failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
#endif

    LoadAnimationConfig();
    LoadSceneConfig();
    animSettings.previewMode = false;

    menu_state_init(state);
    setRenderContext(*renderer, *window, MENU_WIDTH, MENU_HEIGHT);

    *font = NULL;
    menu_state_reload_font(font);
    if (!*font) {
        printf("Font Loading Failed: %s\n", TTF_GetError());
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)*renderer);
        vk_renderer_shutdown_surface((VkRenderer*)*renderer);
#else
        SDL_DestroyRenderer(*renderer);
#endif
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

static void shutdown_menu(SDL_Window* window,
                          SDL_Renderer* renderer,
                          TTF_Font* font,
                          bool keep_running_engine) {
    (void)keep_running_engine;
    setRenderContext(NULL, NULL, 0, 0);

    if (renderer) {
        ray_tracing_text_reset_renderer(renderer);
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    (void)font;
    shutdownFontSystem();
    if (TTF_WasInit() != 0) {
        TTF_Quit();
    }
#if USE_VULKAN
    vk_shared_device_shutdown();
#endif
    SDL_Quit();
}

static bool menu_event_is_host_window_close(SDL_Window* window, const SDL_Event* event) {
    Uint32 window_id = 0;
    if (!window || !event) return false;
    if (event->type != SDL_WINDOWEVENT || event->window.event != SDL_WINDOWEVENT_CLOSE) {
        return false;
    }
    window_id = SDL_GetWindowID(window);
    if (window_id == 0) return false;
    return event->window.windowID == window_id;
}

static uint32_t menu_elapsed_ms(uint64_t start, uint64_t end, uint64_t frequency) {
    uint64_t elapsed_ms = 0u;
    if (frequency == 0u || end < start) return 0u;
    elapsed_ms = ((end - start) * 1000u) / frequency;
    if (elapsed_ms > (uint64_t)UINT32_MAX) return UINT32_MAX;
    return (uint32_t)elapsed_ms;
}

static bool menu_state_interaction_active(const MenuRuntimeState* state) {
    if (!state) return false;
    return state->draggingSlider ||
           state->manifestScrollbarDragging ||
           state->editingBounce ||
           state->editingFrame ||
           state->editingInputRoot ||
           state->editingOutputRoot ||
           menu_batch_panel_edit_active(state);
}

static bool menu_process_event(SDL_Window* window,
                               SDL_Renderer* renderer,
                               TTF_Font** font,
                               MenuRuntimeState* menu_state,
                               SceneEditor* scene_editor,
                               bool* scene_editor_session_active,
                               bool* running,
                               bool* menu_exited_normally,
                               const SDL_Event* event) {
    SDL_Event mutable_event;
    bool dirty = false;
    if (!window || !renderer || !font || !menu_state || !scene_editor ||
        !scene_editor_session_active || !running || !menu_exited_normally || !event) {
        return false;
    }

    mutable_event = *event;
    if (mutable_event.type == SDL_QUIT ||
        menu_event_is_host_window_close(window, &mutable_event)) {
        *menu_exited_normally = false;
        *running = false;
        return false;
    }

    if (menu_state->activeView == MENU_VIEW_SCENE_EDITOR) {
        if (*scene_editor_session_active) {
            SceneEditorSessionHandleEvent(scene_editor, &mutable_event);
            dirty = true;
            if (SceneEditorSessionWantsExit(scene_editor)) {
                SceneEditorSessionEnd(scene_editor);
                *scene_editor_session_active = false;
                menu_state->activeView = MENU_VIEW_MAIN;
                (void)menu_state_reload_font(font);
            }
        }
        return dirty;
    }

    switch (mutable_event.type) {
        case SDL_KEYDOWN:
            menu_input_handle_key(&mutable_event, running, font, menu_state);
            return true;
        case SDL_MOUSEBUTTONDOWN:
            menu_input_handle_mouse_click(&mutable_event,
                                          running,
                                          menu_exited_normally,
                                          renderer,
                                          font,
                                          menu_state);
            return true;
        case SDL_MOUSEBUTTONUP:
            menu_state->draggingSlider = false;
            menu_state->manifestScrollbarDragging = false;
            return true;
        case SDL_MOUSEWHEEL:
            menu_input_handle_mouse_wheel(&mutable_event, menu_state);
            return true;
        case SDL_MOUSEMOTION:
            menu_input_handle_mouse_motion(&mutable_event, menu_state);
            return true;
        case SDL_TEXTINPUT:
            if (menu_batch_panel_append_text(menu_state, mutable_event.text.text)) {
                return true;
            }
            if (menu_state->editingInputRoot || menu_state->editingOutputRoot) {
                if (strlen(menu_state->pathInputBuffer) + strlen(mutable_event.text.text) <
                    sizeof(menu_state->pathInputBuffer)) {
                    strcat(menu_state->pathInputBuffer, mutable_event.text.text);
                    return true;
                }
            } else if (menu_state->editingBounce || menu_state->editingFrame) {
                if (strlen(menu_state->inputBuffer) < sizeof(menu_state->inputBuffer) - 1) {
                    strcat(menu_state->inputBuffer, mutable_event.text.text);
                    return true;
                }
            }
            return false;
        case SDL_WINDOWEVENT:
            return mutable_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                   mutable_event.window.event == SDL_WINDOWEVENT_RESIZED ||
                   mutable_event.window.event == SDL_WINDOWEVENT_EXPOSED;
        default:
            return false;
    }
}

static void menu_record_loop_diag(uint64_t frame_begin_counter,
                                  uint64_t performance_frequency,
                                  uint32_t wait_blocked_ms,
                                  uint32_t wait_call_count) {
    if (performance_frequency == 0u) return;
    {
        uint64_t frame_end_counter = SDL_GetPerformanceCounter();
        double frame_elapsed_sec = (double)(frame_end_counter - frame_begin_counter) /
                                   (double)performance_frequency;
        scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
    }
}

bool RunMenu(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font *font = NULL;
    MenuRuntimeState menuState;
    SceneEditor sceneEditor;
    bool sceneEditorSessionActive = false;
    memset(&sceneEditor, 0, sizeof(sceneEditor));

    if (!initialize_menu(&window, &renderer, &font, &menuState)) {
        return false;
    }

    bool running = true;
    bool menuExitedNormally = false;
    SDL_Event event;
    bool frame_dirty = true;
    Uint32 last_render_ms = 0u;
    const uint64_t perf_freq = SDL_GetPerformanceFrequency();

    while (running) {
        uint64_t frame_begin_counter = SDL_GetPerformanceCounter();
        uint32_t wait_blocked_ms = 0u;
        uint32_t wait_call_count = 0u;

        if (!frame_dirty) {
            SceneLoopWaitPolicyInput wait_input = {
                .high_intensity_mode = false,
                .interaction_active = menu_state_interaction_active(&menuState) ||
                                      (sceneEditorSessionActive &&
                                       SceneEditorSessionInteractionActive(&sceneEditor)),
                .background_busy = false,
                .resize_pending = false,
            };
            int wait_timeout_ms = scene_loop_compute_wait_timeout_ms(&wait_input);
            if (wait_timeout_ms > 0) {
                uint64_t wait_start = SDL_GetPerformanceCounter();
                int wait_result = SDL_WaitEventTimeout(&event, wait_timeout_ms);
                uint64_t wait_end = SDL_GetPerformanceCounter();
                wait_blocked_ms += menu_elapsed_ms(wait_start, wait_end, perf_freq);
                wait_call_count += 1u;
                if (wait_result == 1) {
                    frame_dirty |= menu_process_event(window,
                                                      renderer,
                                                      &font,
                                                      &menuState,
                                                      &sceneEditor,
                                                      &sceneEditorSessionActive,
                                                      &running,
                                                      &menuExitedNormally,
                                                      &event);
                }
            }
        }
        while (running && SDL_PollEvent(&event)) {
            frame_dirty |= menu_process_event(window,
                                              renderer,
                                              &font,
                                              &menuState,
                                              &sceneEditor,
                                              &sceneEditorSessionActive,
                                              &running,
                                              &menuExitedNormally,
                                              &event);
        }
        if (!running) {
            menu_record_loop_diag(frame_begin_counter, perf_freq, wait_blocked_ms, wait_call_count);
            break;
        }

        {
            bool heartbeat_due = (last_render_ms == 0u) ||
                                 ((Uint32)(SDL_GetTicks() - last_render_ms) >= MENU_IDLE_HEARTBEAT_MS);
            bool rendered = false;
            if (!frame_dirty && !heartbeat_due) {
                menu_record_loop_diag(frame_begin_counter, perf_freq, wait_blocked_ms, wait_call_count);
                continue;
            }

            setRenderContext(renderer, window, MENU_WIDTH, MENU_HEIGHT);
            if (menuState.activeView == MENU_VIEW_SCENE_EDITOR) {
                if (!sceneEditorSessionActive) {
                    if (SceneEditorSessionBegin(&sceneEditor, renderer, window)) {
                        sceneEditorSessionActive = true;
                    } else {
                        menuState.activeView = MENU_VIEW_MAIN;
                        snprintf(menuState.statusLabel, sizeof(menuState.statusLabel), "%s", "Scene editor init failed");
                        menuState.statusLabel[sizeof(menuState.statusLabel) - 1] = '\0';
                        menuState.statusColor = (SDL_Color){255, 170, 140, 255};
                        menuState.statusExpireMs = SDL_GetTicks() + 2200;
                        frame_dirty = true;
                    }
                }
                if (sceneEditorSessionActive) {
                    SceneEditorSessionRender(&sceneEditor);
                    rendered = true;
                    if (SceneEditorSessionWantsExit(&sceneEditor)) {
                        SceneEditorSessionEnd(&sceneEditor);
                        sceneEditorSessionActive = false;
                        menuState.activeView = MENU_VIEW_MAIN;
                        (void)menu_state_reload_font(&font);
                        frame_dirty = true;
                    }
                }
            }
            if (!rendered) {
                menu_render_frame(renderer, font, &menuState);
            }
            if (render_device_lost()) {
                running = false;
                menuExitedNormally = false;
            }
            if (running) {
                frame_dirty = false;
                last_render_ms = SDL_GetTicks();
            }
            menu_record_loop_diag(frame_begin_counter, perf_freq, wait_blocked_ms, wait_call_count);
        }
    }

    if (sceneEditorSessionActive) {
        SceneEditorSessionEnd(&sceneEditor);
    }
    shutdown_menu(window, renderer, font, menuExitedNormally);
    ray_tracing_shared_theme_save_persisted();
    SaveAllSettings();
    return menuExitedNormally;
}
