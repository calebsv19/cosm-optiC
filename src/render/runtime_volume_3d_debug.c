#include "render/runtime_volume_3d_debug.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

void RuntimeVolumeDebugSummary3D_Reset(RuntimeVolumeDebugSummary3D* summary) {
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->sourceKind = RUNTIME_VOLUME_3D_SOURCE_NONE;
}

bool RuntimeVolumeDebugSummary3D_Build(const RuntimeVolumeAttachment3D* attachment,
                                       RuntimeVolumeDebugSummary3D* out_summary) {
    bool range_seeded = false;

    if (!attachment || !out_summary) return false;

    RuntimeVolumeDebugSummary3D_Reset(out_summary);
    out_summary->sourceKind = attachment->sourceKind;
    out_summary->enabled = attachment->enabled;
    out_summary->affectsLighting = attachment->affectsLighting;
    out_summary->debugOverlayEnabled = attachment->debugOverlayEnabled;
    out_summary->hasData = attachment->hasData;
    out_summary->layoutValid = RuntimeVolumeGrid3D_IsConfigured(&attachment->grid);
    out_summary->hasDensity =
        RuntimeVolumeAttachment3D_HasChannel(attachment, RUNTIME_VOLUME_3D_CHANNEL_DENSITY);
    out_summary->hasVelocity =
        RuntimeVolumeAttachment3D_HasChannel(attachment, RUNTIME_VOLUME_3D_CHANNEL_VELOCITY);
    out_summary->hasPressure =
        RuntimeVolumeAttachment3D_HasChannel(attachment, RUNTIME_VOLUME_3D_CHANNEL_PRESSURE);
    out_summary->hasSolidMask =
        RuntimeVolumeAttachment3D_HasChannel(attachment, RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK);
    out_summary->channelMask = attachment->channels.channelMask;

    if (!out_summary->layoutValid) {
        return true;
    }

    out_summary->gridW = attachment->grid.gridW;
    out_summary->gridH = attachment->grid.gridH;
    out_summary->gridD = attachment->grid.gridD;
    out_summary->cellCount = attachment->grid.cellCount;
    out_summary->voxelSize = attachment->grid.voxelSize;
    out_summary->boundsMin = attachment->grid.boundsMin;
    out_summary->boundsMax = attachment->grid.boundsMax;

    if (!out_summary->hasDensity || !attachment->channels.density) {
        return true;
    }

    for (uint64_t i = 0; i < attachment->grid.cellCount; ++i) {
        const double value = (double)attachment->channels.density[i];
        if (!isfinite(value)) {
            continue;
        }
        if (!range_seeded) {
            out_summary->densityMin = value;
            out_summary->densityMax = value;
            range_seeded = true;
        } else {
            if (value < out_summary->densityMin) out_summary->densityMin = value;
            if (value > out_summary->densityMax) out_summary->densityMax = value;
        }
        if (fabs(value) > 1e-9) {
            out_summary->densityNonZeroCellCount += 1u;
        }
    }

    out_summary->hasDensityRange = range_seeded;
    return true;
}
