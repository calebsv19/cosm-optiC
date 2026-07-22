#include "test_runtime_caustic_photon_surface_provenance_filter_3d.h"

#include <string.h>

#include "render/runtime_caustic_photon_path_population_3d.h"
#include "test_support.h"

typedef enum {
    SURFACE_PROVENANCE_FIXTURE_TRANSMITTED = 0,
    SURFACE_PROVENANCE_FIXTURE_REFLECTION_ONLY,
    SURFACE_PROVENANCE_FIXTURE_UNRECONCILED
} SurfaceProvenanceFixtureKind3D;

static void surface_provenance_set_transition(
    RuntimeCausticPhotonSceneHitEvent3D* hit,
    RuntimeCausticPhotonMediumTransitionReason3D reason,
    double before_ior,
    double after_ior) {
    hit->mediumTransition.succeeded = true;
    hit->mediumTransition.stackChanged = true;
    hit->mediumTransition.reason = reason;
    hit->mediumTransition.topBefore.ior = before_ior;
    hit->mediumTransition.topAfter.ior = after_ior;
    hit->mediumTransition.boundary.ior = 1.5;
}

static void surface_provenance_set_scatter(
    RuntimeCausticPhotonSceneHitEvent3D* hit,
    uint32_t depth,
    double z,
    RuntimeCausticPhotonBsdfLobe3D lobe,
    bool transparent) {
    memset(hit, 0, sizeof(*hit));
    hit->depth = depth;
    hit->pathStart = vec3(0.0, 0.0, z + 1.0);
    hit->pathPdfBefore = 0.5;
    hit->pathPdfAfter = 0.5;
    hit->hit.position = vec3(0.0, 0.0, z);
    hit->hit.normal = vec3(0.0, 0.0, 1.0);
    hit->hit.sceneObjectIndex = transparent ? 51 : 52;
    hit->hit.primitiveIndex = transparent ? 0 : 1;
    hit->hit.triangleIndex = (int)depth - 1;
    hit->material.valid = true;
    hit->material.materialId = transparent ? 510 : 520;
    hit->material.transparency = transparent ? 1.0 : 0.0;
    hit->material.opticalIor = transparent ? 1.5 : 1.0;
    hit->bsdfSelection.attempted = true;
    hit->bsdfSelection.selected = true;
    hit->bsdfSelection.lobe = lobe;
    hit->bsdfSelection.branchPdf = 1.0;
    hit->bsdfSelection.throughputBefore = vec3(0.25, 0.20, 0.15);
    hit->bsdfSelection.throughputAfter = vec3(0.25, 0.20, 0.15);
    hit->bsdfDirection.valid = true;
    hit->bsdfDirection.outgoingDirection = vec3(0.0, 0.0, -1.0);
    hit->segmentMedium.mediumId = 0;
    hit->segmentMedium.isAir = true;
}

static RuntimeCausticPhotonSceneTrace3D surface_provenance_trace(
    SurfaceProvenanceFixtureKind3D kind) {
    RuntimeCausticPhotonSceneTrace3D trace;
    RuntimeCausticPhotonSceneHitEvent3D* receiver;
    memset(&trace, 0, sizeof(trace));
    trace.trace.valid = true;
    trace.trace.sample.photonId = 27100u + (uint64_t)kind;
    trace.trace.debug.emittedFlux = vec3(0.25, 0.20, 0.15);
    trace.readback.succeeded = true;
    trace.readback.termination =
        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH;

    if (kind == SURFACE_PROVENANCE_FIXTURE_REFLECTION_ONLY) {
        trace.readback.hitEventCount = 2u;
        surface_provenance_set_scatter(
            &trace.hitEvents[0],
            1u,
            1.0,
            RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR,
            false);
        receiver = &trace.hitEvents[1];
    } else {
        trace.readback.hitEventCount =
            kind == SURFACE_PROVENANCE_FIXTURE_TRANSMITTED ? 3u : 2u;
        surface_provenance_set_scatter(
            &trace.hitEvents[0],
            1u,
            2.0,
            RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION,
            true);
        surface_provenance_set_transition(
            &trace.hitEvents[0],
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED,
            1.0,
            1.5);
        if (kind == SURFACE_PROVENANCE_FIXTURE_TRANSMITTED) {
            surface_provenance_set_scatter(
                &trace.hitEvents[1],
                2u,
                1.0,
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION,
                true);
            surface_provenance_set_transition(
                &trace.hitEvents[1],
                RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED,
                1.5,
                1.0);
            receiver = &trace.hitEvents[2];
        } else {
            receiver = &trace.hitEvents[1];
        }
    }
    surface_provenance_set_scatter(
        receiver,
        trace.readback.hitEventCount,
        0.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE,
        false);
    return trace;
}

