#include "app/ray_tracing_window_lifecycle.h"
#include "kit_ui_window_sdl.h"
#include <string.h>

static SDL_Window *host_window;
static KitUiWindowState observed;
static Uint8 blocked_keys[SDL_NUM_SCANCODES];
static bool block_drag;

static void cancel_input(void) {
    int count = 0;
    const Uint8 *keys = SDL_GetKeyboardState(&count);
    memset(blocked_keys, 0, sizeof(blocked_keys));
    if (count > SDL_NUM_SCANCODES) count = SDL_NUM_SCANCODES;
    if (keys && count > 0) memcpy(blocked_keys, keys, (size_t)count);
    block_drag = true;
}

void ray_tracing_window_lifecycle_bind(SDL_Window *window) {
    host_window = window;
    memset(&observed, 0, sizeof(observed));
    memset(blocked_keys, 0, sizeof(blocked_keys));
    block_drag = false;
    if (window) {
        uint32_t changes = 0;
        (void)kit_ui_window_refresh_sdl(&observed, window, &changes);
    }
}

static Uint32 event_window(const SDL_Event *event) {
    switch (event->type) {
        case SDL_WINDOWEVENT: return event->window.windowID;
        case SDL_KEYDOWN: case SDL_KEYUP: return event->key.windowID;
        case SDL_MOUSEMOTION: return event->motion.windowID;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: return event->button.windowID;
        case SDL_MOUSEWHEEL: return event->wheel.windowID;
        case SDL_TEXTINPUT: return event->text.windowID;
        case SDL_TEXTEDITING: return event->edit.windowID;
        default: return 0;
    }
}

bool ray_tracing_window_lifecycle_filter(const SDL_Event *event) {
    if (!event || !host_window) return false;
    Uint32 id = event_window(event);
    if (id && id != SDL_GetWindowID(host_window)) return true;
    uint32_t changes = 0;
    if (kit_ui_window_refresh_sdl(&observed, host_window, &changes).code != CORE_OK)
        observed.presentable = 0;
    if (changes || kit_ui_window_event_invalidates_sdl(event)) cancel_input();
    if (event->type == SDL_QUIT || event->type == SDL_WINDOWEVENT) return false;
    bool key = event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
    bool pointer = event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN ||
                   event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEWHEEL;
    if ((key || pointer) && !observed.presentable) return true;
    if (key && !(observed.flags & SDL_WINDOW_INPUT_FOCUS)) return true;
    if (key && event->key.keysym.scancode >= 0 && event->key.keysym.scancode < SDL_NUM_SCANCODES) {
        int code = event->key.keysym.scancode;
        if (blocked_keys[code]) {
            if (event->type == SDL_KEYUP) blocked_keys[code] = 0;
            return true;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) block_drag = false;
    if (event->type == SDL_MOUSEMOTION && block_drag && event->motion.state) return true;
    if (event->type == SDL_MOUSEBUTTONUP && block_drag) return true;
    if (kit_ui_window_fullscreen_key_sdl(host_window, event)) {
        cancel_input();
        return true;
    }
    return false;
}
