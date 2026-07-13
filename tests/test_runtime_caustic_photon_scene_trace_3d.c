#include "test_runtime_caustic_photon_scene_trace_3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_photon_scene_trace_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "test_support.h"

typedef struct {
    bool resolve;
    bool transparent;
    uint32_t calls;
} PhotonSceneTraceMaterialFixture3D;

typedef struct {
    RuntimeMaterialPayload3D payload;
    uint32_t calls;
} PhotonSceneTraceBsdfFixture3D;

static bool photon_scene_trace_fixture_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonSceneTraceMaterialFixture3D* fixture =
        (PhotonSceneTraceMaterialFixture3D*)user_data;
    if (!hit || !out_payload || !fixture) return false;
    fixture->calls++;
    RuntimeMaterialPayload3D_Reset(out_payload);
    if (!fixture->resolve) return false;
    out_payload->valid = true;
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    out_payload->materialId = 77;
    out_payload->baseColorR = 0.96;
    out_payload->baseColorG = 1.0;
    out_payload->baseColorB = 0.92;
    out_payload->transparency = fixture->transparent ? 1.0 : 0.0;
    out_payload->opticalIor = 1.45;
    out_payload->absorptionDistance = 8.0;
    out_payload->bsdf.ior = 1.45;
    return true;
}

static bool photon_scene_trace_bsdf_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonSceneTraceBsdfFixture3D* fixture =
        (PhotonSceneTraceBsdfFixture3D*)user_data;
    if (!hit || !out_payload || !fixture) return false;
    fixture->calls++;
    *out_payload = fixture->payload;
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    return out_payload->valid;
}

static RuntimeMaterialPayload3D photon_scene_trace_bsdf_payload(void) {
    RuntimeMaterialPayload3D payload;
    RuntimeMaterialPayload3D_Reset(&payload);
    payload.valid = true;
    payload.materialId = 91;
    payload.baseColorR = 0.8;
    payload.baseColorG = 0.6;
    payload.baseColorB = 0.4;
    payload.opticalIor = 1.5;
    payload.bsdf.ior = 1.5;
    payload.bsdf.diffuseWeight = 1.0;
    payload.bsdf.roughness = 0.5;
    return payload;
}

