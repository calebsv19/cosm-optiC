#include "render/runtime_caustic_photon_integration_3d.h"

#include "render/runtime_caustic_photon_receiver_policy_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET = 4096,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_DEPTH = 16
};

static int photon_integration_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_integration_clamp_double(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_integration_unit_sample(uint64_t index, uint64_t count) {
    if (count == 0u) return 0.5;
    return ((double)index + 0.5) / (double)count;
}

static double photon_integration_centered_sample(uint64_t index,
                                                 uint64_t count,
                                                 uint64_t salt) {
    uint64_t mixed = index * UINT64_C(1664525) + salt * UINT64_C(1013904223);
    double unit = photon_integration_unit_sample(mixed % (count + 1u), count + 1u);
    return photon_integration_clamp_double(unit * 2.0 - 1.0, -0.95, 0.95);
}

static double photon_integration_receiver_distribution_radius(
    const RuntimeCausticPhotonSurfaceHit3D* hits,
    uint64_t hit_count,
    double minimum_radius,
    Vec3* out_centroid,
    double* out_mean_distance,
    double* out_max_distance) {
    Vec3 centroid = vec3(0.0, 0.0, 0.0);
    double mean_distance = 0.0;
    double max_distance = 0.0;

    if (!hits || hit_count == 0u) {
        if (out_centroid) *out_centroid = centroid;
        if (out_mean_distance) *out_mean_distance = 0.0;
        if (out_max_distance) *out_max_distance = 0.0;
        return photon_integration_clamp_double(minimum_radius, 0.02, 2.0);
    }
    for (uint64_t i = 0u; i < hit_count; ++i) {
        centroid = vec3_add(centroid, hits[i].position);
    }
    centroid = vec3_scale(centroid, 1.0 / (double)hit_count);
    for (uint64_t i = 0u; i < hit_count; ++i) {
        double distance = vec3_length(vec3_sub(hits[i].position, centroid));
        mean_distance += distance;
        if (distance > max_distance) max_distance = distance;
    }
    mean_distance /= (double)hit_count;
    if (out_centroid) *out_centroid = centroid;
    if (out_mean_distance) *out_mean_distance = mean_distance;
    if (out_max_distance) *out_max_distance = max_distance;
    return photon_integration_clamp_double(fmax(minimum_radius, mean_distance * 1.5),
                                           0.02,
                                           2.0);
}

static bool photon_integration_surface_hit_identity_matches(
    const RuntimeCausticPhotonSurfaceHit3D* a,
    const RuntimeCausticPhotonSurfaceHit3D* b) {
    return a && b &&
           a->sceneObjectIndex == b->sceneObjectIndex &&
           a->primitiveIndex == b->primitiveIndex &&
           a->triangleIndex == b->triangleIndex;
}

static bool photon_integration_surface_hit_bucket_seen_before(
    const RuntimeCausticPhotonSurfaceHit3D* hits,
    uint64_t current_index) {
    if (!hits) return false;
    for (uint64_t i = 0u; i < current_index; ++i) {
        if (photon_integration_surface_hit_identity_matches(&hits[i],
                                                            &hits[current_index])) {
            return true;
        }
    }
    return false;
}

static void photon_integration_merge_receiver_policy_readback(
    RuntimeCausticPhotonMapPopulationReadback3D* io_readback,
    const RuntimeCausticPhotonReceiverPolicyReadback3D* policy_readback) {
    if (!io_readback || !policy_readback) return;
    io_readback->receiverLookupAttemptCount = policy_readback->lookupAttemptCount;
    io_readback->receiverDirectHitCount = policy_readback->directHitCount;
    io_readback->receiverCrossingProbeAttemptCount =
        policy_readback->crossingProbeAttemptCount;
    io_readback->receiverCrossingProbeHitCount =
        policy_readback->crossingProbeHitCount;
    io_readback->receiverCandidateCount = policy_readback->candidateCount;
    io_readback->receiverAcceptedHitCount = policy_readback->acceptedHitCount;
    io_readback->receiverMissRejectCount = policy_readback->missRejectCount;
    io_readback->receiverSelfHitRejectCount = policy_readback->selfHitRejectCount;
    io_readback->receiverInvalidRejectCount = policy_readback->invalidRejectCount;
    io_readback->receiverMaterialFilterRejectCount =
        policy_readback->materialFilterRejectCount;
    io_readback->receiverObjectFilterRejectCount =
        policy_readback->objectFilterRejectCount;
    io_readback->receiverCompetingRejectCount =
        policy_readback->competingRejectCount;
    io_readback->receiverSelectedHitCount = policy_readback->selectedHitCount;
    io_readback->receiverSelectedBucketCount =
        policy_readback->selectedBucketCount;
}


bool RuntimeCausticPhotonIntegration3D_PopulateSurfaceMapFromLightSet(
    RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonIntegrationQuery3D default_query;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    const RuntimeCausticPhotonIntegrationQuery3D* active_query = query;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonEmissionSettings3D emission_settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D emission_diag;
    RuntimeCausticPhotonMapDiagnostics3D map_diag;
    uint64_t sample_budget;
    double query_radius;
    bool populated = false;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (!active_query) {
        RuntimeCausticPhotonIntegration3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }

    readback.attempted = true;
    if (!surface_map || !light_set ||
        RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY ||
        !active_settings->surfaceQueryEnabled || !active_query->querySurface) {
        *out_readback = readback;
        return false;
    }

    sample_budget = (uint64_t)photon_integration_clamp_int(
        active_settings->sampleBudget,
        0,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET);
    readback.requestedSampleBudget = sample_budget;
    if (sample_budget == 0u) {
        *out_readback = readback;
        return false;
    }

    RuntimeCausticPhotonEmission3D_DefaultSettings(&emission_settings);
    emission_settings.sampleBudget = sample_budget;
    emission_settings.baseSeed = active_settings->emissionSeed;
    emission_settings.defaultQueryRadius =
        photon_integration_clamp_double(active_settings->surfaceQueryRadius,
                                        0.001,
                                        10.0);

    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    if (!RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, sample_budget)) {
        *out_readback = readback;
        return false;
    }

    readback.surfaceMapAllocated =
        RuntimeCausticPhotonMap3D_Allocate(surface_map, sample_budget);
    readback.emissionAttempted = true;
    if (readback.surfaceMapAllocated) {
        readback.emissionSucceeded =
            RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                            light_set,
                                                            &emission_settings,
                                                            &emission_diag);
        readback.emittedPhotonCount = emission_diag.emittedPhotonCount;
        readback.rejectedPhotonCount = emission_diag.rejectedPhotonCount;
        readback.totalEmittedFlux = emission_diag.totalEmittedFlux;
        if (readback.emissionSucceeded) {
            query_radius = active_settings->surfaceQueryRadius > 0.0
                               ? active_settings->surfaceQueryRadius
                               : active_query->surface.radius;
            readback.surfaceMapPopulationAttempted = true;
            populated = RuntimeCausticPhotonEmission3D_StoreSurfaceProxyRecords(
                surface_map,
                &batch,
                query_radius,
                active_query->surface.normal,
                active_query->surface.sceneObjectIndex,
                active_query->surface.primitiveIndex,
                active_query->surface.triangleIndex,
                &emission_diag);
            readback.surfaceMapPopulated = populated;
            if (populated) {
                readback.populationSource =
                    RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_SURFACE_PROXY;
            }
            readback.surfaceMapStoreAttemptCount =
                emission_diag.mapStoreAttemptCount;
            readback.surfaceMapStoreAcceptedCount =
                emission_diag.mapStoreAcceptedCount;
            readback.surfaceMapStoreRejectedCount =
                emission_diag.mapStoreRejectedCount;
            readback.totalStoredSurfaceFlux =
                emission_diag.totalStoredSurfaceFlux;
        }
        RuntimeCausticPhotonMap3D_SnapshotDiagnostics(surface_map, &map_diag);
        readback.surfaceMapRecordCount = map_diag.recordCount;
        readback.surfaceMapAccelerationInsertedCount =
            map_diag.accelerationInsertedCount;
    }

    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    *out_readback = readback;
    return populated;
}

