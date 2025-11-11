#ifndef ENGINE_RENDER_PIPELINE_H
#define ENGINE_RENDER_PIPELINE_H

#include "engine/Render/renderer_backend.h"

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int width;
    int height;
} RenderContext;

RenderContext* getRenderContext(void);
void setRenderContext(SDL_Renderer* renderer,
                      SDL_Window* window,
                      int width,
                      int height);

#endif
