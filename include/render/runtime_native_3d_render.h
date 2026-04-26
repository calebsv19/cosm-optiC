#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int hitPixelCount;
    int visiblePixelCount;
    double maxRadiance;
} RuntimeNative3DRenderStats;

bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats);

#endif
