#include "test_runtime_caustic_photon_path_transport_3d.h"

#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_transport_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "test_support.h"

typedef struct {
    RuntimeMaterialPayload3D material;
    uint32_t calls;
} PhotonPathTransportMaterialFixture3D;

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
                    settings.continueTotalInternalReflection);
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

int run_test_runtime_caustic_photon_path_transport_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_path_transport_defaults();
    failures += test_runtime_caustic_photon_path_transport_reflective_depth_loop();
    return failures;
}
