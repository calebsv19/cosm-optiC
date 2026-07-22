#include "render/runtime_caustic_photon_direct_consumer_3d.h"

#include <stdatomic.h>
#include <string.h>

#include "render/runtime_material_payload_3d.h"

static RuntimeCausticPhotonConsumer3D g_consumer =
    RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE;
static const RuntimeCausticPhotonMap3D* g_surface_map = NULL;
static uint64_t g_surface_sample_budget = 4096u;
static const RuntimeCausticBeamMap3D* g_beam_map = NULL;
static uint64_t g_neighbor_limit = 16u;
static RuntimeCausticPhotonEstimator3D g_surface_estimator =
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
static uint64_t g_surface_gather_neighbor_count = 8u;
static double g_surface_radiance_scale = 1.0;
static double g_surface_query_radius = 0.10;
static double g_surface_gather_max_radius = 0.20;
static int g_surface_receiver_scene_object_index = -1;
static RuntimeCausticPhotonSurfacePathFilter3D g_surface_path_filter =
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL;
static RuntimeCausticPhotonVolumeBeamEstimatorSettings3D
    g_volume_beam_estimator_settings;
static RuntimeCausticPhotonReceiverBsdfReadback3D g_receiver_bsdf_readback;
static atomic_flag g_receiver_bsdf_lock = ATOMIC_FLAG_INIT;
enum { RUNTIME_CAUSTIC_PHOTON_SURFACE_DIAGNOSTIC_CAPACITY = 8192 };
static RuntimeCausticPhotonSurfaceDiagnosticSample3D
    g_surface_diagnostic_samples[RUNTIME_CAUSTIC_PHOTON_SURFACE_DIAGNOSTIC_CAPACITY];
static atomic_uint_fast64_t g_surface_diagnostic_count;
static atomic_uint_fast64_t g_surface_query_count;
static atomic_uint_fast64_t g_surface_positive_query_count;
static atomic_uint_fast64_t g_surface_undersampled_positive_query_count;
static atomic_uint_fast64_t g_surface_effective_sample_count_sum;
static atomic_uint_fast64_t g_surface_effective_sample_histogram[5];
enum { RUNTIME_CAUSTIC_PHOTON_VOLUME_DIAGNOSTIC_CAPACITY = 8192 };
enum { RUNTIME_CAUSTIC_PHOTON_SURFACE_MAX_CONVERGENT_NEIGHBORS = 256 };
enum { RUNTIME_CAUSTIC_PHOTON_SURFACE_GATHER_REFERENCE_BUDGET = 4096 };
static RuntimeCausticPhotonVolumeDiagnosticSample3D
    g_volume_diagnostic_samples[RUNTIME_CAUSTIC_PHOTON_VOLUME_DIAGNOSTIC_CAPACITY];
static atomic_uint_fast64_t g_volume_diagnostic_count;

static bool photon_direct_uses_neighbor_gather(
    RuntimeCausticPhotonEstimator3D estimator) {
    return estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER ||
           estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER ||
           estimator ==
               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER;
}

static void photon_direct_bsdf_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_receiver_bsdf_lock,
                                              memory_order_acquire)) {
    }
}

static void photon_direct_bsdf_unlock(void) {
    atomic_flag_clear_explicit(&g_receiver_bsdf_lock, memory_order_release);
}

static Vec3 photon_direct_selected_surface_flux(
    const RuntimeCausticPhotonMapQueryResult3D* result) {
    if (!result) return vec3(0.0, 0.0, 0.0);
    switch (g_surface_path_filter) {
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE:
            return result->directTwoInterfacePhysicalFlux;
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH:
            return result->multipathPhysicalFlux;
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_UNCLASSIFIED:
            return result->unclassifiedPhysicalFlux;
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL:
        default:
            return result->physicalFlux;
    }
}

