#ifndef RUNTIME_TRANSPORT_WEIGHT_PROBE_3D_H
#define RUNTIME_TRANSPORT_WEIGHT_PROBE_3D_H
#include "render/runtime_disney_v2_transport_internal_3d.h"
/* Selected-pixel, thread-local observation only; no image-sized storage. */
void RuntimeTransportWeightProbe3D_Begin(int x, int y, const RuntimeNative3DSamplingContext* sampling);
void RuntimeTransportWeightProbe3D_End(void);
void RuntimeTransportWeightProbe3D_Sample(const HitInfo3D* hit, const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* bsdf, Vec3 incoming, int depth, double pd, double ps, double pt,
    const RuntimeDisneyV2Transport3DVertexSample* sample);
void RuntimeTransportWeightProbe3D_Weight(int depth, Vec3 parent, Vec3 factor,
    double survival, double roulette_sample, double local_share);
void RuntimeTransportWeightProbe3D_Direct(int depth, const HitInfo3D* hit,
    Vec3 area_weight, Vec3 radiance, double mis_weight);
#endif
