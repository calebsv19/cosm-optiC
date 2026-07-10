#ifndef RENDER_RUNTIME_CAUSTIC_BEAM_MAP_3D_H
#define RENDER_RUNTIME_CAUSTIC_BEAM_MAP_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_trace_3d.h"

typedef struct {
    uint64_t defaultCapacity;
    double defaultQueryRadius;
    double minDirectionDot;
} RuntimeCausticBeamMapSettings3D;

typedef struct {
    Vec3 position;
    Vec3 direction;
    double radius;
    int mediumId;
    bool requireMediumId;
    double minDirectionDot;
} RuntimeCausticBeamMapQuery3D;

typedef struct {
    bool hit;
    Vec3 flux;
    double weightSum;
    uint64_t candidateCount;
    uint64_t contributingCount;
    double nearestDistance;
    double nearestT;
    double nearestDirectionDot;
} RuntimeCausticBeamMapQueryResult3D;

typedef struct {
    bool allocated;
    uint64_t segmentCapacity;
    uint64_t segmentCount;
    uint64_t storeAttemptCount;
    uint64_t storeAcceptedCount;
    uint64_t storeRejectedCount;
    uint64_t queryCount;
    uint64_t queryHitCount;
    uint64_t lastQueryCandidateCount;
    uint64_t lastQueryContributingCount;
    double lastQueryNearestDistance;
    double lastQueryNearestT;
    double lastQueryNearestDirectionDot;
    Vec3 totalStoredFlux;
    Vec3 lastQueryFlux;
} RuntimeCausticBeamMapDiagnostics3D;

typedef struct {
    RuntimeCausticPhotonVolumeBeamSegment3D* segments;
    uint64_t segmentCapacity;
    uint64_t segmentCount;
    uint64_t storeAttemptCount;
    uint64_t storeAcceptedCount;
    uint64_t storeRejectedCount;
    uint64_t queryCount;
    uint64_t queryHitCount;
    RuntimeCausticBeamMapQueryResult3D lastQuery;
    bool ownsSegments;
} RuntimeCausticBeamMap3D;

void RuntimeCausticBeamMap3D_DefaultSettings(
    RuntimeCausticBeamMapSettings3D* settings);
void RuntimeCausticBeamMap3D_DefaultQuery(RuntimeCausticBeamMapQuery3D* query);
void RuntimeCausticBeamMap3D_Init(RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_IsAllocated(const RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_Allocate(RuntimeCausticBeamMap3D* map,
                                      uint64_t segment_capacity);
void RuntimeCausticBeamMap3D_Clear(RuntimeCausticBeamMap3D* map);
void RuntimeCausticBeamMap3D_Free(RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_StoreSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment);
bool RuntimeCausticBeamMap3D_StoreTraceSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonTrace3D* trace,
    double radius_start,
    double radius_end,
    double transmittance,
    double density_weight,
    int medium_id);
bool RuntimeCausticBeamMap3D_Query(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticBeamMapQuery3D* query,
    RuntimeCausticBeamMapQueryResult3D* out_result);
void RuntimeCausticBeamMap3D_SnapshotDiagnostics(
    const RuntimeCausticBeamMap3D* map,
    RuntimeCausticBeamMapDiagnostics3D* out_diagnostics);

#endif
