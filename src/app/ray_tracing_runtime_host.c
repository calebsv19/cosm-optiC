#include "app/ray_tracing_runtime_host.h"

#include "app/animation.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/font_runtime.h"
#include "render/timer_hud_adapter.h"
#include "render/timer_hud_api.h"
#include "render/vk_shared_device.h"

#include <stdarg.h>
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

typedef struct RayTracingRuntimeHostFailure {
    char stage[64];
    char detail[256];
} RayTracingRuntimeHostFailure;

static RayTracingRuntimeHostState g_runtime_host = {0};
static RayTracingRuntimeHostFailure g_runtime_host_last_failure = {0};

#if USE_VULKAN
static VkRenderer g_runtime_host_renderer_storage;
#endif

static void ray_tracing_runtime_host_reset_state(void) {
    memset(&g_runtime_host, 0, sizeof(g_runtime_host));
}

static void ray_tracing_runtime_host_clear_last_failure(void) {
    memset(&g_runtime_host_last_failure, 0, sizeof(g_runtime_host_last_failure));
}

static void ray_tracing_runtime_host_set_last_failure(const char *stage,
                                                      const char *format,
                                                      ...) {
    va_list args;
    snprintf(g_runtime_host_last_failure.stage,
             sizeof(g_runtime_host_last_failure.stage),
             "%s",
             stage ? stage : "unknown");
    if (!format) {
        snprintf(g_runtime_host_last_failure.detail,
                 sizeof(g_runtime_host_last_failure.detail),
                 "unknown");
        return;
    }
    va_start(args, format);
    vsnprintf(g_runtime_host_last_failure.detail,
              sizeof(g_runtime_host_last_failure.detail),
              format,
              args);
    va_end(args);
}

void ray_tracing_runtime_host_snapshot(RayTracingRuntimeHostSnapshot *out_snapshot) {
    if (!out_snapshot) return;
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->sdl_ready = g_runtime_host.sdl_ready;
    out_snapshot->window_ready = g_runtime_host.window_ready;
    out_snapshot->shared_device_ready = g_runtime_host.shared_device_ready;
    out_snapshot->renderer_ready = g_runtime_host.renderer_ready;
    out_snapshot->render_context_ready = g_runtime_host.render_context_ready;
    out_snapshot->font_runtime_attached = g_runtime_host.font_runtime_attached;
    out_snapshot->font_system_ready = g_runtime_host.font_system_ready;
    out_snapshot->timer_hud_session_ready = g_runtime_host.timer_hud_session_ready;
    out_snapshot->any_resource_ready =
        out_snapshot->sdl_ready ||
        out_snapshot->window_ready ||
        out_snapshot->shared_device_ready ||
        out_snapshot->renderer_ready ||
        out_snapshot->render_context_ready ||
        out_snapshot->font_runtime_attached ||
        out_snapshot->font_system_ready ||
        out_snapshot->timer_hud_session_ready ||
        window != NULL ||
        renderer != NULL;
    snprintf(out_snapshot->last_failure_stage,
             sizeof(out_snapshot->last_failure_stage),
             "%s",
             g_runtime_host_last_failure.stage);
    snprintf(out_snapshot->last_failure_detail,
             sizeof(out_snapshot->last_failure_detail),
             "%s",
             g_runtime_host_last_failure.detail);
}

bool ray_tracing_runtime_host_is_clean(void) {
    RayTracingRuntimeHostSnapshot snapshot;
    ray_tracing_runtime_host_snapshot(&snapshot);
    return !snapshot.any_resource_ready;
}

const char *ray_tracing_runtime_host_last_failure_stage(void) {
    return g_runtime_host_last_failure.stage;
}

const char *ray_tracing_runtime_host_last_failure_detail(void) {
    return g_runtime_host_last_failure.detail;
}

static int ray_tracing_runtime_host_init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ray_tracing_runtime_host_set_last_failure("sdl_init",
                                                  "SDL_Init failed: %s",
                                                  SDL_GetError());
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=sdl_init error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
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
        ray_tracing_runtime_host_set_last_failure("window_create",
                                                  "SDL_CreateWindow failed width=%d height=%d: %s",
                                                  window_width,
                                                  window_height,
                                                  SDL_GetError());
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=window_create error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
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
        ray_tracing_runtime_host_set_last_failure("vulkan_shared_device_init",
                                                  "vk_shared_device_init failed");
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=vulkan_shared_device_init error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
        return 0;
    }
    g_runtime_host.shared_device_ready = true;

    shared_device = vk_shared_device_get();
    if (!shared_device) {
        ray_tracing_runtime_host_set_last_failure("vulkan_shared_device_get",
                                                  "vk_shared_device_get failed");
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=vulkan_shared_device_get error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
        return 0;
    }

    init = vk_renderer_init_with_device(&g_runtime_host_renderer_storage, shared_device, window, &cfg);
    if (init != VK_SUCCESS) {
        ray_tracing_runtime_host_set_last_failure("vulkan_renderer_init",
                                                  "vk_renderer_init failed result=%d",
                                                  (int)init);
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=vulkan_renderer_init error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
        return 0;
    }
    renderer = (SDL_Renderer*)&g_runtime_host_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)renderer, (float)window_width, (float)window_height);
#else
    (void)window_width;
    (void)window_height;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        ray_tracing_runtime_host_set_last_failure("sdl_renderer_create",
                                                  "SDL_CreateRenderer failed: %s",
                                                  SDL_GetError());
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=sdl_renderer_create error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
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
        ray_tracing_runtime_host_set_last_failure("font_system_init",
                                                  "initFontSystem failed");
        fprintf(stderr,
                "ray_tracing_runtime_host: stage=font_system_init error=%s\n",
                ray_tracing_runtime_host_last_failure_detail());
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
    ray_tracing_runtime_host_clear_last_failure();

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
