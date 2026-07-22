#include "test_runtime_caustic_photon_integration_3d.h"

#include "config/config_manager.h"
#include "material/material_manager.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_photon_map_store_3d.h"
#include "render/runtime_caustic_photon_receiver_policy_3d.h"
#include "render/runtime_caustic_photon_scene_descriptor_3d.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static RuntimeLightSet3D test_population_light_set(void) {
    RuntimeLightSet3D set;
    RuntimeLightSource3D light;

    RuntimeLightSet3D_Init(&set);
    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "ppm_live_population_point");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI;
    light.position = vec3(0.0, 0.0, 0.0);
    light.color = vec3(1.0, 0.8, 0.6);
    light.intensity = 2.0;
    assert_true("runtime_caustic_photon_integration_population_append_light",
                RuntimeLightSet3D_Append(&set, &light, NULL));
    return set;
}

static RuntimeLightSet3D test_mesh_dielectric_population_light_set(void) {
    RuntimeLightSet3D set;
    RuntimeLightSource3D light;

    RuntimeLightSet3D_Init(&set);
    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "ppm_trace_population_point");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI;
    light.position = vec3(0.0, 2.0, 0.0);
    light.color = vec3(1.0, 0.85, 0.65);
    light.intensity = 3.0;
    assert_true("runtime_caustic_photon_integration_trace_append_light",
                RuntimeLightSet3D_Append(&set, &light, NULL));
    return set;
}

static RuntimeCausticLensShape3D test_mesh_dielectric_shape(void) {
    RuntimeCausticLensShape3D shape;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    shape.sceneObjectIndex = 31;
    shape.primitiveIndex = 41;
    shape.center = vec3(0.0, 0.0, 0.0);
    shape.axis = vec3(0.0, 1.0, 0.0);
    shape.radius = 0.75;
    shape.height = 0.30;
    shape.payload.opticalIor = 1.45;
    shape.payload.bsdf.ior = 1.45;
    shape.payload.baseColorR = 0.96;
    shape.payload.baseColorG = 1.0;
    shape.payload.baseColorB = 0.92;
    shape.payload.absorptionDistance = 8.0;
    return shape;
}

static RuntimeTriangle3D test_mesh_dielectric_entry_triangle(void) {
    RuntimeTriangle3D triangle = {0};

    triangle.p0 = vec3(-0.7, 0.0, -0.5);
    triangle.p1 = vec3(0.7, 0.0, -0.5);
    triangle.p2 = vec3(0.0, 0.0, 0.7);
    triangle.normal = vec3(0.0, 1.0, 0.0);
    triangle.sceneObjectIndex = 31;
    triangle.primitiveIndex = 41;
    triangle.localTriangleIndex = 5;
    return triangle;
}

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

static RuntimeCausticPhotonTrace3D test_trace_record(uint64_t photon_id) {
    RuntimeCausticPhotonTrace3D trace;

    memset(&trace, 0, sizeof(trace));
    trace.valid = true;
    trace.sample.photonId = photon_id;
    trace.sample.flux = vec3(1.0, 0.7, 0.3);
    trace.finalState.photonId = photon_id;
    trace.finalState.depth = 2u;
    trace.finalState.throughput = vec3(1.0, 0.7, 0.3);
    trace.finalState.pathPdf = 0.5;
    trace.postExitOrigin = vec3(0.0, 0.0, 0.0);
    trace.postExitDirection = vec3(0.0, 0.0, 1.0);
    trace.receiverPlaneT = 1.0;
    trace.receiverCrossing = vec3(0.0, 0.0, 1.0);
    return trace;
}

static bool test_volume_attachment_with_density(RuntimeVolumeAttachment3D* volume) {
    if (!volume) return false;
    RuntimeVolumeAttachment3D_Init(volume);
    if (!RuntimeVolumeGrid3D_Configure(&volume->grid,
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
                                       0u) ||
        !RuntimeVolumeAttachment3D_AllocateOwnedChannels(
            volume,
            RUNTIME_VOLUME_3D_CHANNEL_DENSITY)) {
        RuntimeVolumeAttachment3D_Free(volume);
        return false;
    }
    volume->enabled = true;
    volume->affectsLighting = true;
    for (uint64_t i = 0u; i < volume->grid.cellCount; ++i) {
        volume->channels.density[i] = 0.25f;
    }
    return true;
}

