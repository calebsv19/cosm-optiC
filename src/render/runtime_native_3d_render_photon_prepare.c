#include "render/runtime_native_3d_render_photon_prepare.h"

#include <string.h>
#include <time.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "render/runtime_caustic_photon_distributed_beam_cache_3d.h"
#include "render/runtime_caustic_photon_map_store_3d.h"
#include "render/runtime_caustic_photon_path_scheduler_3d.h"
#include "render/runtime_caustic_photon_scene_descriptor_3d.h"
#include "render/runtime_caustic_photon_volume_segment_normalization_3d.h"
#include "render/runtime_volume_3d_sampling.h"

static bool gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled = false;
static RuntimeCausticPhotonIntegrationSettings3D
    gRuntimeNative3DCausticPhotonRenderPrepSettings;
static RuntimeCausticPhotonMapStore3D gRuntimeNative3DCausticPhotonMapStore;

static double runtime_native_3d_prepare_elapsed_ms_since(
    const struct timespec* started_at) {
    struct timespec finished_at;
    if (!started_at || clock_gettime(CLOCK_MONOTONIC, &finished_at) != 0) {
        return 0.0;
    }
    return (double)(finished_at.tv_sec - started_at->tv_sec) * 1000.0 +
        (double)(finished_at.tv_nsec - started_at->tv_nsec) / 1000000.0;
}

void RuntimeNative3DRender_SetCausticPhotonRenderPrepPopulation(
    bool enabled,
    const RuntimeCausticPhotonIntegrationSettings3D* settings) {
    gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled = enabled;
    if (settings) {
        gRuntimeNative3DCausticPhotonRenderPrepSettings = *settings;
    } else {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(
            &gRuntimeNative3DCausticPhotonRenderPrepSettings);
    }
    RuntimeCausticPhotonIntegration3D_NormalizeSettings(
        &gRuntimeNative3DCausticPhotonRenderPrepSettings);
    if (!enabled) RuntimeNative3DRender_ResetCausticPhotonMapStore();
}

void RuntimeNative3DRender_ResetCausticPhotonMapStore(void) {
    RuntimeCausticPhotonDirectConsumer3D_Reset();
    RuntimeCausticPhotonMapStore3D_Free(&gRuntimeNative3DCausticPhotonMapStore);
}

static uint32_t runtime_native_3d_prepare_harvest_photon_mesh_dielectric(
    const RuntimeScene3D* scene,
    RuntimeCausticLensShape3D* out_shapes,
    uint32_t shape_capacity,
    RuntimeCausticPhotonMapPopulationReadback3D* io_population) {
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    const RuntimeCausticPhotonMeshDielectricDescriptor3D* selected = NULL;
    uint32_t shape_count = 0u;

    if (io_population) {
        io_population->preparedSceneMeshDielectricAttempted = true;
        io_population->preparedSceneMeshDielectricSceneObjectIndex = -1;
        io_population->preparedSceneMeshDielectricPrimitiveIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleCount = 0;
    }
    if (!scene || !out_shapes || shape_capacity == 0u) return 0u;
    if (!RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(scene,
                                                                          &batch)) {
        if (io_population) {
            io_population->preparedSceneMeshDielectricCandidateCount =
                batch.meshDielectricCandidateCount;
        }
        return 0u;
    }

    selected = RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(&batch);
    if (!selected) return 0u;
    shape_count = RuntimeCausticPhotonSceneDescriptor3D_CopyMeshDielectricShapes(
        &batch, out_shapes, shape_capacity);
    if (io_population) {
        io_population->preparedSceneMeshDielectricSucceeded = true;
        io_population->preparedSceneMeshDielectricCandidateCount =
            batch.meshDielectricCandidateCount;
        io_population->preparedSceneMeshDielectricSceneObjectIndex =
            selected->sceneObjectIndex;
        io_population->preparedSceneMeshDielectricPrimitiveIndex =
            selected->primitiveIndex;
        io_population->preparedSceneMeshDielectricTriangleIndex =
            selected->triangleIndex;
        io_population->preparedSceneMeshDielectricTriangleCount =
            selected->triangleCount;
    }
    return shape_count;
}

