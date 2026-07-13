#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_POLICY_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_POLICY_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_material_payload_3d.h"

#define RUNTIME_CAUSTIC_PHOTON_BSDF_MAX_CANDIDATES 4u

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE,
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY,
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR,
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION,
    RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE
} RuntimeCausticPhotonBsdfLobe3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL,
    RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE,
    RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED
} RuntimeCausticPhotonBsdfTermination3D;

typedef struct {
    RuntimeCausticPhotonBsdfLobe3D lobe;
    double rawWeight;
    double selectionPdf;
    Vec3 responseTint;
    Vec3 expectedThroughput;
    bool terminal;
} RuntimeCausticPhotonBsdfCandidate3D;

typedef struct {
    bool valid;
    uint32_t candidateCount;
    double incidentCosine;
    double fresnelReflectance;
    double rawWeightSum;
    double selectionPdfSum;
    Vec3 parentThroughput;
    Vec3 expectedScatteredThroughput;
    Vec3 expectedAbsorbedThroughput;
    RuntimeCausticPhotonBsdfTermination3D termination;
    RuntimeCausticPhotonBsdfCandidate3D
        candidates[RUNTIME_CAUSTIC_PHOTON_BSDF_MAX_CANDIDATES];
} RuntimeCausticPhotonBsdfPolicy3D;

typedef struct {
    bool attempted;
    bool selected;
    uint32_t candidateIndex;
    RuntimeCausticPhotonBsdfLobe3D lobe;
    double unitSample;
    double branchPdf;
    Vec3 throughputBefore;
    Vec3 throughputAfter;
    Vec3 expectedContribution;
    RuntimeCausticPhotonBsdfTermination3D termination;
} RuntimeCausticPhotonBsdfSelection3D;

const char* RuntimeCausticPhotonBsdfLobe3D_Label(
    RuntimeCausticPhotonBsdfLobe3D lobe);
const char* RuntimeCausticPhotonBsdfTermination3D_Label(
    RuntimeCausticPhotonBsdfTermination3D termination);
bool RuntimeCausticPhotonBsdfPolicy3D_Build(
    const RuntimeMaterialPayload3D* material,
    double incident_cosine,
    Vec3 parent_throughput,
    RuntimeCausticPhotonBsdfPolicy3D* out_policy);
bool RuntimeCausticPhotonBsdfPolicy3D_Select(
    const RuntimeCausticPhotonBsdfPolicy3D* policy,
    double unit_sample,
    RuntimeCausticPhotonBsdfSelection3D* out_selection);

#endif
