#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_TRACE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_TRACE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_trace_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIFFERENT_OBJECT_BEFORE_EXIT,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED,
    RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR
} RuntimeCausticPhotonSceneTermination3D;

typedef bool (*RuntimeCausticPhotonSceneMaterialResolver3D)(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data);

typedef struct {
    uint32_t maxDepth;
    double tMin;
    double tMax;
    double rayOffset;
    double minFluxLuma;
    RuntimeRay3DTraceRoute traceRoute;
    RuntimeCausticPhotonSceneMaterialResolver3D materialResolver;
    void* materialResolverUserData;
} RuntimeCausticPhotonSceneTraceSettings3D;

typedef struct {
    uint32_t depth;
    HitInfo3D hit;
    RuntimeMaterialPayload3D material;
    RuntimeCausticPhotonDielectricEvent3D dielectric;
    RuntimeCausticPhotonSceneTermination3D termination;
} RuntimeCausticPhotonSceneHitEvent3D;

typedef struct {
    bool attempted;
    bool succeeded;
    uint32_t intersectionCount;
    uint32_t materialResolveCount;
    uint32_t materialResolveFailureCount;
    uint32_t hitEventCount;
    bool usedSharedSceneAccelerationRoute;
    RuntimeCausticPhotonSceneTermination3D termination;
    RuntimeRay3DRouteStats routeStats;
} RuntimeCausticPhotonSceneTraceReadback3D;

typedef struct {
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticPhotonSceneTraceReadback3D readback;
    RuntimeCausticPhotonSceneHitEvent3D hitEvents[
        RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS];
} RuntimeCausticPhotonSceneTrace3D;

typedef struct {
    bool compared;
    bool matches;
    bool identityMatches;
    bool directionMatches;
    bool branchMatches;
    bool terminationMatches;
    bool triangleIdentityPreserved;
    double maxDirectionError;
    double maxFluxError;
    char mismatchReason[64];
} RuntimeCausticPhotonSceneTraceParity3D;

void RuntimeCausticPhotonSceneTrace3D_DefaultSettings(
    RuntimeCausticPhotonSceneTraceSettings3D* settings);
const char* RuntimeCausticPhotonSceneTermination3D_Label(
    RuntimeCausticPhotonSceneTermination3D termination);
bool RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonSceneTraceSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace);
bool RuntimeCausticPhotonSceneTrace3D_CompareDescriptorOracle(
    const RuntimeCausticPhotonTrace3D* descriptor_trace,
    const RuntimeCausticPhotonSceneTrace3D* scene_trace,
    double direction_tolerance,
    double flux_tolerance,
    RuntimeCausticPhotonSceneTraceParity3D* out_parity);

#endif
