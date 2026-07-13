#include "test_runtime_caustic_photon_scene_population_3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_scene_population_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "test_support.h"

typedef struct {
    bool resolveReceiver;
    bool receiverTransparent;
    uint32_t calls;
} PhotonScenePopulationMaterialFixture3D;

static bool photon_scene_population_fixture_material(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    PhotonScenePopulationMaterialFixture3D* fixture =
        (PhotonScenePopulationMaterialFixture3D*)user_data;
    bool receiver;
    if (!hit || !out_payload || !fixture) return false;
    fixture->calls++;
    receiver = hit->sceneObjectIndex == 71;
    RuntimeMaterialPayload3D_Reset(out_payload);
    if (receiver && !fixture->resolveReceiver) return false;
    out_payload->valid = true;
    out_payload->sceneObjectIndex = hit->sceneObjectIndex;
    out_payload->materialId = receiver ? 88 : 77;
    out_payload->baseColorR = receiver ? 0.7 : 0.96;
    out_payload->baseColorG = receiver ? 0.6 : 1.0;
    out_payload->baseColorB = receiver ? 0.5 : 0.92;
    out_payload->transparency =
        receiver ? (fixture->receiverTransparent ? 1.0 : 0.0) : 1.0;
    out_payload->opticalIor = receiver ? 1.0 : 1.45;
    out_payload->absorptionDistance = 8.0;
    out_payload->bsdf.ior = out_payload->opticalIor;
    return true;
}

static void photon_scene_population_set_triangle(RuntimeTriangle3D* triangle,
                                                 Vec3 p0,
                                                 Vec3 p1,
                                                 Vec3 p2,
                                                 Vec3 normal,
                                                 int primitive_index,
                                                 int object_index,
                                                 int local_triangle_index) {
    memset(triangle, 0, sizeof(*triangle));
    triangle->p0 = p0;
    triangle->p1 = p1;
    triangle->p2 = p2;
    triangle->normal = normal;
    triangle->twoSided = true;
    triangle->primitiveIndex = primitive_index;
    triangle->sceneObjectIndex = object_index;
    triangle->localTriangleIndex = local_triangle_index;
}

static bool photon_scene_population_build_fixture(RuntimeScene3D* scene) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 3;
    scene->primitives = (RuntimePrimitive3D*)calloc(2u, sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc(3u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 3;

    scene->primitives[0].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 31;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "ppm20_glass_slab");
    scene->primitives[1].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 71;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "ppm20_receiver");

    photon_scene_population_set_triangle(&scene->triangleMesh.triangles[0],
                                         vec3(-2.0, -2.0, 1.0),
                                         vec3(2.0, -2.0, 1.0),
                                         vec3(0.0, 2.0, 1.0),
                                         vec3(0.0, 0.0, 1.0),
                                         0,
                                         31,
                                         0);
    photon_scene_population_set_triangle(&scene->triangleMesh.triangles[1],
                                         vec3(-2.0, -2.0, 0.0),
                                         vec3(0.0, 2.0, 0.0),
                                         vec3(2.0, -2.0, 0.0),
                                         vec3(0.0, 0.0, -1.0),
                                         0,
                                         31,
                                         1);
    photon_scene_population_set_triangle(&scene->triangleMesh.triangles[2],
                                         vec3(-2.0, -2.0, -1.0),
                                         vec3(2.0, -2.0, -1.0),
                                         vec3(0.0, 2.0, -1.0),
                                         vec3(0.0, 0.0, 1.0),
                                         1,
                                         71,
                                         0);
    return RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene);
}

static RuntimeCausticPhotonSample3D photon_scene_population_sample(void) {
    RuntimeCausticPhotonSample3D sample;
    memset(&sample, 0, sizeof(sample));
    sample.photonId = 20002u;
    sample.position = vec3(0.0, 0.0, 2.0);
    sample.direction = vec3(0.0, 0.0, -1.0);
    sample.flux = vec3(2.0, 1.5, 1.0);
    sample.emissionPdf = 0.5;
    return sample;
}

