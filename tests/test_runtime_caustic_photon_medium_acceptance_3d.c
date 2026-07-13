#include "test_runtime_caustic_photon_medium_acceptance_3d.h"

#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_population_3d.h"
#include "render/runtime_caustic_photon_path_transport_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "test_support.h"

typedef enum {
    MEDIUM_ACCEPTANCE_DUPLICATE_ENTRY = 0,
    MEDIUM_ACCEPTANCE_NON_TOP_EXIT,
    MEDIUM_ACCEPTANCE_UNDERFLOW,
    MEDIUM_ACCEPTANCE_OVERFLOW
} MediumAcceptanceScenario3D;

typedef struct {
    RuntimeMaterialPayload3D material;
    uint32_t calls;
} MediumAcceptanceMaterialFixture3D;

static RuntimeMaterialPayload3D medium_acceptance_glass(int material_id) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.materialId = material_id;
    material.baseColorR = 1.0;
    material.baseColorG = 1.0;
    material.baseColorB = 1.0;
    material.transparency = 1.0;
    material.opticalIor = 1.5;
    material.bsdf.ior = 1.5;
    material.bsdf.diffuseWeight = 0.0;
    material.bsdf.specWeight = 0.0;
    material.bsdf.roughness = 0.0;
    return material;
}

static RuntimeCausticPhotonMediumEntry3D medium_acceptance_entry(
    int material_id,
    int object_id) {
    RuntimeMaterialPayload3D material = medium_acceptance_glass(material_id);
    RuntimeCausticPhotonMediumEntry3D entry;
    memset(&entry, 0, sizeof(entry));
    RuntimeCausticPhotonMediumEntry3D_FromMaterial(
        &material, object_id, 0.0, &entry);
    return entry;
}

static bool medium_acceptance_resolve_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    MediumAcceptanceMaterialFixture3D* fixture =
        (MediumAcceptanceMaterialFixture3D*)user_data;
    if (!hit || !out_payload || !fixture) return false;
    fixture->calls++;
    *out_payload = fixture->material;
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    return out_payload->valid;
}

static bool medium_acceptance_build_plane(RuntimeScene3D* scene,
                                          Vec3 normal) {
    RuntimeTriangle3D* triangle;
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 1;
    scene->primitives = calloc(1u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        calloc(1u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 1;
    scene->primitives[0].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.kind =
        RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 801;
    triangle = &scene->triangleMesh.triangles[0];
    triangle->p0 = vec3(-4.0, -4.0, 0.0);
    triangle->p1 = vec3(4.0, -4.0, 0.0);
    triangle->p2 = vec3(0.0, 4.0, 0.0);
    triangle->normal = normal;
    triangle->twoSided = true;
    triangle->sceneObjectIndex = 801;
    triangle->primitiveIndex = 0;
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static bool medium_acceptance_push(
    RuntimeCausticPhotonMediumStack3D* stack,
    RuntimeCausticPhotonMediumEntry3D entry) {
    RuntimeCausticPhotonMediumTransition3D transition;
    return RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        stack, &entry, true, false, &transition);
}

static bool medium_acceptance_prepare_stack(
    MediumAcceptanceScenario3D scenario,
    RuntimeCausticPhotonMediumStack3D* stack) {
    RuntimeCausticPhotonMediumEntry3D target =
        medium_acceptance_entry(801, 801);
    RuntimeCausticPhotonMediumStack3D_Init(stack);
    switch (scenario) {
        case MEDIUM_ACCEPTANCE_DUPLICATE_ENTRY:
            return medium_acceptance_push(stack, target);
        case MEDIUM_ACCEPTANCE_NON_TOP_EXIT:
            return medium_acceptance_push(stack, target) &&
                   medium_acceptance_push(
                       stack, medium_acceptance_entry(802, 802));
        case MEDIUM_ACCEPTANCE_UNDERFLOW:
            return true;
        case MEDIUM_ACCEPTANCE_OVERFLOW:
            for (int i = 0;
                 i < RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY - 1;
                 ++i) {
                if (!medium_acceptance_push(
                        stack,
                        medium_acceptance_entry(900 + i, 900 + i))) {
                    return false;
                }
            }
            return true;
        default:
            return false;
    }
}

static bool medium_acceptance_zero_write_reject(
    const RuntimeCausticPhotonSceneTrace3D* trace) {
    RuntimeCausticPhotonPathPopulationSettings3D settings;
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    bool accepted;
    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    if (!RuntimeCausticPhotonMap3D_Allocate(&surface_map, 2u) ||
        !RuntimeCausticBeamMap3D_Allocate(&beam_map, 2u)) {
        RuntimeCausticBeamMap3D_Free(&beam_map);
        RuntimeCausticPhotonMap3D_Free(&surface_map);
        return false;
    }
    RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&settings);
    accepted = RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
        trace, &settings, &surface_map, &beam_map, &readback);
    accepted = !accepted &&
               readback.termination ==
                   RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE &&
               surface_map.recordCount == 0u && beam_map.segmentCount == 0u &&
               surface_map.storeAttemptCount == 0u &&
               beam_map.storeAttemptCount == 0u;
    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    return accepted;
}

