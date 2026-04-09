#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/vk_shared_device.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/sdl_menu_sections.h"

#define MENU_WIDTH 1000
#define MENU_HEIGHT 900

bool RunMenu(void) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font *font = NULL;

    if (!InitializeMenu(&window, &renderer, &font)) {
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
                    HandleKeyPress(&event, &running, &font);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    HandleMouseClick(&event, &running, &menuExitedNormally, renderer, &font);
                    break;
                case SDL_MOUSEBUTTONUP:
                    draggingSlider = false;
                    g_manifestScrollbarDragging = false;
                    break;
                case SDL_MOUSEWHEEL:
                    HandleMouseWheel(&event);
                    break;
                case SDL_MOUSEMOTION:
                    HandleMouseMotion(&event);
                    break;
                case SDL_TEXTINPUT:
                    if (editingBounce || editingFrame) {
                        if (strlen(inputBuffer) < sizeof(inputBuffer) - 1) {
                            strcat(inputBuffer, event.text.text);
                        }
                    }
                    break;
            }
        }

        setRenderContext(renderer, window, MENU_WIDTH, MENU_HEIGHT);
        RenderMenu(renderer, font);
        if (render_device_lost()) {
            running = false;
            menuExitedNormally = false;
        }
    }

    if (menuExitedNormally) {
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

    ray_tracing_shared_theme_save_persisted();
    SaveAllSettings();
    return menuExitedNormally;
}
