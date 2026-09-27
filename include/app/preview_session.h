#ifndef PREVIEW_SESSION_H
#define PREVIEW_SESSION_H

#include <SDL2/SDL.h>
#include "animation/timeline_clock.h"

void RunPreviewMode(void);
void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer);
/* Opens paused at the supplied sample; returns the last inspected sample. */
void RunPreviewModeEmbeddedAtSample(SDL_Window* host_window, SDL_Renderer* host_renderer,
                                    TimelineSample* in_out_sample);

#endif
