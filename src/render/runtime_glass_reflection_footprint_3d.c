#include "render/runtime_physical_glass_3d.h"
#include <math.h>

/* Ray differentials differentiate one realized continuous path. A finite
 * one-pixel stochastic neighbor can select the other Fresnel lobe; that is
 * not evidence that the central reflection has no local derivative. Compute
 * a symmetric local derivative without forcing a neighbor's branch. */
static bool derivative(const RuntimeMaterialPayload3D *p, const HitInfo3D *h,
    Vec3 normal, const RuntimePhysicalGlass3DSample *event, double from, double to,
    double u, double v, double choice, int axis, double step, Vec3 *out) {
    Vec3 dp=axis?h->pixelDpDy:h->pixelDpDx;
    Vec3 incoming_neighbor=axis?h->incidentDirectionDy:h->incidentDirectionDx;
    Vec3 incident=vec3_normalize(h->incidentDirection);
    Vec3 di=vec3_sub(vec3_normalize(incoming_neighbor),incident);
    Vec3 dn=vec3(0,0,0), raw=normal;
    if (!h->constantShadingNormal) {
        raw=h->reflectionNormalValue;
        dn=vec3_add(vec3_add(vec3_scale(h->reflectionNormalGradient[0],dp.x),
            vec3_scale(h->reflectionNormalGradient[1],dp.y)),
            vec3_scale(h->reflectionNormalGradient[2],dp.z));
    }
    RuntimePhysicalGlass3DSample plus,minus;
    if (!RuntimePhysicalGlass3D_Sample(p,vec3_normalize(vec3_add(raw,vec3_scale(dn,step))),
            vec3_normalize(vec3_add(incident,vec3_scale(di,step))),from,to,u,v,choice,&plus) ||
        !RuntimePhysicalGlass3D_Sample(p,vec3_normalize(vec3_sub(raw,vec3_scale(dn,step))),
            vec3_normalize(vec3_sub(incident,vec3_scale(di,step))),from,to,u,v,choice,&minus) ||
        !plus.reflected || !minus.reflected || plus.scatteringOrder!=1 || minus.scatteringOrder!=1 ||
        plus.delta!=event->delta || minus.delta!=event->delta) return false;
    *out=vec3_scale(vec3_sub(plus.direction,minus.direction),.5/step);
    return isfinite(out->x)&&isfinite(out->y)&&isfinite(out->z);
}

bool RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(
    const RuntimeMaterialPayload3D *p,const HitInfo3D *h,Vec3 normal,
    const RuntimePhysicalGlass3DSample *event,double from,double to,
    double u,double v,double choice,Ray3D *next) {
    Vec3 checked_normals[2], directions[2];
    if (!p || !h || !event || !next || !event->valid || !event->reflected ||
        event->scatteringOrder!=1 || p->hasMicrodetailNormal ||
        !RuntimeRay3D_NormalFootprintNeighbors(h,normal,checked_normals)) return false;
    for (int axis=0;axis<2;++axis) {
        Vec3 coarse,fine;
        if (!derivative(p,h,normal,event,from,to,u,v,choice,axis,1e-4,&coarse) ||
            !derivative(p,h,normal,event,from,to,u,v,choice,axis,5e-5,&fine) ||
            vec3_length(vec3_sub(coarse,fine))>1e-5*fmax(1,vec3_length(fine))) return false;
        directions[axis]=vec3_normalize(vec3_add(event->direction,fine));
        /* Retain the geometric-side guard at the full differential extent. */
        if (!isfinite(directions[axis].x+directions[axis].y+directions[axis].z) ||
            vec3_dot(directions[axis],HitInfo3D_BoundaryNormal(h))*
            vec3_dot(h->incidentDirection,HitInfo3D_BoundaryNormal(h))>=0) return false;
    }
    next->directionDx=directions[0];next->directionDy=directions[1];
    next->originDx=vec3_add(next->origin,h->pixelDpDx);
    next->originDy=vec3_add(next->origin,h->pixelDpDy);
    next->hasDifferentials=next->hasDifferentialOrigins=true;
    next->footprintTransported=true;next->footprintUnbounded=false;
    return true;
}
