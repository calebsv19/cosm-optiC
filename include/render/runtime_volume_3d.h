#ifndef RENDER_RUNTIME_VOLUME_3D_H
#define RENDER_RUNTIME_VOLUME_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"

typedef enum {
    RUNTIME_VOLUME_3D_SOURCE_NONE = 0,
    RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D = 1,
    RUNTIME_VOLUME_3D_SOURCE_PACK = 2,
    RUNTIME_VOLUME_3D_SOURCE_MANIFEST = 3
} RuntimeVolume3DSourceKind;

typedef enum {
    RUNTIME_VOLUME_3D_CHANNEL_NONE = 0,
    RUNTIME_VOLUME_3D_CHANNEL_DENSITY = 1 << 0,
    RUNTIME_VOLUME_3D_CHANNEL_VELOCITY = 1 << 1,
    RUNTIME_VOLUME_3D_CHANNEL_PRESSURE = 1 << 2,
    RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK = 1 << 3
} RuntimeVolume3DChannelMask;

typedef struct {
    uint32_t formatVersion;
    uint32_t gridW;
    uint32_t gridH;
    uint32_t gridD;
    uint64_t cellCount;
    double timeSeconds;
    uint64_t frameIndex;
    double dtSeconds;
    Vec3 origin;
    double voxelSize;
    Vec3 sceneUp;
    Vec3 boundsMin;
    Vec3 boundsMax;
    uint32_t solidMaskCrc32;
    bool valid;
} RuntimeVolumeGrid3D;

typedef struct {
    uint32_t channelMask;
    float* density;
    float* velocityX;
    float* velocityY;
    float* velocityZ;
    float* pressure;
    uint8_t* solidMask;
} RuntimeVolumeChannels3D;

typedef struct {
    RuntimeVolume3DSourceKind sourceKind;
    bool enabled;
    bool affectsLighting;
    bool debugOverlayEnabled;
    bool hasData;
    bool ownsChannelBuffers;
    RuntimeVolumeGrid3D grid;
    RuntimeVolumeChannels3D channels;
} RuntimeVolumeAttachment3D;

const char* RuntimeVolume3DSourceKindLabel(RuntimeVolume3DSourceKind kind);
bool RuntimeVolumeGrid3D_Configure(RuntimeVolumeGrid3D* grid,
                                   uint32_t format_version,
                                   uint32_t grid_w,
                                   uint32_t grid_h,
                                   uint32_t grid_d,
                                   double time_seconds,
                                   uint64_t frame_index,
                                   double dt_seconds,
                                   Vec3 origin,
                                   double voxel_size,
                                   Vec3 scene_up,
                                   uint32_t solid_mask_crc32);
bool RuntimeVolumeGrid3D_IsConfigured(const RuntimeVolumeGrid3D* grid);
void RuntimeVolumeGrid3D_Reset(RuntimeVolumeGrid3D* grid);

bool RuntimeVolumeChannels3D_HasMask(const RuntimeVolumeChannels3D* channels,
                                     uint32_t channel_mask);
void RuntimeVolumeChannels3D_Reset(RuntimeVolumeChannels3D* channels);

void RuntimeVolumeAttachment3D_Init(RuntimeVolumeAttachment3D* attachment);
void RuntimeVolumeAttachment3D_ClearOwnedChannels(RuntimeVolumeAttachment3D* attachment);
bool RuntimeVolumeAttachment3D_AllocateOwnedChannels(RuntimeVolumeAttachment3D* attachment,
                                                     uint32_t channel_mask);
bool RuntimeVolumeAttachment3D_HasChannel(const RuntimeVolumeAttachment3D* attachment,
                                          uint32_t channel_mask);
void RuntimeVolumeAttachment3D_Reset(RuntimeVolumeAttachment3D* attachment);
void RuntimeVolumeAttachment3D_Free(RuntimeVolumeAttachment3D* attachment);

#endif