static RuntimeCausticPhotonSceneTrace3D
surface_provenance_post_receiver_rehit_trace(void) {
    RuntimeCausticPhotonSceneTrace3D trace =
        surface_provenance_trace(SURFACE_PROVENANCE_FIXTURE_TRANSMITTED);
    trace.readback.hitEventCount = 7u;
    surface_provenance_set_scatter(
        &trace.hitEvents[3], 4u, -1.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR, false);
    surface_provenance_set_scatter(
        &trace.hitEvents[4], 5u, -2.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE, false);
    surface_provenance_set_scatter(
        &trace.hitEvents[5], 6u, -3.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR, false);
    surface_provenance_set_scatter(
        &trace.hitEvents[6], 7u, -4.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE, false);
    return trace;
}

static RuntimeCausticPhotonSceneTrace3D
surface_provenance_glossy_receiver_multipath_trace(void) {
    RuntimeCausticPhotonSceneTrace3D trace =
        surface_provenance_trace(SURFACE_PROVENANCE_FIXTURE_TRANSMITTED);
    trace.readback.hitEventCount = 5u;
    trace.hitEvents[2].bsdfSelection.lobe =
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY;
    surface_provenance_set_scatter(
        &trace.hitEvents[3], 4u, -1.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR, false);
    surface_provenance_set_scatter(
        &trace.hitEvents[4], 5u, -2.0,
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE, false);
    return trace;
}

static bool surface_provenance_populate(
    const RuntimeCausticPhotonSceneTrace3D* trace,
    bool require_reconciled,
    RuntimeCausticPhotonMap3D* map,
    RuntimeCausticPhotonPathPopulationReadback3D* readback) {
    RuntimeCausticPhotonPathPopulationSettings3D settings;
    RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&settings);
    assert_true("surface_provenance_filter_default_requires_reconciled",
                settings.requireReconciledDielectricTransmissionForSurface);
    settings.storeTraversedBeams = false;
    settings.requireReconciledDielectricTransmissionForSurface =
        require_reconciled;
    return RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
        trace, &settings, map, NULL, readback);
}

