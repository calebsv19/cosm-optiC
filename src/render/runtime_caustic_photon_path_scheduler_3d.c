#include "render/runtime_caustic_photon_path_scheduler_3d.h"

#include <stddef.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_population_3d.h"
#include "render/runtime_caustic_photon_path_transport_3d.h"
#include "render/runtime_caustic_photon_emission_proposal_3d.h"

bool RuntimeCausticPhotonPathScheduler3D_PopulateOwnedMaps(
    const RuntimeScene3D* scene,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticLensShape3D* emission_lenses,
    uint32_t emission_lens_count,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback) {
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonEmissionSettings3D emission_settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D emission_diag;
    RuntimeCausticPhotonPathTransportSettings3D transport_settings;
    RuntimeCausticPhotonPathPopulationSettings3D population_settings;
    RuntimeCausticPhotonMapDiagnostics3D surface_diag;
    RuntimeCausticBeamMapDiagnostics3D beam_diag;
    uint64_t sample_budget;
    uint64_t map_capacity;
    uint64_t surface_neighbor_minimum;
    uint64_t surface_neighbor_limit;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!scene || !surface_map || !settings || !out_readback ||
        settings->productMode != RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP ||
        (!settings->surfaceQueryEnabled && !settings->volumeQueryEnabled) ||
        settings->sampleBudget <= 0) {
        return false;
    }
    readback.attempted = true;
    readback.tracePopulationAttempted = true;
    readback.surfaceMapPopulationAttempted = settings->surfaceQueryEnabled;
    readback.volumeBeamPopulationAttempted = settings->volumeQueryEnabled;
    sample_budget = (uint64_t)settings->sampleBudget;
    map_capacity = sample_budget *
                   (uint64_t)(settings->maxPathDepth > 0 ? settings->maxPathDepth : 1);
    if (map_capacity < sample_budget || map_capacity > 262144u) {
        map_capacity = 262144u;
    }
    if (!RuntimeCausticPhotonMap3D_Allocate(surface_map, map_capacity) ||
        (settings->volumeQueryEnabled &&
         (!beam_map || !RuntimeCausticBeamMap3D_Allocate(beam_map, map_capacity)))) {
        readback.pathPopulationCapacityRejectCount += 1u;
        *out_readback = readback;
        return false;
    }
    readback.surfaceMapAllocated = true;
    readback.volumeBeamMapAllocated =
        settings->volumeQueryEnabled && RuntimeCausticBeamMap3D_IsAllocated(beam_map);

    RuntimeCausticPhotonEmission3D_DefaultSettings(&emission_settings);
    emission_settings.sampleBudget = sample_budget;
    emission_settings.baseSeed = settings->emissionSeed;
    emission_settings.defaultQueryRadius = settings->surfaceQueryRadius;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    readback.emissionAttempted = true;
    if (!RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, sample_budget)) {
        *out_readback = readback;
        return false;
    }
    readback.emissionSucceeded = RuntimeCausticPhotonEmission3D_EmitFromLightSet(
        &batch, &scene->lightSet, &emission_settings, &emission_diag);
    readback.requestedSampleBudget = sample_budget;
    readback.emissionSeed = emission_settings.baseSeed;
    readback.emittedPhotonCount = emission_diag.emittedPhotonCount;
    readback.rejectedPhotonCount = emission_diag.rejectedPhotonCount;
    readback.totalEmittedFlux = emission_diag.totalEmittedFlux;

    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&transport_settings);
    transport_settings.sceneTrace.maxDepth =
        (uint32_t)(settings->maxPathDepth > 0 ? settings->maxPathDepth : 1);
    RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&population_settings);
    population_settings.storeDiffuseSurfaces = settings->surfaceQueryEnabled;
    population_settings.storeTraversedBeams = settings->volumeQueryEnabled;
    population_settings.surfaceQueryRadius = settings->surfaceQueryRadius;
    population_settings.allowActiveMediumSurfaceReceiver =
        settings->surfaceAllowActiveMediumReceiver;
    population_settings.beamRadiusStart = settings->volumeQueryRadius;
    population_settings.beamRadiusEnd = settings->volumeQueryRadius;
    population_settings.requireBeamMediumId = true;
    population_settings.beamMediumId = settings->volumeMediumId;
    population_settings.requireBeamStage = true;
    population_settings.beamStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    population_settings.clipBeamsToBounds =
        settings->volumeQueryEnabled &&
        RuntimeVolumeGrid3D_IsConfigured(&scene->volume.grid);
    population_settings.beamBoundsMin = scene->volume.grid.boundsMin;
    population_settings.beamBoundsMax = scene->volume.grid.boundsMax;
    for (uint64_t i = 0u; i < batch.sampleCount; ++i) {
        RuntimeCausticPhotonSceneTrace3D trace;
        RuntimeCausticPhotonPathPopulationReadback3D path_population;
        bool traced;

        if (settings->emissionProposalMode !=
                RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED &&
            emission_lenses && emission_lens_count > 0u) {
            RuntimeCausticPhotonEmissionProposalReadback3D proposal_readback;
            const RuntimeLightSource3D* emission_source =
                RuntimeLightSet3D_GetEnabled(&scene->lightSet,
                                             batch.samples[i].lightIndex);
            (void)RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
                &batch.samples[i], emission_source, emission_lenses,
                emission_lens_count, &proposal_readback);
        }
        if (batch.samples[i].guidingChangedSample) {
            readback.emissionGuidedSampleCount++;
            if (batch.samples[i].guidingPdfFluxCorrected) {
                readback.emissionGuidedCorrectedCount++;
            } else {
                readback.emissionGuidedUncorrectedCount++;
            }
        }
        if (!batch.samples[i].guidingChangedSample) {
            readback.emissionUnbiasedSampleCount++;
        }
        readback.emissionProposalPdfSum += batch.samples[i].proposalPdf;
        readback.emissionFluxCorrectionSum +=
            batch.samples[i].emissionFluxCorrection;
        readback.pathTransportAttemptCount += 1u;
        traced = RuntimeCausticPhotonPathTransport3D_Trace(
            scene, &batch.samples[i], &transport_settings, &trace);
        readback.traceInputCount += 1u;
        readback.pathIntersectionCount += trace.readback.intersectionCount;
        readback.pathMaterialResolveFailureCount +=
            trace.readback.materialResolveFailureCount;
        readback.pathMediumTransitionCount += trace.readback.mediumTransitionCount;
        readback.pathMediumTransitionFailureCount +=
            trace.readback.mediumTransitionFailureCount;
        readback.pathAttenuatedSegmentCount += trace.readback.attenuatedSegmentCount;
        readback.pathTotalInternalReflectionCount +=
            trace.trace.debug.totalInternalReflectionCount;
        if (!traced) {
            readback.pathTransportRejectedCount += 1u;
            continue;
        }
        readback.pathTransportSucceededCount += 1u;
        readback.traceSolvedPathCount += 1u;
        if (RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                &trace,
                &population_settings,
                surface_map,
                settings->volumeQueryEnabled ? beam_map : NULL,
                &path_population)) {
            readback.pathPopulationSucceededCount += 1u;
            readback.traceRecordCount += 1u;
        } else {
            readback.pathPopulationRejectedCount += 1u;
            if (path_population.termination ==
                RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED) {
                readback.pathPopulationCapacityRejectCount += 1u;
            }
        }
        readback.pathDielectricEntryCount += path_population.dielectricEntryCount;
        readback.pathDielectricExitCount += path_population.dielectricExitCount;
        if (path_population.dielectricEntryCount > 0u) {
            if (path_population.dielectricEntryCount ==
                path_population.dielectricExitCount) {
                readback.pathSolidMediumReconciledCount++;
            } else {
                readback.pathSolidMediumUnreconciledCount++;
            }
        }
        for (int reason = 0;
             reason < RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_REASON_COUNT;
             ++reason) {
            readback.retention.surfaceRetained[reason] +=
                path_population.retention.surfaceRetained[reason];
        }
        for (int reason = 0;
             reason < RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT;
             ++reason) {
            readback.retention.surfaceRejected[reason] +=
                path_population.retention.surfaceRejected[reason];
            readback.retention.beamRejected[reason] +=
                path_population.retention.beamRejected[reason];
        }
        for (int stage = 0; stage < RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_COUNT;
             ++stage) {
            readback.retention.beamRetained[stage] +=
                path_population.retention.beamRetained[stage];
        }
        readback.volumeBeamExaminedCount += path_population.beamExaminedCount;
        readback.volumeBeamClippedCount += path_population.beamClippedCount;
        for (int reason = 0;
             reason <= RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID;
             ++reason) {
            readback.volumeBeamEligibility[reason] +=
                path_population.beamEligibility[reason];
        }
    }
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    surface_neighbor_minimum =
        settings->qualityTier == RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL
            ? 64u
            : settings->qualityTier == RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION
                  ? 32u
                  : 16u;
    surface_neighbor_limit =
        RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
            surface_map->recordCount,
            surface_neighbor_minimum,
            256u);
    if (surface_map->recordCount > 0u &&
        !RuntimeCausticPhotonMap3D_PrepareSampleCenteredSupports(
            surface_map, surface_neighbor_limit)) {
        *out_readback = readback;
        return false;
    }

    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(surface_map, &surface_diag);
    if (beam_map) RuntimeCausticBeamMap3D_SnapshotDiagnostics(beam_map, &beam_diag);
    else memset(&beam_diag, 0, sizeof(beam_diag));
    readback.surfaceMapStoreAttemptCount = surface_diag.storeAttemptCount;
    readback.surfaceMapStoreAcceptedCount = surface_diag.storeAcceptedCount;
    readback.surfaceMapStoreRejectedCount = surface_diag.storeRejectedCount;
    readback.surfaceMapRecordCount = surface_diag.recordCount;
    readback.surfaceMapAccelerationInsertedCount = surface_diag.accelerationInsertedCount;
    readback.volumeBeamStoreAttemptCount = beam_diag.storeAttemptCount;
    readback.volumeBeamStoreAcceptedCount = beam_diag.storeAcceptedCount;
    readback.volumeBeamStoreRejectedCount = beam_diag.storeRejectedCount;
    readback.volumeBeamSegmentCount = beam_diag.segmentCount;
    readback.volumeBeamAccelerationInsertedCount = beam_diag.accelerationInsertedCount;
    readback.totalStoredSurfaceFlux = surface_diag.totalStoredFlux;
    readback.totalStoredVolumeFlux = beam_diag.totalStoredFlux;
    readback.surfaceMapPopulated = surface_diag.recordCount > 0u;
    readback.volumeBeamPopulated = beam_diag.segmentCount > 0u;
    readback.tracePopulationSucceeded = readback.surfaceMapPopulated ||
                                        readback.volumeBeamPopulated;
    readback.pathSolidMediumTransitionsReconciled =
        readback.pathSolidMediumReconciledCount > 0u;
    readback.emissionFluxCompensationAppliedExactlyOnce =
        readback.emissionGuidedUncorrectedCount == 0u &&
        (readback.emissionGuidedCorrectedCount > 0u ||
         readback.emissionUnbiasedSampleCount > 0u);
    if (readback.tracePopulationSucceeded) {
        readback.populationSource =
            RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_PATH_TRANSPORT;
    }
    *out_readback = readback;
    return readback.tracePopulationSucceeded;
}