static bool medium_acceptance_run_scenario(
    MediumAcceptanceScenario3D scenario,
    RuntimeCausticPhotonMediumTransitionReason3D expected_reason) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample;
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    MediumAcceptanceMaterialFixture3D fixture;
    RuntimeCausticPhotonMediumEntry3D initial_entries[
        RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY];
    uint32_t initial_count;
    bool found_rejection = false;
    Vec3 normal = scenario == MEDIUM_ACCEPTANCE_DUPLICATE_ENTRY ||
                          scenario == MEDIUM_ACCEPTANCE_OVERFLOW
                      ? vec3(0.0, 0.0, 1.0)
                      : vec3(0.0, 0.0, -1.0);

    if (!medium_acceptance_build_plane(&scene, normal)) return false;
    memset(&fixture, 0, sizeof(fixture));
    fixture.material = medium_acceptance_glass(801);
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 1u;
    settings.sceneTrace.materialResolver = medium_acceptance_resolve_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;
    settings.hasInitialMediumStack = true;
    if (!medium_acceptance_prepare_stack(
            scenario, &settings.initialMediumStack)) {
        RuntimeSceneAcceleration3D_ResetTLASForTests();
        RuntimeScene3D_Free(&scene);
        return false;
    }
    initial_count = settings.initialMediumStack.count;
    memcpy(initial_entries,
           settings.initialMediumStack.entries,
           sizeof(initial_entries));

    memset(&sample, 0, sizeof(sample));
    sample.position = vec3(0.0, 0.0, 1.0);
    sample.direction = vec3(0.0, 0.0, -1.0);
    sample.flux = vec3(1.0, 0.8, 0.6);
    sample.emissionPdf = 0.5;
    for (uint64_t i = 0u; i < 512u; ++i) {
        sample.photonId = 31000u + i;
        sample.sampleIndex = i;
        fixture.calls = 0u;
        if (!RuntimeCausticPhotonPathTransport3D_Trace(
                &scene, &sample, &settings, &trace) &&
            trace.readback.termination ==
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MEDIUM_TRANSITION_REJECTED) {
            found_rejection = true;
            break;
        }
    }

    found_rejection =
        found_rejection && !trace.trace.valid && !trace.readback.succeeded &&
        fixture.calls == 1u && trace.readback.hitEventCount == 1u &&
        trace.readback.mediumTransitionCount == 1u &&
        trace.readback.mediumTransitionFailureCount == 1u &&
        trace.readback.terminatedByMediumFailurePolicy &&
        trace.readback.mediumFailureDepth == 1u &&
        trace.readback.mediumFailurePolicy ==
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_FAILURE_FAIL_CLOSED &&
        trace.readback.mediumFailureReason == expected_reason &&
        trace.hitEvents[0].mediumTransition.attempted &&
        !trace.hitEvents[0].mediumTransition.succeeded &&
        !trace.hitEvents[0].mediumTransition.stackChanged &&
        trace.finalMediumStack.count == initial_count &&
        memcmp(trace.finalMediumStack.entries,
               initial_entries,
               sizeof(initial_entries)) == 0 &&
        trace.trace.finalState.rejectReason ==
            RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM &&
        strcmp(RuntimeCausticPhotonSceneTermination3D_Label(
                   trace.readback.termination),
               "medium_transition_rejected") == 0 &&
        strcmp(RuntimeCausticPhotonMediumFailurePolicy3D_Label(
                   trace.readback.mediumFailurePolicy),
               "fail_closed") == 0 &&
        medium_acceptance_zero_write_reject(&trace);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return found_rejection;
}

int run_test_runtime_caustic_photon_medium_acceptance_3d_tests(void) {
    assert_true("runtime_caustic_photon_medium_duplicate_entry",
                medium_acceptance_run_scenario(
                    MEDIUM_ACCEPTANCE_DUPLICATE_ENTRY,
                    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH));
    assert_true("runtime_caustic_photon_medium_non_top_exit",
                medium_acceptance_run_scenario(
                    MEDIUM_ACCEPTANCE_NON_TOP_EXIT,
                    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH));
    assert_true("runtime_caustic_photon_medium_underflow",
                medium_acceptance_run_scenario(
                    MEDIUM_ACCEPTANCE_UNDERFLOW,
                    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_UNDERFLOW));
    assert_true("runtime_caustic_photon_medium_overflow",
                medium_acceptance_run_scenario(
                    MEDIUM_ACCEPTANCE_OVERFLOW,
                    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW));
    return test_support_failures();
}
