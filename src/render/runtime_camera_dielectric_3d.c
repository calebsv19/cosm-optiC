#include "render/runtime_camera_dielectric_3d.h"
#include <math.h>
void RuntimeCameraMedium3D_Init(RuntimeCameraMedium3D* m) { RuntimeCausticPhotonMediumStack3D_Init(m); }
bool RuntimeCameraDielectric3D_Resolve(RuntimeCameraMedium3D* m,
    const RuntimeMaterialPayload3D* p,const HitInfo3D* h,Vec3 incoming,
    RuntimeDielectricTransport3D* out) {
    if(!p || !h || !out) return false;
    Vec3 boundary_normal=HitInfo3D_BoundaryNormal(h);
    if(!m) return RuntimeDielectricTransport3D_Resolve(p,boundary_normal,incoming,out);
    const RuntimeCausticPhotonMediumEntry3D* top=RuntimeCausticPhotonMediumStack3D_Top(m);
    if(!top) return false;
    if(p->thinWalled) return RuntimeDielectricTransport3D_ResolveInterface(p,boundary_normal,incoming,
        top->ior,p->opticalIor>=1?p->opticalIor:p->bsdf.ior,out);
    RuntimeCausticPhotonMediumEntry3D boundary={0};
    if(!RuntimeCausticPhotonMediumEntry3D_FromMaterial(p,h->sceneObjectIndex,0,&boundary)) return false;
    bool entering=vec3_dot(boundary_normal,incoming)<0;
    /* A path may start inside a single solid. Seed only its first outward boundary;
       once a medium is known, mismatched exits and overflow remain failures. */
    if(!entering && m->count==1 && m->pushCount==0 && m->popCount==0) {
        RuntimeCausticPhotonMediumTransition3D transition;
        if(!RuntimeCausticPhotonMediumStack3D_ObserveBoundary(m,&boundary,true,false,&transition)) return false;
    }
    double from,to;
    if(!RuntimeCausticPhotonMediumStack3D_ResolveInterface(m,&boundary,entering,&from,&to)) return false;
    return RuntimeDielectricTransport3D_ResolveInterface(p,boundary_normal,incoming,from,to,out);
}
bool RuntimeCameraMedium3D_Transmit(RuntimeCameraMedium3D* m,
    const RuntimeMaterialPayload3D* p,const HitInfo3D* h,const RuntimeDielectricTransport3D* d) {
    if(!m || !p || !h || !d || p->thinWalled) return true;
    RuntimeCausticPhotonMediumEntry3D boundary={0};RuntimeCausticPhotonMediumTransition3D transition;
    if(!RuntimeCausticPhotonMediumEntry3D_FromMaterial(p,h->sceneObjectIndex,0,&boundary)) return false;
    return RuntimeCausticPhotonMediumStack3D_ObserveBoundary(m,&boundary,d->entering,d->totalInternalReflection,&transition);
}
double RuntimeCameraDielectric3D_RadianceScale(const RuntimeMaterialPayload3D* p,
    const RuntimeDielectricTransport3D* d) {
    if(!p || !d || p->thinWalled) return 1;
    if(!(d->etaFrom>0) || !(d->etaTo>0)) return 0;
    const double ratio=d->etaFrom/d->etaTo;return ratio*ratio;
}
