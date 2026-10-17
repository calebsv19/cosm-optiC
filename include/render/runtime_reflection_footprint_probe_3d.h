#ifndef RUNTIME_REFLECTION_FOOTPRINT_PROBE_3D_H
#define RUNTIME_REFLECTION_FOOTPRINT_PROBE_3D_H
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"
/* Selected-pixel, thread-local, capped stderr observation. No transport changes. */
void RuntimeReflectionFootprintProbe3D_Begin(int x,int y,const RuntimeNative3DSamplingContext* sampling);
void RuntimeReflectionFootprintProbe3D_End(void);
bool RuntimeReflectionFootprintProbe3D_Active(void);
void RuntimeReflectionFootprintProbe3D_Record(const char* stage,const char* branch,int depth,
    const HitInfo3D* hit,const RuntimeMaterialPayload3D* payload,const Ray3D* ray);
void RuntimeReflectionFootprintProbe3D_Filter(const HitInfo3D* hit,unsigned width,unsigned height,
    const double uv[2],const double dx[2],const double dy[2],double lod,
    const double* filtered,const double* point);
#endif