static int test_surface_provenance_filter_cohorts(void) {
    RuntimeCausticPhotonSceneTrace3D transmitted =
        surface_provenance_trace(SURFACE_PROVENANCE_FIXTURE_TRANSMITTED);
    RuntimeCausticPhotonSceneTrace3D reflection =
        surface_provenance_trace(SURFACE_PROVENANCE_FIXTURE_REFLECTION_ONLY);
    RuntimeCausticPhotonSceneTrace3D unreconciled =
        surface_provenance_trace(SURFACE_PROVENANCE_FIXTURE_UNRECONCILED);
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonMap3D map;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("surface_provenance_filter_labels",
                strcmp(RuntimeCausticPhotonSurfaceRetentionReason3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_RECONCILED_TRANSMISSION),
                       "reconciled_dielectric_transmission") == 0 &&
                    strcmp(RuntimeCausticPhotonRecordRejectReason3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE),
                           "reflection_only_surface") == 0 &&
                    strcmp(RuntimeCausticPhotonRecordRejectReason3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE),
                           "unreconciled_dielectric_surface") == 0);
    assert_true("surface_provenance_filter_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    assert_true("surface_provenance_filter_transmitted_store",
                surface_provenance_populate(
                    &transmitted, true, &map, &readback) &&
                    readback.storedSurfaceCount == 1u &&
                    readback.dielectricEntryCount == 1u &&
                    readback.dielectricExitCount == 1u &&
                    readback.retention.surfaceRetained[
                        RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_RECONCILED_TRANSMISSION] ==
                        1u &&
                    map.records[0].provenance.dielectricEntryCount == 1u &&
                    map.records[0].provenance.dielectricExitCount == 1u &&
                    map.records[0].provenance
                            .firstDielectricSceneObjectIndex == 51);

    RuntimeCausticPhotonMap3D_Clear(&map);
    assert_true("surface_provenance_filter_reflection_reject",
                !surface_provenance_populate(
                    &reflection, true, &map, &readback) &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_REFLECTION_ONLY_SURFACE_REJECTED &&
                    readback.retention.surfaceRejected[
                        RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE] == 1u &&
                    map.recordCount == 0u);

    assert_true("surface_provenance_filter_unreconciled_reject",
                !surface_provenance_populate(
                    &unreconciled, true, &map, &readback) &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_UNRECONCILED_DIELECTRIC_SURFACE_REJECTED &&
                    readback.retention.surfaceRejected[
                        RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE] ==
                        1u &&
                    map.recordCount == 0u);

    assert_true("surface_provenance_filter_compatibility_accepts_reflection",
                surface_provenance_populate(
                    &reflection, false, &map, &readback) &&
                    readback.retention.surfaceRetained[
                        RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITH_SPECULAR_HISTORY] ==
                        1u &&
                    map.recordCount == 1u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_surface_provenance_filter_retains_first_diffuse_receiver_only(
    void) {
    RuntimeCausticPhotonSceneTrace3D trace =
        surface_provenance_post_receiver_rehit_trace();
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonMap3D map;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("surface_provenance_first_receiver_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    assert_true(
        "surface_provenance_first_receiver_only",
        surface_provenance_populate(&trace, true, &map, &readback) &&
            readback.diffuseReceiverCount == 1u &&
            readback.storedSurfaceCount == 1u && map.recordCount == 1u &&
            map.records[0].depth == 3u &&
            readback.retention.surfaceRejected[
                RUNTIME_CAUSTIC_PHOTON_REJECT_POST_FIRST_DIFFUSE_RECEIVER] == 2u);
    assert_true(
        "surface_provenance_post_first_receiver_label",
        strcmp(RuntimeCausticPhotonRecordRejectReason3D_Label(
                   RUNTIME_CAUSTIC_PHOTON_REJECT_POST_FIRST_DIFFUSE_RECEIVER),
               "post_first_diffuse_receiver") == 0);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_surface_provenance_filter_labels_receiver_bounce_multipath(
    void) {
    RuntimeCausticPhotonSceneTrace3D trace =
        surface_provenance_glossy_receiver_multipath_trace();
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonMap3D map;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("surface_provenance_receiver_multipath_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 2u));
    assert_true(
        "surface_provenance_receiver_bounce_is_multipath",
        surface_provenance_populate(&trace, true, &map, &readback) &&
            readback.storedSurfaceCount == 1u && map.recordCount == 1u &&
            map.records[0].depth == 5u &&
            map.records[0].provenance.surfacePathClass ==
                RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_MULTIPATH);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

int run_test_runtime_caustic_photon_surface_provenance_filter_3d_tests(void) {
    int failures = 0;
    failures += test_surface_provenance_filter_cohorts();
    failures +=
        test_surface_provenance_filter_retains_first_diffuse_receiver_only();
    failures +=
        test_surface_provenance_filter_labels_receiver_bounce_multipath();
    return failures;
}
