#ifndef PREVIEW_SESSION_H
#define PREVIEW_SESSION_H

#include <SDL2/SDL.h>

void RunPreviewMode(void);
void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer);

#endif
