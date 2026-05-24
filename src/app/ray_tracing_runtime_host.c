#include "app/ray_tracing_runtime_host.h"

#include "app/animation.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/font_runtime.h"
#include "render/timer_hud_adapter.h"
#include "render/timer_hud_api.h"
#include "render/vk_shared_device.h"

#include <stdio.h>
#include <string.h>

typedef struct RayTracingRuntimeHostState {
    bool sdl_ready;
    bool window_ready;
    bool shared_device_ready;
    bool renderer_ready;
    bool render_context_ready;
    bool font_runtime_attached;
    bool font_system_ready;
    bool timer_hud_session_ready;
} RayTracingRuntimeHostState;

static RayTracingRuntimeHostState g_runtime_host = {0};

#if USE_VULKAN
static VkRenderer g_runtime_host_renderer_storage;
#endif

static void ray_tracing_runtime_host_reset_state(void) {
    memset(&g_runtime_host, 0, sizeof(g_runtime_host));
}

static int ray_tracing_runtime_host_init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 0;
    }
    g_runtime_host.sdl_ready = true;
    return 1;
}

static int ray_tracing_runtime_host_create_window(int window_width, int window_height) {
    window = SDL_CreateWindow("Raytracing Animation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              window_width,
                              window_height,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 0;
    }
    g_runtime_host.window_ready = true;
    return 1;
}

static int ray_tracing_runtime_host_create_renderer(int window_width, int window_height) {
#if USE_VULKAN
    VkRendererConfig cfg;
    VkRendererDevice* shared_device = NULL;
    VkResult init = VK_SUCCESS;

    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        return 0;
    }
    g_runtime_host.shared_device_ready = true;

    shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        return 0;
    }

    init = vk_renderer_init_with_device(&g_runtime_host_renderer_storage, shared_device, window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        return 0;
    }
    renderer = (SDL_Renderer*)&g_runtime_host_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)renderer, (float)window_width, (float)window_height);
#else
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 0;
    }
#endif
    g_runtime_host.renderer_ready = true;
    return 1;
}

static int ray_tracing_runtime_host_attach_runtime_contract(int window_width, int window_height) {
    setRenderContext(renderer, window, window_width, window_height);
    g_runtime_host.render_context_ready = true;

    ray_tracing_font_runtime_attach_renderer(renderer);
    g_runtime_host.font_runtime_attached = true;

    if (!initFontSystem()) {
        fprintf(stderr, "[runtime_host] Failed to initialise font system.\n");
        return 0;
    }
    g_runtime_host.font_system_ready = true;

    timer_hud_register_backend();
    ts_session_init(timer_hud_session());
    timer_hud_apply_startup_env_overrides();
    g_runtime_host.timer_hud_session_ready = true;
    return 1;
}

int ray_tracing_runtime_host_init(int window_width, int window_height) {
    if (g_runtime_host.sdl_ready || window || renderer) {
        ray_tracing_runtime_host_shutdown();
    }
    ray_tracing_runtime_host_reset_state();

    if (!ray_tracing_runtime_host_init_sdl()) {
        ray_tracing_runtime_host_shutdown();
        return -1;
    }
    if (!ray_tracing_runtime_host_create_window(window_width, window_height)) {
        ray_tracing_runtime_host_shutdown();
        return -1;
    }
    if (!ray_tracing_runtime_host_create_renderer(window_width, window_height)) {
        ray_tracing_runtime_host_shutdown();
        return -1;
    }
    if (!ray_tracing_runtime_host_attach_runtime_contract(window_width, window_height)) {
        ray_tracing_runtime_host_shutdown();
        return -1;
    }
    return 0;
}

void ray_tracing_runtime_host_shutdown(void) {
    if (g_runtime_host.timer_hud_session_ready) {
        timer_hud_shutdown_session();
        g_runtime_host.timer_hud_session_ready = false;
    }
    if (g_runtime_host.font_system_ready) {
        shutdownFontSystem();
        g_runtime_host.font_system_ready = false;
    }
    if (g_runtime_host.font_runtime_attached) {
        ray_tracing_font_runtime_detach_renderer(renderer);
        g_runtime_host.font_runtime_attached = false;
    }
    if (g_runtime_host.render_context_ready) {
        setRenderContext(NULL, NULL, 0, 0);
        g_runtime_host.render_context_ready = false;
    }
    if (g_runtime_host.renderer_ready && renderer) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        renderer = NULL;
        g_runtime_host.renderer_ready = false;
    }
    if (g_runtime_host.window_ready && window) {
        SDL_DestroyWindow(window);
        window = NULL;
        g_runtime_host.window_ready = false;
    }
#if USE_VULKAN
    if (g_runtime_host.shared_device_ready) {
        vk_shared_device_shutdown();
        g_runtime_host.shared_device_ready = false;
    }
#endif
    if (g_runtime_host.sdl_ready) {
        SDL_Quit();
        g_runtime_host.sdl_ready = false;
    }
    ray_tracing_runtime_host_reset_state();
}
