#ifndef RAY_TRACING_WINDOW_LIFECYCLE_H
#define RAY_TRACING_WINDOW_LIFECYCLE_H
#include <SDL2/SDL.h>
#include <stdbool.h>
/* App-owned event scope and transient input; shared kit owns observation. */
void ray_tracing_window_lifecycle_bind(SDL_Window *window);
bool ray_tracing_window_lifecycle_filter(const SDL_Event *event);
#endif
