#ifndef TIMESCOPE_HUD_RENDERER_H
#define TIMESCOPE_HUD_RENDERER_H

#include "engine/Render/renderer_backend.h"

// Call once during init
void hud_init(void);

// Render all timer HUD elements each frame
void ts_render(SDL_Renderer* renderer);

#endif // TIMESCOPE_HUD_RENDERER_H
