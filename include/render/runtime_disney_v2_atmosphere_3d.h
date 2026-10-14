#ifndef RENDER_RUNTIME_DISNEY_V2_ATMOSPHERE_3D_H
#define RENDER_RUNTIME_DISNEY_V2_ATMOSPHERE_3D_H
#include "render/runtime_volume_3d_scatter.h"

bool RuntimeDisneyV2Atmosphere3D_Active(const RuntimeScene3D* scene);
/* Add incident single scattering with incoming path weight, then attenuate that
 * weight for subsequent surfaces/emitters. Legacy profiles are an exact no-op. */
bool RuntimeDisneyV2Atmosphere3D_Advance(const RuntimeScene3D* scene,
    const Ray3D* ray,double distance,const RuntimeNative3DSamplingContext* sampling,
    Vec3* throughput,Vec3* radiance);
void RuntimeDisneyV2Atmosphere3D_Compose(const RuntimeScene3D* scene,
    const Ray3D* ray,double distance,const RuntimeNative3DSamplingContext* sampling,
    double* r,double* g,double* b);
#endif
