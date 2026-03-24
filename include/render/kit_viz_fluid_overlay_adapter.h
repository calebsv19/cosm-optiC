#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/fluid_import.h"

typedef struct KitVizFluidOverlayConfig {
    uint8_t alpha;
    bool auto_range;
    float min_density;
    float max_density;
    int colormap;
} KitVizFluidOverlayConfig;

bool kit_viz_fluid_overlay_build_density_rgba(const FluidFrame *frame,
                                              const KitVizFluidOverlayConfig *config,
                                              uint8_t *out_rgba,
                                              size_t out_rgba_size);
