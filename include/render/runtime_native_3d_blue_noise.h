#ifndef RENDER_RUNTIME_NATIVE_3D_BLUE_NOISE_H
#define RENDER_RUNTIME_NATIVE_3D_BLUE_NOISE_H

#include <stdint.h>

void RuntimeNative3DBlueNoise_Jitter2D(uint32_t sample_sequence,
                                       uint32_t base_seed,
                                       uint32_t dimension,
                                       double* out_u,
                                       double* out_v);

#endif