static void photon_direct_record_volume_diagnostic(
    const RuntimeCausticBeamMapQueryResult3D* query,
    const RuntimeCausticPhotonVolumeBeamEstimatorInput3D* input,
    const RuntimeCausticPhotonVolumeBeamEstimatorReadback3D* readback) {
    const uint64_t index = atomic_fetch_add_explicit(
        &g_volume_diagnostic_count, 1u, memory_order_relaxed);
    RuntimeCausticPhotonVolumeDiagnosticSample3D* sample;
    if (!query || !input || !readback ||
        index >= RUNTIME_CAUSTIC_PHOTON_VOLUME_DIAGNOSTIC_CAPACITY) {
        return;
    }
    sample = &g_volume_diagnostic_samples[index];
    memset(sample, 0, sizeof(*sample));
    sample->position = query->queryPosition;
    sample->queryRadius = query->queryRadius;
    sample->mediumId = query->queryMediumId;
    sample->segmentStage = query->querySegmentStage;
    sample->beamDirection = input->beamDirection;
    sample->viewToCameraDirection = input->viewToCameraDirection;
    sample->beamDistance = input->beamDistance;
    sample->mediumDensity = input->mediumDensity;
    sample->stepLength = input->stepLength;
    sample->phaseCosine = readback->phaseCosine;
    sample->phaseValue = readback->phaseValue;
    sample->beamTransmittance = readback->beamTransmittance;
    sample->scatterProbability = readback->scatterProbability;
    sample->cameraTransmittance = readback->cameraTransmittance;
    sample->integrationWeight = readback->integrationWeight;
    sample->radiance = readback->radiance;
    sample->contributed = readback->contributed;
}

