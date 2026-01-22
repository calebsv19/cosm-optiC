#ifndef RENDER_VK_SHARED_DEVICE_H
#define RENDER_VK_SHARED_DEVICE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "vk_renderer_device.h"

#ifdef __cplusplus
extern "C" {
#endif

bool vk_shared_device_init(SDL_Window* window, const VkRendererConfig* config);
void vk_shared_device_shutdown(void);
void vk_shared_device_wait_idle(void);
VkRendererDevice* vk_shared_device_get(void);
void vk_shared_device_mark_lost(void);

#ifdef __cplusplus
}
#endif

#endif // RENDER_VK_SHARED_DEVICE_H
