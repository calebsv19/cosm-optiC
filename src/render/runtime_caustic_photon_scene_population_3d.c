#include "render/runtime_caustic_photon_scene_population_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_scene_accel_3d.h"

static bool photon_scene_population_default_material_resolver(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    (void)user_data;
    return RuntimeMaterialPayload3D_ResolveFromHit(hit, out_payload);
}

static bool photon_scene_population_receiver_is_opaque(
    const RuntimeMaterialPayload3D* material) {
    return material && material->valid && !(material->transparency > 1.0e-9) &&
           !material->thinWalled;
}

static void photon_scene_population_snapshot_surface(
    const RuntimeCausticPhotonMap3D* map,
    RuntimeCausticPhotonScenePopulationReadback3D* readback) {
    if (!map || !readback || map->recordCount == 0u || !map->records) return;
    readback->surfaceRecord = map->records[map->recordCount - 1u];
}

static void photon_scene_population_snapshot_beam(
    const RuntimeCausticBeamMap3D* map,
    RuntimeCausticPhotonScenePopulationReadback3D* readback) {
    if (!map || !readback || map->segmentCount == 0u || !map->segments) return;
    readback->beamSegment = map->segments[map->segmentCount - 1u];
}

static RuntimeCausticPhotonScenePopulationTermination3D
photon_scene_population_store_termination(
    const RuntimeCausticPhotonScenePopulationSettings3D* settings,
    const RuntimeCausticPhotonScenePopulationReadback3D* readback) {
    bool surface_ok = !settings->storeSurface || readback->surfaceStoreAccepted;
    bool beam_ok = !settings->storeBeam || readback->beamStoreAccepted;
    if (surface_ok && beam_ok) {
        return RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE;
    }
    if (readback->surfaceStoreAccepted || readback->beamStoreAccepted) {
        return RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_PARTIAL_STORE;
    }
    if (settings->storeSurface && !readback->surfaceStoreAccepted) {
        return RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_SURFACE_STORE_REJECTED;
    }
    return RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_BEAM_STORE_REJECTED;
}

void RuntimeCausticPhotonScenePopulation3D_DefaultSettings(
    RuntimeCausticPhotonScenePopulationSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->storeSurface = true;
    settings->storeBeam = true;
    settings->requireOpaqueReceiver = true;
    settings->tMin = 1.0e-6;
    settings->tMax = 1.0e6;
    settings->rayOffset = 1.0e-5;
    settings->surfaceQueryRadius = 0.10;
    settings->beamRadiusStart = 0.04;
    settings->beamRadiusEnd = 0.08;
    settings->beamTransmittance = 1.0;
    settings->beamDensityWeight = 1.0;
    settings->beamMediumId = 0;
    settings->traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
    settings->materialResolver = photon_scene_population_default_material_resolver;
}

const char* RuntimeCausticPhotonScenePopulationTermination3D_Label(
    RuntimeCausticPhotonScenePopulationTermination3D termination) {
    switch (termination) {
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE:
            return "complete";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TRACE:
            return "invalid_trace";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TARGETS:
            return "invalid_targets";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_ESCAPED:
            return "receiver_escaped";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_MATERIAL_UNRESOLVED:
            return "material_unresolved";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_NOT_OPAQUE:
            return "receiver_not_opaque";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_SURFACE_STORE_REJECTED:
            return "surface_store_rejected";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_BEAM_STORE_REJECTED:
            return "beam_store_rejected";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_PARTIAL_STORE:
            return "partial_store";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_NONE:
        default:
            return "none";
    }
}

