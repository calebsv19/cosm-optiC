#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SAMPLE_SUPPORT_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SAMPLE_SUPPORT_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_settings_3d.h"

typedef struct {
    bool valid;
    uint64_t recordCount;
    uint64_t neighborLimit;
    uint64_t adaptiveRecordCount;
    uint64_t maximumRadiusRecordCount;
    double minimumSupportRadius;
    double meanSupportRadius;
    double maximumSupportRadius;
} RuntimeCausticPhotonSampleSupportReadback3D;

bool RuntimeCausticPhotonSampleSupport3D_Prepare(
    RuntimeCausticPhotonMapRecord3D* records,
    uint64_t record_count,
    uint64_t neighbor_limit,
    double min_normal_dot,
    RuntimeCausticPhotonSampleSupportReadback3D* out_readback);

#endif