static bool photon_scene_trace_build_slab(RuntimeScene3D* scene) {
    RuntimePrimitive3D* primitive = NULL;
    RuntimeTriangle3D* entry = NULL;
    RuntimeTriangle3D* exit = NULL;
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = (RuntimePrimitive3D*)calloc(1u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc(2u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 2;
    primitive = &scene->primitives[0];
    primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.sceneObjectIndex = 31;
    snprintf(primitive->source.objectId,
             sizeof(primitive->source.objectId),
             "%s",
             "ppm20_glass_slab");

    entry = &scene->triangleMesh.triangles[0];
    entry->p0 = vec3(-2.0, -2.0, 1.0);
    entry->p1 = vec3(2.0, -2.0, 1.0);
    entry->p2 = vec3(0.0, 2.0, 1.0);
    entry->normal = vec3(0.0, 0.0, 1.0);
    entry->twoSided = true;
    entry->primitiveIndex = 0;
    entry->sceneObjectIndex = 31;
    entry->localTriangleIndex = 0;

    exit = &scene->triangleMesh.triangles[1];
    exit->p0 = vec3(-2.0, -2.0, 0.0);
    exit->p1 = vec3(0.0, 2.0, 0.0);
    exit->p2 = vec3(2.0, -2.0, 0.0);
    exit->normal = vec3(0.0, 0.0, -1.0);
    exit->twoSided = true;
    exit->primitiveIndex = 0;
    exit->sceneObjectIndex = 31;
    exit->localTriangleIndex = 1;
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static RuntimeCausticPhotonSample3D photon_scene_trace_sample(void) {
    RuntimeCausticPhotonSample3D sample;
    memset(&sample, 0, sizeof(sample));
    sample.photonId = 20001u;
    sample.position = vec3(0.0, 0.0, 2.0);
    sample.direction = vec3(0.0, 0.0, -1.0);
    sample.flux = vec3(2.0, 1.5, 1.0);
    sample.emissionPdf = 0.5;
    return sample;
}

static bool photon_scene_trace_build_oracle(
    const RuntimeCausticPhotonSample3D* sample,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeMaterialPayload3D payload;
    RuntimeCausticLensTraversalProfile3D profile;
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry;
    RuntimeCausticLensInterfaceEvent3D exit;
    bool tir = false;
    Vec3 inside_direction;
    Vec3 exit_direction;
    double entry_fresnel = 0.0;
    double exit_fresnel = 0.0;

    if (!sample || !out_trace) return false;
    RuntimeMaterialPayload3D_Reset(&payload);
    payload.valid = true;
    payload.baseColorR = 0.96;
    payload.baseColorG = 1.0;
    payload.baseColorB = 0.92;
    payload.transparency = 1.0;
    payload.opticalIor = 1.45;
    payload.absorptionDistance = 8.0;
    payload.bsdf.ior = 1.45;
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(&payload, &profile);

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    path.sceneObjectIndex = 31;
    path.primitiveIndex = 0;
    path.lightSamplePosition = sample->position;
    path.targetPosition = vec3(0.0, 0.0, 1.0);
    path.traversalProfile = profile;

    entry_fresnel = RuntimeCausticLensTransport3D_FresnelSchlick(
        sample->direction,
        vec3(0.0, 0.0, 1.0),
        profile.outsideIor,
        profile.materialIor);
    if (!RuntimeCausticLensTransport3D_Refract(sample->direction,
                                               vec3(0.0, 0.0, 1.0),
                                               profile.outsideIor,
                                               profile.materialIor,
                                               &inside_direction,
                                               &tir)) {
        return false;
    }
    exit_fresnel = RuntimeCausticLensTransport3D_FresnelSchlick(
        inside_direction,
        vec3(0.0, 0.0, -1.0),
        profile.materialIor,
        profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_direction,
                                               vec3(0.0, 0.0, -1.0),
                                               profile.materialIor,
                                               profile.outsideIor,
                                               &exit_direction,
                                               &tir)) {
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry);
    entry.position = vec3(0.0, 0.0, 1.0);
    entry.normal = vec3(0.0, 0.0, 1.0);
    entry.incidentDirection = sample->direction;
    entry.outgoingDirection = inside_direction;
    entry.etaFrom = profile.outsideIor;
    entry.etaTo = profile.materialIor;
    entry.fresnel = entry_fresnel;
    entry.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry)) return false;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit);
    exit.position = vec3(0.0, 0.0, 0.0);
    exit.normal = vec3(0.0, 0.0, -1.0);
    exit.incidentDirection = inside_direction;
    exit.outgoingDirection = exit_direction;
    exit.etaFrom = profile.materialIor;
    exit.etaTo = profile.outsideIor;
    exit.fresnel = exit_fresnel;
    exit.distanceInMedium = 1.0;
    exit.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit)) return false;
    path.postExitOrigin = exit.position;
    path.postExitDirection = exit_direction;
    path.throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        sample->flux,
        entry_fresnel,
        &profile);
    path.throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        path.throughput,
        exit_fresnel,
        &profile);
    path.throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        path.throughput,
        1.0,
        &profile);
    return RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(&path,
                                                               sample,
                                                               NULL,
                                                               out_trace);
}

