#ifndef RAY_TRACING_RENDER_FONT_BRIDGE_H
#define RAY_TRACING_RENDER_FONT_BRIDGE_H

#include <SDL2/SDL.h>

#include "core_base.h"
#include "kit_render.h"

typedef enum RayTracingFontSlot {
    RAY_TRACING_FONT_SLOT_UI_REGULAR = 0
} RayTracingFontSlot;

typedef struct RayTracingResolvedFont {
    KitRenderResolvedTextRun text_run;
    char resolved_path[384];
    int used_shared_font;
} RayTracingResolvedFont;

CoreResult ray_tracing_font_bridge_resolve(SDL_Renderer* renderer,
                                           RayTracingFontSlot slot,
                                           int logical_point_size,
                                           int min_point_size,
                                           RayTracingResolvedFont* out_resolved);

int ray_tracing_font_bridge_base_point_size(RayTracingFontSlot slot, int fallback_point_size);

#endif
