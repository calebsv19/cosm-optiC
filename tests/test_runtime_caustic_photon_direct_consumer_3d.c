#include "test_runtime_caustic_photon_direct_consumer_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "test_support.h"

static int test_runtime_caustic_photon_direct_consumer_surface_and_beam(void) {
    RuntimeCausticPhotonMap3D surface;
    RuntimeCausticBeamMap3D beam;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticPhotonIntegrationSettings3D settings;
    HitInfo3D hit;
    Vec3 radiance;
    RuntimeCausticPhotonMapQueryResult3D surface_query;
    RuntimeCausticPhotonSurfaceDiagnosticSample3D surface_diagnostic;
    RuntimeCausticPhotonSurfaceDiagnosticAggregate3D surface_aggregate;
    RuntimeCausticPhotonReceiverBsdfReadback3D receiver_bsdf;
    double unscaled_surface_radiance;
    double direct_surface_radiance;
    double multipath_surface_radiance;
    double unclassified_surface_radiance;

    RuntimeCausticPhotonMap3D_Init(&surface);
    RuntimeCausticBeamMap3D_Init(&beam);
    assert_true("runtime_caustic_photon_direct_surface_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&surface, 4u));
    assert_true("runtime_caustic_photon_direct_beam_allocate",
                RuntimeCausticBeamMap3D_Allocate(&beam, 2u));
    record.photonId = 1u;
    record.position = vec3(0.0, 0.0, 0.0);
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0, 0.5, 0.25);
    record.pathPdf = 1.0;
    record.queryRadius = 0.2;
    record.sceneObjectIndex = 3;
    record.primitiveIndex = 4;
    record.triangleIndex = 5;
    for (uint64_t i = 0u; i < 4u; ++i) {
        record.photonId = i + 1u;
        record.position = vec3((double)i * 0.01, 0.0, 0.0);
        record.primitiveIndex = 4 + (int)(i % 2u);
        record.triangleIndex = 5 + (int)(i % 2u);
        record.provenance.surfacePathClass =
            i < 2u
                ? RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_DIRECT_TWO_INTERFACE
                : i == 2u ? RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_MULTIPATH
                          : RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_UNCLASSIFIED;
        assert_true("runtime_caustic_photon_direct_surface_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&surface, &record));
    }
    segment.photonId = 1u;
    segment.start = vec3(0.0, 0.0, 0.0);
    segment.end = vec3(0.0, 0.0, 1.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(0.5, 0.25, 0.125);
    segment.radiusStart = 0.2;
    segment.radiusEnd = 0.2;
    segment.transmittance = 0.75;
    segment.densityWeight = 0.5;
    segment.mediumId = 0;
    segment.provenance.originalMediumId = 0;
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    assert_true("runtime_caustic_photon_direct_beam_store",
                RuntimeCausticBeamMap3D_StoreSegment(&beam, &segment));

    HitInfo3D_Reset(&hit);
    hit.position = vec3(0.01, 0.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.sceneObjectIndex = 3;
    hit.primitiveIndex = 4;
    hit.triangleIndex = 5;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    assert_true("runtime_caustic_photon_direct_default_consumer",
                settings.consumer == RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP);
    assert_true("runtime_caustic_photon_direct_consumer_labels",
                RuntimeCausticPhotonConsumer3D_FromLabel("cache_bridge") ==
                        RUNTIME_CAUSTIC_PHOTON_CONSUMER_CACHE_BRIDGE &&
                    RuntimeCausticPhotonConsumer3D_FromLabel("direct_map") ==
                        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP &&
                    RuntimeCausticPhotonConsumer3D_FromLabel("invalid") ==
                        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP);
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_direct_surface_sample",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) && radiance.x > 0.0);
    assert_true("runtime_caustic_photon_surface_class_partition",
                surface_query.directTwoInterfaceContributingCount == 2u &&
                    surface_query.multipathContributingCount == 1u &&
                    surface_query.unclassifiedContributingCount == 1u &&
                    surface_query.contributingCount == 4u &&
                    fabs(surface_query.physicalFlux.x -
                         surface_query.directTwoInterfacePhysicalFlux.x -
                         surface_query.multipathPhysicalFlux.x -
                         surface_query.unclassifiedPhysicalFlux.x) < 1.0e-12);
    assert_true("runtime_caustic_photon_surface_diagnostic_query_shape",
                RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticAt(
                    0u, &surface_diagnostic) &&
                    surface_diagnostic.candidateCount ==
                        surface_query.candidateCount &&
                    surface_diagnostic.effectiveSampleCount ==
                        surface_query.effectiveSampleCount &&
                    surface_diagnostic.radiusRejectCount ==
                        surface_query.radiusRejectCount &&
                    surface_diagnostic.normalRejectCount ==
                        surface_query.normalRejectCount &&
                    surface_diagnostic.incidentHemisphereRejectCount ==
                        surface_query.incidentHemisphereRejectCount &&
                    surface_diagnostic.receiverRejectCount ==
                        surface_query.receiverRejectCount &&
                    fabs(surface_diagnostic.nearestContributionDistance -
                         surface_query.nearestContributionDistance) < 1.0e-12 &&
                    fabs(surface_diagnostic.farthestContributionDistance -
                         surface_query.farthestContributionDistance) < 1.0e-12);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(
        &surface_aggregate);
    assert_true("runtime_caustic_photon_surface_diagnostic_aggregate",
                surface_aggregate.queryCount == 1u &&
                    surface_aggregate.positiveQueryCount == 1u &&
                    surface_aggregate.undersampledPositiveQueryCount == 0u &&
                    surface_aggregate.effectiveSampleCountSum == 4u &&
                    surface_aggregate.effectiveSampleHistogram[4] == 1u);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotReceiverBsdf(&receiver_bsdf);
    assert_true("runtime_caustic_photon_surface_flux_compensated_once",
                surface_query.storedFluxAlreadyPdfCompensated &&
                    fabs(receiver_bsdf.inputPhysicalFlux.x -
                         surface_query.physicalFlux.x) < 1.0e-12);
    assert_close("runtime_caustic_photon_surface_lambertian_once",
                 receiver_bsdf.outputRadiance.x,
                 surface_query.physicalFlux.x / M_PI,
                 1.0e-12);
    assert_close("runtime_caustic_photon_surface_scale_once",
                 radiance.x,
                 receiver_bsdf.outputRadiance.x * settings.surfaceRadianceScale,
                 1.0e-12);
    unscaled_surface_radiance = radiance.x;
    settings.surfacePathFilter =
        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_direct_class_surface_sample",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) && radiance.x > 0.0);
    direct_surface_radiance = radiance.x;
    settings.surfacePathFilter =
        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_multipath_class_surface_sample",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) && radiance.x > 0.0);
    multipath_surface_radiance = radiance.x;
    settings.surfacePathFilter =
        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_UNCLASSIFIED;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_unclassified_surface_sample",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) && radiance.x > 0.0);
    unclassified_surface_radiance = radiance.x;
    assert_close("runtime_caustic_photon_surface_class_radiance_reconciles",
                 direct_surface_radiance + multipath_surface_radiance +
                     unclassified_surface_radiance,
                 unscaled_surface_radiance,
                 1.0e-12);
    assert_true("runtime_caustic_photon_surface_filter_labels",
                RuntimeCausticPhotonSurfacePathFilter3D_FromLabel("direct") ==
                        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE &&
                    RuntimeCausticPhotonSurfacePathFilter3D_FromLabel("multipath") ==
                        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH &&
                    RuntimeCausticPhotonSurfacePathFilter3D_FromLabel("bad") ==
                        RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL);
    settings.surfacePathFilter = RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL;
    settings.surfaceReceiverSceneObjectIndex = 7;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_direct_surface_receiver_filter",
                !RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) &&
                    radiance.x == 0.0 && radiance.y == 0.0 && radiance.z == 0.0);
    settings.surfaceReceiverSceneObjectIndex = 3;
    settings.surfaceRadianceScale = 0.25;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_direct_surface_scale",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) &&
                    fabs(radiance.x - unscaled_surface_radiance * 0.25) < 1.0e-9);
    settings.surfaceRadianceScale = 1.0;
    settings.surfaceEstimator =
        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER;
    settings.surfaceGatherNeighborCount = 8u;
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, &beam, &settings);
    assert_true("runtime_caustic_photon_direct_population_scaled_gather",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &surface_query) && radiance.x > 0.0 &&
                    surface_query.estimator.estimator ==
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER &&
                    surface_query.estimator.neighborLimit == surface.recordCount &&
                    strcmp(surface_query.estimatorLabel,
                           "population_scaled_gather") == 0);
    assert_true("runtime_caustic_photon_direct_beam_sample",
                RuntimeCausticPhotonDirectConsumer3D_SampleBeam(
                    vec3(0.01, 0.0, 0.5),
                    vec3(0.0, 0.0, 1.0),
                    0.0,
                    -1,
                    &radiance,
                    NULL) &&
                    radiance.x > 0.0);
    RuntimeCausticPhotonDirectConsumer3D_Reset();
    assert_true("runtime_caustic_photon_direct_reset",
                !RuntimeCausticPhotonDirectConsumer3D_Active() &&
                    !RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                        &hit, &radiance, NULL));
    RuntimeCausticBeamMap3D_Free(&beam);
    RuntimeCausticPhotonMap3D_Free(&surface);
    return 0;
}

