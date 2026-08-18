#ifndef RENDER_RUNTIME_HEIGHTFIELD_PERIMETER_RESOLVER_H
#define RENDER_RUNTIME_HEIGHTFIELD_PERIMETER_RESOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Maps an outer-ring sample to the adjacent inboard sample. Corners map
 * diagonally, so every resolved sample belongs to the current interior ring. */
static inline bool RuntimeHeightfieldPerimeter_ResolveInboardIndex(uint32_t grid_w,
                                                                    uint32_t grid_d,
                                                                    uint32_t x,
                                                                    uint32_t z,
                                                                    size_t* out_index) {
    if (!out_index || grid_w < 3u || grid_d < 3u || x >= grid_w || z >= grid_d ||
        (x != 0u && z != 0u && x + 1u != grid_w && z + 1u != grid_d)) {
        return false;
    }
    if (x == 0u) x = 1u;
    if (z == 0u) z = 1u;
    if (x + 1u == grid_w) x = grid_w - 2u;
    if (z + 1u == grid_d) z = grid_d - 2u;
    *out_index = (size_t)z * (size_t)grid_w + (size_t)x;
    return true;
}

#endif