bool RuntimeCausticPhotonIntegration3D_PopulateMapsFromTraceRecords(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonTrace3D* traces,
    uint64_t trace_count,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonIntegrationQuery3D default_query;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    const RuntimeCausticPhotonIntegrationQuery3D* active_query = query;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonMapDiagnostics3D surface_diag;
    RuntimeCausticBeamMapDiagnostics3D beam_diag;
    uint64_t i;
    bool surface_populated = false;
    bool volume_populated = false;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (!active_query) {
        RuntimeCausticPhotonIntegration3D_DefaultQuery(&default_query);
        active_query = &default_query;
    }

    readback.attempted = true;
    readback.tracePopulationAttempted = true;
    readback.traceInputCount = trace_count;
    if (!traces || trace_count == 0u ||
        RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        *out_readback = readback;
        return false;
    }

    if (surface_map && active_settings->surfaceQueryEnabled &&
        active_query->querySurface) {
        if (!RuntimeCausticPhotonMap3D_IsAllocated(surface_map)) {
            readback.surfaceMapAllocated =
                RuntimeCausticPhotonMap3D_Allocate(surface_map, trace_count);
        } else {
            readback.surfaceMapAllocated = true;
        }
        readback.surfaceMapPopulationAttempted = readback.surfaceMapAllocated;
    }
    if (beam_map && active_settings->volumeQueryEnabled &&
        active_query->queryVolume) {
        if (!RuntimeCausticBeamMap3D_IsAllocated(beam_map)) {
            readback.volumeBeamMapAllocated =
                RuntimeCausticBeamMap3D_Allocate(beam_map, trace_count);
        } else {
            readback.volumeBeamMapAllocated = true;
        }
        readback.volumeBeamPopulationAttempted = readback.volumeBeamMapAllocated;
    }

    for (i = 0u; i < trace_count; ++i) {
        const RuntimeCausticPhotonTrace3D* trace = &traces[i];
        if (readback.surfaceMapPopulationAttempted &&
            RuntimeCausticPhotonMap3D_StoreTraceReceiver(
                surface_map,
                trace,
                active_query->surface.normal,
                active_settings->surfaceQueryRadius > 0.0
                    ? active_settings->surfaceQueryRadius
                    : active_query->surface.radius,
                active_query->surface.sceneObjectIndex,
                active_query->surface.primitiveIndex,
                active_query->surface.triangleIndex)) {
            surface_populated = true;
            readback.totalStoredSurfaceFlux =
                vec3_add(readback.totalStoredSurfaceFlux, trace->finalState.throughput);
        }
        if (readback.volumeBeamPopulationAttempted &&
            RuntimeCausticBeamMap3D_StoreTraceSegment(
                beam_map,
                trace,
                active_query->volume.radius,
                active_settings->volumeQueryRadius > 0.0
                    ? active_settings->volumeQueryRadius
                    : active_query->volume.radius,
                1.0,
                1.0,
                active_query->volume.mediumId)) {
            volume_populated = true;
            readback.totalStoredVolumeFlux =
                vec3_add(readback.totalStoredVolumeFlux, trace->finalState.throughput);
        }
    }

    if (surface_map) {
        RuntimeCausticPhotonMap3D_SnapshotDiagnostics(surface_map, &surface_diag);
        readback.surfaceMapStoreAttemptCount = surface_diag.storeAttemptCount;
        readback.surfaceMapStoreAcceptedCount = surface_diag.storeAcceptedCount;
        readback.surfaceMapStoreRejectedCount = surface_diag.storeRejectedCount;
        readback.surfaceMapRecordCount = surface_diag.recordCount;
        readback.surfaceMapAccelerationInsertedCount =
            surface_diag.accelerationInsertedCount;
    }
    if (beam_map) {
        RuntimeCausticBeamMap3D_SnapshotDiagnostics(beam_map, &beam_diag);
        readback.volumeBeamStoreAttemptCount = beam_diag.storeAttemptCount;
        readback.volumeBeamStoreAcceptedCount = beam_diag.storeAcceptedCount;
        readback.volumeBeamStoreRejectedCount = beam_diag.storeRejectedCount;
        readback.volumeBeamSegmentCount = beam_diag.segmentCount;
        readback.volumeBeamAccelerationInsertedCount =
            beam_diag.accelerationInsertedCount;
    }

    readback.surfaceMapPopulated = surface_populated;
    readback.volumeBeamPopulated = volume_populated;
    readback.tracePopulationSucceeded = surface_populated || volume_populated;
    if (readback.tracePopulationSucceeded) {
        readback.populationSource =
            RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_TRACE_RECORDS;
    }
    *out_readback = readback;
    return readback.tracePopulationSucceeded;
}

