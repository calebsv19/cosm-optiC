#ifndef RENDER_RUNTIME_CAUSTIC_TRANSPORT_DEBUG_3D_H
#define RENDER_RUNTIME_CAUSTIC_TRANSPORT_DEBUG_3D_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_transport_3d.h"

typedef struct {
    uint64_t pathId;
    char emissionPolicy[32];
    int lightIndex;
    char lightId[64];
    char lightKind[24];
    Vec3 lightPosition;
    double lightRadius;
    double lightIntensity;
    Vec3 lightColor;
    int targetTriangleIndex;
    int targetPrimitiveIndex;
    int targetSceneObjectIndex;
    int targetSampleIndex;
    Vec3 targetPosition;
    double targetDistance;
    Vec3 firstHitPosition;
    Vec3 firstHitGeometricNormal;
    Vec3 firstHitOrientedNormal;
    int materialId;
    double transparency;
    double opticalIor;
    double bsdfIor;
    double roughness;
    double reflectivity;
    bool eligible;
    char eventType[32];
    Vec3 outgoingDirection;
    Vec3 throughput;
    Vec3 initialRadiance;
    char lensShapeKind[32];
    int lensSceneObjectIndex;
    int lensPrimitiveIndex;
    uint32_t lensInterfaceEventCount;
    Vec3 lensEntryPosition;
    Vec3 lensEntryNormal;
    Vec3 lensEntryIncidentDirection;
    Vec3 lensEntryOutgoingDirection;
    double lensEntryEtaFrom;
    double lensEntryEtaTo;
    double lensEntryFresnel;
    bool lensEntryTotalInternalReflection;
    Vec3 lensExitPosition;
    Vec3 lensExitNormal;
    Vec3 lensExitIncidentDirection;
    Vec3 lensExitOutgoingDirection;
    double lensExitEtaFrom;
    double lensExitEtaTo;
    double lensExitFresnel;
    bool lensExitTotalInternalReflection;
    Vec3 lensPostExitOrigin;
    Vec3 lensPostExitDirection;
    Vec3 lensReceiverCrossing;
    double lensInsideDistance;
    double lensSampleWeight;
    double lensPathPdf;
    bool lensTotalInternalReflection;
    Vec3 sphereLensEntryPosition;
    Vec3 sphereLensExitPosition;
    Vec3 sphereLensReceiverCrossing;
    double sphereLensInsideDistance;
    bool insideSpecularObjectAfterEvent;
    uint64_t continuationEventCount;
    bool exitedSpecularObjectBeforeVolumeDeposit;
    int mediumExitSceneObjectIndex;
    Vec3 mediumExitPosition;
    Vec3 mediumExitDirection;
    bool volumeClipHit;
    double volumeTEnter;
    double volumeTExit;
    int volumeStepCount;
    Vec3 volumeFirstDepositPosition;
    Vec3 volumeLastDepositPosition;
    double footprintRadiusMin;
    double footprintRadiusMax;
    uint64_t volumeDepositAcceptedCount;
    uint64_t volumeDepositRejectedCount;
    Vec3 volumeDepositedRadiance;
} RuntimeCausticTransportDebugPath3D;

typedef struct {
    bool enabled;
    char outputRoot[1024];
    char summaryPath[1024];
    char pathsPath[1024];
    uint64_t recordedPathCount;
    uint64_t droppedPathCount;
} RuntimeCausticTransportDebug3DState;

void RuntimeCausticTransportDebug3D_Reset(void);
void RuntimeCausticTransportDebug3D_SetEnabled(bool enabled);
void RuntimeCausticTransportDebug3D_SetOutputRoot(const char* output_root);
bool RuntimeCausticTransportDebug3D_IsEnabled(void);
void RuntimeCausticTransportDebug3D_BeginFrame(void);
void RuntimeCausticTransportDebug3D_RecordPath(
    const RuntimeCausticTransportDebugPath3D* path);
bool RuntimeCausticTransportDebug3D_WriteArtifacts(
    const RuntimeCausticTransport3DRequestState* request_state,
    const RuntimeCausticTransport3DDiagnostics* diagnostics);
bool RuntimeCausticTransportDebug3D_WriteCameraStats(int contributing_sample_count,
                                                     int contributing_pixel_count,
                                                     double total_pixel_x,
                                                     double total_pixel_y,
                                                     int pixel_min_x,
                                                     int pixel_min_y,
                                                     int pixel_max_x,
                                                     int pixel_max_y,
                                                     double sampled_cache_radiance_sum,
                                                     double sampled_cache_radiance_max);
RuntimeCausticTransportDebug3DState RuntimeCausticTransportDebug3D_State(void);
size_t RuntimeCausticTransportDebug3D_RecordCount(void);
const RuntimeCausticTransportDebugPath3D* RuntimeCausticTransportDebug3D_RecordAt(
    size_t index);

#endif