static void photon_direct_record_surface_diagnostic(
    const HitInfo3D* hit,
    const RuntimeCausticPhotonMapQueryResult3D* result) {
    const uint64_t index = atomic_fetch_add_explicit(
        &g_surface_diagnostic_count, 1u, memory_order_relaxed);
    RuntimeCausticPhotonSurfaceDiagnosticSample3D* sample;
    uint64_t histogram_index;
    if (!hit || !result) return;
    histogram_index = result->effectiveSampleCount < 4u
                          ? result->effectiveSampleCount
                          : 4u;
    atomic_fetch_add_explicit(&g_surface_query_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(
        &g_surface_effective_sample_count_sum,
        result->effectiveSampleCount,
        memory_order_relaxed);
    atomic_fetch_add_explicit(
        &g_surface_effective_sample_histogram[histogram_index],
        1u,
        memory_order_relaxed);
    if (result->effectiveSampleCount > 0u) {
        atomic_fetch_add_explicit(
            &g_surface_positive_query_count, 1u, memory_order_relaxed);
        if (result->undersampled) {
            atomic_fetch_add_explicit(
                &g_surface_undersampled_positive_query_count,
                1u,
                memory_order_relaxed);
        }
    }
    if (
        index >= RUNTIME_CAUSTIC_PHOTON_SURFACE_DIAGNOSTIC_CAPACITY) {
        return;
    }
    sample = &g_surface_diagnostic_samples[index];
    sample->position = hit->position;
    sample->supportRadius = result->supportRadius;
    sample->densityEstimate = result->densityEstimate;
    sample->nearestDistance = result->nearestDistance;
    sample->nearestContributionDistance = result->nearestContributionDistance;
    sample->farthestContributionDistance = result->farthestContributionDistance;
    sample->candidateCount = result->candidateCount;
    sample->effectiveSampleCount = result->effectiveSampleCount;
    sample->neighborLimit = result->estimator.neighborLimit;
    sample->radiusRejectCount = result->radiusRejectCount;
    sample->normalRejectCount = result->normalRejectCount;
    sample->incidentHemisphereRejectCount =
        result->incidentHemisphereRejectCount;
    sample->receiverRejectCount = result->receiverRejectCount;
    sample->receiverObjectRejectCount = result->receiverObjectRejectCount;
    sample->receiverMaterialRejectCount = result->receiverMaterialRejectCount;
    sample->receiverExactTriangleRejectCount =
        result->receiverExactTriangleRejectCount;
    sample->physicalFlux = result->physicalFlux;
    sample->queryHit = result->hit;
    sample->undersampled = result->undersampled;
}

const char* RuntimeCausticPhotonConsumer3D_Label(RuntimeCausticPhotonConsumer3D consumer) {
    return consumer == RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP
               ? "direct_map"
               : "cache_bridge";
}

RuntimeCausticPhotonConsumer3D RuntimeCausticPhotonConsumer3D_FromLabel(
    const char* label) {
    return label && (strcmp(label, "cache_bridge") == 0 ||
                     strcmp(label, "compatibility_cache") == 0)
               ? RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE
               : RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP;
}

void RuntimeCausticPhotonDirectConsumer3D_Bind(
    RuntimeCausticPhotonConsumer3D consumer,
    const RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonIntegrationSettings3D* settings) {
    g_consumer = consumer;
    g_surface_map = surface_map;
    g_surface_sample_budget =
        settings && settings->sampleBudget > 0
            ? (uint64_t)settings->sampleBudget
            : 4096u;
    g_beam_map = beam_map;
    g_surface_radiance_scale =
        settings && settings->surfaceRadianceScale >= 0.0
            ? settings->surfaceRadianceScale
            : 1.0;
    g_surface_estimator =
        settings ? settings->surfaceEstimator
                 : RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    g_surface_gather_neighbor_count =
        settings && settings->surfaceGatherNeighborCount >= 4u
            ? settings->surfaceGatherNeighborCount
            : 8u;
    g_surface_query_radius =
        settings && settings->surfaceQueryRadius > 0.0
            ? settings->surfaceQueryRadius
            : 0.10;
    g_surface_gather_max_radius =
        settings && settings->surfaceGatherMaxRadius > 0.0
            ? settings->surfaceGatherMaxRadius
            : 0.20;
    g_surface_receiver_scene_object_index =
        settings ? settings->surfaceReceiverSceneObjectIndex : -1;
    g_surface_path_filter =
        settings ? settings->surfacePathFilter
                 : RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL;
    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(
        &g_volume_beam_estimator_settings);
    if (settings) {
        if (settings->volumeQueryRadius > 0.0) {
            g_volume_beam_estimator_settings.queryRadius =
                settings->volumeQueryRadius;
        }
        g_volume_beam_estimator_settings.mediumId = settings->volumeMediumId;
        if (settings->volumeScatteringCoefficient >= 0.0) {
            g_volume_beam_estimator_settings.scatteringCoefficient =
                settings->volumeScatteringCoefficient;
        }
        if (settings->volumeExtinctionCoefficient >= 0.0) {
            g_volume_beam_estimator_settings.extinctionCoefficient =
                settings->volumeExtinctionCoefficient;
        }
        if (settings->volumePhaseAnisotropy >= -0.95 &&
            settings->volumePhaseAnisotropy <= 0.95) {
            g_volume_beam_estimator_settings.phaseAnisotropy =
                settings->volumePhaseAnisotropy;
        }
    }
    g_neighbor_limit = settings &&
                               settings->qualityTier ==
                                   RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL
                           ? 64u
                           : settings &&
                                     settings->qualityTier ==
                                         RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION
                                 ? 32u
                                 : 16u;
}

void RuntimeCausticPhotonDirectConsumer3D_Reset(void) {
    g_consumer = RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE;
    g_surface_map = NULL;
    g_surface_sample_budget = 4096u;
    g_beam_map = NULL;
    g_neighbor_limit = 16u;
    g_surface_estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    g_surface_gather_neighbor_count = 8u;
    g_surface_radiance_scale = 1.0;
    g_surface_query_radius = 0.10;
    g_surface_gather_max_radius = 0.20;
    g_surface_receiver_scene_object_index = -1;
    g_surface_path_filter = RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL;
    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(
        &g_volume_beam_estimator_settings);
    photon_direct_bsdf_lock();
    memset(&g_receiver_bsdf_readback, 0, sizeof(g_receiver_bsdf_readback));
    photon_direct_bsdf_unlock();
    memset(g_surface_diagnostic_samples, 0, sizeof(g_surface_diagnostic_samples));
    atomic_store_explicit(&g_surface_diagnostic_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_surface_query_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_surface_positive_query_count, 0u, memory_order_relaxed);
    atomic_store_explicit(
        &g_surface_undersampled_positive_query_count, 0u, memory_order_relaxed);
    atomic_store_explicit(
        &g_surface_effective_sample_count_sum, 0u, memory_order_relaxed);
    for (uint64_t i = 0u; i < 5u; ++i) {
        atomic_store_explicit(
            &g_surface_effective_sample_histogram[i], 0u, memory_order_relaxed);
    }
    memset(g_volume_diagnostic_samples, 0, sizeof(g_volume_diagnostic_samples));
    atomic_store_explicit(&g_volume_diagnostic_count, 0u, memory_order_relaxed);
}

bool RuntimeCausticPhotonDirectConsumer3D_Active(void) {
    return g_consumer == RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP;
}

double RuntimeCausticPhotonDirectConsumer3D_VolumeQueryRadius(void) {
    return g_volume_beam_estimator_settings.queryRadius > 0.0
               ? g_volume_beam_estimator_settings.queryRadius
               : 0.10;
}

void RuntimeCausticPhotonDirectConsumer3D_SnapshotReceiverBsdf(
    RuntimeCausticPhotonReceiverBsdfReadback3D* out_readback) {
    if (!out_readback) return;
    photon_direct_bsdf_lock();
    *out_readback = g_receiver_bsdf_readback;
    photon_direct_bsdf_unlock();
}

uint64_t RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticCount(void) {
    const uint64_t count = atomic_load_explicit(
        &g_surface_diagnostic_count, memory_order_relaxed);
    return count < RUNTIME_CAUSTIC_PHOTON_SURFACE_DIAGNOSTIC_CAPACITY
               ? count
               : RUNTIME_CAUSTIC_PHOTON_SURFACE_DIAGNOSTIC_CAPACITY;
}

void RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(
    RuntimeCausticPhotonSurfaceDiagnosticAggregate3D* out_aggregate) {
    if (!out_aggregate) return;
    memset(out_aggregate, 0, sizeof(*out_aggregate));
    out_aggregate->queryCount = atomic_load_explicit(
        &g_surface_query_count, memory_order_relaxed);
    out_aggregate->positiveQueryCount = atomic_load_explicit(
        &g_surface_positive_query_count, memory_order_relaxed);
    out_aggregate->undersampledPositiveQueryCount = atomic_load_explicit(
        &g_surface_undersampled_positive_query_count, memory_order_relaxed);
    out_aggregate->effectiveSampleCountSum = atomic_load_explicit(
        &g_surface_effective_sample_count_sum, memory_order_relaxed);
    for (uint64_t i = 0u; i < 5u; ++i) {
        out_aggregate->effectiveSampleHistogram[i] = atomic_load_explicit(
            &g_surface_effective_sample_histogram[i], memory_order_relaxed);
    }
}

bool RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticAt(
    uint64_t index,
    RuntimeCausticPhotonSurfaceDiagnosticSample3D* out_sample) {
    if (!out_sample ||
        index >= RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticCount()) {
        return false;
    }
    *out_sample = g_surface_diagnostic_samples[index];
    return true;
}

uint64_t RuntimeCausticPhotonDirectConsumer3D_VolumeDiagnosticCount(void) {
    const uint64_t count = atomic_load_explicit(
        &g_volume_diagnostic_count, memory_order_relaxed);
    return count < RUNTIME_CAUSTIC_PHOTON_VOLUME_DIAGNOSTIC_CAPACITY
               ? count
               : RUNTIME_CAUSTIC_PHOTON_VOLUME_DIAGNOSTIC_CAPACITY;
}

bool RuntimeCausticPhotonDirectConsumer3D_VolumeDiagnosticAt(
    uint64_t index,
    RuntimeCausticPhotonVolumeDiagnosticSample3D* out_sample) {
    if (!out_sample ||
        index >= RuntimeCausticPhotonDirectConsumer3D_VolumeDiagnosticCount()) {
        return false;
    }
    *out_sample = g_volume_diagnostic_samples[index];
    return true;
}

bool RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
    const HitInfo3D* hit,
    Vec3* out_radiance,
    RuntimeCausticPhotonMapQueryResult3D* out_readback) {
    return RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithReceiverArea(
        hit, 0.0, out_radiance, out_readback);
}