static int test_runtime_caustic_photon_direct_consumer_ignores_insertion_prefix(void) {
    RuntimeCausticPhotonMap3D surface;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonMapQueryResult3D query;
    HitInfo3D hit;
    Vec3 radiance;

    RuntimeCausticPhotonMap3D_Init(&surface);
    assert_true("runtime_caustic_photon_direct_unbiased_prefix_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&surface, 4100u));
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0 / 4100.0, 1.0 / 4100.0, 1.0 / 4100.0);
    record.pathPdf = 1.0;
    record.queryRadius = 0.2;
    record.sceneObjectIndex = 3;
    record.primitiveIndex = 4;
    record.triangleIndex = 5;
    for (uint64_t i = 0u; i < 4096u; ++i) {
        record.photonId = i + 1u;
        record.position = vec3(10.0 + (double)i, 0.0, 0.0);
        assert_true("runtime_caustic_photon_direct_unbiased_prefix_far_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&surface, &record));
    }
    for (uint64_t i = 0u; i < 4u; ++i) {
        record.photonId = 4097u + i;
        record.position = vec3(0.01 * (double)i, 0.0, 0.0);
        assert_true("runtime_caustic_photon_direct_unbiased_prefix_near_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&surface, &record));
    }
    HitInfo3D_Reset(&hit);
    hit.position = vec3(0.01, 0.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.sceneObjectIndex = 3;
    hit.primitiveIndex = 4;
    hit.triangleIndex = 5;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonDirectConsumer3D_Bind(
        RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP, &surface, NULL, &settings);
    assert_true("runtime_caustic_photon_direct_unbiased_prefix_query",
                RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                    &hit, &radiance, &query) &&
                    query.effectiveSampleCount == 4u &&
                    !query.candidateLimitReached && radiance.x > 0.0);
    RuntimeCausticPhotonDirectConsumer3D_Reset();
    RuntimeCausticPhotonMap3D_Free(&surface);
    return 0;
}

int run_test_runtime_caustic_photon_direct_consumer_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_direct_consumer_surface_and_beam();
    failures +=
        test_runtime_caustic_photon_direct_consumer_ignores_insertion_prefix();
    return failures;
}
