#include "ui/sdl_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/vk_shared_device.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/sdl_menu_input.h"
#include "ui/sdl_menu_render.h"
#include "ui/sdl_menu_state.h"

#define MENU_WIDTH 1000
#define MENU_HEIGHT 900

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
    if (keep_running_engine) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
#if USE_VULKAN
        vk_shared_device_shutdown();
#endif
        SDL_Quit();
    }
}

bool RunMenu(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font *font = NULL;
    MenuRuntimeState menuState;

    if (!initialize_menu(&window, &renderer, &font, &menuState)) {
        return false;
    }

    bool running = true;
    bool menuExitedNormally = false;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    menuExitedNormally = false;
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    menu_input_handle_key(&event, &running, &font, &menuState);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    menu_input_handle_mouse_click(&event,
                                                  &running,
                                                  &menuExitedNormally,
                                                  renderer,
                                                  &font,
                                                  &menuState);
                    break;
                case SDL_MOUSEBUTTONUP:
                    menuState.draggingSlider = false;
                    menuState.manifestScrollbarDragging = false;
                    break;
                case SDL_MOUSEWHEEL:
                    menu_input_handle_mouse_wheel(&event, &menuState);
                    break;
                case SDL_MOUSEMOTION:
                    menu_input_handle_mouse_motion(&event, &menuState);
                    break;
                case SDL_TEXTINPUT:
                    if (menuState.editingInputRoot || menuState.editingOutputRoot) {
                        if (strlen(menuState.pathInputBuffer) + strlen(event.text.text) <
                            sizeof(menuState.pathInputBuffer)) {
                            strcat(menuState.pathInputBuffer, event.text.text);
                        }
                    } else if (menuState.editingBounce || menuState.editingFrame) {
                        if (strlen(menuState.inputBuffer) < sizeof(menuState.inputBuffer) - 1) {
                            strcat(menuState.inputBuffer, event.text.text);
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        setRenderContext(renderer, window, MENU_WIDTH, MENU_HEIGHT);
        menu_render_frame(renderer, font, &menuState);
        if (render_device_lost()) {
            running = false;
            menuExitedNormally = false;
        }
    }

    shutdown_menu(window, renderer, font, menuExitedNormally);
    ray_tracing_shared_theme_save_persisted();
    SaveAllSettings();
    return menuExitedNormally;
}
