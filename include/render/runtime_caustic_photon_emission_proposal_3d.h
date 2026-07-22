#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_EMISSION_PROPOSAL_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_EMISSION_PROPOSAL_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_photon_settings_3d.h"
#include "render/runtime_light_set_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED = 0,
    RUNTIME_CAUSTIC_PHOTON_EMISSION_LENS_GUIDED = 1
} RuntimeCausticPhotonEmissionProposalMode3D;

typedef struct {
    bool attempted;
    bool applied;
    double apertureRadius;
    double apertureAreaPdf;
    double guidedDirectionPdf;
    double unbiasedDirectionPdf;
    double fluxCorrection;
    uint32_t lensCount;
    uint32_t selectedLensIndex;
    double selectedComponentDirectionPdf;
    double mixtureDirectionPdf;
} RuntimeCausticPhotonEmissionProposalReadback3D;

RuntimeCausticPhotonEmissionProposalMode3D
RuntimeCausticPhotonEmissionProposalMode3D_FromLabel(const char* label);
const char* RuntimeCausticPhotonEmissionProposalMode3D_Label(
    RuntimeCausticPhotonEmissionProposalMode3D mode);
bool RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidance(
    RuntimeCausticPhotonSample3D* sample,
    const RuntimeLightSource3D* source,
    const RuntimeCausticLensShape3D* lens,
    RuntimeCausticPhotonEmissionProposalReadback3D* out_readback);
bool RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
    RuntimeCausticPhotonSample3D* sample,
    const RuntimeLightSource3D* source,
    const RuntimeCausticLensShape3D* lenses,
    uint32_t lens_count,
    RuntimeCausticPhotonEmissionProposalReadback3D* out_readback);
#endif
