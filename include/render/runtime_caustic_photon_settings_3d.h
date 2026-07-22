#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SETTINGS_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SETTINGS_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_photon_provenance_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT = 0,
    RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP = 1
} RuntimeCausticTransportEngine3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_EVENT_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_EVENT_EMISSION = 1,
    RUNTIME_CAUSTIC_PHOTON_EVENT_DIELECTRIC = 2,
    RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE = 3,
    RUNTIME_CAUSTIC_PHOTON_EVENT_VOLUME = 4,
    RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED = 5
} RuntimeCausticPhotonEventKind3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_BRANCH_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_BRANCH_REFLECTED = 1,
    RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED = 2,
    RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION = 3,
    RUNTIME_CAUSTIC_PHOTON_BRANCH_ABSORBED = 4
} RuntimeCausticPhotonBranch3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_REJECT_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH = 1,
    RUNTIME_CAUSTIC_PHOTON_REJECT_RUSSIAN_ROULETTE = 2,
    RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE = 3,
    RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM = 4,
    RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD = 5,
    RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY = 6
} RuntimeCausticPhotonRejectReason3D;

typedef struct {
    uint64_t photonId;
    uint64_t sampleIndex;
    uint32_t rngSeed;
    int lightIndex;
    int wavelengthBucket;
    Vec3 position;
    Vec3 direction;
    Vec3 flux;
    double emissionPdf;
    double sourceSelectionPdf;
    double positionPdf;
    double directionPdf;
    double baseDirectionPdf;
    double emissionFluxCorrection;
    bool fluxPdfCompensated;
    Vec3 proposalDirection;
    double proposalPdf;
    bool guidingChangedSample;
    bool guidingPdfFluxCorrected;
} RuntimeCausticPhotonSample3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    RuntimeCausticPhotonEventKind3D kind;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    Vec3 position;
    Vec3 normal;
    Vec3 incidentDirection;
    Vec3 outgoingDirection;
    Vec3 throughput;
    double pathPdf;
} RuntimeCausticPhotonEvent3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    Vec3 position;
    Vec3 normal;
    Vec3 incidentDirection;
    Vec3 reflectedDirection;
    Vec3 refractedDirection;
    Vec3 selectedDirection;
    Vec3 throughputBefore;
    Vec3 throughputAfter;
    double etaFrom;
    double etaTo;
    double fresnel;
    double branchPdf;
    double distanceInMedium;
    RuntimeCausticPhotonBranch3D selectedBranch;
    bool totalInternalReflection;
} RuntimeCausticPhotonDielectricEvent3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int materialId;
    Vec3 position;
    Vec3 normal;
    Vec3 incidentDirection;
    Vec3 flux;
    double footprintRadius;
    double normalDotPhoton;
} RuntimeCausticPhotonSurfaceHit3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    Vec3 start;
    Vec3 end;
    Vec3 direction;
    Vec3 flux;
    double radiusStart;
    double radiusEnd;
    double transmittance;
    double densityWeight;
    int mediumId;
    RuntimeCausticPhotonPathProvenance3D provenance;
} RuntimeCausticPhotonVolumeBeamSegment3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    Vec3 position;
    Vec3 normal;
    Vec3 incidentDirection;
    Vec3 flux;
    double pathPdf;
    double queryRadius;
    double sampleCenteredSupportRadius;
    uint64_t sampleCenteredSupportNeighborCount;
    bool sampleCenteredSupportAdaptive;
    bool sampleCenteredSupportPrepared;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int materialId;
    RuntimeCausticPhotonPathProvenance3D provenance;
} RuntimeCausticPhotonMapRecord3D;

typedef struct {
    uint64_t photonId;
    uint32_t eventCount;
    Vec3 emittedFlux;
    Vec3 storedSurfaceFlux;
    Vec3 storedVolumeFlux;
    Vec3 rejectedFlux;
    uint32_t reflectedBranchCount;
    uint32_t refractedBranchCount;
    uint32_t totalInternalReflectionCount;
    uint32_t absorbedBranchCount;
    uint32_t rejectedPhotonCount;
    RuntimeCausticPhotonRejectReason3D lastRejectReason;
} RuntimeCausticPhotonDebugRecord3D;

RuntimeCausticTransportEngine3D RuntimeCausticTransportEngine3D_FromLabel(
    const char* label);
const char* RuntimeCausticTransportEngine3D_Label(
    RuntimeCausticTransportEngine3D engine);

#endif
