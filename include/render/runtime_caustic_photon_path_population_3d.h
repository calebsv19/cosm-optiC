#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_scene_trace_3d.h"
#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_COMPLETE,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TARGETS,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_CAUSTIC_HISTORY,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_REFLECTION_ONLY_SURFACE_REJECTED,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_UNRECONCILED_DIELECTRIC_SURFACE_REJECTED,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED,
    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED
} RuntimeCausticPhotonPathPopulationTermination3D;

typedef struct {
    bool storeDiffuseSurfaces;
    bool storeTraversedBeams;
    bool requirePriorSpecularOrTransmission;
    bool requireReconciledDielectricTransmissionForSurface;
    bool allowActiveMediumSurfaceReceiver;
    double surfaceQueryRadius;
    double beamRadiusStart;
    double beamRadiusEnd;
    double beamTransmittance;
    double beamDensityWeight;
    int beamMediumId;
    bool requireBeamMediumId;
    bool requireBeamStage;
    RuntimeCausticPhotonSegmentStage3D beamStage;
    bool clipBeamsToBounds;
    Vec3 beamBoundsMin;
    Vec3 beamBoundsMax;
} RuntimeCausticPhotonPathPopulationSettings3D;

typedef struct {
    bool attempted;
    bool succeeded;
    bool preflightAccepted;
    uint32_t diffuseReceiverCount;
    uint32_t beamCandidateCount;
    uint32_t beamExaminedCount;
    uint32_t beamClippedCount;
    uint32_t transparentHitCount;
    uint32_t terminalHitCount;
    uint32_t storedSurfaceCount;
    uint32_t storedBeamCount;
    uint32_t dielectricEntryCount;
    uint32_t dielectricExitCount;
    RuntimeCausticPhotonRetentionReadback3D retention;
    uint32_t beamEligibility[
        RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID + 1];
    RuntimeCausticPhotonPathPopulationTermination3D termination;
    Vec3 emittedFlux;
    Vec3 activeFlux;
    Vec3 storedSurfaceFlux;
    Vec3 storedBeamFlux;
    Vec3 mediumAbsorbedFlux;
    Vec3 escapedFlux;
    Vec3 absorbedFlux;
    Vec3 rouletteTerminatedFlux;
    Vec3 rejectedFlux;
} RuntimeCausticPhotonPathPopulationReadback3D;

typedef struct {
    uint64_t pathCount;
    uint64_t succeededPathCount;
    uint64_t rejectedPathCount;
    uint64_t storedSurfaceCount;
    uint64_t storedBeamCount;
    uint64_t dielectricEntryCount;
    uint64_t dielectricExitCount;
    RuntimeCausticPhotonRetentionReadback3D retention;
    uint64_t beamExaminedCount;
    uint64_t beamClippedCount;
    uint64_t beamEligibility[
        RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID + 1];
    Vec3 emittedFlux;
    Vec3 activeFlux;
    Vec3 storedSurfaceFlux;
    Vec3 storedBeamFlux;
    Vec3 mediumAbsorbedFlux;
    Vec3 escapedFlux;
    Vec3 absorbedFlux;
    Vec3 rouletteTerminatedFlux;
    Vec3 rejectedFlux;
} RuntimeCausticPhotonPathPopulationBatch3D;

void RuntimeCausticPhotonPathPopulation3D_DefaultSettings(
    RuntimeCausticPhotonPathPopulationSettings3D* settings);
const char* RuntimeCausticPhotonPathPopulationTermination3D_Label(
    RuntimeCausticPhotonPathPopulationTermination3D termination);
bool RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
    const RuntimeCausticPhotonSceneTrace3D* path,
    const RuntimeCausticPhotonPathPopulationSettings3D* settings,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticPhotonPathPopulationReadback3D* out_readback);
void RuntimeCausticPhotonPathPopulationBatch3D_Accumulate(
    RuntimeCausticPhotonPathPopulationBatch3D* batch,
    const RuntimeCausticPhotonPathPopulationReadback3D* path_readback);

#endif
