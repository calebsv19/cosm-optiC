#include "engine/Render/render_pipeline.h"

static RenderContext globalContext = {0};

RenderContext* getRenderContext(void) {
    if (!globalContext.renderer) {
        return NULL;
    }
    return &globalContext;
}

void setRenderContext(SDL_Renderer* renderer,
                      SDL_Window* window,
                      int width,
                      int height) {
    globalContext.renderer = renderer;
    globalContext.window = window;
    globalContext.width = width;
    globalContext.height = height;
}
