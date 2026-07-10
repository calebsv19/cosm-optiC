#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_INTEGRATION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_INTEGRATION_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_settings_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PRODUCT_MODE_OFF = 0,
    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE = 1,
    RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION = 2
} RuntimeCausticProductMode3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE = 1,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY = 2
} RuntimeCausticPhotonIntegrationRoute3D;

typedef struct {
    RuntimeCausticProductMode3D productMode;
    bool surfaceQueryEnabled;
    bool volumeQueryEnabled;
    bool renderContributionEnabled;
    int sampleBudget;
    int maxPathDepth;
    double surfaceRadianceScale;
    double surfaceQueryRadius;
    double volumeQueryRadius;
} RuntimeCausticPhotonIntegrationSettings3D;

typedef struct {
    bool querySurface;
    RuntimeCausticPhotonMapQuery3D surface;
    bool queryVolume;
    RuntimeCausticBeamMapQuery3D volume;
} RuntimeCausticPhotonIntegrationQuery3D;

typedef struct {
    RuntimeCausticProductMode3D productMode;
    RuntimeCausticPhotonIntegrationRoute3D route;
    bool surfaceQueryEnabled;
    bool volumeQueryEnabled;
    bool renderContributionEnabled;
    bool renderContributionSuppressed;
    bool surfaceHit;
    bool volumeHit;
    Vec3 surfaceFlux;
    Vec3 volumeFlux;
    Vec3 combinedFlux;
    uint64_t surfaceCandidateCount;
    uint64_t surfaceContributingCount;
    uint64_t volumeCandidateCount;
    uint64_t volumeContributingCount;
} RuntimeCausticPhotonIntegrationResult3D;

void RuntimeCausticPhotonIntegration3D_DefaultSettings(
    RuntimeCausticPhotonIntegrationSettings3D* settings);
void RuntimeCausticPhotonIntegration3D_DefaultQuery(
    RuntimeCausticPhotonIntegrationQuery3D* query);
RuntimeCausticProductMode3D RuntimeCausticProductMode3D_FromLabel(
    const char* label);
const char* RuntimeCausticProductMode3D_Label(RuntimeCausticProductMode3D mode);
const char* RuntimeCausticPhotonIntegrationRoute3D_Label(
    RuntimeCausticPhotonIntegrationRoute3D route);
RuntimeCausticPhotonIntegrationRoute3D
RuntimeCausticPhotonIntegration3D_RouteForSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* settings);
void RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* integration,
    RuntimeCausticSettings3D* caustic);
bool RuntimeCausticPhotonIntegration3D_Query(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonIntegrationResult3D* out_result);

#endif
