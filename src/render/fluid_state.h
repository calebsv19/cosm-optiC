#pragma once
#include <stdbool.h>

// Simple globals used by animation/render paths to control the fluid overlay.
extern bool g_fluidOverlayEnabled;
extern int  g_fluidFrameIndex;

typedef struct {
    bool  valid;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
} FluidGridBounds;

extern FluidGridBounds g_fluidGrid;