bool RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSceneTrace3D* scene_trace,
    const RuntimeCausticPhotonScenePopulationSettings3D* settings,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticPhotonScenePopulationReadback3D* out_readback) {
    RuntimeCausticPhotonScenePopulationSettings3D defaults;
    const RuntimeCausticPhotonScenePopulationSettings3D* active = settings;
    RuntimeCausticPhotonScenePopulationReadback3D readback;
    RuntimeRay3DTraceContext ray_context;
    RuntimeCausticPhotonSurfaceHit3D surface_hit;
    RuntimeCausticPhotonVolumeBeamSegment3D beam_segment;
    HitInfo3D receiver_hit;
    RuntimeMaterialPayload3D receiver_material;
    Ray3D receiver_ray;
    Vec3 offset_normal = vec3(0.0, 0.0, 0.0);
    bool found;

    memset(&readback, 0, sizeof(readback));
    HitInfo3D_Reset(&readback.receiverHit);
    if (out_readback) *out_readback = readback;
    if (!scene || !scene_trace || !out_readback) return false;
    readback.attempted = true;
    if (!active) {
        RuntimeCausticPhotonScenePopulation3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    if (!scene_trace->trace.valid || !scene_trace->readback.succeeded ||
        scene_trace->readback.termination !=
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TRACE;
        *out_readback = readback;
        return false;
    }
    if ((!active->storeSurface && !active->storeBeam) ||
        (active->storeSurface && !surface_map) ||
        (active->storeBeam && !beam_map)) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_INVALID_TARGETS;
        *out_readback = readback;
        return false;
    }

    if (scene_trace->trace.dielectricEventCount > 0u) {
        offset_normal = scene_trace->trace.dielectricEvents[
                            scene_trace->trace.dielectricEventCount - 1u]
                            .normal;
    }
    receiver_ray = RuntimeRay3D_MakeOffset(scene_trace->trace.postExitOrigin,
                                           offset_normal,
                                           scene_trace->trace.postExitDirection,
                                           active->rayOffset);
    RuntimeRay3DTraceContext_Init(&ray_context);
    RuntimeRay3DTraceContext_SetTraceRoute(&ray_context, active->traceRoute);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &ray_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    HitInfo3D_Reset(&receiver_hit);
    found = RuntimeRay3D_TraceSceneFirstHitWithContext(&ray_context,
                                                       scene,
                                                       &receiver_ray,
                                                       active->tMin,
                                                       active->tMax,
                                                       &receiver_hit);
    RuntimeRay3DTraceContext_SnapshotRouteStats(&ray_context, &readback.routeStats);
    readback.usedSharedSceneAccelerationRoute = readback.routeStats.tlasTraceCalls > 0u;
    if (!found) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_ESCAPED;
        *out_readback = readback;
        return false;
    }
    readback.receiverHitFound = true;
    readback.receiverHit = receiver_hit;

    RuntimeMaterialPayload3D_Reset(&receiver_material);
    if (!active->materialResolver ||
        !active->materialResolver(&receiver_hit,
                                  &receiver_material,
                                  active->materialResolverUserData) ||
        !receiver_material.valid) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_MATERIAL_UNRESOLVED;
        *out_readback = readback;
        return false;
    }
    readback.receiverMaterialResolved = true;
    readback.receiverMaterial = receiver_material;
    if (active->requireOpaqueReceiver &&
        !photon_scene_population_receiver_is_opaque(&receiver_material)) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_NOT_OPAQUE;
        *out_readback = readback;
        return false;
    }
    readback.receiverAccepted = true;

    memset(&surface_hit, 0, sizeof(surface_hit));
    surface_hit.photonId = scene_trace->trace.sample.photonId;
    surface_hit.depth = scene_trace->trace.finalState.depth;
    surface_hit.sceneObjectIndex = receiver_hit.sceneObjectIndex;
    surface_hit.primitiveIndex = receiver_hit.primitiveIndex;
    surface_hit.triangleIndex = receiver_hit.triangleIndex;
    surface_hit.materialId = receiver_material.materialId;
    surface_hit.position = receiver_hit.position;
    surface_hit.normal = receiver_hit.normal;
    surface_hit.incidentDirection = scene_trace->trace.postExitDirection;
    surface_hit.flux = scene_trace->trace.finalState.throughput;
    surface_hit.footprintRadius = active->surfaceQueryRadius;
    surface_hit.normalDotPhoton = fmax(
        vec3_dot(receiver_hit.normal,
                 vec3_scale(scene_trace->trace.postExitDirection, -1.0)),
        0.0);

    memset(&beam_segment, 0, sizeof(beam_segment));
    beam_segment.photonId = scene_trace->trace.sample.photonId;
    beam_segment.depth = scene_trace->trace.finalState.depth;
    beam_segment.start = scene_trace->trace.postExitOrigin;
    beam_segment.end = receiver_hit.position;
    beam_segment.direction = scene_trace->trace.postExitDirection;
    beam_segment.flux = scene_trace->trace.finalState.throughput;
    beam_segment.radiusStart = active->beamRadiusStart;
    beam_segment.radiusEnd = active->beamRadiusEnd;
    beam_segment.transmittance = active->beamTransmittance;
    beam_segment.densityWeight = active->beamDensityWeight;
    beam_segment.mediumId = active->beamMediumId;

    if (active->storeSurface) {
        readback.surfaceStoreAttempted = true;
        readback.surfaceStoreAccepted = RuntimeCausticPhotonMap3D_StoreSurfaceHit(
            surface_map,
            &surface_hit,
            scene_trace->trace.finalState.pathPdf,
            active->surfaceQueryRadius);
        if (readback.surfaceStoreAccepted) {
            photon_scene_population_snapshot_surface(surface_map, &readback);
        }
    }
    if (active->storeBeam) {
        readback.beamStoreAttempted = true;
        readback.beamStoreAccepted = RuntimeCausticBeamMap3D_StoreSegment(
            beam_map,
            &beam_segment);
        if (readback.beamStoreAccepted) {
            photon_scene_population_snapshot_beam(beam_map, &readback);
        }
    }
    readback.termination = photon_scene_population_store_termination(active, &readback);
    readback.succeeded =
        readback.termination == RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE;
    *out_readback = readback;
    return readback.succeeded;
}
