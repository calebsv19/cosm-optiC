#include "render/runtime_volume_3d_transform.h"
#include <math.h>

static bool finite_vec(Vec3 v) {return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);}
bool RuntimeVolume3D_PlacementIsIdentity(const RuntimeVolumeAttachment3D* v) {
    if(!v || !v->hasPlacement) return true;
    const double tolerance=1e-12;
    if(vec3_length(v->placementTranslation)>tolerance ||
       vec3_length(vec3_sub(v->placementScale,vec3(1,1,1)))>tolerance) return false;
    for(int i=0;i<3;++i) {
        Vec3 axis=i==0?vec3(1,0,0):i==1?vec3(0,1,0):vec3(0,0,1);
        if(vec3_length(vec3_sub(axis,v->placementBasis[i]))>tolerance) return false;
    }
    return true;
}
double RuntimeVolume3D_WorldVoxelSize(const RuntimeVolumeAttachment3D* v) {
    if(!v) return 0;
    return v->grid.voxelSize*(v->hasPlacement?fmin(v->placementScale.x,fmin(v->placementScale.y,v->placementScale.z)):1);
}
static Vec3 rotate(Vec3 p,Vec3 degrees) {
    const double k=0.01745329251994329577;
    double c=cos(degrees.x*k),s=sin(degrees.x*k);
    p=vec3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);
    c=cos(degrees.y*k);s=sin(degrees.y*k);
    p=vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);
    c=cos(degrees.z*k);s=sin(degrees.z*k);
    return vec3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);
}
bool RuntimeVolume3D_SetPlacement(RuntimeVolumeAttachment3D* v,Vec3 t,Vec3 r,Vec3 s) {
    if(!v || !finite_vec(t) || !finite_vec(r) || !finite_vec(s) || s.x<=0 || s.y<=0 || s.z<=0) return false;
    v->placementTranslation=t;v->placementScale=s;
    v->placementBasis[0]=rotate(vec3(1,0,0),r);
    v->placementBasis[1]=rotate(vec3(0,1,0),r);
    v->placementBasis[2]=rotate(vec3(0,0,1),r);
    v->hasPlacement=true;return true;
}
Vec3 RuntimeVolume3D_ToCacheDirection(const RuntimeVolumeAttachment3D* v,Vec3 p) {
    if(!v || !v->hasPlacement) return p;
    return vec3(vec3_dot(p,v->placementBasis[0])/v->placementScale.x,
        vec3_dot(p,v->placementBasis[1])/v->placementScale.y,
        vec3_dot(p,v->placementBasis[2])/v->placementScale.z);
}
Vec3 RuntimeVolume3D_ToCachePoint(const RuntimeVolumeAttachment3D* v,Vec3 p) {
    if(!v || !v->hasPlacement) return p;
    return vec3_add(v->grid.origin,RuntimeVolume3D_ToCacheDirection(v,
        vec3_sub(vec3_sub(p,v->grid.origin),v->placementTranslation)));
}
Vec3 RuntimeVolume3D_ToWorldDirection(const RuntimeVolumeAttachment3D* v,Vec3 p) {
    if(RuntimeVolume3D_PlacementIsIdentity(v)) return p;
    return vec3_add(vec3_add(vec3_scale(v->placementBasis[0],p.x*v->placementScale.x),
        vec3_scale(v->placementBasis[1],p.y*v->placementScale.y)),
        vec3_scale(v->placementBasis[2],p.z*v->placementScale.z));
}
Vec3 RuntimeVolume3D_ToWorldPoint(const RuntimeVolumeAttachment3D* v,Vec3 p) {
    if(RuntimeVolume3D_PlacementIsIdentity(v)) return p;
    return vec3_add(vec3_add(v->grid.origin,v->placementTranslation),
        RuntimeVolume3D_ToWorldDirection(v,vec3_sub(p,v->grid.origin)));
}
void RuntimeVolume3D_WorldBounds(const RuntimeVolumeAttachment3D* v,Vec3* minimum,Vec3* maximum) {
    if(!v || !minimum || !maximum) return;
    *minimum=vec3(INFINITY,INFINITY,INFINITY);*maximum=vec3(-INFINITY,-INFINITY,-INFINITY);
    for(int i=0;i<8;++i) {
        Vec3 p=RuntimeVolume3D_ToWorldPoint(v,vec3(i&1?v->grid.boundsMax.x:v->grid.boundsMin.x,
            i&2?v->grid.boundsMax.y:v->grid.boundsMin.y,i&4?v->grid.boundsMax.z:v->grid.boundsMin.z));
        minimum->x=fmin(minimum->x,p.x);minimum->y=fmin(minimum->y,p.y);minimum->z=fmin(minimum->z,p.z);
        maximum->x=fmax(maximum->x,p.x);maximum->y=fmax(maximum->y,p.y);maximum->z=fmax(maximum->z,p.z);
    }
}
