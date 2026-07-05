#include "app/ray_tracing_runtime_host.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
static bool g_fail_sdl_init = false;

int SDL_Init(Uint32 flags) {
    (void)flags;
    if (g_fail_sdl_init) return -1;
    return 0;
}

const char* SDL_GetError(void) {
    return "test SDL";
}

SDL_Window* SDL_CreateWindow(const char* title,
                             int x,
                             int y,
                             int w,
                             int h,
                             Uint32 flags) {
    (void)title;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)flags;
    return (SDL_Window*)0x1;
}

SDL_Renderer* SDL_CreateRenderer(SDL_Window* host_window, int index, Uint32 flags) {
    (void)host_window;
    (void)index;
    (void)flags;
    return (SDL_Renderer*)0x2;
}

void SDL_DestroyRenderer(SDL_Renderer* host_renderer) {
    (void)host_renderer;
}

void SDL_DestroyWindow(SDL_Window* host_window) {
    (void)host_window;
}

void SDL_Quit(void) {
}

void setRenderContext(SDL_Renderer* host_renderer,
                      SDL_Window* host_window,
                      int width,
                      int height) {
    (void)host_renderer;
    (void)host_window;
    (void)width;
    (void)height;
}

void ray_tracing_font_runtime_attach_renderer(SDL_Renderer* host_renderer) {
    (void)host_renderer;
}

void ray_tracing_font_runtime_detach_renderer(SDL_Renderer* host_renderer) {
    (void)host_renderer;
}

void MaterialEditorFacePreviewDetachRenderer(SDL_Renderer* host_renderer) {
    (void)host_renderer;
}

int initFontSystem(void) {
    return 1;
}

void shutdownFontSystem(void) {
}

void timer_hud_register_backend(void) {
}

void* timer_hud_session(void) {
    return NULL;
}

void ts_session_init(void* session) {
    (void)session;
}

void timer_hud_apply_startup_env_overrides(void) {
}

void timer_hud_shutdown_session(void) {
}

static bool expect_clean_snapshot(void) {
    RayTracingRuntimeHostSnapshot snapshot;
    ray_tracing_runtime_host_snapshot(&snapshot);
    return ray_tracing_runtime_host_is_clean() &&
           !snapshot.any_resource_ready &&
           !snapshot.sdl_ready &&
           !snapshot.window_ready &&
           !snapshot.shared_device_ready &&
           !snapshot.renderer_ready &&
           !snapshot.render_context_ready &&
           !snapshot.font_runtime_attached &&
           !snapshot.font_system_ready &&
           !snapshot.timer_hud_session_ready;
}

static bool test_runtime_host_initial_and_shutdown_state(void) {
    if (!expect_clean_snapshot()) return false;
    ray_tracing_runtime_host_shutdown();
    if (!expect_clean_snapshot()) return false;
    return true;
}

static bool test_runtime_host_failed_init_records_last_failure(void) {
    RayTracingRuntimeHostSnapshot snapshot;
    g_fail_sdl_init = true;
    if (ray_tracing_runtime_host_init(320, 180) == 0) return false;
    g_fail_sdl_init = false;
    if (!expect_clean_snapshot()) return false;
    ray_tracing_runtime_host_snapshot(&snapshot);
    if (strcmp(snapshot.last_failure_stage, "sdl_init") != 0) return false;
    if (strstr(snapshot.last_failure_detail, "SDL_Init failed") == NULL) return false;
    if (strcmp(ray_tracing_runtime_host_last_failure_stage(), "sdl_init") != 0) return false;
    if (strstr(ray_tracing_runtime_host_last_failure_detail(), "SDL_Init failed") == NULL) {
        return false;
    }
    return true;
}

int main(void) {
    if (!test_runtime_host_initial_and_shutdown_state()) {
        fprintf(stderr, "ray_tracing_runtime_host_lifecycle_contract_test: failed\n");
        return 1;
    }
    if (!test_runtime_host_failed_init_records_last_failure()) {
        fprintf(stderr, "ray_tracing_runtime_host_lifecycle_contract_test: failed last failure\n");
        return 1;
    }
    fprintf(stdout, "ray_tracing_runtime_host_lifecycle_contract_test: success\n");
    return 0;
}