static bool photon_direct_sample_surface_map(
    const RuntimeCausticPhotonMap3D* surface_map,
    uint64_t surface_sample_budget,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* material,
    double receiver_area_m2,
    Vec3* out_radiance,
    RuntimeCausticPhotonMapQueryResult3D* out_readback) {
    RuntimeCausticPhotonMap3D query_map;
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonReceiverBsdfReadback3D receiver_bsdf;

    (void)receiver_area_m2;
    memset(&receiver_bsdf, 0, sizeof(receiver_bsdf));
    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (out_readback) memset(out_readback, 0, sizeof(*out_readback));
    if (!surface_map || !hit || !material || !out_radiance ||
        !RuntimeCausticPhotonMap3D_IsAllocated(surface_map)) {
        return false;
    }
    RuntimeCausticPhotonMap3D_DefaultQuery(&query);
    query.position = hit->position;
    query.normal = hit->normal;
    query.sceneObjectIndex = hit->sceneObjectIndex;
    query.primitiveIndex = hit->primitiveIndex;
    query.triangleIndex = hit->triangleIndex;
    query.materialId = material->materialId;
    query.radius =
        photon_direct_uses_neighbor_gather(g_surface_estimator)
            ? g_surface_gather_max_radius
            : g_surface_query_radius;
    query.receiverDomain = RUNTIME_CAUSTIC_PHOTON_RECEIVER_PATCH;
    query.estimator.estimator = g_surface_estimator;
    query.candidateLimit = 0u;
    if (g_surface_estimator ==
        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER) {
        query.estimator.neighborLimit =
            RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
                surface_sample_budget,
                RUNTIME_CAUSTIC_PHOTON_SURFACE_GATHER_REFERENCE_BUDGET,
                g_surface_gather_neighbor_count,
                4u,
                RUNTIME_CAUSTIC_PHOTON_SURFACE_MAX_CONVERGENT_NEIGHBORS);
    } else if (g_surface_estimator ==
               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER) {
        query.estimator.neighborLimit =
            RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
                surface_map->recordCount,
                g_surface_gather_neighbor_count,
                RUNTIME_CAUSTIC_PHOTON_SURFACE_MAX_CONVERGENT_NEIGHBORS);
    } else if (g_surface_estimator ==
               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER) {
        query.estimator.neighborLimit = g_surface_gather_neighbor_count;
    } else {
        query.estimator.neighborLimit =
            RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
                surface_map->recordCount,
                g_neighbor_limit,
                RUNTIME_CAUSTIC_PHOTON_SURFACE_MAX_CONVERGENT_NEIGHBORS);
    }
    query.estimator.minimumEffectiveSamples = 4u;
    query_map = *surface_map;
    if (!RuntimeCausticPhotonMap3D_Query(&query_map, &query, &result)) {
        photon_direct_record_surface_diagnostic(hit, &result);
        if (out_readback) *out_readback = result;
        return false;
    }
    result.physicalFlux = photon_direct_selected_surface_flux(&result);
    result.flux = result.physicalFlux;
    result.displayFlux = vec3_scale(result.physicalFlux, result.displayGain);
    result.hit = result.physicalFlux.x > 0.0 || result.physicalFlux.y > 0.0 ||
                 result.physicalFlux.z > 0.0;
    if (!result.hit) {
        photon_direct_record_surface_diagnostic(hit, &result);
        if (out_readback) *out_readback = result;
        return false;
    }
    photon_direct_record_surface_diagnostic(hit, &result);
    if (!RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
            material, &result, out_radiance, &receiver_bsdf)) {
        if (out_readback) *out_readback = result;
        return false;
    }
    photon_direct_bsdf_lock();
    g_receiver_bsdf_readback = receiver_bsdf;
    photon_direct_bsdf_unlock();
    *out_radiance = vec3_scale(*out_radiance, g_surface_radiance_scale);
    if (out_readback) *out_readback = result;
    return true;
}

