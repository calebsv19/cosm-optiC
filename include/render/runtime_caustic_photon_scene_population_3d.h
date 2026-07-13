#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_scene_trace_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TRACE,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TARGETS,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_ESCAPED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_MATERIAL_UNRESOLVED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_NOT_OPAQUE,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_SURFACE_STORE_REJECTED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_BEAM_STORE_REJECTED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_PARTIAL_STORE
} RuntimeCausticPhotonScenePopulationTermination3D;

typedef struct {
    bool storeSurface;
    bool storeBeam;
    bool requireOpaqueReceiver;
    double tMin;
    double tMax;
    double rayOffset;
    double surfaceQueryRadius;
    double beamRadiusStart;
    double beamRadiusEnd;
    double beamTransmittance;
    double beamDensityWeight;
    int beamMediumId;
    RuntimeRay3DTraceRoute traceRoute;
    RuntimeCausticPhotonSceneMaterialResolver3D materialResolver;
    void* materialResolverUserData;
} RuntimeCausticPhotonScenePopulationSettings3D;

typedef struct {
    bool attempted;
    bool succeeded;
    bool receiverHitFound;
    bool receiverMaterialResolved;
    bool receiverAccepted;
    bool surfaceStoreAttempted;
    bool surfaceStoreAccepted;
    bool beamStoreAttempted;
    bool beamStoreAccepted;
    bool usedSharedSceneAccelerationRoute;
    RuntimeCausticPhotonScenePopulationTermination3D termination;
    HitInfo3D receiverHit;
    RuntimeMaterialPayload3D receiverMaterial;
    RuntimeCausticPhotonMapRecord3D surfaceRecord;
    RuntimeCausticPhotonVolumeBeamSegment3D beamSegment;
    RuntimeRay3DRouteStats routeStats;
} RuntimeCausticPhotonScenePopulationReadback3D;

void RuntimeCausticPhotonScenePopulation3D_DefaultSettings(
    RuntimeCausticPhotonScenePopulationSettings3D* settings);
const char* RuntimeCausticPhotonScenePopulationTermination3D_Label(
    RuntimeCausticPhotonScenePopulationTermination3D termination);
bool RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSceneTrace3D* scene_trace,
    const RuntimeCausticPhotonScenePopulationSettings3D* settings,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticPhotonScenePopulationReadback3D* out_readback);

#endif
