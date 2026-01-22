#ifndef ENGINE_RENDER_RENDERER_BACKEND_H
#define ENGINE_RENDER_RENDERER_BACKEND_H

#include "engine/Render/build_config.h"
#include <SDL2/SDL.h>

#if USE_VULKAN
#ifndef VK_RENDERER_SHADER_ROOT
#define VK_RENDERER_SHADER_ROOT "shared/vk_renderer"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#endif

#include "vk_renderer_sdl.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#ifndef SDL_RenderFillRect
#error "Vulkan SDL compatibility layer not active: SDL_RenderFillRect macro missing."
#endif

typedef char SDL_Vulkan_Renderer_Alias_Check[
    (sizeof(VkRenderer*) == sizeof(SDL_Renderer*)) ? 1 : -1];
#endif

#endif
