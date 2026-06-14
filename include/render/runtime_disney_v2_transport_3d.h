#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSPORT_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSPORT_3D_H

#include <stdbool.h>

#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_scene_3d.h"

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoop(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result);

#endif
