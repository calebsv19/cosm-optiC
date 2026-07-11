#ifndef APP_RAY_TRACING_DESKTOP_ASYNC_BRIDGE_H
#define APP_RAY_TRACING_DESKTOP_ASYNC_BRIDGE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

bool RayTracingDesktopAsyncBridge_SubmitFrame(SDL_Window* window,
                                              SDL_Renderer* renderer,
                                              double light_x,
                                              double light_y,
                                              int* frame_counter,
                                              bool* running);
void RayTracingDesktopAsyncBridge_Shutdown(void);

#endif
