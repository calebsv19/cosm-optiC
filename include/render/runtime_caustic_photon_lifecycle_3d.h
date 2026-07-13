#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_LIFECYCLE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_LIFECYCLE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_scene_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_FIRST_BUILD,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_PER_FRAME_POLICY,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_GEOMETRY_CHANGED,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_LIGHT_CHANGED,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_MATERIAL_CHANGED,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_VOLUME_CHANGED,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_BUDGET_CHANGED,
    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_EXPLICIT_REQUEST
} RuntimeCausticPhotonMapRebuildReason3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW = 0,
    RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION,
    RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL
} RuntimeCausticPhotonBudgetTier3D;

typedef struct {
    uint64_t geometryKey;
    uint64_t lightKey;
    uint64_t materialKey;
    uint64_t volumeKey;
    uint64_t budgetKey;
    bool explicitRebuildRequested;
    bool persistentMapOwnershipEnabled;
} RuntimeCausticPhotonMapLifecycleInput3D;

typedef struct {
    bool valid;
    RuntimeCausticPhotonMapLifecycleInput3D input;
} RuntimeCausticPhotonMapLifecycleState3D;

typedef struct {
    bool evaluated;
    bool rebuilt;
    bool reused;
    bool persistentMapOwnershipEnabled;
    RuntimeCausticPhotonMapRebuildReason3D rebuildReason;
    uint64_t geometryKey;
    uint64_t lightKey;
    uint64_t materialKey;
    uint64_t volumeKey;
    uint64_t budgetKey;
    RuntimeCausticPhotonBudgetTier3D budgetTier;
    uint64_t generation;
    uint64_t rebuildCount;
    uint64_t reuseCount;
    double fingerprintCpuMs;
    double mapBuildCpuMs;
    double queryAndDepositCpuMs;
    uint64_t emissionCount;
    uint64_t tracedCount;
    uint64_t storedSurfaceRecordCount;
    uint64_t storedBeamSegmentCount;
    uint64_t accelerationBuildCount;
    uint64_t queryCount;
    uint64_t cacheDepositCount;
} RuntimeCausticPhotonMapLifecycleReadback3D;

void RuntimeCausticPhotonMapLifecycle3D_Init(RuntimeCausticPhotonMapLifecycleState3D* state);
void RuntimeCausticPhotonMapLifecycle3D_BuildInputFromScene(
    const RuntimeScene3D* scene,
    int sample_budget,
    int max_path_depth,
    double surface_query_radius,
    double volume_query_radius,
    bool volume_query_enabled,
    bool persistent_map_ownership_enabled,
    RuntimeCausticPhotonMapLifecycleInput3D* out_input);
const char* RuntimeCausticPhotonMapRebuildReason3D_Label(
    RuntimeCausticPhotonMapRebuildReason3D reason);
RuntimeCausticPhotonBudgetTier3D RuntimeCausticPhotonBudgetTier3D_FromBudget(
    int sample_budget,
    int max_path_depth);
const char* RuntimeCausticPhotonBudgetTier3D_Label(
    RuntimeCausticPhotonBudgetTier3D tier);
void RuntimeCausticPhotonMapLifecycle3D_Evaluate(
    const RuntimeCausticPhotonMapLifecycleInput3D* input,
    RuntimeCausticPhotonMapLifecycleState3D* io_state,
    RuntimeCausticPhotonMapLifecycleReadback3D* out_readback);

#endif
