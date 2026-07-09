#ifndef RENDER_RUNTIME_DISNEY_V2_TRANSMISSION_3D_H
#define RENDER_RUNTIME_DISNEY_V2_TRANSMISSION_3D_H

#include <stdbool.h>

#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_principled_bsdf_3d.h"

typedef struct {
    bool valid;
    Vec3 direction;
    double pdf;
    double throughputR;
    double throughputG;
    double throughputB;
    RuntimeDielectricTransport3D dielectric;
} RuntimeDisneyV2_3DTransmissionSample;

bool RuntimeDisneyV2_3D_SampleTransmission(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const HitInfo3D* hit,
    Vec3 view_dir,
    double transmission_probability,
    RuntimeDisneyV2_3DTransmissionSample* out_sample);

bool RuntimeDisneyV2_3D_ApplyPrimaryTransmissionContinuation(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result);

bool RuntimeDisneyV2_3D_ApplyReflectedTransmissionContinuation(
    const RuntimeScene3D* scene,
    const HitInfo3D* reflected_hit,
    Ray3D reflected_ray,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result);

#endif
