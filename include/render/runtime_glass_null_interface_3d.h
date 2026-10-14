#ifndef RENDER_RUNTIME_GLASS_NULL_INTERFACE_3D_H
#define RENDER_RUNTIME_GLASS_NULL_INTERFACE_3D_H
#include <math.h>
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_surface_sampling.h"

/* Explicit, smooth, index-matched white glass is a null boundary in air.
 * Keep the original ray and its differentials: restarting at an offset point
 * changes filtering and stochastic receiver identity despite zero refraction.
 * Conservative exclusions retain authored effects and all legacy glass. */
static inline bool RuntimeGlassNullInterface3D_IsNeutral(
    const RuntimeMaterialPayload3D* p) {
    return p && p->valid && p->hasGlassTransportOverride &&
        p->opticalIor == 1.0 && p->transparency == 1.0 &&
        p->bsdf.roughness == 0.0 && p->emissive == 0.0 &&
        p->hasGlassInterfaceTintOverride && p->hasGlassAbsorptionColorOverride &&
        p->glassInterfaceTintR == 1.0 && p->glassInterfaceTintG == 1.0 &&
        p->glassInterfaceTintB == 1.0 && p->glassAbsorptionColorR == 1.0 &&
        p->glassAbsorptionColorG == 1.0 && p->glassAbsorptionColorB == 1.0 &&
        !p->hasMicrodetailNormal &&
        !RuntimeSurfaceSamplingNormalResponseActive(p->sceneObjectIndex);
}

static inline bool RuntimeGlassNullInterface3D_Trace(
    const RuntimeScene3D* scene, const Ray3D* ray, double t_min,
    double t_max, HitInfo3D* hit) {
    for (int boundary = 0; boundary < 128; ++boundary) {
        RuntimeMaterialPayload3D payload = {0};
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,ray,t_min,t_max,hit)) return false;
        if (scene->volume.enabled ||
            !RuntimeMaterialPayload3D_ResolveFromHit(hit,&payload) ||
            !RuntimeGlassNullInterface3D_IsNeutral(&payload)) return true;
        double next = nextafter(hit->t,INFINITY);
        if (!(next > t_min) || !(next < t_max)) return true;
        t_min = next;
    }
    /* Bounded fallback keeps the last geometry rather than hiding uncertainty. */
    return true;
}
#endif