bool RuntimeCausticPhotonIntegration3D_PopulateMapsFromMeshDielectricFixture(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonTraceSettings3D trace_settings;
    RuntimeCausticPhotonEmissionSettings3D emission_settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D emission_diag;
    RuntimeCausticPhotonTrace3D* traces = NULL;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonMapPopulationReadback3D trace_readback;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    uint64_t sample_budget;
    uint64_t trace_count = 0u;
    uint64_t solved_path_count = 0u;
    bool populated = false;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }

    readback.attempted = true;
    readback.tracePopulationAttempted = true;
    if ((!surface_map && !beam_map) || !light_set || !mesh_dielectric || !entry_triangle ||
        RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        *out_readback = readback;
        return false;
    }

    sample_budget = (uint64_t)photon_integration_clamp_int(
        active_settings->sampleBudget,
        0,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET);
    readback.requestedSampleBudget = sample_budget;
    if (sample_budget == 0u ||
        sample_budget >
            (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticPhotonTrace3D))) {
        *out_readback = readback;
        return false;
    }

    traces = (RuntimeCausticPhotonTrace3D*)calloc((size_t)sample_budget,
                                                  sizeof(RuntimeCausticPhotonTrace3D));
    if (!traces) {
        *out_readback = readback;
        return false;
    }

    RuntimeCausticPhotonEmission3D_DefaultSettings(&emission_settings);
    emission_settings.sampleBudget = sample_budget;
    emission_settings.baseSeed = active_settings->emissionSeed;
    emission_settings.defaultQueryRadius =
        photon_integration_clamp_double(active_settings->surfaceQueryRadius,
                                        0.001,
                                        10.0);
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    if (!RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, sample_budget)) {
        free(traces);
        *out_readback = readback;
        return false;
    }

    readback.emissionAttempted = true;
    readback.emissionSucceeded =
        RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                        light_set,
                                                        &emission_settings,
                                                        &emission_diag);
    readback.emittedPhotonCount = emission_diag.emittedPhotonCount;
    readback.rejectedPhotonCount = emission_diag.rejectedPhotonCount;
    readback.totalEmittedFlux = emission_diag.totalEmittedFlux;
    RuntimeCausticPhotonTrace3D_DefaultSettings(&trace_settings);
    trace_settings.maxDepth = (uint32_t)photon_integration_clamp_int(
        active_settings->maxPathDepth,
        1,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_DEPTH);

    if (readback.emissionSucceeded) {
        uint64_t i;
        for (i = 0u; i < batch.sampleCount; ++i) {
            const RuntimeCausticPhotonSample3D* photon = &batch.samples[i];
            const RuntimeLightSource3D* source = NULL;
            RuntimeCausticLensLightSample3D lens_light;
            RuntimeCausticLensSample3D lens_sample;
            RuntimeCausticLensPath3D lens_path;
            RuntimeCausticPhotonTrace3D trace;

            source = RuntimeLightSet3D_GetEnabled(light_set, photon->lightIndex);
            if (!source) {
                source = RuntimeLightSet3D_GetEnabled(light_set, 0);
            }
            if (!source) continue;

            RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
            RuntimeCausticLensTransport3D_DefaultSample(&lens_sample);
            lens_light.position = photon->position;
            lens_light.radius = source->radius;
            lens_light.intensity = source->intensity;
            lens_light.color = source->color;
            lens_light.lightIndex = photon->lightIndex;
            lens_sample.apertureU =
                photon_integration_centered_sample(i, sample_budget, 1u);
            lens_sample.apertureV =
                photon_integration_centered_sample(i, sample_budget, 2u);
            lens_sample.lensU =
                photon_integration_centered_sample(i, sample_budget, 3u);
            lens_sample.lensV =
                photon_integration_centered_sample(i, sample_budget, 4u);
            lens_sample.sampleWeight = sample_budget > 0u
                                           ? 1.0 / (double)sample_budget
                                           : 1.0;
            lens_sample.receiverDistance =
                mesh_dielectric->radius > 1.0e-9
                    ? mesh_dielectric->radius * 4.0
                    : 1.0;

            if (!RuntimeCausticLensTransport3D_SolveMeshDielectricPath(
                    mesh_dielectric,
                    entry_triangle,
                    &lens_light,
                    &lens_sample,
                    &lens_path)) {
                continue;
            }
            solved_path_count += 1u;
            if (!RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(&lens_path,
                                                                     photon,
                                                                     &trace_settings,
                                                                     &trace)) {
                continue;
            }
            traces[trace_count++] = trace;
        }
    }

    if (trace_count > 0u) {
        populated = RuntimeCausticPhotonIntegration3D_PopulateMapsFromTraceRecords(
            surface_map,
            beam_map,
            traces,
            trace_count,
            active_settings,
            query,
            &trace_readback);
        readback = trace_readback;
        readback.emissionAttempted = true;
        readback.emissionSucceeded = true;
        readback.requestedSampleBudget = sample_budget;
        readback.emittedPhotonCount = emission_diag.emittedPhotonCount;
        readback.rejectedPhotonCount = emission_diag.rejectedPhotonCount;
        readback.totalEmittedFlux = emission_diag.totalEmittedFlux;
    }
    readback.traceSolvedPathCount = solved_path_count;
    readback.traceRecordCount = trace_count;
    readback.traceInputCount = trace_count;

    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    free(traces);
    *out_readback = readback;
    return populated;
}

