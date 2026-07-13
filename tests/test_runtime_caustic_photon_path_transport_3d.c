#include "test_runtime_caustic_photon_path_transport_3d.h"

#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_population_3d.h"
#include "render/runtime_caustic_photon_path_transport_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "test_support.h"

typedef struct {
    RuntimeMaterialPayload3D material;
    uint32_t calls;
} PhotonPathTransportMaterialFixture3D;

typedef struct {
    RuntimeMaterialPayload3D object51;
    RuntimeMaterialPayload3D object52;
    int resolvedObjects[8];
    uint32_t calls;
} PhotonPathTransportMultiMaterialFixture3D;

typedef struct {
    RuntimeMaterialPayload3D glass;
    RuntimeMaterialPayload3D water;
    int resolvedObjects[16];
    uint32_t calls;
} PhotonPathTransportNestedMaterialFixture3D;

static bool photon_path_transport_fixture_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonPathTransportMaterialFixture3D* fixture =
        (PhotonPathTransportMaterialFixture3D*)user_data;
    if (!hit || !out_payload || !fixture) return false;
    fixture->calls++;
    *out_payload = fixture->material;
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    return out_payload->valid;
}

static bool photon_path_transport_multi_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonPathTransportMultiMaterialFixture3D* fixture =
        (PhotonPathTransportMultiMaterialFixture3D*)user_data;
    if (!hit || !out_payload || !fixture || fixture->calls >= 8u) return false;
    fixture->resolvedObjects[fixture->calls++] = hit->sceneObjectIndex;
    if (hit->sceneObjectIndex == 51) {
        *out_payload = fixture->object51;
    } else if (hit->sceneObjectIndex == 52) {
        *out_payload = fixture->object52;
    } else {
        return false;
    }
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    return out_payload->valid;
}

static bool photon_path_transport_nested_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonPathTransportNestedMaterialFixture3D* fixture =
        (PhotonPathTransportNestedMaterialFixture3D*)user_data;
    if (!hit || !out_payload || !fixture || fixture->calls >= 16u) return false;
    fixture->resolvedObjects[fixture->calls++] = hit->sceneObjectIndex;
    if (hit->sceneObjectIndex == 71) {
        *out_payload = fixture->glass;
    } else if (hit->sceneObjectIndex == 72) {
        *out_payload = fixture->water;
    } else {
        return false;
    }
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    return out_payload->valid;
}

static RuntimeMaterialPayload3D photon_path_transport_mirror(void) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.baseColorR = 1.0;
    material.baseColorG = 1.0;
    material.baseColorB = 1.0;
    material.opticalIor = 1.5;
    material.bsdf.ior = 1.5;
    material.bsdf.diffuseWeight = 0.0;
    material.bsdf.specWeight = 1.0;
    material.bsdf.reflectivity = 1.0;
    material.bsdf.roughness = 0.0;
    return material;
}

static RuntimeMaterialPayload3D photon_path_transport_glass(void) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
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

static RuntimeMaterialPayload3D photon_path_transport_diffuse(void) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.baseColorR = 0.8;
    material.baseColorG = 0.7;
    material.baseColorB = 0.6;
    material.opticalIor = 1.5;
    material.bsdf.ior = 1.5;
    material.bsdf.diffuseWeight = 1.0;
    material.bsdf.specWeight = 0.0;
    material.bsdf.reflectivity = 0.0;
    material.bsdf.roughness = 1.0;
    return material;
}

static void photon_path_transport_set_triangle(RuntimeTriangle3D* triangle,
                                               double z,
                                               Vec3 normal,
                                               int local_index) {
    memset(triangle, 0, sizeof(*triangle));
    triangle->p0 = vec3(-4.0, -4.0, z);
    triangle->p1 = vec3(4.0, -4.0, z);
    triangle->p2 = vec3(0.0, 4.0, z);
    triangle->normal = normal;
    triangle->twoSided = true;
    triangle->primitiveIndex = 0;
    triangle->sceneObjectIndex = 41;
    triangle->localTriangleIndex = local_index;
}

