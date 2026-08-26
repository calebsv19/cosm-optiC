#include "render/vk_shared_device.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool check(bool condition, const char* message) {
    if (condition) return true;
    fprintf(stderr, "ray_tracing_vulkan_runtime_lifecycle_contract_test: %s\n", message);
    return false;
}

static bool draw_frame(VkRenderer* renderer, VkExtent2D* out_extent) {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent = {0};
    SDL_Rect rect = {16, 12, 64, 36};

    if (vk_renderer_begin_frame(renderer, &command_buffer, &framebuffer, &extent) != VK_SUCCESS) {
        return false;
    }
    if (command_buffer == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE ||
        extent.width == 0u || extent.height == 0u) {
        return false;
    }
    vk_renderer_set_draw_color(renderer, 0.15f, 0.55f, 0.90f, 1.0f);
    vk_renderer_fill_rect(renderer, &rect);
    if (vk_renderer_end_frame(renderer, command_buffer) != VK_SUCCESS) return false;
    if (out_extent) *out_extent = extent;
    return true;
}

static bool capture_exists(const char* path) {
    struct stat info;
    return path && stat(path, &info) == 0 && info.st_size > 64;
}

int main(void) {
    const int initial_width = 320;
    const int initial_height = 180;
    const int resized_width = 480;
    const int resized_height = 270;
    const char* capture_path = "/private/tmp/ray_tracing_vk_runtime_lifecycle_capture.ppm";
    SDL_Window* window = NULL;
    VkRenderer renderer;
    VkRendererConfig config;
    VkRendererDevice* shared_device = NULL;
    VkExtent2D initial_extent = {0};
    VkExtent2D resized_extent = {0};
    int logical_width = 0;
    int logical_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    bool renderer_ready = false;
    bool passed = false;

    memset(&renderer, 0, sizeof(renderer));
    (void)remove(capture_path);
    if (!check(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL_Init failed")) goto cleanup;

    window = SDL_CreateWindow("RayTracing Vulkan runtime lifecycle contract",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              initial_width,
                              initial_height,
                              SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!check(window != NULL, "hidden Vulkan window creation failed")) goto cleanup;

    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_TRUE;
    config.clear_color[0] = 0.02f;
    config.clear_color[1] = 0.03f;
    config.clear_color[2] = 0.05f;
    config.clear_color[3] = 1.0f;

    if (!check(vk_shared_device_init(window, &config), "shared runtime device initialization failed")) {
        goto cleanup;
    }
    shared_device = vk_shared_device_get();
    if (!check(shared_device != NULL, "shared runtime device was unavailable")) goto cleanup;
    if (!check(shared_device->runtime.instance == shared_device->instance &&
                   shared_device->runtime.physical_device == shared_device->physical_device &&
                   shared_device->runtime.device == shared_device->device &&
                   shared_device->runtime.graphics_queue == shared_device->graphics_queue &&
                   shared_device->runtime.present_queue == shared_device->present_queue,
               "runtime and renderer device handles diverged")) {
        goto cleanup;
    }
    if (!check(shared_device->runtime.report.validation_requested &&
                   shared_device->runtime.report.validation_enabled &&
                   shared_device->runtime.report.validation_warning_count == 0u &&
                   shared_device->runtime.report.validation_error_count == 0u,
               "validation was not enabled and clean at device initialization")) {
        goto cleanup;
    }

    if (!check(vk_renderer_init_with_device(&renderer, shared_device, window, &config) == VK_SUCCESS,
               "runtime-backed renderer initialization failed")) {
        goto cleanup;
    }
    renderer_ready = true;
    if (!check(renderer.context.device == shared_device,
               "renderer did not retain the shared runtime-backed device")) {
        goto cleanup;
    }

    SDL_GetWindowSize(window, &logical_width, &logical_height);
    SDL_Vulkan_GetDrawableSize(window, &drawable_width, &drawable_height);
    if (!check(logical_width == initial_width && logical_height == initial_height &&
                   drawable_width >= logical_width && drawable_height >= logical_height,
               "initial logical and drawable dimensions were invalid")) {
        goto cleanup;
    }
    vk_renderer_set_logical_size(&renderer, (float)logical_width, (float)logical_height);
    if (!check(draw_frame(&renderer, &initial_extent) &&
                   initial_extent.width == (uint32_t)drawable_width &&
                   initial_extent.height == (uint32_t)drawable_height,
               "initial frame did not use drawable-pixel extent")) {
        goto cleanup;
    }

    if (!check(vk_renderer_request_capture(&renderer, capture_path) == VK_SUCCESS,
               "capture request was rejected")) {
        goto cleanup;
    }
    if (!check(draw_frame(&renderer, NULL) && renderer.debug_capture.dumped == VK_TRUE &&
                   capture_exists(capture_path),
               "capture frame did not produce a readable artifact")) {
        goto cleanup;
    }

    SDL_SetWindowSize(window, resized_width, resized_height);
    SDL_PumpEvents();
    SDL_GetWindowSize(window, &logical_width, &logical_height);
    SDL_Vulkan_GetDrawableSize(window, &drawable_width, &drawable_height);
    if (!check(logical_width == resized_width && logical_height == resized_height &&
                   drawable_width >= logical_width && drawable_height >= logical_height,
               "hidden window resize did not take effect")) {
        goto cleanup;
    }
    if (!check(vk_renderer_recreate_swapchain(&renderer, window) == VK_SUCCESS,
               "swapchain recreation after resize failed")) {
        goto cleanup;
    }
    vk_renderer_set_logical_size(&renderer, (float)logical_width, (float)logical_height);
    if (!check(draw_frame(&renderer, &resized_extent) &&
                   resized_extent.width == (uint32_t)drawable_width &&
                   resized_extent.height == (uint32_t)drawable_height &&
                   (resized_extent.width != initial_extent.width ||
                    resized_extent.height != initial_extent.height),
               "recreated frame did not use the resized drawable extent")) {
        goto cleanup;
    }
    if (!check(shared_device->runtime.report.validation_warning_count == 0u &&
                   shared_device->runtime.report.validation_error_count == 0u,
               "validation reported diagnostics after frame, capture, or recreation")) {
        goto cleanup;
    }

    printf("ray_tracing_vulkan_runtime_lifecycle_contract_test: success logical=%dx%d drawable=%dx%d scale=%.2fx%.2f capture=%s\n",
           logical_width,
           logical_height,
           drawable_width,
           drawable_height,
           (double)drawable_width / (double)logical_width,
           (double)drawable_height / (double)logical_height,
           capture_path);
    passed = true;

cleanup:
    if (renderer_ready) vk_renderer_shutdown_surface(&renderer);
    vk_shared_device_shutdown();
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    (void)remove(capture_path);
    return passed ? 0 : 1;
}