bool RuntimeCausticPhotonIntegration3D_PopulateReceiverSurfaceMapFromMeshDielectricScene(
    RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeScene3D* scene,
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback,
    RuntimeCausticPhotonReceiverSelection3D* out_receiver) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonEmissionSettings3D emission_settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D emission_diag;
    RuntimeCausticPhotonTraceSettings3D trace_settings;
    RuntimeCausticPhotonMapDiagnostics3D map_diag;
    RuntimeCausticPhotonReceiverPolicy3D receiver_policy;
    RuntimeCausticPhotonReceiverPolicyReadback3D receiver_policy_readback;
    RuntimeCausticPhotonReceiverBucket3D receiver_bucket;
    RuntimeCausticPhotonSurfaceHit3D* receiver_hits = NULL;
    double* receiver_path_pdfs = NULL;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    uint64_t sample_budget;
    uint64_t solved_path_count = 0u;
    uint64_t trace_count = 0u;
    uint64_t receiver_hit_count = 0u;
    uint64_t selected_receiver_hit_count = 0u;
    double minimum_radius;
    double footprint_radius = 0.0;
    Vec3 receiver_centroid = vec3(0.0, 0.0, 0.0);
    double receiver_mean_distance = 0.0;
    double receiver_max_distance = 0.0;
    double selected_receiver_mean_distance = 0.0;
    double selected_receiver_max_distance = 0.0;
    bool populated = false;

    memset(&readback, 0, sizeof(readback));
    memset(&emission_diag, 0, sizeof(emission_diag));
    RuntimeCausticPhotonReceiverPolicy3D_Default(&receiver_policy);
    RuntimeCausticPhotonReceiverPolicy3D_ResetReadback(&receiver_policy_readback);
    memset(&receiver_bucket, 0, sizeof(receiver_bucket));
    if (out_readback) *out_readback = readback;
    if (out_receiver) memset(out_receiver, 0, sizeof(*out_receiver));
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }

    readback.attempted = true;
    readback.tracePopulationAttempted = true;
    if (!surface_map || !scene || !mesh_dielectric || !entry_triangle ||
        RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY ||
        !active_settings->surfaceQueryEnabled) {
        *out_readback = readback;
        return false;
    }

    sample_budget = (uint64_t)photon_integration_clamp_int(
        active_settings->sampleBudget,
        0,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_SAMPLE_BUDGET);
    readback.requestedSampleBudget = sample_budget;
    if (sample_budget == 0u ||
        sample_budget > (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticPhotonSurfaceHit3D)) ||
        sample_budget > (uint64_t)(SIZE_MAX / sizeof(double))) {
        *out_readback = readback;
        return false;
    }

    receiver_hits = (RuntimeCausticPhotonSurfaceHit3D*)calloc(
        (size_t)sample_budget,
        sizeof(RuntimeCausticPhotonSurfaceHit3D));
    receiver_path_pdfs = (double*)calloc((size_t)sample_budget, sizeof(double));
    if (!receiver_hits || !receiver_path_pdfs) {
        free(receiver_hits);
        free(receiver_path_pdfs);
        *out_readback = readback;
        return false;
    }

    minimum_radius = active_settings->surfaceQueryRadius > 0.0
                         ? active_settings->surfaceQueryRadius
                         : 0.10;
    minimum_radius = photon_integration_clamp_double(minimum_radius, 0.02, 2.0);

    RuntimeCausticPhotonEmission3D_DefaultSettings(&emission_settings);
    emission_settings.sampleBudget = sample_budget;
    emission_settings.baseSeed = active_settings->emissionSeed;
    emission_settings.defaultQueryRadius = minimum_radius;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    if (!RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, sample_budget)) {
        free(receiver_hits);
        free(receiver_path_pdfs);
        *out_readback = readback;
        return false;
    }

    readback.surfaceMapAllocated =
        RuntimeCausticPhotonMap3D_Allocate(surface_map, sample_budget);
    readback.surfaceMapPopulationAttempted = readback.surfaceMapAllocated;
    readback.emissionAttempted = true;
    if (readback.surfaceMapAllocated) {
        readback.emissionSucceeded =
            RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                            &scene->lightSet,
                                                            &emission_settings,
                                                            &emission_diag);
        readback.emittedPhotonCount = emission_diag.emittedPhotonCount;
        readback.rejectedPhotonCount = emission_diag.rejectedPhotonCount;
        readback.totalEmittedFlux = emission_diag.totalEmittedFlux;
    }

    RuntimeCausticPhotonTrace3D_DefaultSettings(&trace_settings);
    trace_settings.maxDepth = (uint32_t)photon_integration_clamp_int(
        active_settings->maxPathDepth,
        1,
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_MAX_DEPTH);

    if (readback.emissionSucceeded) {
        for (uint64_t i = 0u; i < batch.sampleCount; ++i) {
            const RuntimeCausticPhotonSample3D* photon = &batch.samples[i];
            const RuntimeLightSource3D* source =
                RuntimeLightSet3D_GetEnabled(&scene->lightSet, photon->lightIndex);
            RuntimeCausticLensLightSample3D lens_light;
            RuntimeCausticLensSample3D lens_sample;
            RuntimeCausticLensPath3D lens_path;
            RuntimeCausticPhotonTrace3D trace;
            HitInfo3D receiver_hit;

            if (!source) {
                source = RuntimeLightSet3D_GetEnabled(&scene->lightSet, 0);
            }
            if (!source) continue;

            RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
            RuntimeCausticLensTransport3D_DefaultSample(&lens_sample);
            lens_light.position = photon->position;
            lens_light.radius = source->radius;
            lens_light.intensity = source->intensity;
            lens_light.color = source->color;
            lens_light.lightIndex = photon->lightIndex;
            lens_sample.apertureU = photon_integration_centered_sample(i, sample_budget, 1u);
            lens_sample.apertureV = photon_integration_centered_sample(i, sample_budget, 2u);
            lens_sample.lensU = photon_integration_centered_sample(i, sample_budget, 3u);
            lens_sample.lensV = photon_integration_centered_sample(i, sample_budget, 4u);
            lens_sample.sampleWeight = sample_budget > 0u
                                           ? 1.0 / (double)sample_budget
                                           : 1.0;
            lens_sample.receiverDistance =
                mesh_dielectric->radius > 1.0e-9 ? mesh_dielectric->radius * 4.0 : 1.0;

            if (!RuntimeCausticLensTransport3D_SolveMeshDielectricPath(mesh_dielectric,
                                                                       entry_triangle,
                                                                       &lens_light,
                                                                       &lens_sample,
                                                                       &lens_path)) {
                continue;
            }
            solved_path_count += 1u;
            if (!RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(&lens_path,
                                                                     photon,
                                                                     &trace_settings,
                                                                     &trace)) {
                continue;
            }
            trace_count += 1u;
            receiver_policy.dielectricSceneObjectIndex =
                mesh_dielectric->sceneObjectIndex;
            receiver_policy.receiverProbeDistance =
                fmax(mesh_dielectric->radius * 5.0, 1.0);
            if (!RuntimeCausticPhotonReceiverPolicy3D_SelectHit(
                    scene,
                    &trace,
                    &receiver_policy,
                    &receiver_hit,
                    &receiver_policy_readback)) {
                continue;
            }
            if (receiver_hit_count < sample_budget) {
                RuntimeCausticPhotonSurfaceHit3D* surface_hit =
                    &receiver_hits[receiver_hit_count];
                RuntimeMaterialPayload3D receiver_material;
                memset(surface_hit, 0, sizeof(*surface_hit));
                surface_hit->photonId = trace.finalState.photonId;
                surface_hit->depth = trace.finalState.depth;
                surface_hit->sceneObjectIndex = receiver_hit.sceneObjectIndex;
                surface_hit->primitiveIndex = receiver_hit.primitiveIndex;
                surface_hit->triangleIndex = receiver_hit.triangleIndex;
                surface_hit->materialId =
                    RuntimeMaterialPayload3D_ResolveFromHit(&receiver_hit,
                                                            &receiver_material) &&
                            receiver_material.valid
                        ? receiver_material.materialId
                        : -1;
                surface_hit->position = receiver_hit.position;
                surface_hit->normal = receiver_hit.normal;
                surface_hit->incidentDirection = trace.postExitDirection;
                surface_hit->flux = trace.finalState.throughput;
                surface_hit->normalDotPhoton =
                    fabs(vec3_dot(vec3_normalize(receiver_hit.normal),
                                  vec3_normalize(trace.postExitDirection)));
                receiver_path_pdfs[receiver_hit_count] = trace.finalState.pathPdf;
                receiver_hit_count += 1u;
            }
        }
    }

    readback.traceSolvedPathCount = solved_path_count;
    readback.traceRecordCount = trace_count;
    readback.traceInputCount = trace_count;
    RuntimeCausticPhotonReceiverPolicy3D_SelectPrimaryBucket(receiver_hits,
                                                             receiver_hit_count,
                                                             &receiver_bucket);
    receiver_policy_readback.selectedHitCount =
        receiver_bucket.valid ? receiver_bucket.hitCount : 0u;
    receiver_policy_readback.selectedBucketCount =
        receiver_bucket.valid ? receiver_bucket.bucketCount : 0u;
    receiver_policy_readback.competingRejectCount =
        receiver_bucket.valid ? receiver_bucket.competingHitCount : 0u;
    photon_integration_merge_receiver_policy_readback(&readback,
                                                      &receiver_policy_readback);
    if (receiver_bucket.valid && receiver_bucket.hitCount > 0u) {
        for (uint64_t i = 0u; i < receiver_hit_count; ++i) {
            if (RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(
                    &receiver_hits[i],
                    &receiver_bucket)) {
                selected_receiver_hit_count += 1u;
            }
        }
    }

    if (receiver_bucket.valid && selected_receiver_hit_count > 0u) {
        RuntimeCausticPhotonSurfaceHit3D* selected_hits = NULL;
        uint64_t selected_index = 0u;
        selected_hits = (RuntimeCausticPhotonSurfaceHit3D*)calloc(
            (size_t)selected_receiver_hit_count,
            sizeof(RuntimeCausticPhotonSurfaceHit3D));
        if (selected_hits) {
            for (uint64_t i = 0u; i < receiver_hit_count; ++i) {
                if (!RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(
                        &receiver_hits[i],
                        &receiver_bucket)) {
                    continue;
                }
                selected_hits[selected_index++] = receiver_hits[i];
            }
        }
        footprint_radius = photon_integration_receiver_distribution_radius(
            selected_hits ? selected_hits : receiver_hits,
            selected_hits ? selected_receiver_hit_count : receiver_hit_count,
            minimum_radius,
            &receiver_centroid,
            &selected_receiver_mean_distance,
            &selected_receiver_max_distance);
        free(selected_hits);
    }

    if (receiver_hit_count > 0u) {
        for (uint64_t bucket_i = 0u; bucket_i < receiver_hit_count; ++bucket_i) {
            RuntimeCausticPhotonSurfaceHit3D* bucket_hits = NULL;
            uint64_t bucket_count = 0u;
            uint64_t bucket_index = 0u;
            double bucket_radius = minimum_radius;
            Vec3 bucket_centroid = vec3(0.0, 0.0, 0.0);
            double bucket_mean_distance = 0.0;
            double bucket_max_distance = 0.0;

            if (photon_integration_surface_hit_bucket_seen_before(receiver_hits,
                                                                  bucket_i)) {
                continue;
            }
            for (uint64_t i = 0u; i < receiver_hit_count; ++i) {
                if (photon_integration_surface_hit_identity_matches(&receiver_hits[i],
                                                                    &receiver_hits[bucket_i])) {
                    bucket_count += 1u;
                }
            }
            if (bucket_count == 0u) continue;
            bucket_hits = (RuntimeCausticPhotonSurfaceHit3D*)calloc(
                (size_t)bucket_count,
                sizeof(RuntimeCausticPhotonSurfaceHit3D));
            if (!bucket_hits) continue;
            for (uint64_t i = 0u; i < receiver_hit_count; ++i) {
                if (photon_integration_surface_hit_identity_matches(&receiver_hits[i],
                                                                    &receiver_hits[bucket_i])) {
                    bucket_hits[bucket_index++] = receiver_hits[i];
                }
            }
            bucket_radius = photon_integration_receiver_distribution_radius(
                bucket_hits,
                bucket_count,
                minimum_radius,
                &bucket_centroid,
                &bucket_mean_distance,
                &bucket_max_distance);
            if (!receiver_bucket.valid ||
                RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(
                    &receiver_hits[bucket_i],
                    &receiver_bucket)) {
                footprint_radius = bucket_radius;
                receiver_centroid = bucket_centroid;
                receiver_mean_distance = bucket_mean_distance;
                receiver_max_distance = bucket_max_distance;
            }
            for (uint64_t i = 0u; i < receiver_hit_count; ++i) {
                if (!photon_integration_surface_hit_identity_matches(&receiver_hits[i],
                                                                     &receiver_hits[bucket_i])) {
                    continue;
                }
                receiver_hits[i].footprintRadius = bucket_radius;
                if (RuntimeCausticPhotonMap3D_StoreSurfaceHit(surface_map,
                                                              &receiver_hits[i],
                                                              receiver_path_pdfs[i],
                                                              bucket_radius)) {
                    readback.totalStoredSurfaceFlux =
                        vec3_add(readback.totalStoredSurfaceFlux, receiver_hits[i].flux);
                    populated = true;
                }
            }
            free(bucket_hits);
        }
    }

    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(surface_map, &map_diag);
    readback.receiverFootprintRadius = footprint_radius;
    readback.receiverMeanDistance = receiver_mean_distance;
    readback.receiverMaxDistance = receiver_max_distance;
    readback.surfaceMapStoreAttemptCount = map_diag.storeAttemptCount;
    readback.surfaceMapStoreAcceptedCount = map_diag.storeAcceptedCount;
    readback.surfaceMapStoreRejectedCount = map_diag.storeRejectedCount;
    readback.surfaceMapRecordCount = map_diag.recordCount;
    readback.surfaceMapAccelerationInsertedCount = map_diag.accelerationInsertedCount;
    readback.surfaceMapPopulated = populated;
    readback.tracePopulationSucceeded = populated;
    if (populated) {
        readback.populationSource =
            RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_TRACE_RECORDS;
        if (out_receiver) {
            out_receiver->valid = true;
            out_receiver->position = receiver_centroid;
            out_receiver->normal = receiver_hits[receiver_bucket.firstIndex].normal;
            out_receiver->sceneObjectIndex = receiver_bucket.sceneObjectIndex;
            out_receiver->primitiveIndex = receiver_bucket.primitiveIndex;
            out_receiver->triangleIndex = receiver_bucket.triangleIndex;
            out_receiver->footprintRadius = footprint_radius;
            out_receiver->receiverCentroid = receiver_centroid;
            out_receiver->receiverMeanDistance = receiver_mean_distance;
            out_receiver->receiverMaxDistance = receiver_max_distance;
            out_receiver->receiverCount = receiver_bucket.hitCount;
        }
    }

    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    free(receiver_path_pdfs);
    free(receiver_hits);
    *out_readback = readback;
    return populated;
}
