#include "test_runtime_caustic_photon_emit_3d.h"

#include "render/runtime_caustic_photon_emit_3d.h"
#include "test_support.h"

#include <stdio.h>

static RuntimeLightSet3D test_light_set(void) {
    RuntimeLightSet3D set;
    RuntimeLightSource3D light;

    RuntimeLightSet3D_Init(&set);

    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "ppm7_key_sphere");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.position = vec3(0.0, 2.0, 0.0);
    light.radius = 0.20;
    light.color = vec3(1.0, 0.8, 0.6);
    light.intensity = 2.0;
    assert_true("runtime_caustic_photon_emit_append_sphere",
                RuntimeLightSet3D_Append(&set, &light, NULL));

    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "ppm7_rect_emitter");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER;
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
    light.position = vec3(1.0, 3.0, 0.0);
    light.axisU = vec3(1.0, 0.0, 0.0);
    light.axisV = vec3(0.0, 0.0, 1.0);
    light.normal = vec3(0.0, -1.0, 0.0);
    light.width = 2.0;
    light.height = 1.0;
    light.color = vec3(0.4, 0.9, 1.0);
    light.intensity = 4.0;
    assert_true("runtime_caustic_photon_emit_append_rect",
                RuntimeLightSet3D_Append(&set, &light, NULL));

    return set;
}