static bool photon_path_transport_build_mirror_box(RuntimeScene3D* scene) {
    RuntimePrimitive3D* primitive;
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = calloc(1u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        calloc(2u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 2;
    primitive = &scene->primitives[0];
    primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.sceneObjectIndex = 41;
    photon_path_transport_set_triangle(
        &scene->triangleMesh.triangles[0], 0.0, vec3(0.0, 0.0, 1.0), 0);
    photon_path_transport_set_triangle(
        &scene->triangleMesh.triangles[1], 1.0, vec3(0.0, 0.0, -1.0), 1);
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static bool photon_path_transport_build_two_object_scene(RuntimeScene3D* scene,
                                                         double second_z) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = calloc(2u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        calloc(2u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 2;
    for (int i = 0; i < 2; ++i) {
        scene->primitives[i].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        scene->primitives[i].source.kind =
            RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        scene->primitives[i].source.sceneObjectIndex = 51 + i;
    }
    photon_path_transport_set_triangle(
        &scene->triangleMesh.triangles[0], 1.0, vec3(0.0, 0.0, 1.0), 0);
    scene->triangleMesh.triangles[0].sceneObjectIndex = 51;
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    photon_path_transport_set_triangle(
        &scene->triangleMesh.triangles[1],
        second_z,
        second_z > 1.0 ? vec3(0.0, 0.0, -1.0) : vec3(0.0, 0.0, 1.0),
        0);
    scene->triangleMesh.triangles[1].sceneObjectIndex = 52;
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static void photon_path_transport_set_large_triangle(RuntimeTriangle3D* triangle,
                                                     double z,
                                                     Vec3 normal,
                                                     int local_index) {
    memset(triangle, 0, sizeof(*triangle));
    triangle->p0 = vec3(-20.0, -20.0, z);
    triangle->p1 = vec3(20.0, -20.0, z);
    triangle->p2 = vec3(0.0, 20.0, z);
    triangle->normal = normal;
    triangle->twoSided = true;
    triangle->primitiveIndex = 0;
    triangle->sceneObjectIndex = 61;
    triangle->localTriangleIndex = local_index;
}

static bool photon_path_transport_build_tir_slab(RuntimeScene3D* scene) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = calloc(1u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        calloc(2u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 2;
    scene->primitives[0].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 61;
    photon_path_transport_set_large_triangle(
        &scene->triangleMesh.triangles[0], 0.0, vec3(0.0, 0.0, -1.0), 0);
    photon_path_transport_set_large_triangle(
        &scene->triangleMesh.triangles[1], 1.0, vec3(0.0, 0.0, 1.0), 1);
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static bool photon_path_transport_build_nested_media(RuntimeScene3D* scene) {
    static const double z[4] = {3.0, 2.0, 1.0, 0.0};
    static const int object_id[4] = {71, 72, 72, 71};
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 4;
    scene->primitives = calloc(2u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        calloc(4u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 4;
    for (int i = 0; i < 2; ++i) {
        scene->primitives[i].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        scene->primitives[i].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        scene->primitives[i].source.sceneObjectIndex = 71 + i;
    }
    for (int i = 0; i < 4; ++i) {
        Vec3 normal = i < 2 ? vec3(0.0, 0.0, 1.0)
                            : vec3(0.0, 0.0, -1.0);
        photon_path_transport_set_large_triangle(
            &scene->triangleMesh.triangles[i], z[i], normal, i);
        scene->triangleMesh.triangles[i].sceneObjectIndex = object_id[i];
        scene->triangleMesh.triangles[i].primitiveIndex = object_id[i] - 71;
    }
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static RuntimeCausticPhotonSample3D photon_path_transport_sample(void) {
    RuntimeCausticPhotonSample3D sample;
    memset(&sample, 0, sizeof(sample));
    sample.photonId = 22001u;
    sample.sampleIndex = 71u;
    sample.rngSeed = 0x22a15e77u;
    sample.position = vec3(0.0, 0.0, 0.5);
    sample.direction = vec3(0.0, 0.0, -1.0);
    sample.flux = vec3(0.8, 0.6, 0.4);
    sample.emissionPdf = 0.25;
    return sample;
}

static int test_runtime_caustic_photon_path_transport_defaults(void) {
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    assert_true("runtime_caustic_photon_path_transport_defaults",
                settings.sceneTrace.maxDepth ==
                        RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS &&
                    settings.sceneTrace.traceRoute ==
                        RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS &&
                    settings.applyRoulette &&
                    settings.continueTotalInternalReflection &&
                    settings.mediumFailurePolicy ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_FAILURE_FAIL_CLOSED);
    return 0;
}

static int test_runtime_caustic_photon_path_transport_reflective_depth_loop(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    RuntimeCausticPhotonSceneTrace3D replay;
    PhotonPathTransportMaterialFixture3D fixture;

    assert_true("runtime_caustic_photon_path_transport_build_mirror_box",
                photon_path_transport_build_mirror_box(&scene));
    memset(&fixture, 0, sizeof(fixture));
    fixture.material = photon_path_transport_mirror();
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 3u;
    settings.sceneTrace.materialResolver = photon_path_transport_fixture_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;

    assert_true("runtime_caustic_photon_path_transport_reflective_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &trace));
    fixture.calls = 0u;
    assert_true("runtime_caustic_photon_path_transport_reflective_replay",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &replay));
    assert_true("runtime_caustic_photon_path_transport_reflective_ledger",
                trace.trace.valid && trace.readback.succeeded &&
                    trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH &&
                    trace.readback.hitEventCount == 3u &&
                    trace.readback.intersectionCount == 3u &&
                    trace.readback.materialResolveCount == 3u &&
                    trace.readback.routeStats.tlasTraceCalls == 3u &&
                    trace.trace.eventCount == 4u &&
                    trace.trace.finalState.depth == 3u &&
                    trace.trace.finalState.terminated &&
                    trace.trace.finalState.rejectReason ==
                        RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH &&
                    trace.trace.debug.reflectedBranchCount == 3u);
    for (uint32_t i = 0u; i < 3u; ++i) {
        assert_true("runtime_caustic_photon_path_transport_per_hit_state",
                    trace.hitEvents[i].depth == i + 1u &&
                        trace.hitEvents[i].hit.sceneObjectIndex == 41 &&
                        trace.hitEvents[i].usedSeededBsdfSamples &&
                        trace.hitEvents[i].bsdfSampleStream.depth == i + 1u &&
                        trace.hitEvents[i].bsdfSelection.lobe ==
                            RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR &&
                        trace.trace.events[i + 1u].depth == i + 1u);
        assert_true("runtime_caustic_photon_path_transport_replay_stream",
                    memcmp(&trace.hitEvents[i].bsdfSampleStream,
                           &replay.hitEvents[i].bsdfSampleStream,
                           sizeof(trace.hitEvents[i].bsdfSampleStream)) == 0);
    }
    assert_close("runtime_caustic_photon_path_transport_pdf",
                 trace.trace.finalState.pathPdf,
                 sample.emissionPdf,
                 1.0e-12);
    assert_close("runtime_caustic_photon_path_transport_flux_r",
                 trace.trace.finalState.throughput.x,
                 sample.flux.x,
                 1.0e-12);
    assert_true("runtime_caustic_photon_path_transport_direction_sequence",
                trace.trace.events[1].outgoingDirection.z > 0.999 &&
                    trace.trace.events[2].outgoingDirection.z < -0.999 &&
                    trace.trace.events[3].outgoingDirection.z > 0.999);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_path_transport_two_object_reflection(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    PhotonPathTransportMultiMaterialFixture3D fixture;

    sample.position = vec3(0.0, 0.0, 2.0);
    assert_true("runtime_caustic_photon_path_transport_two_object_reflect_build",
                photon_path_transport_build_two_object_scene(&scene, 3.0));
    memset(&fixture, 0, sizeof(fixture));
    fixture.object51 = photon_path_transport_mirror();
    fixture.object52 = photon_path_transport_mirror();
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 2u;
    settings.sceneTrace.materialResolver = photon_path_transport_multi_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;

    assert_true("runtime_caustic_photon_path_transport_two_object_reflect_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &trace));
    assert_true("runtime_caustic_photon_path_transport_two_object_reflect_ledger",
                fixture.calls == 2u && fixture.resolvedObjects[0] == 51 &&
                    fixture.resolvedObjects[1] == 52 &&
                    trace.hitEvents[0].hit.sceneObjectIndex == 51 &&
                    trace.hitEvents[1].hit.sceneObjectIndex == 52 &&
                    trace.trace.events[1].outgoingDirection.z > 0.999 &&
                    trace.trace.events[2].outgoingDirection.z < -0.999 &&
                    trace.trace.debug.reflectedBranchCount == 2u &&
                    trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH);
    assert_close("runtime_caustic_photon_path_transport_two_object_reflect_pdf",
                 trace.trace.finalState.pathPdf,
                 sample.emissionPdf,
                 1.0e-12);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_path_transport_two_object_transmission(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    PhotonPathTransportMultiMaterialFixture3D fixture;
    RuntimeCausticPhotonBsdfSampleStream3D stream;
    bool found_transmission_sample = false;

    sample.position = vec3(0.0, 0.0, 2.0);
    for (uint64_t i = 0u; i < 128u; ++i) {
        sample.photonId = 23000u + i;
        sample.sampleIndex = i;
        if (RuntimeCausticPhotonBsdfSampling3D_Generate(&sample, 1u, &stream) &&
            stream.bsdfSample.lobeUnitSample > 0.2) {
            found_transmission_sample = true;
            break;
        }
    }
    assert_true("runtime_caustic_photon_path_transport_find_transmission_sample",
                found_transmission_sample);
    assert_true("runtime_caustic_photon_path_transport_two_object_transmit_build",
                photon_path_transport_build_two_object_scene(&scene, 0.0));
    memset(&fixture, 0, sizeof(fixture));
    fixture.object51 = photon_path_transport_glass();
    fixture.object51.materialId = 51;
    fixture.object52 = photon_path_transport_mirror();
    fixture.object52.materialId = 52;
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 2u;
    settings.sceneTrace.materialResolver = photon_path_transport_multi_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;

    assert_true("runtime_caustic_photon_path_transport_two_object_transmit_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &trace));
    assert_true("runtime_caustic_photon_path_transport_two_object_transmit_ledger",
                fixture.calls == 2u && fixture.resolvedObjects[0] == 51 &&
                    fixture.resolvedObjects[1] == 52 &&
                    trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                    trace.hitEvents[0].bsdfDirection.dielectric.entering &&
                    trace.hitEvents[0].bsdfDirection.outgoingDirection.z < -0.999 &&
                    trace.hitEvents[1].hit.sceneObjectIndex == 52 &&
                    trace.trace.events[2].outgoingDirection.z > 0.999 &&
                    trace.trace.dielectricEventCount == 1u &&
                    trace.readback.mediumTransitionCount == 1u &&
                    trace.readback.mediumTransitionFailureCount == 0u &&
                    trace.hitEvents[0].mediumTransition.succeeded &&
                    trace.hitEvents[0].mediumTransition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED &&
                    trace.hitEvents[0].mediumTransition.depthBefore == 0u &&
                    trace.hitEvents[0].mediumTransition.depthAfter == 1u &&
                    trace.finalMediumStack.pushCount == 1u &&
                    RuntimeCausticPhotonMediumStack3D_Depth(
                        &trace.finalMediumStack) == 1u &&
                    trace.trace.debug.refractedBranchCount == 1u &&
                    trace.trace.debug.reflectedBranchCount == 1u);
    assert_close("runtime_caustic_photon_path_transport_two_object_transmit_pdf",
                 trace.trace.finalState.pathPdf,
                 sample.emissionPdf *
                     trace.hitEvents[0].bsdfSelection.branchPdf,
                 1.0e-12);
    assert_close("runtime_caustic_photon_path_transport_two_object_transmit_flux",
                 trace.trace.finalState.throughput.x,
                 sample.flux.x,
                 1.0e-12);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_path_transport_tir_continuation(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    PhotonPathTransportMaterialFixture3D fixture;

    sample.photonId = 24001u;
    sample.sampleIndex = 91u;
    sample.position = vec3(0.0, 0.0, 0.5);
    sample.direction = vec3_normalize(vec3(0.75, 0.0, -0.6614378278));
    assert_true("runtime_caustic_photon_path_transport_tir_build",
                photon_path_transport_build_tir_slab(&scene));
    memset(&fixture, 0, sizeof(fixture));
    fixture.material = photon_path_transport_glass();
    fixture.material.materialId = 61;
    fixture.material.baseColorR = 0.25;
    fixture.material.baseColorG = 0.5;
    fixture.material.absorptionDistance = 2.0;
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 4u;
    settings.sceneTrace.materialResolver = photon_path_transport_fixture_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;
    settings.hasInitialMediumStack = true;
    {
        RuntimeCausticPhotonMediumEntry3D glass;
        RuntimeCausticPhotonMediumTransition3D transition;
        memset(&glass, 0, sizeof(glass));
        RuntimeCausticPhotonMediumEntry3D_FromMaterial(
            &fixture.material, 61, 0.0, &glass);
        RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
            &settings.initialMediumStack, &glass, true, false, &transition);
    }

    assert_true("runtime_caustic_photon_path_transport_tir_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &trace));
    assert_true("runtime_caustic_photon_path_transport_tir_ledger",
                fixture.calls == 4u && trace.readback.hitEventCount == 4u &&
                    trace.readback.intersectionCount == 4u &&
                    trace.trace.dielectricEventCount == 4u &&
                    trace.trace.debug.totalInternalReflectionCount == 4u &&
                    trace.trace.debug.reflectedBranchCount == 4u &&
                    trace.trace.debug.refractedBranchCount == 0u &&
                    trace.readback.mediumTransitionCount == 4u &&
                    trace.readback.mediumTransitionFailureCount == 0u &&
                    trace.readback.attenuatedSegmentCount == 4u &&
                    trace.readback.attenuatedSegmentDistance > 4.5 &&
                    trace.readback.mediumAbsorbedFlux.x > 0.0 &&
                    trace.finalMediumStack.tirNoChangeCount == 4u &&
                    RuntimeCausticPhotonMediumStack3D_Depth(
                        &trace.finalMediumStack) == 1u &&
                    trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH);
    for (uint32_t i = 0u; i < 4u; ++i) {
        assert_true("runtime_caustic_photon_path_transport_tir_per_hit",
                    trace.hitEvents[i].hit.sceneObjectIndex == 61 &&
                        trace.hitEvents[i].bsdfSelection.lobe ==
                            RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                        trace.hitEvents[i].bsdfSelection.branchPdf == 1.0 &&
                        trace.hitEvents[i].bsdfDirection.valid &&
                        trace.hitEvents[i].bsdfDirection.totalInternalReflection &&
                        trace.hitEvents[i].dielectric.totalInternalReflection &&
                        trace.hitEvents[i].dielectric.selectedBranch ==
                            RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION &&
                        trace.hitEvents[i].mediumTransition.succeeded &&
                        trace.hitEvents[i].mediumTransition.reason ==
                            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_TIR_NO_CHANGE &&
                        !trace.hitEvents[i].mediumTransition.stackChanged &&
                        trace.hitEvents[i].mediumTransition.depthBefore == 1u &&
                        trace.hitEvents[i].mediumTransition.depthAfter == 1u &&
                        trace.hitEvents[i].segmentMedium.mediumId == 61 &&
                        trace.hitEvents[i].segmentAttenuationApplied &&
                        trace.hitEvents[i].segmentDistance > 0.0 &&
                        trace.hitEvents[i].segmentTransmittance.x < 1.0 &&
                        trace.hitEvents[i].dielectric.etaFrom >
                            trace.hitEvents[i].dielectric.etaTo);
    }
    assert_true("runtime_caustic_photon_path_transport_tir_direction_sequence",
                trace.trace.events[1].outgoingDirection.z > 0.0 &&
                    trace.trace.events[2].outgoingDirection.z < 0.0 &&
                    trace.trace.events[3].outgoingDirection.z > 0.0 &&
                    trace.trace.events[4].outgoingDirection.z < 0.0);
    assert_close("runtime_caustic_photon_path_transport_tir_pdf",
                 trace.trace.finalState.pathPdf,
                 sample.emissionPdf,
                 1.0e-12);
    assert_close("runtime_caustic_photon_path_transport_tir_flux",
                 trace.trace.finalState.throughput.x,
                 sample.flux.x *
                     pow(0.25,
                         trace.readback.attenuatedSegmentDistance /
                             fixture.material.absorptionDistance),
                 1.0e-12);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_path_transport_nested_ior_replay(void) {
    static const int object_id[4] = {71, 72, 72, 71};
    static const int material_id[4] = {701, 702, 702, 701};
    static const double eta_from[4] = {1.0, 1.5, 1.33, 1.5};
    static const double eta_to[4] = {1.5, 1.33, 1.5, 1.0};
    static const uint32_t depth_before[4] = {0u, 1u, 2u, 1u};
    static const uint32_t depth_after[4] = {1u, 2u, 1u, 0u};
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D trace;
    RuntimeCausticPhotonSceneTrace3D replay;
    PhotonPathTransportNestedMaterialFixture3D fixture;
    bool found_closed_path = false;

    sample.position = vec3(0.0, 0.0, 4.0);
    sample.direction = vec3_normalize(vec3(0.35, 0.0, -0.9367496998));
    assert_true("runtime_caustic_photon_path_transport_nested_build",
                photon_path_transport_build_nested_media(&scene));
    memset(&fixture, 0, sizeof(fixture));
    fixture.glass = photon_path_transport_glass();
    fixture.glass.materialId = 701;
    fixture.glass.opticalIor = 1.5;
    fixture.glass.bsdf.ior = 1.5;
    fixture.water = photon_path_transport_glass();
    fixture.water.materialId = 702;
    fixture.water.opticalIor = 1.33;
    fixture.water.bsdf.ior = 1.33;
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 5u;
    settings.sceneTrace.materialResolver = photon_path_transport_nested_material;
    settings.sceneTrace.materialResolverUserData = &fixture;
    settings.applyRoulette = false;

    for (uint64_t i = 0u; i < 2048u; ++i) {
        sample.photonId = 25000u + i;
        sample.sampleIndex = 100u + i;
        fixture.calls = 0u;
        if (RuntimeCausticPhotonPathTransport3D_Trace(
                &scene, &sample, &settings, &trace) &&
            trace.trace.dielectricEventCount == 4u &&
            trace.readback.mediumTransitionCount == 4u &&
            RuntimeCausticPhotonMediumStack3D_Depth(&trace.finalMediumStack) ==
                0u) {
            found_closed_path = true;
            break;
        }
    }
    assert_true("runtime_caustic_photon_path_transport_nested_find_path",
                found_closed_path);
    fixture.calls = 0u;
    assert_true("runtime_caustic_photon_path_transport_nested_replay",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &settings, &replay));
    assert_true("runtime_caustic_photon_path_transport_nested_ledger",
                trace.readback.succeeded && replay.readback.succeeded &&
                    trace.readback.hitEventCount == 4u && fixture.calls == 4u &&
                    trace.finalMediumStack.pushCount == 2u &&
                    trace.finalMediumStack.popCount == 2u &&
                    trace.finalMediumStack.maxDepth == 2u &&
                    trace.readback.mediumTransitionFailureCount == 0u &&
                    trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED);
    for (uint32_t i = 0u; i < 4u; ++i) {
        RuntimeCausticPhotonSceneHitEvent3D* hit = &trace.hitEvents[i];
        RuntimeCausticPhotonSceneHitEvent3D* replay_hit = &replay.hitEvents[i];
        assert_true("runtime_caustic_photon_path_transport_nested_identity",
                    hit->hit.sceneObjectIndex == object_id[i] &&
                        hit->material.materialId == material_id[i] &&
                        hit->mediumTransition.boundary.sceneObjectIndex ==
                            object_id[i] &&
                        hit->mediumTransition.boundary.materialId == material_id[i]);
        assert_true("runtime_caustic_photon_path_transport_nested_transition",
                    hit->mediumTransition.succeeded &&
                        hit->mediumTransition.depthBefore == depth_before[i] &&
                        hit->mediumTransition.depthAfter == depth_after[i] &&
                        hit->mediumTransition.reason ==
                            (i < 2u
                                 ? RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED
                                 : RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED));
        assert_close("runtime_caustic_photon_path_transport_nested_eta_from",
                     hit->dielectric.etaFrom,
                     eta_from[i],
                     1.0e-12);
        assert_close("runtime_caustic_photon_path_transport_nested_eta_to",
                     hit->dielectric.etaTo,
                     eta_to[i],
                     1.0e-12);
        assert_true("runtime_caustic_photon_path_transport_nested_direction_replay",
                    vec3_length(vec3_sub(
                        hit->bsdfDirection.outgoingDirection,
                        replay_hit->bsdfDirection.outgoingDirection)) < 1.0e-12);
    }
    assert_true("runtime_caustic_photon_path_transport_nested_refraction_shape",
                trace.hitEvents[0].bsdfDirection.outgoingDirection.x <
                        sample.direction.x &&
                    trace.hitEvents[1].bsdfDirection.outgoingDirection.x >
                        trace.hitEvents[0].bsdfDirection.outgoingDirection.x &&
                    trace.hitEvents[2].bsdfDirection.outgoingDirection.x <
                        trace.hitEvents[1].bsdfDirection.outgoingDirection.x &&
                    trace.hitEvents[3].bsdfDirection.outgoingDirection.x >
                        trace.hitEvents[2].bsdfDirection.outgoingDirection.x &&
                    fabs(trace.hitEvents[3].bsdfDirection.outgoingDirection.x -
                         sample.direction.x) < 1.0e-9);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_path_population_transaction(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    RuntimeCausticPhotonPathTransportSettings3D transport_settings;
    RuntimeCausticPhotonPathPopulationSettings3D population_settings;
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonPathPopulationBatch3D batch;
    RuntimeCausticPhotonSceneTrace3D trace;
    RuntimeCausticPhotonSceneTrace3D invalid_trace;
    RuntimeCausticPhotonSceneTrace3D terminal_trace;
    PhotonPathTransportMultiMaterialFixture3D fixture;
    RuntimeCausticPhotonBsdfSampleStream3D stream;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;
    bool found_transmission_sample = false;

    sample.position = vec3(0.0, 0.0, 2.0);
    for (uint64_t i = 0u; i < 128u; ++i) {
        sample.photonId = 25000u + i;
        sample.sampleIndex = i;
        if (RuntimeCausticPhotonBsdfSampling3D_Generate(&sample, 1u, &stream) &&
            stream.bsdfSample.lobeUnitSample > 0.2) {
            found_transmission_sample = true;
            break;
        }
    }
    assert_true("runtime_caustic_photon_path_population_find_transmission_sample",
                found_transmission_sample);
    assert_true("runtime_caustic_photon_path_population_build",
                photon_path_transport_build_two_object_scene(&scene, 0.0));
    memset(&fixture, 0, sizeof(fixture));
    fixture.object51 = photon_path_transport_glass();
    fixture.object52 = photon_path_transport_diffuse();
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&transport_settings);
    transport_settings.sceneTrace.maxDepth = 2u;
    transport_settings.sceneTrace.materialResolver =
        photon_path_transport_multi_material;
    transport_settings.sceneTrace.materialResolverUserData = &fixture;
    transport_settings.applyRoulette = false;
    assert_true("runtime_caustic_photon_path_population_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &transport_settings, &trace));
    assert_true("runtime_caustic_photon_path_population_trace_shape",
                trace.readback.hitEventCount == 2u &&
                    trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                    trace.hitEvents[1].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE &&
                    trace.hitEvents[0].pathPdfBefore == sample.emissionPdf &&
                    trace.hitEvents[1].pathStart.z == 1.0 &&
                    trace.hitEvents[1].pathPdfBefore ==
                        trace.hitEvents[0].pathPdfAfter);

    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    assert_true("runtime_caustic_photon_path_population_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 2u));
    assert_true("runtime_caustic_photon_path_population_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 3u));
    RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&population_settings);
    assert_true("runtime_caustic_photon_path_population_store",
                RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                    &trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback));
    assert_true("runtime_caustic_photon_path_population_store_readback",
                readback.succeeded && readback.preflightAccepted &&
                    readback.diffuseReceiverCount == 1u &&
                    readback.beamCandidateCount == 2u &&
                    readback.transparentHitCount == 1u &&
                    readback.storedSurfaceCount == 1u &&
                    readback.storedBeamCount == 2u &&
                    surface_map.recordCount == 1u && beam_map.segmentCount == 2u &&
                    surface_map.records[0].sceneObjectIndex == 52 &&
                    surface_map.records[0].depth == 2u &&
                    beam_map.segments[0].start.z == 2.0 &&
                    beam_map.segments[0].end.z == 1.0 &&
                    beam_map.segments[0].mediumId == 0 &&
                    beam_map.segments[1].start.z == 1.0 &&
                    beam_map.segments[1].end.z == 0.0 &&
                    beam_map.segments[1].mediumId ==
                        trace.hitEvents[1].segmentMedium.mediumId);
    assert_close("runtime_caustic_photon_path_population_surface_pdf",
                 surface_map.records[0].pathPdf,
                 trace.hitEvents[1].pathPdfBefore,
                 1.0e-12);
    assert_close("runtime_caustic_photon_path_population_surface_flux",
                 readback.storedSurfaceFlux.x,
                 trace.hitEvents[1].bsdfSelection.throughputBefore.x,
                 1.0e-12);
    memset(&batch, 0, sizeof(batch));
    RuntimeCausticPhotonPathPopulationBatch3D_Accumulate(&batch, &readback);
    assert_true("runtime_caustic_photon_path_population_batch",
                batch.pathCount == 1u && batch.succeededPathCount == 1u &&
                    batch.storedSurfaceCount == 1u && batch.storedBeamCount == 2u &&
                    batch.mediumAbsorbedFlux.x ==
                        trace.readback.mediumAbsorbedFlux.x);

    RuntimeCausticPhotonMap3D_Clear(&surface_map);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    assert_true("runtime_caustic_photon_path_population_small_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 1u));
    assert_true("runtime_caustic_photon_path_population_capacity_reject",
                !RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                    &trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback));
    assert_true("runtime_caustic_photon_path_population_capacity_atomic",
                readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED &&
                    !readback.preflightAccepted && surface_map.recordCount == 0u &&
                    beam_map.segmentCount == 0u &&
                    surface_map.storeAttemptCount == 0u &&
                    beam_map.storeAttemptCount == 0u &&
                    readback.storedSurfaceFlux.x == 0.0 &&
                    readback.storedBeamFlux.x == 0.0);

    invalid_trace = trace;
    invalid_trace.trace.valid = false;
    assert_true("runtime_caustic_photon_path_population_invalid_trace_reject",
                !RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                    &invalid_trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback) &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE &&
                    surface_map.recordCount == 0u && beam_map.segmentCount == 0u);

    terminal_trace = trace;
    terminal_trace.hitEvents[1].bsdfSelection.lobe =
        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE;
    terminal_trace.hitEvents[1].bsdfSelection.termination =
        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE;
    terminal_trace.hitEvents[1].bsdfDirection.valid = false;
    terminal_trace.readback.termination =
        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EMISSIVE;
    assert_true("runtime_caustic_photon_path_population_terminal_no_write",
                !RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                    &terminal_trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback) &&
                    readback.terminalHitCount == 1u &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER &&
                    surface_map.recordCount == 0u && beam_map.segmentCount == 0u);

    memset(&fixture, 0, sizeof(fixture));
    fixture.object51 = photon_path_transport_glass();
    fixture.object52 = photon_path_transport_glass();
    transport_settings.sceneTrace.materialResolverUserData = &fixture;
    assert_true("runtime_caustic_photon_path_population_transparent_trace",
                RuntimeCausticPhotonPathTransport3D_Trace(
                    &scene, &sample, &transport_settings, &trace));
    assert_true("runtime_caustic_photon_path_population_transparent_no_write",
                !RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
                    &trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback) &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER &&
                    surface_map.recordCount == 0u && beam_map.segmentCount == 0u);

    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

int run_test_runtime_caustic_photon_path_transport_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_path_transport_defaults();
    failures += test_runtime_caustic_photon_path_transport_reflective_depth_loop();
    failures += test_runtime_caustic_photon_path_transport_two_object_reflection();
    failures += test_runtime_caustic_photon_path_transport_two_object_transmission();
    failures += test_runtime_caustic_photon_path_transport_tir_continuation();
    failures += test_runtime_caustic_photon_path_transport_nested_ior_replay();
    failures += test_runtime_caustic_photon_path_population_transaction();
    return failures;
}
