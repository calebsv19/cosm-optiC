#pragma once

#include "engine/Render/renderer_backend.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Alignment flags (can be OR'ed)
#define ALIGN_LEFT     0x01
#define ALIGN_CENTER   0x02
#define ALIGN_RIGHT    0x04
#define ALIGN_TOP      0x10
#define ALIGN_MIDDLE   0x20
#define ALIGN_BOTTOM   0x40

// Initialize and shut down text system
bool Text_Init(void);
void Text_Quit(void);

// Text size measuring
SDL_Rect Text_Measure(const char* text);

// Text drawing
void Text_Draw(SDL_Renderer* renderer, const char* text, int x, int y, int alignFlags, SDL_Color color);

#ifdef __cplusplus
}
#endif
