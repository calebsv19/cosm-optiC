#ifndef RUNTIME_CAUSTIC_VOLUME_CACHE_PLACEMENT_3D_H
#define RUNTIME_CAUSTIC_VOLUME_CACHE_PLACEMENT_3D_H
#include "render/runtime_caustic_volume_cache_3d.h"
Vec3 runtime_caustic_cache_to_local(const RuntimeCausticVolumeCache3D* cache,Vec3 point);
Vec3 runtime_caustic_cache_to_world(const RuntimeCausticVolumeCache3D* cache,Vec3 point);
Vec3 runtime_caustic_cache_voxel_displacement(const RuntimeCausticVolumeCache3D* cache,Vec3 delta);
double runtime_caustic_cache_world_voxel(const RuntimeCausticVolumeCache3D* cache);
#endif
