#ifndef RENDER_RUNTIME_NATIVE_3D_SAMPLING_H
#define RENDER_RUNTIME_NATIVE_3D_SAMPLING_H

#include <stdint.h>

typedef enum {
    RUNTIME_CAMERA_PIXEL_AUTO = 0,
    RUNTIME_CAMERA_PIXEL_CENTER,
    RUNTIME_CAMERA_PIXEL_STRATIFIED,
    RUNTIME_CAMERA_PIXEL_PROGRESSIVE
} RuntimeCameraPixelSampler3D;

typedef struct {
    uint32_t sampleSequence;
    uint16_t temporalSubpassIndex;
    uint16_t temporalSubpassCount;
    RuntimeCameraPixelSampler3D cameraPixelSampler;
    uint32_t cameraSequenceSeed; /* Stable frame seed, captured before transport increments. */
} RuntimeNative3DSamplingContext;

void RuntimeNative3DSampling_Stratified2D(const RuntimeNative3DSamplingContext* sampling,
                                          uint32_t base_seed,
                                          int sample_count,
                                          int sample_index,
                                          uint32_t dimension,
                                          double* out_u,
                                          double* out_v);

/* Resolve AUTO once at frame/unit capture, never by consulting the environment per pixel. */
RuntimeCameraPixelSampler3D RuntimeNative3DSampling_ResolveCameraSampler(
    RuntimeCameraPixelSampler3D sampler);
/* Pixel coordinates retain the projector convention: integer means pixel center.
 * One spatial sample per outer subpass; <=1 subpass retains center behavior. */
void RuntimeNative3DSampling_PixelPosition(const RuntimeNative3DSamplingContext* sampling,
    int x, int y, double* out_x, double* out_y);

#endif