bool RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithReceiverArea(
    const HitInfo3D* hit,
    double receiver_area_m2,
    Vec3* out_radiance,
    RuntimeCausticPhotonMapQueryResult3D* out_readback) {
    RuntimeMaterialPayload3D material;

    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (out_readback) memset(out_readback, 0, sizeof(*out_readback));
    if (!RuntimeCausticPhotonDirectConsumer3D_Active() || !hit || !out_radiance) {
        return false;
    }
    if (g_surface_receiver_scene_object_index >= 0 &&
        hit->sceneObjectIndex != g_surface_receiver_scene_object_index) {
        return false;
    }
    RuntimeMaterialPayload3D_Reset(&material);
    if (!RuntimeMaterialPayload3D_ResolveFromHit(hit, &material) || !material.valid) {
        material.valid = true;
        material.materialId = -1;
        material.baseColorR = 1.0;
        material.baseColorG = 1.0;
        material.baseColorB = 1.0;
        material.bsdf.diffuseWeight = 1.0;
        material.bsdf.roughness = 0.5;
    }
    return photon_direct_sample_surface_map(
        g_surface_map, g_surface_sample_budget, hit, &material,
        receiver_area_m2, out_radiance, out_readback);
}

bool RuntimeCausticPhotonDirectConsumer3D_SampleBeam(
    Vec3 position,
    Vec3 direction,
    double radius,
    int medium_id,
    Vec3* out_radiance,
    RuntimeCausticBeamMapQueryResult3D* out_readback) {
    RuntimeCausticBeamMap3D query_map;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMapQueryResult3D result;

    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (out_readback) memset(out_readback, 0, sizeof(*out_readback));
    if (!RuntimeCausticPhotonDirectConsumer3D_Active() || !out_radiance ||
        !g_beam_map || !RuntimeCausticBeamMap3D_IsAllocated(g_beam_map)) {
        return false;
    }
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.position = position;
    query.direction = direction;
    query.radius = radius > 0.0
                       ? radius
                       : g_volume_beam_estimator_settings.queryRadius;
    query.mediumId = medium_id >= 0
                         ? medium_id
                         : g_volume_beam_estimator_settings.mediumId;
    query.requireMediumId = true;
    query.segmentStage = g_volume_beam_estimator_settings.segmentStage;
    query.requireSegmentStage = true;
    query.minDirectionDot = 0.0;
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    query.estimator.minimumEffectiveSamples = 1u;
    query_map = *g_beam_map;
    if (!RuntimeCausticBeamMap3D_Query(&query_map, &query, &result)) {
        if (out_readback) *out_readback = result;
        return false;
    }
    *out_radiance = result.physicalFlux;
    if (out_readback) *out_readback = result;
    return true;
}

