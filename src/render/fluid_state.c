#include "render/fluid_state.h"

bool g_fluidOverlayEnabled = false;
int  g_fluidFrameIndex = 0;
FluidOverlayMode g_fluidOverlayMode = FLUID_OVERLAY_MODE_DENSITY;
FluidGridBounds g_fluidGrid = {0};
