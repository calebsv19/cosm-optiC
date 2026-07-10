#include "test_runtime_caustic_photon_integration_3d.h"

#include "render/runtime_caustic_photon_integration_3d.h"
#include "test_support.h"

static RuntimeCausticPhotonMapRecord3D test_surface_record(void) {
    RuntimeCausticPhotonMapRecord3D record = {0};
    record.photonId = 1u;
    record.position = vec3(0.0, 0.0, 0.0);
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(2.0, 1.0, 0.5);
    record.pathPdf = 0.5;
    record.queryRadius = 0.20;
    record.sceneObjectIndex = 9;
    record.primitiveIndex = 8;
    record.triangleIndex = 7;
    return record;
}

static RuntimeCausticPhotonVolumeBeamSegment3D test_volume_segment(void) {
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    segment.photonId = 2u;
    segment.start = vec3(0.0, 0.0, 0.0);
    segment.end = vec3(0.0, 0.0, 2.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 0.5, 0.25);
    segment.radiusStart = 0.20;
    segment.radiusEnd = 0.30;
    segment.transmittance = 0.8;
    segment.densityWeight = 0.5;
    segment.mediumId = 4;
    return segment;
}

static int test_runtime_caustic_photon_integration_modes_and_settings(void) {
    RuntimeCausticPhotonIntegrationSettings3D integration;
    RuntimeCausticSettings3D caustic;

    assert_true("runtime_caustic_product_mode_label_off",
                RuntimeCausticProductMode3D_FromLabel("off") ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_OFF);
    assert_true("runtime_caustic_product_mode_label_reference",
                RuntimeCausticProductMode3D_FromLabel("reference") ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE);
    assert_true("runtime_caustic_product_mode_label_production",
                RuntimeCausticProductMode3D_FromLabel("production") ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION);

    RuntimeCausticPhotonIntegration3D_DefaultSettings(&integration);
    RuntimeCausticSettings3D_Default(&caustic);
    assert_true("runtime_caustic_photon_integration_default_reference",
                integration.productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE &&
                    !integration.renderContributionEnabled);
    RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(&integration, &caustic);
    assert_true("runtime_caustic_photon_integration_reference_route",
                caustic.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
                    caustic.transportEngine ==
                        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT);

    integration.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    integration.surfaceQueryEnabled = true;
    integration.volumeQueryEnabled = true;
    integration.sampleBudget = 100000;
    integration.maxPathDepth = 100;
    RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(&integration, &caustic);
    assert_true("runtime_caustic_photon_integration_production_route",
                caustic.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
                    caustic.transportEngine ==
                        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP &&
                    caustic.surfaceCacheEnabled &&
                    caustic.volumeCacheEnabled);
    assert_true("runtime_caustic_photon_integration_cost_bounds",
                caustic.sampleBudget == 4096 && caustic.maxPathDepth == 16);

    integration.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_OFF;
    RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(&integration, &caustic);
    assert_true("runtime_caustic_photon_integration_off_route",
                caustic.mode == RUNTIME_CAUSTIC_MODE_OFF &&
                    !caustic.surfaceCacheEnabled &&
                    !caustic.volumeCacheEnabled);
    return 0;
}

