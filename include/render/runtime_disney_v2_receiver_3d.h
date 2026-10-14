#ifndef RUNTIME_DISNEY_V2_RECEIVER_3D_H
#define RUNTIME_DISNEY_V2_RECEIVER_3D_H

#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_native_3d_pixel_probe.h"

/* A transmitted receiver is viewed along the arriving ray, not its normal.
 * Keep the retained legacy receiver estimator unchanged. */
static inline bool runtime_disney_v2_shade_receiver_3d(
    const RuntimeScene3D* scene, const HitInfo3D* hit, const Ray3D* ray,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimeDisneyV2_3DResult* parent, RuntimeDisneyV2_3DResult* result) {
    if (!parent->payload.hasGlassTransportOverride)
        return RuntimeDisneyV2_3D_ShadeHitWithTraceContext(scene, hit, sampling,
            parent->tracePixelX, parent->tracePixelY,
            parent->tracePixelWidth, parent->tracePixelHeight, result);
    RuntimeNative3DPixelProbe_Receiver(parent->tracePixelX,parent->tracePixelY,
        hit,payload,ray);
    RuntimePrimaryHit3DResult receiver = {0};
    receiver.hit = true;
    receiver.hitInfo = *hit;
    receiver.primaryRay = *ray;
    receiver.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    bool shaded = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
        scene, &receiver, payload, sampling, parent->tracePixelX,
        parent->tracePixelY, parent->tracePixelWidth, parent->tracePixelHeight,
        result);
    RuntimeNative3DPixelProbe_Shade(parent->tracePixelX,parent->tracePixelY,
        sampling,result,0,0,0);
    return shaded;
}
#endif