static int test_runtime_caustic_photon_emit_deterministic_samples(void) {
    RuntimeLightSet3D set = test_light_set();
    RuntimeCausticPhotonEmissionSettings3D settings;
    RuntimeCausticPhotonEmissionBatch3D batch_a;
    RuntimeCausticPhotonEmissionBatch3D batch_b;
    RuntimeCausticPhotonEmissionDiagnostics3D diag_a;
    RuntimeCausticPhotonEmissionDiagnostics3D diag_b;

    RuntimeCausticPhotonEmission3D_DefaultSettings(&settings);
    settings.sampleBudget = 12u;
    settings.baseSeed = 12345u;
    settings.firstPhotonId = 900u;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch_a);
    RuntimeCausticPhotonEmission3D_InitBatch(&batch_b);
    assert_true("runtime_caustic_photon_emit_alloc_a",
                RuntimeCausticPhotonEmission3D_AllocateBatch(&batch_a, 12u));
    assert_true("runtime_caustic_photon_emit_alloc_b",
                RuntimeCausticPhotonEmission3D_AllocateBatch(&batch_b, 12u));

    assert_true("runtime_caustic_photon_emit_emit_a",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch_a,
                                                                &set,
                                                                &settings,
                                                                &diag_a));
    assert_true("runtime_caustic_photon_emit_emit_b",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch_b,
                                                                &set,
                                                                &settings,
                                                                &diag_b));
    assert_true("runtime_caustic_photon_emit_counts",
                batch_a.sampleCount == 12u &&
                    batch_b.sampleCount == batch_a.sampleCount &&
                    diag_a.emittedPhotonCount == 12u &&
                    diag_a.rejectedPhotonCount == 0u &&
                    diag_a.enabledSourceCount == 2u);
    assert_true("runtime_caustic_photon_emit_repeat_seed",
                batch_a.samples[0].rngSeed == batch_b.samples[0].rngSeed &&
                    diag_a.firstSeed == diag_b.firstSeed &&
                    diag_a.lastSeed == diag_b.lastSeed);
    assert_close("runtime_caustic_photon_emit_repeat_position_x",
                 batch_a.samples[5].position.x,
                 batch_b.samples[5].position.x,
                 1e-12);
    assert_close("runtime_caustic_photon_emit_repeat_flux_y",
                 batch_a.samples[7].flux.y,
                 batch_b.samples[7].flux.y,
                 1e-12);
    assert_true("runtime_caustic_photon_emit_pdf_and_flux",
                batch_a.samples[0].emissionPdf > 0.0 &&
                    batch_a.samples[0].emissionPdf <= 1.0 &&
                    diag_a.sourcePdfSum > 0.0 &&
                    diag_a.totalEmittedFlux.x > 0.0);
    assert_true("runtime_caustic_photon_emit_identity",
                batch_a.samples[0].photonId == 900u &&
                    batch_a.samples[11].sampleIndex == 11u);

    RuntimeCausticPhotonEmission3D_FreeBatch(&batch_a);
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch_b);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_caustic_photon_emit_populates_surface_map_proxy(void) {
    RuntimeLightSet3D set = test_light_set();
    RuntimeCausticPhotonEmissionSettings3D settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D diag;
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapDiagnostics3D map_diag;

    RuntimeCausticPhotonEmission3D_DefaultSettings(&settings);
    settings.sampleBudget = 4u;
    settings.baseSeed = 7u;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    RuntimeCausticPhotonMap3D_Init(&map);
    RuntimeCausticPhotonMap3D_DefaultQuery(&query);

    assert_true("runtime_caustic_photon_emit_map_batch_alloc",
                RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, 4u));
    assert_true("runtime_caustic_photon_emit_map_emit",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                                &set,
                                                                &settings,
                                                                &diag));
    assert_true("runtime_caustic_photon_emit_map_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    assert_true("runtime_caustic_photon_emit_map_store_proxy",
                RuntimeCausticPhotonEmission3D_StoreSurfaceProxyRecords(
                    &map,
                    &batch,
                    settings.defaultQueryRadius,
                    vec3(0.0, 1.0, 0.0),
                    21,
                    22,
                    23,
                    &diag));
    assert_true("runtime_caustic_photon_emit_map_store_diag",
                diag.mapStoreAttemptCount == 4u &&
                    diag.mapStoreAcceptedCount == 4u &&
                    diag.mapStoreRejectedCount == 0u &&
                    diag.totalStoredSurfaceFlux.x > 0.0);

    query.position = batch.samples[0].position;
    query.normal = vec3(0.0, 1.0, 0.0);
    query.radius = 0.15;
    query.sceneObjectIndex = 21;
    query.primitiveIndex = 22;
    query.triangleIndex = 23;
    assert_true("runtime_caustic_photon_emit_map_query",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_photon_emit_map_query_flux",
                result.flux.x > 0.0 && result.contributingCount >= 1u);
    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(&map, &map_diag);
    assert_true("runtime_caustic_photon_emit_map_diag",
                map_diag.recordCount == 4u &&
                    map_diag.storeAcceptedCount == 4u &&
                    map_diag.queryHitCount == 1u);

    RuntimeCausticPhotonMap3D_Free(&map);
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_caustic_photon_emit_map_capacity_rejects(void) {
    RuntimeLightSet3D set = test_light_set();
    RuntimeCausticPhotonEmissionSettings3D settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D diag;
    RuntimeCausticPhotonMap3D map;

    RuntimeCausticPhotonEmission3D_DefaultSettings(&settings);
    settings.sampleBudget = 3u;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    RuntimeCausticPhotonMap3D_Init(&map);

    assert_true("runtime_caustic_photon_emit_capacity_batch_alloc",
                RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, 3u));
    assert_true("runtime_caustic_photon_emit_capacity_emit",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                                &set,
                                                                &settings,
                                                                &diag));
    assert_true("runtime_caustic_photon_emit_capacity_map_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&map, 1u));
    assert_true("runtime_caustic_photon_emit_capacity_store_some",
                RuntimeCausticPhotonEmission3D_StoreSurfaceProxyRecords(
                    &map,
                    &batch,
                    settings.defaultQueryRadius,
                    vec3(0.0, 1.0, 0.0),
                    -1,
                    -1,
                    -1,
                    &diag));
    assert_true("runtime_caustic_photon_emit_capacity_diag",
                diag.mapStoreAttemptCount == 3u &&
                    diag.mapStoreAcceptedCount == 1u &&
                    diag.mapStoreRejectedCount == 2u &&
                    diag.lastRejectReason == RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY);

    RuntimeCausticPhotonMap3D_Free(&map);
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

int run_test_runtime_caustic_photon_emit_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_emit_deterministic_samples();
    failures += test_runtime_caustic_photon_emit_populates_surface_map_proxy();
    failures += test_runtime_caustic_photon_emit_map_capacity_rejects();
    return failures;
}
