#include "render/runtime_disney_v2_atmosphere_3d.h"
#include "render/runtime_volume_3d_integrate.h"

bool RuntimeDisneyV2Atmosphere3D_Active(const RuntimeScene3D* scene) {
    return scene && RuntimeVolume3D_HasActiveExtinction(&scene->volume) &&
        RuntimeVolume3DMaterial_ForAttachment(&scene->volume).physical_smoke;
}
bool RuntimeDisneyV2Atmosphere3D_Advance(const RuntimeScene3D* scene,
    const Ray3D* ray,double distance,const RuntimeNative3DSamplingContext* sampling,
    Vec3* throughput,Vec3* radiance) {
    if(!ray || !throughput || !radiance || !RuntimeDisneyV2Atmosphere3D_Active(scene)) return false;
    const RuntimeVolume3DScatterResult scatter=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(scene,ray,0,distance,sampling);
    const RuntimeVisibility3DTransmittance t=RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,ray,0,distance);
    radiance->x+=throughput->x*scatter.radianceR;
    radiance->y+=throughput->y*scatter.radianceG;
    radiance->z+=throughput->z*scatter.radianceB;
    throughput->x*=t.r;throughput->y*=t.g;throughput->z*=t.b;
    return scatter.radiance>0;
}
void RuntimeDisneyV2Atmosphere3D_Compose(const RuntimeScene3D* scene,
    const Ray3D* ray,double distance,const RuntimeNative3DSamplingContext* sampling,
    double* r,double* g,double* b) {
    if(!r || !g || !b || !RuntimeDisneyV2Atmosphere3D_Active(scene)) return;
    Vec3 throughput=vec3(1,1,1),radiance=vec3(0,0,0);
    RuntimeDisneyV2Atmosphere3D_Advance(scene,ray,distance,sampling,&throughput,&radiance);
    *r=*r*throughput.x+radiance.x;*g=*g*throughput.y+radiance.y;*b=*b*throughput.z+radiance.z;
}