static bool photon_scene_population_build_descriptor_trace(
    const RuntimeCausticPhotonSceneTrace3D* scene_trace,
    Vec3 receiver_crossing,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeCausticLensPath3D path;
    RuntimeMaterialPayload3D payload;
    if (!scene_trace || !out_trace || scene_trace->trace.dielectricEventCount != 2u) {
        return false;
    }
    RuntimeCausticLensTransport3D_DefaultPath(&path);
    RuntimeMaterialPayload3D_Reset(&payload);
    payload.valid = true;
    payload.baseColorR = 0.96;
    payload.baseColorG = 1.0;
    payload.baseColorB = 0.92;
    payload.transparency = 1.0;
    payload.opticalIor = 1.45;
    payload.absorptionDistance = 8.0;
    payload.bsdf.ior = 1.45;
    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(
        &payload,
        &path.traversalProfile);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    path.sceneObjectIndex = scene_trace->trace.dielectricEvents[0].sceneObjectIndex;
    path.primitiveIndex = scene_trace->trace.dielectricEvents[0].primitiveIndex;
    path.lightSamplePosition = scene_trace->trace.sample.position;
    path.targetPosition = scene_trace->trace.dielectricEvents[0].position;
    for (uint32_t i = 0u; i < scene_trace->trace.dielectricEventCount; ++i) {
        const RuntimeCausticPhotonDielectricEvent3D* source =
            &scene_trace->trace.dielectricEvents[i];
        RuntimeCausticLensInterfaceEvent3D event;
        RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&event);
        event.position = source->position;
        event.normal = source->normal;
        event.incidentDirection = source->incidentDirection;
        event.outgoingDirection = source->selectedDirection;
        event.etaFrom = source->etaFrom;
        event.etaTo = source->etaTo;
        event.fresnel = source->fresnel;
        event.distanceInMedium = source->distanceInMedium;
        event.refracted = true;
        if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &event)) {
            return false;
        }
    }
    path.postExitOrigin = scene_trace->trace.postExitOrigin;
    path.postExitDirection = scene_trace->trace.postExitDirection;
    path.insideDistance = scene_trace->trace.insideDistance;
    path.throughput = scene_trace->trace.finalState.throughput;
    if (!RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(
            &path,
            &scene_trace->trace.sample,
            NULL,
            out_trace)) {
        return false;
    }
    out_trace->receiverCrossing = receiver_crossing;
    out_trace->receiverPlaneT =
        vec3_length(vec3_sub(receiver_crossing, out_trace->postExitOrigin));
    return out_trace->receiverPlaneT > 0.0;
}

static void assert_vec3_close(const char* name, Vec3 actual, Vec3 expected) {
    assert_close(name, actual.x, expected.x, 1.0e-9);
    assert_close(name, actual.y, expected.y, 1.0e-9);
    assert_close(name, actual.z, expected.z, 1.0e-9);
}

static int test_runtime_caustic_photon_scene_population_defaults(void) {
    RuntimeCausticPhotonScenePopulationSettings3D settings;
    RuntimeCausticPhotonScenePopulation3D_DefaultSettings(&settings);
    assert_true("runtime_caustic_photon_scene_population_defaults",
                settings.storeSurface && settings.storeBeam &&
                    settings.requireOpaqueReceiver &&
                    settings.traceRoute == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS &&
                    settings.materialResolver != NULL);
    assert_true("runtime_caustic_photon_scene_population_label",
                strcmp(RuntimeCausticPhotonScenePopulationTermination3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE),
                       "complete") == 0);
    return 0;
}

