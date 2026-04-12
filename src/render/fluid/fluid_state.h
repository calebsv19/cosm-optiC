#pragma once
#include <stdbool.h>

// Simple globals used by animation/render paths to control the fluid overlay.
extern bool g_fluidOverlayEnabled;
extern int  g_fluidFrameIndex;

typedef enum FluidOverlayMode {
    FLUID_OVERLAY_MODE_DENSITY = 0,
    FLUID_OVERLAY_MODE_DENSITY_VELOCITY = 1,
    FLUID_OVERLAY_MODE_VELOCITY_HEATMAP = 2
} FluidOverlayMode;

extern FluidOverlayMode g_fluidOverlayMode;

typedef struct {
    bool  valid;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
} FluidGridBounds;

extern FluidGridBounds g_fluidGrid;
