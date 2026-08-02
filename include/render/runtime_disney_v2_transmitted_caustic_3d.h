#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSMITTED_CAUSTIC_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSMITTED_CAUSTIC_3D_H

#include <stdbool.h>

#include "render/runtime_disney_v2_3d.h"

void RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(
    RuntimeDisneyV2_3DResult* result,
    bool query_hit,
    Vec3 radiance,
    double throughput_r,
    double throughput_g,
    double throughput_b);

bool RuntimeDisneyV2TransmittedCaustic3D_SampleDirectMap(
    RuntimeDisneyV2_3DResult* result,
    const HitInfo3D* receiver_hit,
    double throughput_r,
    double throughput_g,
    double throughput_b);

void RuntimeDisneyV2TransmittedCaustic3D_Finalize(
    RuntimeDisneyV2_3DResult* result,
    int transmission_contributing_sample_count);

#endif