static int test_runtime_caustic_photon_scene_trace_defaults(void) {
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    assert_true("runtime_caustic_photon_scene_trace_default_depth",
                settings.maxDepth == RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS);
    assert_true("runtime_caustic_photon_scene_trace_default_route",
                settings.traceRoute == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
    assert_true("runtime_caustic_photon_scene_trace_default_resolver",
                settings.materialResolver != NULL);
    assert_true("runtime_caustic_photon_scene_trace_labels",
                strcmp(RuntimeCausticPhotonSceneTermination3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT),
                       "dielectric_exit") == 0);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_accel_events_and_oracle_parity(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    RuntimeCausticPhotonTrace3D oracle_trace;
    RuntimeCausticPhotonSceneTraceParity3D parity;
    PhotonSceneTraceMaterialFixture3D material_fixture = {true, true, 0u};

    assert_true("runtime_caustic_photon_scene_trace_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    settings.materialResolver = photon_scene_trace_fixture_material;
    settings.materialResolverUserData = &material_fixture;

    assert_true("runtime_caustic_photon_scene_trace_general_trace",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &settings,
                    &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_general_state",
                scene_trace.trace.valid && scene_trace.readback.succeeded &&
                    scene_trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT);
    assert_true("runtime_caustic_photon_scene_trace_shared_accel",
                scene_trace.readback.usedSharedSceneAccelerationRoute &&
                    scene_trace.readback.routeStats.tlasTraceCalls == 2u &&
                    scene_trace.readback.routeStats.flattenedFallbackCalls == 0u);
    assert_true("runtime_caustic_photon_scene_trace_event_counts",
                scene_trace.trace.eventCount == 3u &&
                    scene_trace.trace.dielectricEventCount == 2u &&
                    scene_trace.readback.hitEventCount == 2u &&
                    scene_trace.readback.materialResolveCount == 2u &&
                    material_fixture.calls == 2u);
    assert_true("runtime_caustic_photon_scene_trace_identity",
                scene_trace.hitEvents[0].hit.sceneObjectIndex == 31 &&
                    scene_trace.hitEvents[0].hit.primitiveIndex == 0 &&
                    scene_trace.hitEvents[0].hit.triangleIndex == 0 &&
                    scene_trace.hitEvents[1].hit.triangleIndex == 1);
    assert_true("runtime_caustic_photon_scene_trace_material_payload",
                scene_trace.hitEvents[0].material.valid &&
                    scene_trace.hitEvents[0].material.materialId == 77 &&
                    scene_trace.hitEvents[1].material.materialId == 77);
    assert_true("runtime_caustic_photon_scene_trace_no_storage",
                scene_trace.trace.debug.storedSurfaceFlux.x == 0.0 &&
                    scene_trace.trace.debug.storedVolumeFlux.x == 0.0);

    assert_true("runtime_caustic_photon_scene_trace_build_oracle",
                photon_scene_trace_build_oracle(&sample, &oracle_trace));
    assert_true("runtime_caustic_photon_scene_trace_oracle_parity",
                RuntimeCausticPhotonSceneTrace3D_CompareDescriptorOracle(
                    &oracle_trace,
                    &scene_trace,
                    1.0e-9,
                    1.0e-9,
                    &parity));
    assert_true("runtime_caustic_photon_scene_trace_oracle_components",
                parity.matches && parity.identityMatches && parity.directionMatches &&
                    parity.branchMatches && parity.terminationMatches &&
                    parity.triangleIdentityPreserved && parity.mismatchReason[0] == '\0');
    assert_close("runtime_caustic_photon_scene_trace_oracle_direction_error",
                 parity.maxDirectionError,
                 0.0,
                 1.0e-12);
    assert_close("runtime_caustic_photon_scene_trace_oracle_flux_error",
                 parity.maxFluxError,
                 0.0,
                 1.0e-12);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_reports_material_failures(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    PhotonSceneTraceMaterialFixture3D material_fixture = {false, true, 0u};

    assert_true("runtime_caustic_photon_scene_trace_failure_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    settings.materialResolver = photon_scene_trace_fixture_material;
    settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_trace_material_failure",
                !RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &settings,
                    &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_material_failure_readback",
                scene_trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED &&
                    scene_trace.readback.materialResolveFailureCount == 1u &&
                    scene_trace.trace.finalState.rejectReason ==
                        RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_preserves_opaque_hit_payload(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    PhotonSceneTraceMaterialFixture3D material_fixture = {true, false, 0u};

    assert_true("runtime_caustic_photon_scene_trace_opaque_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    settings.materialResolver = photon_scene_trace_fixture_material;
    settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_trace_opaque_terminal",
                !RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &settings,
                    &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_opaque_payload",
                scene_trace.readback.hitEventCount == 1u &&
                    scene_trace.hitEvents[0].material.valid &&
                    scene_trace.hitEvents[0].material.materialId == 77 &&
                    scene_trace.hitEvents[0].termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE &&
                    scene_trace.trace.events[1].kind ==
                        RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_bsdf_scattering_events(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneBsdfSample3D bsdf_sample = {0};
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    PhotonSceneTraceBsdfFixture3D fixture;

    assert_true("runtime_caustic_photon_scene_trace_bsdf_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    settings.materialResolver = photon_scene_trace_bsdf_material;
    settings.materialResolverUserData = &fixture;
    bsdf_sample.lobeUnitSample = 0.5;
    bsdf_sample.directionSample.unitU = 0.0;
    bsdf_sample.directionSample.unitV = 0.0;

    memset(&fixture, 0, sizeof(fixture));
    fixture.payload = photon_scene_trace_bsdf_payload();
    assert_true("runtime_caustic_photon_scene_trace_bsdf_diffuse",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_diffuse_ledger",
                scene_trace.readback.succeeded &&
                    scene_trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY &&
                    scene_trace.readback.hitEventCount == 1u &&
                    scene_trace.trace.eventCount == 2u &&
                    scene_trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE &&
                    scene_trace.hitEvents[0].bsdfDirection.valid &&
                    scene_trace.trace.events[1].kind ==
                        RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE &&
                    scene_trace.trace.events[1].outgoingDirection.z > 0.999 &&
                    scene_trace.readback.routeStats.tlasTraceCalls == 1u);
    assert_close("runtime_caustic_photon_scene_trace_bsdf_diffuse_pdf",
                 scene_trace.trace.events[1].pathPdf,
                 sample.emissionPdf / 3.14159265358979323846,
                 1.0e-12);
    assert_close("runtime_caustic_photon_scene_trace_bsdf_diffuse_flux_r",
                 scene_trace.trace.events[1].throughput.x,
                 sample.flux.x * fixture.payload.baseColorR,
                 1.0e-12);

    fixture.calls = 0u;
    fixture.payload = photon_scene_trace_bsdf_payload();
    fixture.payload.bsdf.diffuseWeight = 0.0;
    fixture.payload.bsdf.specWeight = 1.0;
    fixture.payload.bsdf.roughness = 0.4;
    bsdf_sample.directionSample.unitV = 0.64;
    assert_true("runtime_caustic_photon_scene_trace_bsdf_glossy",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_glossy_ledger",
                scene_trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY &&
                    scene_trace.hitEvents[0].bsdfDirection.angularPdf > 0.0 &&
                    scene_trace.trace.events[1].outgoingDirection.x > 0.0 &&
                    scene_trace.trace.events[1].outgoingDirection.z > 0.0);

    fixture.calls = 0u;
    fixture.payload.bsdf.roughness = 0.0;
    assert_true("runtime_caustic_photon_scene_trace_bsdf_specular",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_specular_ledger",
                scene_trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR &&
                    scene_trace.hitEvents[0].bsdfDirection.angularPdf == 1.0 &&
                    scene_trace.trace.events[1].outgoingDirection.z > 0.999);

    fixture.calls = 0u;
    fixture.payload = photon_scene_trace_bsdf_payload();
    fixture.payload.transparency = 1.0;
    fixture.payload.bsdf.diffuseWeight = 0.0;
    fixture.payload.bsdf.specWeight = 0.0;
    bsdf_sample.lobeUnitSample = 0.5;
    assert_true("runtime_caustic_photon_scene_trace_bsdf_transmission",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_transmission_ledger",
                scene_trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                    scene_trace.hitEvents[0].bsdfSelection.branchPdf > 0.9 &&
                    scene_trace.hitEvents[0].bsdfDirection.dielectric.hasRefraction &&
                    scene_trace.trace.events[1].outgoingDirection.z < -0.999 &&
                    scene_trace.trace.finalState.active);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_bsdf_terminal_events(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneBsdfSample3D bsdf_sample = {0};
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    PhotonSceneTraceBsdfFixture3D fixture;

    assert_true("runtime_caustic_photon_scene_trace_bsdf_terminal_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    settings.materialResolver = photon_scene_trace_bsdf_material;
    settings.materialResolverUserData = &fixture;
    bsdf_sample.lobeUnitSample = 0.5;
    bsdf_sample.directionSample.unitU = 0.5;
    bsdf_sample.directionSample.unitV = 0.5;

    memset(&fixture, 0, sizeof(fixture));
    fixture.payload = photon_scene_trace_bsdf_payload();
    fixture.payload.emissive = 1.0;
    assert_true("runtime_caustic_photon_scene_trace_bsdf_emissive",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_emissive_ledger",
                scene_trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EMISSIVE &&
                    scene_trace.hitEvents[0].bsdfSelection.lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE &&
                    scene_trace.hitEvents[0].bsdfSelection.termination ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE &&
                    scene_trace.trace.events[1].kind ==
                        RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED &&
                    scene_trace.trace.finalState.terminated);

    fixture.calls = 0u;
    fixture.payload = photon_scene_trace_bsdf_payload();
    sample.flux = vec3(0.0, 0.0, 0.0);
    assert_true("runtime_caustic_photon_scene_trace_bsdf_absorbed",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
                    &scene, &sample, &bsdf_sample, &settings, &scene_trace));
    assert_true("runtime_caustic_photon_scene_trace_bsdf_absorbed_ledger",
                scene_trace.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED &&
                    scene_trace.hitEvents[0].bsdfSelection.termination ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED &&
                    scene_trace.trace.events[1].throughput.x == 0.0 &&
                    scene_trace.trace.finalState.rejectReason ==
                        RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_trace_seeded_roulette(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_trace_sample();
    RuntimeCausticPhotonSample3D survive_sample = sample;
    RuntimeCausticPhotonSample3D terminate_sample = sample;
    RuntimeCausticPhotonSceneTraceSettings3D settings;
    RuntimeCausticPhotonSceneTrace3D first;
    RuntimeCausticPhotonSceneTrace3D repeat;
    RuntimePathDepthPolicy3D policy = {0};
    PhotonSceneTraceBsdfFixture3D fixture;
    bool found_survive = false;
    bool found_terminate = false;

    assert_true("runtime_caustic_photon_scene_trace_seeded_build_slab",
                photon_scene_trace_build_slab(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings);
    memset(&fixture, 0, sizeof(fixture));
    fixture.payload = photon_scene_trace_bsdf_payload();
    settings.materialResolver = photon_scene_trace_bsdf_material;
    settings.materialResolverUserData = &fixture;
    sample.flux = vec3(0.25, 0.25, 0.25);
    policy.minDepthBeforeRoulette = 2;
    policy.rouletteThreshold = 1.0;

    for (uint64_t i = 0u; i < 256u && (!found_survive || !found_terminate); ++i) {
        RuntimeCausticPhotonBsdfSampleStream3D stream;
        RuntimeCausticPhotonSample3D candidate = sample;
        candidate.photonId += i;
        candidate.sampleIndex = i;
        if (!RuntimeCausticPhotonBsdfSampling3D_Generate(&candidate, 2u, &stream)) {
            continue;
        }
        if (!found_survive && stream.rouletteUnitSample < 0.1) {
            survive_sample = candidate;
            found_survive = true;
        }
        if (!found_terminate && stream.rouletteUnitSample > 0.9) {
            terminate_sample = candidate;
            found_terminate = true;
        }
    }
    assert_true("runtime_caustic_photon_scene_trace_seeded_find_branches",
                found_survive && found_terminate);

    assert_true("runtime_caustic_photon_scene_trace_seeded_survive",
                RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHitWithRoulette(
                    &scene, &survive_sample, 2u, &policy, &settings, &first));
    fixture.calls = 0u;
    assert_true("runtime_caustic_photon_scene_trace_seeded_replay",
                RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHitWithRoulette(
                    &scene, &survive_sample, 2u, &policy, &settings, &repeat) &&
                    memcmp(&first.hitEvents[0].bsdfSampleStream,
                           &repeat.hitEvents[0].bsdfSampleStream,
                           sizeof(first.hitEvents[0].bsdfSampleStream)) == 0);
    assert_true("runtime_caustic_photon_scene_trace_seeded_survive_ledger",
                first.hitEvents[0].usedSeededBsdfSamples &&
                    first.hitEvents[0].roulette.valid &&
                    first.hitEvents[0].roulette.evaluated &&
                    !first.hitEvents[0].roulette.terminated &&
                    first.trace.finalState.active &&
                    first.trace.events[1].kind == RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE &&
                    first.readback.routeStats.tlasTraceCalls == 1u);

    fixture.calls = 0u;
    assert_true("runtime_caustic_photon_scene_trace_seeded_terminate",
                RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHitWithRoulette(
                    &scene, &terminate_sample, 2u, &policy, &settings, &first));
    assert_true("runtime_caustic_photon_scene_trace_seeded_terminate_ledger",
                first.hitEvents[0].roulette.terminated &&
                    first.hitEvents[0].termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED &&
                    first.readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED &&
                    first.trace.events[1].kind ==
                        RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED &&
                    first.trace.finalState.terminated &&
                    first.trace.finalState.rejectReason ==
                        RUNTIME_CAUSTIC_PHOTON_REJECT_RUSSIAN_ROULETTE &&
                    first.trace.finalState.throughput.x == 0.0 &&
                    first.trace.debug.rejectedFlux.x > 0.0);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

int run_test_runtime_caustic_photon_scene_trace_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_scene_trace_defaults();
    failures += test_runtime_caustic_photon_scene_trace_accel_events_and_oracle_parity();
    failures += test_runtime_caustic_photon_scene_trace_reports_material_failures();
    failures += test_runtime_caustic_photon_scene_trace_preserves_opaque_hit_payload();
    failures += test_runtime_caustic_photon_scene_trace_bsdf_scattering_events();
    failures += test_runtime_caustic_photon_scene_trace_bsdf_terminal_events();
    failures += test_runtime_caustic_photon_scene_trace_seeded_roulette();
    return failures;
}