void runtime_native_3d_prepare_populate_photon_render_prep(
    RuntimeNative3DPreparedFrame* frame) {
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonMap3D* surface_map;
    RuntimeCausticBeamMap3D* beam_map;
    RuntimeCausticPhotonMapPopulationReadback3D population;
    RuntimeCausticPhotonMapPopulationReadback3D harvest;
    RuntimeCausticPhotonMapLifecycleInput3D lifecycle_input;
    RuntimeCausticPhotonMapLifecycleReadback3D lifecycle_readback;
    RuntimeCausticLensShape3D emission_lenses[MAX_OBJECTS];
    uint32_t emission_lens_count = 0u;
    struct timespec fingerprint_started_at = {0};
    struct timespec map_build_started_at = {0};
    struct timespec query_started_at = {0};
    double map_build_cpu_ms = 0.0;
    uint64_t cache_capacity = 1u;

    if (!frame) return;
    settings = gRuntimeNative3DCausticPhotonRenderPrepSettings;
    if (!gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled) return;
    if (RuntimeCausticPhotonIntegration3D_RouteForSettings(&settings) !=
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        return;
    }
    /* Population is an explicit request-owned lifecycle independent of whether
     * the resulting map contributes to this frame. Keeping it active for a
     * contribution-off request gives ordinary A/B renders the same transport
     * population while the integration layer remains responsible for
     * suppressing query deposits and shading. */
    if (!settings.surfaceQueryEnabled && !settings.volumeQueryEnabled) {
        return;
    }
    if (settings.sampleBudget <= 0) settings.sampleBudget = 8;
    RuntimeCausticPhotonIntegration3D_NormalizeSettings(&settings);
    cache_capacity = (uint64_t)settings.sampleBudget;

    memset(&harvest, 0, sizeof(harvest));
    emission_lens_count =
        runtime_native_3d_prepare_harvest_photon_mesh_dielectric(
            &frame->scene, emission_lenses, MAX_OBJECTS, &harvest);

    if (settings.surfaceQueryEnabled &&
        !RuntimeCausticSurfaceCache3D_IsAllocated(&frame->causticSurfaceCache)) {
        (void)RuntimeCausticSurfaceCache3D_Allocate(&frame->causticSurfaceCache,
                                                    cache_capacity);
    }
    if (settings.volumeQueryEnabled &&
        !RuntimeCausticVolumeCache3D_IsAllocated(&frame->causticVolumeCache)) {
        (void)RuntimeCausticVolumeCache3D_AllocateFromVolume(&frame->causticVolumeCache,
                                                             &frame->scene.volume);
    }

    clock_gettime(CLOCK_MONOTONIC, &fingerprint_started_at);
    RuntimeCausticPhotonMapLifecycle3D_BuildInputFromScene(
        &frame->scene,
        settings.sampleBudget,
        settings.emissionSeed,
        settings.maxPathDepth,
        settings.surfaceQueryRadius,
        settings.volumeQueryRadius,
        settings.volumeQueryEnabled,
        true,
        &lifecycle_input);
    if (!RuntimeCausticPhotonMapStore3D_Begin(
        &gRuntimeNative3DCausticPhotonMapStore,
        &lifecycle_input,
        &lifecycle_readback)) {
        return;
    }
    lifecycle_readback.fingerprintCpuMs =
        runtime_native_3d_prepare_elapsed_ms_since(&fingerprint_started_at);
    lifecycle_readback.budgetTier = RuntimeCausticPhotonBudgetTier3D_FromBudget(
        settings.sampleBudget,
        settings.maxPathDepth);
    surface_map = &gRuntimeNative3DCausticPhotonMapStore.surfaceMap;
    beam_map = &gRuntimeNative3DCausticPhotonMapStore.beamMap;

    if (lifecycle_readback.rebuilt) {
        clock_gettime(CLOCK_MONOTONIC, &map_build_started_at);
        memset(&population, 0, sizeof(population));
        (void)RuntimeCausticPhotonPathScheduler3D_PopulateOwnedMaps(
            &frame->scene,
            surface_map,
            settings.volumeQueryEnabled ? beam_map : NULL,
            emission_lens_count > 0u ? emission_lenses : NULL,
            emission_lens_count,
            &settings,
            &population);
        population.preparedSceneMeshDielectricAttempted =
            harvest.preparedSceneMeshDielectricAttempted;
        population.preparedSceneMeshDielectricSucceeded =
            harvest.preparedSceneMeshDielectricSucceeded;
        population.fixtureMeshDielectricFallbackUsed =
            harvest.fixtureMeshDielectricFallbackUsed;
        population.preparedSceneMeshDielectricCandidateCount =
            harvest.preparedSceneMeshDielectricCandidateCount;
        population.preparedSceneMeshDielectricSceneObjectIndex =
            harvest.preparedSceneMeshDielectricSceneObjectIndex;
        population.preparedSceneMeshDielectricPrimitiveIndex =
            harvest.preparedSceneMeshDielectricPrimitiveIndex;
        population.preparedSceneMeshDielectricTriangleIndex =
            harvest.preparedSceneMeshDielectricTriangleIndex;
        population.preparedSceneMeshDielectricTriangleCount =
            harvest.preparedSceneMeshDielectricTriangleCount;

        map_build_cpu_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&map_build_started_at);
        RuntimeCausticPhotonMapStore3D_CommitPopulation(
            &gRuntimeNative3DCausticPhotonMapStore,
            &population);
    } else {
        population = gRuntimeNative3DCausticPhotonMapStore.population;
    }

    memset(&frame->causticPhotonRenderPrepReadback,
           0,
           sizeof(frame->causticPhotonRenderPrepReadback));
    frame->causticPhotonRenderPrepReadback.mapLifecycle = lifecycle_readback;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.mapBuildCpuMs =
        map_build_cpu_ms;
    frame->causticPhotonRenderPrepReadback.productMode = settings.productMode;
    frame->causticPhotonRenderPrepReadback.consumer = settings.consumer;
    frame->causticPhotonRenderPrepReadback.route =
        RuntimeCausticPhotonIntegration3D_RouteForSettings(&settings);
    frame->causticPhotonRenderPrepReadback.renderContributionSuppressed =
        !settings.renderContributionEnabled;
    frame->causticPhotonRenderPrepReadback.queryAttempted = true;
    frame->causticPhotonRenderPrepReadback.contributionAttempted = true;
    frame->causticPhotonRenderPrepReadback.cacheDepositAttempted =
        settings.consumer == RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE ||
        settings.volumeQueryEnabled;
    clock_gettime(CLOCK_MONOTONIC, &query_started_at);
    if (settings.surfaceQueryEnabled) {
        (void)RuntimeCausticPhotonIntegration3D_DepositSurfaceContributionsForReceiverBuckets(
            surface_map,
            &frame->causticSurfaceCache,
            &settings,
            &frame->causticPhotonRenderPrepReadback.receiverContribution);
    }
    frame->causticPhotonRenderPrepReadback.queryHit =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverQueryHitCount > 0u;
    frame->causticPhotonRenderPrepReadback.contributionEligible =
        frame->causticPhotonRenderPrepReadback.receiverContribution.eligible;
    frame->causticPhotonRenderPrepReadback.surfaceDeposited =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceDepositAcceptedCount > 0u;
    frame->causticPhotonRenderPrepReadback.surfaceCandidateCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceCandidateCount;
    frame->causticPhotonRenderPrepReadback.surfaceContributingCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceContributingCount;
    frame->causticPhotonRenderPrepReadback.estimatedCost =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverQueryAttemptCount;
    frame->causticPhotonRenderPrepReadback.radiance =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceRadiance;
    if (settings.renderContributionEnabled && settings.volumeQueryEnabled &&
        RuntimeCausticVolumeCache3D_IsAllocated(&frame->causticVolumeCache)) {
        RuntimeCausticDistributedBeamCacheSettings3D distributed_settings;
        RuntimeCausticPhotonBeamContributionReadback3D* beam_readback =
            &frame->causticPhotonRenderPrepReadback.beamContribution;
        RuntimeCausticDistributedBeamCache3D_DefaultSettings(&distributed_settings);
        RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
            &distributed_settings, settings.qualityTier);
        distributed_settings.queryRadius = settings.volumeQueryRadius;
        distributed_settings.storageBackend =
            settings.volumeCacheStorageBackend;
        distributed_settings.mediumId = settings.volumeMediumId;
        distributed_settings.requireMediumId = true;
        distributed_settings.segmentStage =
            RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
        distributed_settings.requireSegmentStage = true;
        distributed_settings.buildMode =
            RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
        beam_readback->attempted = true;
        beam_readback->volumeSampleable =
            RuntimeVolume3D_HasSampleableDensity(&frame->scene.volume);
        beam_readback->beamMapAllocated =
            RuntimeCausticBeamMap3D_IsAllocated(beam_map);
        if (beam_readback->volumeSampleable && beam_readback->beamMapAllocated) {
            beam_readback->radius = settings.volumeQueryRadius;
            beam_readback->mediumId = settings.volumeMediumId;
            beam_readback->contributionEligible = beam_map->segmentCount > 0u;
            if (settings.consumer !=
                RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE) {
                RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D
                    normalization;
                RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D
                    normalization_readback;
                RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(
                    &normalization);
                normalization.queryRadius = settings.volumeQueryRadius;
                normalization.targetVoxelSize =
                    RuntimeCausticPhotonVolumeSegmentNormalization3D_TargetVoxelSize(
                        settings.volumeQueryRadius, settings.qualityTier);
                normalization.mediumId = settings.volumeMediumId;
                normalization.requireMediumId = true;
                normalization.segmentStage =
                    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
                normalization.requireSegmentStage = true;
                normalization.accelerated = true;
                normalization.maxSegments = distributed_settings.maxSegments;
                normalization.maxAxialSamples =
                    distributed_settings.maxAxialSamples;
                normalization.maxCellTests = distributed_settings.maxCellTests;
                beam_readback->contributionEligible =
                    RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
                        beam_map,
                        &frame->scene.volume.grid,
                        &normalization,
                        &normalization_readback) &&
                    beam_readback->contributionEligible;
            }
            if (settings.consumer ==
                RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE) {
                beam_readback->volumeDepositAttemptCount = 1u;
                beam_readback->volumeDeposited =
                    RuntimeCausticDistributedBeamCache3D_Build(
                        &frame->causticVolumeCache,
                        beam_map,
                        &distributed_settings,
                        &beam_readback->distributedCache);
                beam_readback->volumeDepositAcceptedCount =
                    beam_readback->volumeDeposited ? 1u : 0u;
                beam_readback->contributionEligible =
                    beam_readback->volumeDeposited;
                beam_readback->queryHit = beam_readback->volumeDeposited;
                beam_readback->queryAttemptCount = 1u;
                beam_readback->queryHitCount =
                    beam_readback->volumeDeposited ? 1u : 0u;
                beam_readback->candidateCount =
                    beam_readback->distributedCache.cellTestCount;
                beam_readback->contributingCount =
                    beam_readback->distributedCache.cellContributionCount;
            }
        }
    }
    frame->causticPhotonRenderPrepReadback.mapLifecycle.queryAndDepositCpuMs =
        runtime_native_3d_prepare_elapsed_ms_since(&query_started_at);
    frame->causticPhotonRenderPrepReadback.volumeDeposited =
        frame->causticPhotonRenderPrepReadback.beamContribution.volumeDeposited;
    frame->causticPhotonRenderPrepReadback.volumeCandidateCount =
        frame->causticPhotonRenderPrepReadback.beamContribution.candidateCount;
    frame->causticPhotonRenderPrepReadback.volumeContributingCount =
        frame->causticPhotonRenderPrepReadback.beamContribution.contributingCount;
    frame->causticPhotonRenderPrepReadback.estimatedCost +=
        frame->causticPhotonRenderPrepReadback.beamContribution.queryAttemptCount;
    frame->causticPhotonRenderPrepReadback.radiance = vec3_add(
        frame->causticPhotonRenderPrepReadback.radiance,
        frame->causticPhotonRenderPrepReadback.beamContribution.radiance);
    frame->causticPhotonRenderPrepReadback.mapLifecycle.emissionCount =
        lifecycle_readback.rebuilt ? population.emittedPhotonCount : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.tracedCount =
        lifecycle_readback.rebuilt ? population.traceRecordCount : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.storedSurfaceRecordCount =
        population.surfaceMapRecordCount;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.storedBeamSegmentCount =
        population.volumeBeamSegmentCount;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.accelerationBuildCount =
        lifecycle_readback.rebuilt
            ? population.surfaceMapAccelerationInsertedCount +
                  population.volumeBeamAccelerationInsertedCount
            : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.queryCount =
        frame->causticPhotonRenderPrepReadback.estimatedCost;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.cacheDepositCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceDepositAcceptedCount +
        frame->causticPhotonRenderPrepReadback.beamContribution
            .volumeDepositAcceptedCount;
    frame->causticPhotonRenderPrepReadback.mapPopulation = population;
    frame->causticPhotonRenderPrepReadback.mapPopulation.surfaceMapCapacity =
        surface_map->recordCapacity;
    frame->causticPhotonRenderPrepReadback.mapPopulation.volumeBeamCapacity =
        beam_map->segmentCapacity;
    frame->causticPhotonRenderPrepReadback.mapPopulation.recordStorageCeilingBytes =
        frame->causticPhotonRenderPrepReadback.mapPopulation.surfaceMapCapacity *
            sizeof(*surface_map->records) +
        beam_map->segmentCapacity * sizeof(*beam_map->segments);
    frame->causticPhotonRenderPrepReadbackBuilt = true;
    if (settings.renderContributionEnabled) {
        RuntimeCausticPhotonDirectConsumer3D_Bind(
            settings.consumer,
            settings.surfaceQueryEnabled ? surface_map : NULL,
            settings.volumeQueryEnabled ? beam_map : NULL, &settings);
    } else {
        RuntimeCausticPhotonDirectConsumer3D_Reset();
    }
}
