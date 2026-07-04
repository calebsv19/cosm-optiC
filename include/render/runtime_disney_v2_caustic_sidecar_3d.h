#ifndef RENDER_RUNTIME_DISNEY_V2_CAUSTIC_SIDECAR_3D_H
#define RENDER_RUNTIME_DISNEY_V2_CAUSTIC_SIDECAR_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef enum {
    RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF = 0,
    RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC = 1,
    RUNTIME_DISNEY_V2_CAUSTIC_MODE_TRANSPORT = 2
} RuntimeDisneyV2CausticMode3D;

typedef struct {
    bool valid;
    Vec3 center;
    double radius;
    double receiverZ;
    Vec3 lightPosition;
    Vec3 lightColor;
    double lightIntensity;
    double strength;
} RuntimeDisneyV2CausticSidecarProbe3D;

typedef struct {
    double r;
    double g;
    double b;
    double luma;
} RuntimeDisneyV2CausticSidecarContribution3D;

typedef struct {
    uint64_t probeBuildCount;
    uint64_t triangleScanCount;
    uint64_t objectTransmissiveLookupCount;
    uint64_t materialResolveCount;
} RuntimeDisneyV2CausticSidecarDiagnostics3D;

const char* RuntimeDisneyV2_3D_CausticModeLabel(RuntimeDisneyV2CausticMode3D mode);
void RuntimeDisneyV2_3D_SetCausticSidecar(bool enabled, double strength);
void RuntimeDisneyV2_3D_SetCausticMode(RuntimeDisneyV2CausticMode3D mode,
                                       double strength);
bool RuntimeDisneyV2_3D_CausticSidecarEnabled(void);
RuntimeDisneyV2CausticMode3D RuntimeDisneyV2_3D_CausticMode(void);
double RuntimeDisneyV2_3D_CausticSidecarStrength(void);
void RuntimeDisneyV2_3D_ResetCausticSidecarDiagnostics(void);
void RuntimeDisneyV2_3D_SnapshotCausticSidecarDiagnostics(
    RuntimeDisneyV2CausticSidecarDiagnostics3D* out_diagnostics);
bool RuntimeDisneyV2_3D_BuildCausticSidecarProbe(
    const RuntimeScene3D* scene,
    RuntimeDisneyV2CausticSidecarProbe3D* out_probe);
bool RuntimeDisneyV2_3D_BuildCausticSidecarProbeForSpatialCache(
    const RuntimeScene3D* scene,
    RuntimeDisneyV2CausticSidecarProbe3D* out_probe);
bool RuntimeDisneyV2_3D_EvaluateCausticSidecar(
    const RuntimeDisneyV2CausticSidecarProbe3D* probe,
    const HitInfo3D* receiver_hit,
    RuntimeDisneyV2CausticSidecarContribution3D* out_contribution);

#endif
