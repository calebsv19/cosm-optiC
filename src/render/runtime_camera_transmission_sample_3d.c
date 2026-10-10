#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_camera_dielectric_3d.h"
#include <math.h>
bool RuntimeDisneyV2_3D_SampleTransmissionInterface(
    const RuntimeMaterialPayload3D* p,const RuntimePrincipledBSDF3D* b,
    const RuntimeDielectricTransport3D* d,double probability,
    RuntimeDisneyV2_3DTransmissionSample* out) {
    if(!out) return false;*out=(RuntimeDisneyV2_3DTransmissionSample){0};
    if(!p || !b || !b->valid || !d || !d->hasRefraction || !(probability>0) || !(b->transmissionWeight>0)) return false;
    out->dielectric=*d;out->direction=d->refractionDir;out->pdf=probability;
    double weight=b->transmissionWeight*fmax(0,1-d->fresnel)*RuntimeCameraDielectric3D_RadianceScale(p,d)/probability;
    out->throughputR=(p->hasGlassInterfaceTintOverride?p->glassInterfaceTintR:b->baseColorR)*weight;
    out->throughputG=(p->hasGlassInterfaceTintOverride?p->glassInterfaceTintG:b->baseColorG)*weight;
    out->throughputB=(p->hasGlassInterfaceTintOverride?p->glassInterfaceTintB:b->baseColorB)*weight;
    out->valid=isfinite(out->throughputR) && isfinite(out->throughputG) && isfinite(out->throughputB) &&
        out->throughputR>=0 && out->throughputG>=0 && out->throughputB>=0 && weight>0;
    return out->valid;
}
bool RuntimeDisneyV2_3D_SampleTransmission(const RuntimeMaterialPayload3D* p,
    const RuntimePrincipledBSDF3D* b,const HitInfo3D* h,Vec3 view,double probability,
    RuntimeDisneyV2_3DTransmissionSample* out) {
    RuntimeDielectricTransport3D d={0};
    if(!out) return false;*out=(RuntimeDisneyV2_3DTransmissionSample){0};
    if(!RuntimeCameraDielectric3D_Resolve(NULL,p,h,vec3_scale(vec3_normalize(view),-1),&d)) return false;
    bool valid=RuntimeDisneyV2_3D_SampleTransmissionInterface(p,b,&d,probability,out);
    if(!valid) out->dielectric=d;
    return valid;
}
