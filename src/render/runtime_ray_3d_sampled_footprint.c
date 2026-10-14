#include "render/runtime_ray_3d.h"
#include <math.h>

static bool finite_direction(Vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) && vec3_length(v)>1e-12;
}
/* Rodrigues rotation without an arbitrary basis axis. The cone's selected
   center direction and its camera neighbors undergo the same rotation. */
static Vec3 rotate_sample(Vec3 v,Vec3 cross,double dot) {
    Vec3 first=vec3_cross(cross,v);
    return vec3_normalize(vec3_add(v,vec3_add(first,
        vec3_scale(vec3_cross(cross,first),1/(1+dot)))));
}
bool RuntimeRay3D_TransportSampledFootprint(const HitInfo3D* hit,Vec3 normal,
    RuntimeRay3DIdealFootprintEvent event,double eta_from,double eta_to,
    Vec3 ideal_direction,Ray3D* ray) {
    if(!ray)return false;
    (void)RuntimeRay3D_TransportIdealFootprint(NULL,normal,event,eta_from,eta_to,ray);
    if(!finite_direction(ideal_direction) || !finite_direction(ray->direction))return false;
    Vec3 from=vec3_normalize(ideal_direction),to=vec3_normalize(ray->direction);
    double dot=vec3_dot(from,to);
    if(!isfinite(dot) || dot<=-1+1e-8)return false;
    Ray3D ideal=*ray;ideal.direction=from;
    if(!RuntimeRay3D_TransportIdealFootprint(hit,normal,event,eta_from,eta_to,&ideal))return false;
    Vec3 cross=vec3_cross(from,to);
    Vec3 dx=rotate_sample(ideal.directionDx,cross,dot);
    Vec3 dy=rotate_sample(ideal.directionDy,cross,dot);
    Vec3 geometric=vec3_normalize(hit->geometricNormal);
    Vec3 directions[]={to,dx,dy};
    for(int i=0;i<3;++i) {
        double side=vec3_dot(directions[i],geometric);
        if(!finite_direction(directions[i]) || !isfinite(side) ||
            (event==RUNTIME_RAY_IDEAL_REFLECTION ? side<=1e-8 : side>=-1e-8))return false;
    }
    ray->originDx=ideal.originDx;ray->originDy=ideal.originDy;
    ray->directionDx=dx;ray->directionDy=dy;
    ray->hasDifferentials=ray->hasDifferentialOrigins=true;
    ray->footprintUnbounded=false;ray->footprintTransported=true;
    return true;
}