bool RuntimeCausticPhotonDirectConsumer3D_EvaluateBeamRadiance(
    const RuntimeCausticBeamMapQueryResult3D* beam_query,
    Vec3 view_ray_direction,
    double medium_density,
    double camera_transmittance,
    double step_length,
    Vec3* out_radiance,
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D* out_readback) {
    RuntimeCausticPhotonVolumeBeamEstimatorInput3D input;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D readback;

    if (out_radiance) *out_radiance = vec3(0.0, 0.0, 0.0);
    if (out_readback) memset(out_readback, 0, sizeof(*out_readback));
    if (!beam_query || !beam_query->hit || !out_radiance) return false;
    memset(&input, 0, sizeof(input));
    input.beamFluxDensity = beam_query->physicalFlux;
    input.beamDirection = beam_query->meanBeamDirection;
    input.viewToCameraDirection = vec3_scale(view_ray_direction, -1.0);
    input.beamDistance = beam_query->meanBeamDistance;
    input.mediumDensity = medium_density;
    input.cameraTransmittance = camera_transmittance;
    input.stepLength = step_length;
    if (!RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
            &g_volume_beam_estimator_settings, &input, &readback)) {
        photon_direct_record_volume_diagnostic(beam_query, &input, &readback);
        if (out_readback) *out_readback = readback;
        return false;
    }
    photon_direct_record_volume_diagnostic(beam_query, &input, &readback);
    *out_radiance = readback.radiance;
    if (out_readback) *out_readback = readback;
    return true;
}