static int test_runtime_caustic_photon_scene_population_maps_and_descriptor_ab(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_population_sample();
    PhotonScenePopulationMaterialFixture3D material_fixture = {true, false, 0u};
    RuntimeCausticPhotonSceneTraceSettings3D trace_settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    RuntimeCausticPhotonScenePopulationSettings3D population_settings;
    RuntimeCausticPhotonScenePopulationReadback3D readback;
    RuntimeCausticPhotonTrace3D descriptor_trace;
    RuntimeCausticPhotonMap3D general_surface;
    RuntimeCausticPhotonMap3D descriptor_surface;
    RuntimeCausticBeamMap3D general_beam;
    RuntimeCausticBeamMap3D descriptor_beam;
    const RuntimeCausticPhotonMapRecord3D* general_record;
    const RuntimeCausticPhotonMapRecord3D* descriptor_record;
    const RuntimeCausticPhotonVolumeBeamSegment3D* general_segment;
    const RuntimeCausticPhotonVolumeBeamSegment3D* descriptor_segment;

    assert_true("runtime_caustic_photon_scene_population_build_fixture",
                photon_scene_population_build_fixture(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&trace_settings);
    trace_settings.materialResolver = photon_scene_population_fixture_material;
    trace_settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_population_trace",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &trace_settings,
                    &scene_trace));

    RuntimeCausticPhotonMap3D_Init(&general_surface);
    RuntimeCausticPhotonMap3D_Init(&descriptor_surface);
    RuntimeCausticBeamMap3D_Init(&general_beam);
    RuntimeCausticBeamMap3D_Init(&descriptor_beam);
    assert_true("runtime_caustic_photon_scene_population_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&general_surface, 2u) &&
                    RuntimeCausticPhotonMap3D_Allocate(&descriptor_surface, 2u));
    assert_true("runtime_caustic_photon_scene_population_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&general_beam, 2u) &&
                    RuntimeCausticBeamMap3D_Allocate(&descriptor_beam, 2u));

    RuntimeCausticPhotonScenePopulation3D_DefaultSettings(&population_settings);
    population_settings.materialResolver = photon_scene_population_fixture_material;
    population_settings.materialResolverUserData = &material_fixture;
    population_settings.surfaceQueryRadius = 0.12;
    population_settings.beamRadiusStart = 0.04;
    population_settings.beamRadiusEnd = 0.08;
    population_settings.beamTransmittance = 0.9;
    population_settings.beamDensityWeight = 1.25;
    population_settings.beamMediumId = 3;
    assert_true("runtime_caustic_photon_scene_population_populate",
                RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
                    &scene,
                    &scene_trace,
                    &population_settings,
                    &general_surface,
                    &general_beam,
                    &readback));
    assert_true("runtime_caustic_photon_scene_population_readback",
                readback.succeeded && readback.receiverHitFound &&
                    readback.receiverMaterialResolved && readback.receiverAccepted &&
                    readback.surfaceStoreAccepted && readback.beamStoreAccepted &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_COMPLETE);
    assert_true("runtime_caustic_photon_scene_population_receiver_identity",
                readback.receiverHit.sceneObjectIndex == 71 &&
                    readback.receiverHit.primitiveIndex == 1 &&
                    readback.receiverHit.triangleIndex == 2 &&
                    readback.receiverMaterial.materialId == 88);
    assert_true("runtime_caustic_photon_scene_population_shared_accel",
                readback.usedSharedSceneAccelerationRoute &&
                    readback.routeStats.tlasTraceCalls == 1u &&
                    readback.routeStats.flattenedFallbackCalls == 0u &&
                    material_fixture.calls == 3u);
    assert_true("runtime_caustic_photon_scene_population_counts",
                general_surface.recordCount == 1u && general_beam.segmentCount == 1u &&
                    scene_trace.trace.debug.storedSurfaceFlux.x == 0.0 &&
                    scene_trace.trace.debug.storedVolumeFlux.x == 0.0);

    assert_true("runtime_caustic_photon_scene_population_descriptor_trace",
                photon_scene_population_build_descriptor_trace(
                    &scene_trace,
                    readback.receiverHit.position,
                    &descriptor_trace));
    assert_true("runtime_caustic_photon_scene_population_descriptor_surface",
                RuntimeCausticPhotonMap3D_StoreTraceReceiver(
                    &descriptor_surface,
                    &descriptor_trace,
                    readback.receiverHit.normal,
                    population_settings.surfaceQueryRadius,
                    readback.receiverHit.sceneObjectIndex,
                    readback.receiverHit.primitiveIndex,
                    readback.receiverHit.triangleIndex));
    assert_true("runtime_caustic_photon_scene_population_descriptor_beam",
                RuntimeCausticBeamMap3D_StoreTraceSegment(
                    &descriptor_beam,
                    &descriptor_trace,
                    population_settings.beamRadiusStart,
                    population_settings.beamRadiusEnd,
                    population_settings.beamTransmittance,
                    population_settings.beamDensityWeight,
                    population_settings.beamMediumId));

    general_record = &general_surface.records[0];
    descriptor_record = &descriptor_surface.records[0];
    assert_true("runtime_caustic_photon_scene_population_surface_identity_ab",
                general_record->photonId == descriptor_record->photonId &&
                    general_record->depth == descriptor_record->depth &&
                    general_record->sceneObjectIndex == descriptor_record->sceneObjectIndex &&
                    general_record->primitiveIndex == descriptor_record->primitiveIndex &&
                    general_record->triangleIndex == descriptor_record->triangleIndex);
    assert_vec3_close("runtime_caustic_photon_scene_population_surface_position_ab",
                      general_record->position,
                      descriptor_record->position);
    assert_vec3_close("runtime_caustic_photon_scene_population_surface_flux_ab",
                      general_record->flux,
                      descriptor_record->flux);
    assert_close("runtime_caustic_photon_scene_population_surface_pdf_ab",
                 general_record->pathPdf,
                 descriptor_record->pathPdf,
                 1.0e-9);

    general_segment = &general_beam.segments[0];
    descriptor_segment = &descriptor_beam.segments[0];
    assert_true("runtime_caustic_photon_scene_population_beam_identity_ab",
                general_segment->photonId == descriptor_segment->photonId &&
                    general_segment->depth == descriptor_segment->depth &&
                    general_segment->mediumId == descriptor_segment->mediumId);
    assert_vec3_close("runtime_caustic_photon_scene_population_beam_start_ab",
                      general_segment->start,
                      descriptor_segment->start);
    assert_vec3_close("runtime_caustic_photon_scene_population_beam_end_ab",
                      general_segment->end,
                      descriptor_segment->end);
    assert_vec3_close("runtime_caustic_photon_scene_population_beam_flux_ab",
                      general_segment->flux,
                      descriptor_segment->flux);

    RuntimeCausticBeamMap3D_Free(&descriptor_beam);
    RuntimeCausticBeamMap3D_Free(&general_beam);
    RuntimeCausticPhotonMap3D_Free(&descriptor_surface);
    RuntimeCausticPhotonMap3D_Free(&general_surface);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_population_rejects_transparent_receiver(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_population_sample();
    PhotonScenePopulationMaterialFixture3D material_fixture = {true, true, 0u};
    RuntimeCausticPhotonSceneTraceSettings3D trace_settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    RuntimeCausticPhotonScenePopulationSettings3D population_settings;
    RuntimeCausticPhotonScenePopulationReadback3D readback;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;

    assert_true("runtime_caustic_photon_scene_population_transparent_fixture",
                photon_scene_population_build_fixture(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&trace_settings);
    trace_settings.materialResolver = photon_scene_population_fixture_material;
    trace_settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_population_transparent_trace",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &trace_settings,
                    &scene_trace));
    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    assert_true("runtime_caustic_photon_scene_population_transparent_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 1u) &&
                    RuntimeCausticBeamMap3D_Allocate(&beam_map, 1u));
    RuntimeCausticPhotonScenePopulation3D_DefaultSettings(&population_settings);
    population_settings.materialResolver = photon_scene_population_fixture_material;
    population_settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_population_transparent_reject",
                !RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
                    &scene,
                    &scene_trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback));
    assert_true("runtime_caustic_photon_scene_population_transparent_readback",
                readback.receiverHitFound && readback.receiverMaterialResolved &&
                    !readback.receiverAccepted && !readback.surfaceStoreAttempted &&
                    !readback.beamStoreAttempted &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_RECEIVER_NOT_OPAQUE &&
                    surface_map.storeAttemptCount == 0u &&
                    beam_map.storeAttemptCount == 0u);
    material_fixture.resolveReceiver = false;
    material_fixture.receiverTransparent = false;
    assert_true("runtime_caustic_photon_scene_population_unresolved_reject",
                !RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
                    &scene,
                    &scene_trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback));
    assert_true("runtime_caustic_photon_scene_population_unresolved_readback",
                readback.receiverHitFound && !readback.receiverMaterialResolved &&
                    !readback.receiverAccepted && !readback.surfaceStoreAttempted &&
                    !readback.beamStoreAttempted &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_MATERIAL_UNRESOLVED &&
                    surface_map.storeAttemptCount == 0u &&
                    beam_map.storeAttemptCount == 0u);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_caustic_photon_scene_population_reports_partial_store(void) {
    RuntimeScene3D scene;
    RuntimeCausticPhotonSample3D sample = photon_scene_population_sample();
    PhotonScenePopulationMaterialFixture3D material_fixture = {true, false, 0u};
    RuntimeCausticPhotonSceneTraceSettings3D trace_settings;
    RuntimeCausticPhotonSceneTrace3D scene_trace;
    RuntimeCausticPhotonScenePopulationSettings3D population_settings;
    RuntimeCausticPhotonScenePopulationReadback3D readback;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticBeamMap3D beam_map;

    assert_true("runtime_caustic_photon_scene_population_partial_fixture",
                photon_scene_population_build_fixture(&scene));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&trace_settings);
    trace_settings.materialResolver = photon_scene_population_fixture_material;
    trace_settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_population_partial_trace",
                RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
                    &scene,
                    &sample,
                    &trace_settings,
                    &scene_trace));
    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    assert_true("runtime_caustic_photon_scene_population_partial_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 1u));
    RuntimeCausticPhotonScenePopulation3D_DefaultSettings(&population_settings);
    population_settings.materialResolver = photon_scene_population_fixture_material;
    population_settings.materialResolverUserData = &material_fixture;
    assert_true("runtime_caustic_photon_scene_population_partial_reject",
                !RuntimeCausticPhotonScenePopulation3D_PopulateMaps(
                    &scene,
                    &scene_trace,
                    &population_settings,
                    &surface_map,
                    &beam_map,
                    &readback));
    assert_true("runtime_caustic_photon_scene_population_partial_readback",
                readback.surfaceStoreAttempted && !readback.surfaceStoreAccepted &&
                    readback.beamStoreAttempted && readback.beamStoreAccepted &&
                    readback.termination ==
                        RUNTIME_CAUSTIC_PHOTON_SCENE_POPULATION_PARTIAL_STORE &&
                    surface_map.storeRejectedCount == 1u && beam_map.segmentCount == 1u);
    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    return 0;
}

int run_test_runtime_caustic_photon_scene_population_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_scene_population_defaults();
    failures += test_runtime_caustic_photon_scene_population_maps_and_descriptor_ab();
    failures += test_runtime_caustic_photon_scene_population_rejects_transparent_receiver();
    failures += test_runtime_caustic_photon_scene_population_reports_partial_store();
    return failures;
}
