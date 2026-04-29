#ifndef RENDER_RUNTIME_NATIVE_3D_SAMPLING_H
#define RENDER_RUNTIME_NATIVE_3D_SAMPLING_H

#include <stdint.h>

typedef struct {
    uint32_t sampleSequence;
    uint16_t temporalSubpassIndex;
    uint16_t temporalSubpassCount;
} RuntimeNative3DSamplingContext;

void RuntimeNative3DSampling_Stratified2D(const RuntimeNative3DSamplingContext* sampling,
                                          uint32_t base_seed,
                                          int sample_count,
                                          int sample_index,
                                          uint32_t dimension,
                                          double* out_u,
                                          double* out_v);

#endif
