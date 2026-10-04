#ifndef RUNTIME_VOLUME_3D_TRANSFORM_H
#define RUNTIME_VOLUME_3D_TRANSFORM_H
#include "render/runtime_volume_3d.h"
bool RuntimeVolume3D_SetPlacement(RuntimeVolumeAttachment3D* volume,
    Vec3 translation, Vec3 rotation_degrees, Vec3 scale);
Vec3 RuntimeVolume3D_ToCachePoint(const RuntimeVolumeAttachment3D* volume,Vec3 point);
Vec3 RuntimeVolume3D_ToCacheDirection(const RuntimeVolumeAttachment3D* volume,Vec3 direction);
Vec3 RuntimeVolume3D_ToWorldPoint(const RuntimeVolumeAttachment3D* volume,Vec3 point);
Vec3 RuntimeVolume3D_ToWorldDirection(const RuntimeVolumeAttachment3D* volume,Vec3 direction);
void RuntimeVolume3D_WorldBounds(const RuntimeVolumeAttachment3D* volume,Vec3* minimum,Vec3* maximum);
bool RuntimeVolume3D_PlacementIsIdentity(const RuntimeVolumeAttachment3D* volume);
double RuntimeVolume3D_WorldVoxelSize(const RuntimeVolumeAttachment3D* volume);
#endif
