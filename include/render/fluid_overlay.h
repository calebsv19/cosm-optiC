#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera/camera.h"
#include "import/fluid_import.h"

// Draws a translucent density overlay for a fluid frame using the current camera transform.
// Returns true on success.
bool fluid_overlay_draw(SDL_Renderer *renderer,
                        const FluidFrame *frame,
                        const Camera *camera,
                        int screen_w,
                        int screen_h);
