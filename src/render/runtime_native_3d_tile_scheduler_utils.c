#include "render/runtime_native_3d_tile_scheduler_internal.h"

#include <SDL2/SDL.h>

double runtime_native_3d_tile_scheduler_ticks_to_ms(uint64_t ticks) {
    const uint64_t frequency = (uint64_t)SDL_GetPerformanceFrequency();
    if (frequency == 0u) {
        return 0.0;
    }
    return ((double)ticks * 1000.0) / (double)frequency;
}
