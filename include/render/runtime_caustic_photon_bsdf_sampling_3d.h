#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_SAMPLING_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_SAMPLING_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_bsdf_direction_3d.h"
#include "render/runtime_caustic_photon_settings_3d.h"
#include "render/runtime_path_depth_policy_3d.h"

typedef struct {
    double lobeUnitSample;
    RuntimeCausticPhotonBsdfDirectionSample3D directionSample;
} RuntimeCausticPhotonBsdfExplicitSample3D;

typedef struct {
    bool valid;
    uint32_t depth;
    uint32_t baseSeed;
    RuntimeCausticPhotonBsdfExplicitSample3D bsdfSample;
    double rouletteUnitSample;
} RuntimeCausticPhotonBsdfSampleStream3D;

typedef struct {
    bool valid;
    bool evaluated;
    bool terminated;
    uint32_t depth;
    double unitSample;
    double throughputLuma;
    double survivalProbability;
    double branchPdf;
    Vec3 throughputBefore;
    Vec3 throughputAfter;
    Vec3 terminatedThroughput;
    Vec3 expectedThroughput;
} RuntimeCausticPhotonRoulette3D;

bool RuntimeCausticPhotonBsdfSampling3D_Generate(
    const RuntimeCausticPhotonSample3D* photon,
    uint32_t depth,
    RuntimeCausticPhotonBsdfSampleStream3D* out_stream);
bool RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
    const RuntimePathDepthPolicy3D* policy,
    uint32_t depth,
    Vec3 transport_weight,
    double unit_sample,
    RuntimeCausticPhotonRoulette3D* out_roulette);

#endif
