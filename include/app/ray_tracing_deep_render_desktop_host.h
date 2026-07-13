#ifndef APP_RAY_TRACING_DEEP_RENDER_DESKTOP_HOST_H
#define APP_RAY_TRACING_DEEP_RENDER_DESKTOP_HOST_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum RayTracingDeepRenderDesktopSelection {
    RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED = 0,
    RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED,
    RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE,
    RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_DYNAMIC_DEPENDENCY,
} RayTracingDeepRenderDesktopSelection;

RayTracingDeepRenderDesktopSelection
RayTracingDeepRenderDesktopHost_AssessSelection(bool deep_render,
                                                bool async_deep_render,
                                                bool native_3d,
                                                bool tiled,
                                                bool dynamic_dependency);

bool RayTracingDeepRenderDesktopHost_BeginRun(int start_frame_index,
                                              int frame_count);
bool RayTracingDeepRenderDesktopHost_SubmitFrame(SDL_Window* window,
                                                SDL_Renderer* renderer,
                                                double light_x,
                                                double light_y,
                                                int* frame_counter,
                                                bool* running);
bool RayTracingDeepRenderDesktopHost_HasActiveWork(void);
bool RayTracingDeepRenderDesktopHost_IsSelected(void);
bool RayTracingDeepRenderDesktopHost_CompletedSuccessfully(void);
void RayTracingDeepRenderDesktopHost_Shutdown(void);

const char* RayTracingDeepRenderDesktopSelection_Name(
    RayTracingDeepRenderDesktopSelection selection);

#endif