static int test_runtime_caustic_photon_receiver_policy_selects_primary_bucket(void) {
    RuntimeCausticPhotonSurfaceHit3D hits[4];
    RuntimeCausticPhotonReceiverBucket3D bucket;

    memset(hits, 0, sizeof(hits));
    hits[0].sceneObjectIndex = 5;
    hits[0].primitiveIndex = 20;
    hits[0].triangleIndex = 2;
    hits[1].sceneObjectIndex = 7;
    hits[1].primitiveIndex = 21;
    hits[1].triangleIndex = 3;
    hits[2].sceneObjectIndex = 5;
    hits[2].primitiveIndex = 20;
    hits[2].triangleIndex = 2;
    hits[3].sceneObjectIndex = 5;
    hits[3].primitiveIndex = 20;
    hits[3].triangleIndex = 2;

    memset(&bucket, 0, sizeof(bucket));
    assert_true("runtime_caustic_photon_receiver_policy_bucket_selected",
                RuntimeCausticPhotonReceiverPolicy3D_SelectPrimaryBucket(hits,
                                                                         4u,
                                                                         &bucket));
    assert_true("runtime_caustic_photon_receiver_policy_bucket_identity",
                bucket.valid &&
                    bucket.firstIndex == 0u &&
                    bucket.hitCount == 3u &&
                    bucket.bucketCount == 2u &&
                    bucket.competingHitCount == 1u &&
                    bucket.sceneObjectIndex == 5 &&
                    bucket.primitiveIndex == 20 &&
                    bucket.triangleIndex == 2);
    assert_true("runtime_caustic_photon_receiver_policy_bucket_match",
                RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(&hits[2],
                                                                      &bucket) &&
                    !RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(&hits[1],
                                                                           &bucket));
    return 0;
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
    assert_true("runtime_caustic_product_mode_canonical_labels",
                RuntimeCausticProductMode3D_FromLabel("reference_analytic") ==
                        RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC &&
                    RuntimeCausticProductMode3D_FromLabel("reference_transport") ==
                        RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT &&
                    RuntimeCausticProductMode3D_FromLabel("photon_map") ==
                        RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP);

    RuntimeCausticPhotonIntegration3D_DefaultSettings(&integration);
    RuntimeCausticSettings3D_Default(&caustic);
    assert_true("runtime_caustic_photon_integration_default_off",
                integration.productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_OFF &&
                    !integration.renderContributionEnabled);
    RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(&integration, &caustic);
    assert_true("runtime_caustic_photon_integration_default_off_route",
                caustic.mode == RUNTIME_CAUSTIC_MODE_OFF);
    integration.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC;
    RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(&integration, &caustic);
    assert_true("runtime_caustic_photon_integration_analytic_route",
                caustic.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC);
    integration.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT;
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
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT;
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

static int test_runtime_caustic_photon_integration_deposits_volume_beam_contribution(void) {
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeVolumeAttachment3D volume;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticPhotonBeamContributionReadback3D readback;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = test_volume_segment();
    RuntimeCausticVolumeCacheDiagnostics3D cache_diag;

    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.surfaceQueryEnabled = false;
    settings.volumeQueryEnabled = true;
    settings.renderContributionEnabled = true;
    settings.volumeQueryRadius = 1.0;

    assert_true("runtime_caustic_photon_integration_beam_contribution_map_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 4u));
    assert_true("runtime_caustic_photon_integration_beam_contribution_store",
                RuntimeCausticBeamMap3D_StoreSegment(&beam_map, &segment));
    assert_true("runtime_caustic_photon_integration_beam_contribution_volume",
                test_volume_attachment_with_density(&volume));
    assert_true("runtime_caustic_photon_integration_beam_contribution_cache_alloc",
                RuntimeCausticVolumeCache3D_AllocateFromVolume(&volume_cache, &volume));

    query.position = vec3(0.0, 0.0, 1.0);
    query.direction = vec3(0.0, 0.0, 1.0);
    query.radius = 1.0;
    query.mediumId = 4;
    query.requireMediumId = true;
    query.minDirectionDot = 0.90;
    assert_true("runtime_caustic_photon_integration_beam_contribution_deposit",
                RuntimeCausticPhotonIntegration3D_DepositVolumeContributionFromBeamMap(
                    &beam_map,
                    &volume_cache,
                    &volume,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_beam_contribution_readback",
                readback.attempted && readback.volumeSampleable && readback.beamMapAllocated &&
                    readback.queryHit && readback.contributionEligible &&
                    readback.volumeDeposited && readback.candidateCount == 1u &&
                    readback.contributingCount == 1u && readback.density > 0.0 &&
                    readback.transmittance > 0.0 && readback.transmittance < 1.0 &&
                    readback.volumeDepositAcceptedCount == 1u && readback.radiance.x > 0.0);
    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&volume_cache, &cache_diag);
    assert_true("runtime_caustic_photon_integration_beam_contribution_cache_written",
                cache_diag.depositAcceptedCount == 1u && cache_diag.nonZeroCellCount > 0u);

    query.mediumId = 99;
    assert_true("runtime_caustic_photon_integration_beam_contribution_medium_reject",
                !RuntimeCausticPhotonIntegration3D_DepositVolumeContributionFromBeamMap(
                    &beam_map,
                    &volume_cache,
                    &volume,
                    &settings,
                    &query,
                    &readback) &&
                    !readback.queryHit && readback.mediumRejectCount == 1u &&
                    readback.volumeDepositAcceptedCount == 0u);

    query.mediumId = 4;
    query.direction = vec3(1.0, 0.0, 0.0);
    assert_true("runtime_caustic_photon_integration_beam_irradiance_camera_independent",
                RuntimeCausticPhotonIntegration3D_DepositVolumeContributionFromBeamMap(
                    &beam_map,
                    &volume_cache,
                    &volume,
                    &settings,
                    &query,
                    &readback) &&
                    readback.queryHit && readback.directionRejectCount == 0u &&
                    readback.volumeDepositAcceptedCount == 1u);

    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeVolumeAttachment3D_Free(&volume);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    return 0;
}

static int test_runtime_caustic_photon_integration_deposits_receiver_buckets(void) {
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticSurfaceCacheDiagnostics3D surface_diag;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonReceiverContributionReadback3D readback;
    RuntimeCausticPhotonMapRecord3D surface_a = test_surface_record();
    RuntimeCausticPhotonMapRecord3D surface_b = test_surface_record();
    RuntimeCausticPhotonMapRecord3D surface_b2 = test_surface_record();

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.renderContributionEnabled = true;
    settings.surfaceQueryEnabled = true;
    settings.surfaceQueryRadius = 0.25;

    surface_b.photonId = 2u;
    surface_b.position = vec3(0.6, 0.0, 0.0);
    surface_b.sceneObjectIndex = 10;
    surface_b.primitiveIndex = 11;
    surface_b.triangleIndex = 12;
    surface_b2.photonId = 3u;
    surface_b2.position = vec3(0.62, 0.0, 0.0);
    surface_b2.sceneObjectIndex = 10;
    surface_b2.primitiveIndex = 11;
    surface_b2.triangleIndex = 12;

    assert_true("runtime_caustic_photon_receiver_bucket_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 8u));
    assert_true("runtime_caustic_photon_receiver_bucket_cache_alloc",
                RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, 4u));
    assert_true("runtime_caustic_photon_receiver_bucket_store_a",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface_a));
    assert_true("runtime_caustic_photon_receiver_bucket_store_b",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface_b));
    assert_true("runtime_caustic_photon_receiver_bucket_store_b2",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface_b2));

    assert_true("runtime_caustic_photon_receiver_bucket_deposit",
                RuntimeCausticPhotonIntegration3D_DepositSurfaceContributionsForReceiverBuckets(
                    &surface_map,
                    &surface_cache,
                    &settings,
                    &readback));
    assert_true("runtime_caustic_photon_receiver_bucket_readback",
                readback.attempted &&
                    readback.eligible &&
                    readback.receiverBucketCount == 2u &&
                    readback.receiverQueryAttemptCount == 2u &&
                    readback.receiverQueryHitCount == 2u &&
                    readback.receiverSurfaceDepositAcceptedCount == 2u &&
                    readback.receiverSurfaceContributingCount == 3u &&
                    readback.recordCount == 2u);
    assert_true("runtime_caustic_photon_receiver_bucket_records",
                readback.records[0].storedRecordCount == 1u &&
                    readback.records[1].storedRecordCount == 2u);

    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&surface_cache, &surface_diag);
    assert_true("runtime_caustic_photon_receiver_bucket_cache_records",
                surface_diag.recordCount == 2u &&
                    surface_diag.depositAcceptedCount == 2u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
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
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT;
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

static int test_runtime_caustic_photon_integration_render_callsite_readback(void) {
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticSurfaceCacheDiagnostics3D surface_diag;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonRenderCallsiteReadback3D readback;
    RuntimeCausticPhotonMapRecord3D surface = test_surface_record();

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);

    assert_true("runtime_caustic_photon_integration_callsite_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 4u));
    assert_true("runtime_caustic_photon_integration_callsite_surface_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&surface_map, &surface));
    assert_true("runtime_caustic_photon_integration_callsite_cache_alloc",
                RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, 4u));

    query.surface.position = vec3(0.02, 0.0, 0.0);
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;
    query.surface.sceneObjectIndex = 9;
    query.surface.primitiveIndex = 8;
    query.surface.triangleIndex = 7;
    query.queryVolume = false;

    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_OFF;
    assert_true("runtime_caustic_photon_integration_callsite_off_no_query",
                !RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(
                    &surface_map,
                    NULL,
                    &surface_cache,
                    NULL,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_callsite_off_readback",
                readback.route == RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_NONE &&
                    !readback.queryAttempted &&
                    !readback.cacheDepositAttempted);

    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE;
    assert_true("runtime_caustic_photon_integration_callsite_reference_no_query",
                !RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(
                    &surface_map,
                    NULL,
                    &surface_cache,
                    NULL,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_callsite_reference_readback",
                readback.route ==
                        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE &&
                    !readback.queryAttempted &&
                    !readback.cacheDepositAttempted);

    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.renderContributionEnabled = false;
    assert_true("runtime_caustic_photon_integration_callsite_production_suppressed",
                !RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(
                    &surface_map,
                    NULL,
                    &surface_cache,
                    NULL,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_callsite_suppressed_readback",
                readback.queryAttempted &&
                    readback.queryHit &&
                    readback.contributionAttempted &&
                    !readback.contributionEligible &&
                    readback.renderContributionSuppressed &&
                    !readback.cacheDepositAttempted &&
                    readback.surfaceContributingCount == 1u);

    settings.renderContributionEnabled = true;
    assert_true("runtime_caustic_photon_integration_callsite_production_deposit",
                RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(
                    &surface_map,
                    NULL,
                    &surface_cache,
                    NULL,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_callsite_deposit_readback",
                readback.queryAttempted &&
                    readback.contributionEligible &&
                    readback.cacheDepositAttempted &&
                    readback.surfaceDeposited &&
                    !readback.volumeDeposited &&
                    readback.estimatedCost == 1u &&
                    readback.radiance.x > 0.0);

    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&surface_cache, &surface_diag);
    assert_true("runtime_caustic_photon_integration_callsite_cache_written",
                surface_diag.depositAcceptedCount == 1u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    return 0;
}

static int test_runtime_caustic_photon_integration_populates_surface_map_callsite(void) {
    RuntimeLightSet3D set = test_population_light_set();
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticSurfaceCacheDiagnostics3D surface_diag;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonRenderCallsiteReadback3D readback;

    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.renderContributionEnabled = true;
    settings.sampleBudget = 4;
    settings.surfaceQueryRadius = 0.20;

    query.surface.position = vec3(0.0, 0.0, 0.0);
    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;
    query.surface.sceneObjectIndex = 9;
    query.surface.primitiveIndex = 8;
    query.surface.triangleIndex = 7;
    query.queryVolume = false;

    assert_true("runtime_caustic_photon_integration_population_cache_alloc",
                RuntimeCausticSurfaceCache3D_Allocate(&surface_cache, 8u));
    assert_true("runtime_caustic_photon_integration_population_callsite",
                RuntimeCausticPhotonIntegration3D_EvaluatePopulatedRenderCallsite(
                    &set,
                    NULL,
                    &surface_cache,
                    NULL,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_population_counts",
                readback.mapPopulation.attempted &&
                    readback.mapPopulation.surfaceMapAllocated &&
                    readback.mapPopulation.emissionAttempted &&
                    readback.mapPopulation.emissionSucceeded &&
                    readback.mapPopulation.surfaceMapPopulationAttempted &&
                    readback.mapPopulation.surfaceMapPopulated &&
                    readback.mapPopulation.requestedSampleBudget == 4u &&
                    readback.mapPopulation.emittedPhotonCount == 4u &&
                    readback.mapPopulation.surfaceMapStoreAcceptedCount == 4u &&
                    readback.mapPopulation.surfaceMapRecordCount == 4u &&
                    readback.mapPopulation.surfaceMapAccelerationInsertedCount == 4u);
    assert_true("runtime_caustic_photon_integration_population_query_counts",
                readback.queryAttempted &&
                    readback.queryHit &&
                    readback.contributionEligible &&
                    readback.cacheDepositAttempted &&
                    readback.surfaceDeposited &&
                    readback.surfaceContributingCount == 4u &&
                    readback.estimatedCost == 1u &&
                    readback.radiance.x > 0.0);

    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&surface_cache, &surface_diag);
    assert_true("runtime_caustic_photon_integration_population_cache_written",
                surface_diag.depositAcceptedCount == 1u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_caustic_photon_integration_populates_maps_from_trace_records(void) {
    RuntimeCausticPhotonTrace3D trace = test_trace_record(9001u);
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonIntegrationResult3D result;

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.surfaceQueryEnabled = true;
    settings.volumeQueryEnabled = true;
    settings.renderContributionEnabled = true;
    settings.surfaceQueryRadius = 0.15;
    settings.volumeQueryRadius = 0.20;

    query.surface.position = trace.receiverCrossing;
    query.surface.normal = vec3(0.0, 0.0, -1.0);
    query.surface.radius = 0.15;
    query.surface.sceneObjectIndex = 12;
    query.surface.primitiveIndex = 13;
    query.surface.triangleIndex = 14;
    query.volume.position = vec3(0.0, 0.0, 0.5);
    query.volume.direction = trace.postExitDirection;
    query.volume.radius = 0.20;
    query.volume.mediumId = 2;
    query.volume.requireMediumId = true;

    assert_true("runtime_caustic_photon_integration_trace_population",
                RuntimeCausticPhotonIntegration3D_PopulateMapsFromTraceRecords(
                    &surface_map,
                    &beam_map,
                    &trace,
                    1u,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_trace_population_counts",
                readback.populationSource ==
                        RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_TRACE_RECORDS &&
                    readback.tracePopulationAttempted &&
                    readback.tracePopulationSucceeded &&
                    readback.traceInputCount == 1u &&
                    readback.surfaceMapAllocated &&
                    readback.surfaceMapPopulated &&
                    readback.surfaceMapStoreAcceptedCount == 1u &&
                    readback.surfaceMapRecordCount == 1u &&
                    readback.volumeBeamMapAllocated &&
                    readback.volumeBeamPopulated &&
                    readback.volumeBeamStoreAcceptedCount == 1u &&
                    readback.volumeBeamSegmentCount == 1u);
    assert_true("runtime_caustic_photon_integration_trace_population_query",
                RuntimeCausticPhotonIntegration3D_Query(&surface_map,
                                                        &beam_map,
                                                        &settings,
                                                        &query,
                                                        &result));
    assert_true("runtime_caustic_photon_integration_trace_population_result",
                result.surfaceContributingCount == 1u &&
                    result.volumeContributingCount == 1u &&
                    result.combinedFlux.x > 0.0);

    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    return 0;
}

static int
test_runtime_caustic_photon_integration_harvests_mesh_dielectric_fixture_traces(void) {
    RuntimeLightSet3D set = test_mesh_dielectric_population_light_set();
    RuntimeCausticLensShape3D shape = test_mesh_dielectric_shape();
    RuntimeTriangle3D triangle = test_mesh_dielectric_entry_triangle();
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegrationQuery3D query;
    RuntimeCausticPhotonMapPopulationReadback3D readback;
    RuntimeCausticPhotonIntegrationResult3D result;

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
    settings.productMode = RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION;
    settings.surfaceQueryEnabled = true;
    settings.volumeQueryEnabled = true;
    settings.renderContributionEnabled = true;
    settings.sampleBudget = 4;
    settings.maxPathDepth = 6;
    settings.surfaceQueryRadius = 0.20;
    settings.volumeQueryRadius = 0.20;

    query.surface.normal = vec3(0.0, 1.0, 0.0);
    query.surface.radius = 0.20;
    query.surface.sceneObjectIndex = 81;
    query.surface.primitiveIndex = 82;
    query.surface.triangleIndex = 83;
    query.volume.radius = 0.20;
    query.volume.mediumId = 5;
    query.volume.requireMediumId = true;

    assert_true("runtime_caustic_photon_integration_trace_fixture_population",
                RuntimeCausticPhotonIntegration3D_PopulateMapsFromMeshDielectricFixture(
                    &surface_map,
                    &beam_map,
                    &set,
                    &shape,
                    &triangle,
                    &settings,
                    &query,
                    &readback));
    assert_true("runtime_caustic_photon_integration_trace_fixture_counts",
                readback.populationSource ==
                        RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_TRACE_RECORDS &&
                    readback.emissionAttempted &&
                    readback.emissionSucceeded &&
                    readback.emittedPhotonCount == 4u &&
                    readback.traceSolvedPathCount == 4u &&
                    readback.traceRecordCount == 4u &&
                    readback.traceInputCount == 4u &&
                    readback.surfaceMapStoreAcceptedCount == 4u &&
                    readback.surfaceMapRecordCount == 4u &&
                    readback.volumeBeamStoreAcceptedCount == 4u &&
                    readback.volumeBeamSegmentCount == 4u);

    query.surface.position = surface_map.records[0].position;
    query.volume.position = vec3_add(
        beam_map.segments[0].start,
        vec3_scale(vec3_sub(beam_map.segments[0].end, beam_map.segments[0].start),
                   0.5));
    query.volume.direction = beam_map.segments[0].direction;
    assert_true("runtime_caustic_photon_integration_trace_fixture_query",
                RuntimeCausticPhotonIntegration3D_Query(&surface_map,
                                                        &beam_map,
                                                        &settings,
                                                        &query,
                                                        &result));
    assert_true("runtime_caustic_photon_integration_trace_fixture_result",
                result.surfaceContributingCount > 0u &&
                    result.volumeContributingCount > 0u &&
                    result.combinedFlux.x > 0.0);

    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_caustic_photon_scene_descriptor_harvests_mesh_dielectric(void) {
    RuntimeScene3D scene;
    RuntimeTriangle3D triangles[2];
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    RuntimeCausticLensShape3D shapes[2];
    const RuntimeCausticPhotonMeshDielectricDescriptor3D* selected = NULL;

    memset(&scene, 0, sizeof(scene));
    memset(triangles, 0, sizeof(triangles));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;

    triangles[0].p0 = vec3(-0.5, 0.0, -0.5);
    triangles[0].p1 = vec3(0.5, 0.0, -0.5);
    triangles[0].p2 = vec3(0.0, 0.0, 0.5);
    triangles[0].normal = vec3(0.0, 1.0, 0.0);
    triangles[0].sceneObjectIndex = 0;
    triangles[0].primitiveIndex = 12;
    triangles[0].localTriangleIndex = 0;

    triangles[1].p0 = vec3(-0.5, 0.1, -0.5);
    triangles[1].p1 = vec3(0.5, 0.1, -0.5);
    triangles[1].p2 = vec3(0.0, 0.1, 0.5);
    triangles[1].normal = vec3(0.0, 1.0, 0.0);
    triangles[1].sceneObjectIndex = 0;
    triangles[1].primitiveIndex = 12;
    triangles[1].localTriangleIndex = 1;

    scene.triangleMesh.triangles = triangles;
    scene.triangleMesh.triangleCount = 2;

    assert_true("runtime_caustic_photon_scene_descriptor_harvest",
                RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(
                    &scene,
                    &batch));
    selected = RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(&batch);
    assert_true("runtime_caustic_photon_scene_descriptor_selected", selected != NULL);
    assert_true("runtime_caustic_photon_scene_descriptor_counts",
                batch.attempted &&
                    batch.succeeded &&
                    batch.meshDielectricCandidateCount == 1u &&
                    batch.descriptorCount == 1 &&
                    selected->triangleCount == 2);
    assert_true("runtime_caustic_photon_scene_descriptor_identity",
                selected->sceneObjectIndex == 0 &&
                    selected->primitiveIndex == 12 &&
                    selected->shape.kind == RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC &&
                    selected->shape.sceneObjectIndex == 0 &&
                    selected->shape.primitiveIndex == 12);
    assert_true("runtime_caustic_photon_scene_descriptor_shape",
                selected->shape.radius > 0.0 &&
                    selected->shape.height > 0.0 &&
                    selected->shape.payload.valid &&
                    selected->shape.payload.materialId == MATERIAL_PRESET_TRANSPARENT);
    assert_true("runtime_caustic_photon_scene_descriptor_single_shape_copy",
                RuntimeCausticPhotonSceneDescriptor3D_CopyMeshDielectricShapes(
                    &batch, shapes, 2u) == 1u &&
                    shapes[0].sceneObjectIndex == selected->sceneObjectIndex);
    return 0;
}

static int test_runtime_caustic_photon_scene_descriptor_copies_all_guidance_shapes(void) {
    RuntimeScene3D scene;
    RuntimeTriangle3D triangles[2];
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    RuntimeCausticLensShape3D shapes[2];

    memset(&scene, 0, sizeof(scene));
    memset(triangles, 0, sizeof(triangles));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_TRANSPARENT;

    for (int i = 0; i < 2; ++i) {
        const double x = i == 0 ? -1.0 : 1.0;
        triangles[i].p0 = vec3(x - 0.4, 0.0, -0.4);
        triangles[i].p1 = vec3(x + 0.4, 0.0, -0.4);
        triangles[i].p2 = vec3(x, 0.0, 0.4);
        triangles[i].normal = vec3(0.0, 1.0, 0.0);
        triangles[i].sceneObjectIndex = i;
        triangles[i].primitiveIndex = 20 + i;
        triangles[i].localTriangleIndex = 0;
    }
    scene.triangleMesh.triangles = triangles;
    scene.triangleMesh.triangleCount = 2;

    assert_true("runtime_caustic_photon_scene_descriptor_multi_harvest",
                RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(
                    &scene, &batch));
    assert_true("runtime_caustic_photon_scene_descriptor_multi_candidates",
                batch.meshDielectricCandidateCount == 2u &&
                    batch.descriptorCount == 2 &&
                    RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(
                        &batch) != NULL);
    assert_true("runtime_caustic_photon_scene_descriptor_multi_shape_copy",
                RuntimeCausticPhotonSceneDescriptor3D_CopyMeshDielectricShapes(
                    &batch, shapes, 2u) == 2u &&
                    shapes[0].sceneObjectIndex == 0 &&
                    shapes[1].sceneObjectIndex == 1);
    return 0;
}

static int test_runtime_caustic_photon_lifecycle_classifies_rebuilds(void) {
    RuntimeCausticPhotonMapLifecycleInput3D input = {0};
    RuntimeCausticPhotonMapLifecycleState3D state;
    RuntimeCausticPhotonMapLifecycleReadback3D readback;

    RuntimeCausticPhotonMapLifecycle3D_Init(&state);
    input.geometryKey = 1u;
    input.lightKey = 2u;
    input.materialKey = 3u;
    input.volumeKey = 4u;
    input.budgetKey = 5u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_per_frame",
                readback.evaluated && readback.rebuilt && !readback.reused &&
                    readback.rebuildReason ==
                        RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_PER_FRAME_POLICY);

    input.persistentMapOwnershipEnabled = true;
    RuntimeCausticPhotonMapLifecycle3D_Init(&state);
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_first_build",
                readback.rebuildReason == RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_FIRST_BUILD);
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_reuse",
                readback.reused && !readback.rebuilt);
    input.lightKey += 1u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_light_change",
                readback.rebuilt && readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_LIGHT_CHANGED);
    input.materialKey += 1u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_material_change",
                readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_MATERIAL_CHANGED);
    input.volumeKey += 1u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_volume_change",
                readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_VOLUME_CHANGED);
    input.geometryKey += 1u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_geometry_change",
                readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_GEOMETRY_CHANGED);
    input.budgetKey += 1u;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_budget_change",
                readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_BUDGET_CHANGED);
    input.explicitRebuildRequested = true;
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(&input, &state, &readback);
    assert_true("runtime_caustic_photon_lifecycle_explicit_rebuild",
                readback.rebuildReason ==
                    RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_EXPLICIT_REQUEST);
    assert_true("runtime_caustic_photon_budget_tiers",
                RuntimeCausticPhotonBudgetTier3D_FromBudget(64, 4) ==
                    RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW &&
                    RuntimeCausticPhotonBudgetTier3D_FromBudget(512, 8) ==
                        RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION &&
                    RuntimeCausticPhotonBudgetTier3D_FromBudget(513, 8) ==
                        RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL);
    return 0;
}

static int test_runtime_caustic_photon_map_store_reuses_owned_maps(void) {
    RuntimeCausticPhotonMapStore3D store;
    RuntimeCausticPhotonMapLifecycleInput3D input = {0};
    RuntimeCausticPhotonMapLifecycleReadback3D readback;
    RuntimeCausticPhotonMapPopulationReadback3D population = {0};

    RuntimeCausticPhotonMapStore3D_Init(&store);
    input.geometryKey = 10u;
    input.lightKey = 20u;
    input.materialKey = 30u;
    input.volumeKey = 40u;
    input.budgetKey = 50u;
    input.persistentMapOwnershipEnabled = true;
    assert_true("runtime_caustic_photon_map_store_first_build",
                RuntimeCausticPhotonMapStore3D_Begin(&store, &input, &readback) &&
                    readback.rebuilt && readback.generation == 1u &&
                    readback.rebuildCount == 1u && readback.reuseCount == 0u);
    assert_true("runtime_caustic_photon_map_store_allocates_owned_surface_map",
                RuntimeCausticPhotonMap3D_Allocate(&store.surfaceMap, 4u));
    population.surfaceMapRecordCount = 7u;
    RuntimeCausticPhotonMapStore3D_CommitPopulation(&store, &population);
    assert_true("runtime_caustic_photon_map_store_reuse",
                RuntimeCausticPhotonMapStore3D_Begin(&store, &input, &readback) &&
                    readback.reused && readback.generation == 1u &&
                    readback.rebuildCount == 1u && readback.reuseCount == 1u &&
                    RuntimeCausticPhotonMap3D_IsAllocated(&store.surfaceMap) &&
                    store.population.surfaceMapRecordCount == 7u);
    input.lightKey += 1u;
    assert_true("runtime_caustic_photon_map_store_invalidates",
                RuntimeCausticPhotonMapStore3D_Begin(&store, &input, &readback) &&
                    readback.rebuilt && readback.generation == 2u &&
                    readback.rebuildCount == 2u && readback.reuseCount == 1u &&
                    !RuntimeCausticPhotonMap3D_IsAllocated(&store.surfaceMap) &&
                    store.population.surfaceMapRecordCount == 0u);
    RuntimeCausticPhotonMapStore3D_Free(&store);
    return 0;
}

int run_test_runtime_caustic_photon_integration_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_integration_modes_and_settings();
    failures += test_runtime_caustic_photon_integration_queries_maps();
    failures +=
        test_runtime_caustic_photon_integration_suppresses_contribution_by_default();
    failures += test_runtime_caustic_photon_integration_deposits_gated_contribution();
    failures += test_runtime_caustic_photon_integration_deposits_volume_beam_contribution();
    failures += test_runtime_caustic_photon_integration_deposits_receiver_buckets();
    failures += test_runtime_caustic_photon_integration_reference_does_not_query_maps();
    failures += test_runtime_caustic_photon_integration_render_callsite_readback();
    failures +=
        test_runtime_caustic_photon_integration_populates_surface_map_callsite();
    failures +=
        test_runtime_caustic_photon_integration_populates_maps_from_trace_records();
    failures += test_runtime_caustic_photon_receiver_policy_selects_primary_bucket();
    failures +=
        test_runtime_caustic_photon_integration_harvests_mesh_dielectric_fixture_traces();
    failures += test_runtime_caustic_photon_scene_descriptor_harvests_mesh_dielectric();
    failures +=
        test_runtime_caustic_photon_scene_descriptor_copies_all_guidance_shapes();
    failures += test_runtime_caustic_photon_lifecycle_classifies_rebuilds();
    failures += test_runtime_caustic_photon_map_store_reuses_owned_maps();
    return failures;
}
