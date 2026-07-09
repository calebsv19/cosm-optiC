#ifndef RENDER_RUNTIME_VOLUME_3D_DEBUG_H
#define RENDER_RUNTIME_VOLUME_3D_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_volume_3d.h"

typedef struct {
    RuntimeVolume3DSourceKind sourceKind;
    bool enabled;
    bool affectsLighting;
    bool debugOverlayEnabled;
    bool hasData;
    bool layoutValid;
    bool hasDensity;
    bool hasVelocity;
    bool hasPressure;
    bool hasSolidMask;
    bool hasDensityRange;
    uint32_t gridW;
    uint32_t gridH;
    uint32_t gridD;
    uint32_t channelMask;
    uint64_t cellCount;
    uint64_t densityNonZeroCellCount;
    double voxelSize;
    Vec3 boundsMin;
    Vec3 boundsMax;
    double densityMin;
    double densityMax;
} RuntimeVolumeDebugSummary3D;

void RuntimeVolumeDebugSummary3D_Reset(RuntimeVolumeDebugSummary3D* summary);
bool RuntimeVolumeDebugSummary3D_Build(const RuntimeVolumeAttachment3D* attachment,
                                       RuntimeVolumeDebugSummary3D* out_summary);

#endif
