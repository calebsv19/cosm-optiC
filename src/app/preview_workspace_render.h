#ifndef RAY_TRACING_PREVIEW_WORKSPACE_RENDER_H
#define RAY_TRACING_PREVIEW_WORKSPACE_RENDER_H

#include <SDL2/SDL.h>

#include "app/preview_workspace.h"

void PreviewWorkspaceRender(SDL_Renderer *renderer,
                            const PreviewWorkspace *workspace);

#endif