static int test_runtime_caustic_photon_integration_queries_maps(void) {
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationResult3D result;
    RuntimeCausticPhotonMapRecord3D surface = test_surface_record();
    RuntimeCausticPhotonVolumeBeamSegment3D volume = test_volume_segment();

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.volumeQueryEnabled = true;

    assert_true("runtime_caustic_photon_integration_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 4u));
    assert_true("runtime_caustic_photon_integration_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 4u));
    assert_true("runtime_caustic_photon_integration_surface_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface));
    assert_true("runtime_caustic_photon_integration_beam_store",
                RuntimeCausticBeamMap3D_StoreSegment(&beam_map, &volume));

    query.surface.position = vec3(0.02, 0.0, 0.0);
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;
    query.surface.sceneObjectIndex = 9;
    query.surface.primitiveIndex = 8;
    query.surface.triangleIndex = 7;
    query.volume.position = vec3(0.05, 0.0, 1.0);
    query.volume.direction = vec3(0.0, 0.0, 1.0);
    query.volume.radius = 0.25;
    query.volume.mediumId = 4;

    assert_true("runtime_caustic_photon_integration_query_hit",
                RuntimeCausticPhotonIntegration3D_Query(&surface_map,
                                                        &beam_map,
                                                        &settings,
                                                        &query,
                                                        &result));
    assert_true("runtime_caustic_photon_integration_query_route",
                result.route ==
                    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY);
    assert_true("runtime_caustic_photon_integration_query_components",
                result.surfaceHit && result.volumeHit &&
                    result.surfaceContributingCount == 1u &&
                    result.volumeContributingCount == 1u);
    assert_true("runtime_caustic_photon_integration_query_combined_flux",
                result.combinedFlux.x > result.surfaceFlux.x &&
                    result.combinedFlux.y > result.surfaceFlux.y);
    assert_true("runtime_caustic_photon_integration_render_suppressed",
                result.renderContributionSuppressed &&
                    !result.renderContributionEnabled);

    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    return 0;
}

static int test_runtime_caustic_photon_integration_suppresses_contribution_by_default(void) {
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationResult3D query_result = {0};
    RuntimeCausticPhotonContribution3D contribution;

    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    query_result.route = RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY;
    query_result.surfaceHit = true;
    query_result.surfaceFlux = vec3(1.0, 1.0, 1.0);
    query_result.surfaceContributingCount = 1u;
    query.surface.position = vec3(0.0, 0.0, 0.0);
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;

    assert_true("runtime_caustic_photon_integration_contribution_default_suppressed",
                !RuntimeCausticPhotonIntegration3D_BuildContribution(&settings,
                                                                     &query,
                                                                     &query_result,
                                                                     &contribution));
    assert_true("runtime_caustic_photon_integration_contribution_suppressed_readback",
                contribution.suppressed && !contribution.eligible &&
                    !contribution.hasSurfaceContribution);
    return 0;
}

static int test_runtime_caustic_photon_integration_deposits_gated_contribution(void) {
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCacheDiagnostics3D surface_diag;
    RuntimeCausticVolumeCacheDiagnostics3D volume_diag;
    RuntimeVolumeGrid3D grid;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationResult3D query_result;
    RuntimeCausticPhotonContribution3D contribution;
    RuntimeCausticPhotonContributionDepositResult3D deposit_result;
    RuntimeCausticPhotonMapRecord3D surface = test_surface_record();
    RuntimeCausticPhotonVolumeBeamSegment3D volume = test_volume_segment();

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeVolumeGrid3D_Reset(&grid);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.volumeQueryEnabled = true;
    settings.renderContributionEnabled = true;

    assert_true("runtime_caustic_photon_integration_deposit_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 4u));
    assert_true("runtime_caustic_photon_integration_deposit_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 4u));
    assert_true("runtime_caustic_photon_integration_deposit_surface_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface));
    assert_true("runtime_caustic_photon_integration_deposit_beam_store",
                RuntimeCausticBeamMap3D_StoreSegment(&beam_map, &volume));
    assert_true("runtime_caustic_photon_integration_surface_cache_alloc",
                RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, 4u));
    assert_true("runtime_caustic_photon_integration_volume_grid_config",
                RuntimeVolumeGrid3D_Configure(&grid,
                                              1u,
                                              4u,
                                              4u,
                                              4u,
                                              0.0,
                                              0u,
                                              0.0,
                                              vec3(-1.0, -1.0, -1.0),
                                              1.0,
                                              vec3(0.0, 1.0, 0.0),
                                              0u));
    assert_true("runtime_caustic_photon_integration_volume_cache_alloc",
                RuntimeCausticVolumeCache3D_Allocate(&volume_cache, &grid));

    query.surface.position = vec3(0.02, 0.0, 0.0);
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;
    query.surface.sceneObjectIndex = 9;
    query.surface.primitiveIndex = 8;
    query.surface.triangleIndex = 7;
    query.volume.position = vec3(0.05, 0.0, 1.0);
    query.volume.direction = vec3(0.0, 0.0, 1.0);
    query.volume.radius = 1.0;
    query.volume.mediumId = 4;

    assert_true("runtime_caustic_photon_integration_deposit_query_hit",
                RuntimeCausticPhotonIntegration3D_Query(&surface_map,
                                                        &beam_map,
                                                        &settings,
                                                        &query,
                                                        &query_result));
    assert_true("runtime_caustic_photon_integration_build_contribution",
                RuntimeCausticPhotonIntegration3D_BuildContribution(&settings,
                                                                    &query,
                                                                    &query_result,
                                                                    &contribution));
    assert_true("runtime_caustic_photon_integration_contribution_fields",
                contribution.eligible && !contribution.suppressed &&
                    contribution.hasSurfaceContribution &&
                    contribution.hasVolumeContribution &&
                    contribution.combinedRadiance.x > contribution.surfaceRadiance.x);
    assert_true("runtime_caustic_photon_integration_deposit_contribution",
                RuntimeCausticPhotonIntegration3D_DepositContributionToCaches(
                    &surface_cache,
                    &volume_cache,
                    &contribution,
                    &deposit_result));
    assert_true("runtime_caustic_photon_integration_deposit_result",
                deposit_result.attempted && deposit_result.surfaceDeposited &&
                    deposit_result.volumeDeposited);

    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&surface_cache, &surface_diag);
    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&volume_cache, &volume_diag);
    assert_true("runtime_caustic_photon_integration_surface_cache_written",
                surface_diag.recordCount == 1u &&
                    surface_diag.depositAcceptedCount == 1u);
    assert_true("runtime_caustic_photon_integration_volume_cache_written",
                volume_diag.depositAcceptedCount == 1u &&
                    volume_diag.footprintDepositCount == 1u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    return 0;
}

static int test_runtime_caustic_photon_integration_reference_does_not_query_maps(void) {
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonIntegrationResult3D result;
    RuntimeCausticPhotonMapRecord3D surface = test_surface_record();
    RuntimeCausticPhotonVolumeBeamSegment3D volume = test_volume_segment();

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);

    assert_true("runtime_caustic_photon_integration_reference_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 4u));
    assert_true("runtime_caustic_photon_integration_reference_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 4u));
    assert_true("runtime_caustic_photon_integration_reference_surface_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface));
    assert_true("runtime_caustic_photon_integration_reference_beam_store",
                RuntimeCausticBeamMap3D_StoreSegment(&beam_map, &volume));

    query.surface.position = surface.position;
    query.volume.position = vec3(0.0, 0.0, 1.0);
    assert_true("runtime_caustic_photon_integration_reference_no_query",
                !RuntimeCausticPhotonIntegration3D_Query(&surface_map,
                                                         &beam_map,
                                                         &settings,
                                                         &query,
                                                         &result));
    assert_true("runtime_caustic_photon_integration_reference_route_result",
                result.route ==
                    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE &&
                    !result.surfaceHit &&
                    !result.volumeHit &&
                    result.combinedFlux.x == 0.0);

    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    return 0;
}

int run_test_runtime_caustic_photon_integration_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_integration_modes_and_settings();
    failures += test_runtime_caustic_photon_integration_queries_maps();
    failures +=
        test_runtime_caustic_photon_integration_suppresses_contribution_by_default();
    failures += test_runtime_caustic_photon_integration_deposits_gated_contribution();
    failures += test_runtime_caustic_photon_integration_reference_does_not_query_maps();
    return failures;
}
