#include "test_runtime_caustic_photon_trace_3d.h"

#include <math.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_emit_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_trace_3d.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "test_support.h"

static void assert_vec3_close(const char* name, Vec3 actual, Vec3 expected, double epsilon) {
    assert_close(name, actual.x, expected.x, epsilon);
    assert_close(name, actual.y, expected.y, epsilon);
    assert_close(name, actual.z, expected.z, epsilon);
}

static int test_runtime_caustic_photon_trace_defaults(void) {
    RuntimeCausticPhotonTraceSettings3D settings;
    RuntimeCausticPhotonSample3D sample = {0};
    RuntimeCausticPhotonPathState3D state;

    RuntimeCausticPhotonTrace3D_DefaultSettings(&settings);
    sample.photonId = 42u;
    sample.position = vec3(0.0, 1.0, 2.0);
    sample.direction = vec3(0.0, 0.0, -2.0);
    sample.flux = vec3(3.0, 2.0, 1.0);
    sample.emissionPdf = 0.5;
    RuntimeCausticPhotonTrace3D_InitPathState(&sample, &state);

    assert_true("runtime_caustic_photon_trace_default_depth",
                settings.maxDepth == RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS);
    assert_true("runtime_caustic_photon_trace_state_active", state.active);
    assert_true("runtime_caustic_photon_trace_state_identity", state.photonId == 42u);
    assert_vec3_close("runtime_caustic_photon_trace_state_position",
                      state.position,
                      sample.position,
                      1e-9);
    assert_vec3_close("runtime_caustic_photon_trace_state_direction",
                      state.direction,
                      vec3(0.0, 0.0, -1.0),
                      1e-9);
    assert_vec3_close("runtime_caustic_photon_trace_state_transport_weight",
                      state.transportWeight,
                      vec3(1.0, 1.0, 1.0),
                      1e-9);
    assert_close("runtime_caustic_photon_trace_state_pdf", state.pathPdf, 0.5, 1e-9);
    return 0;
}

static int test_runtime_caustic_photon_trace_sphere_lens_events_and_ledger(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticPhotonTraceSettings3D settings;
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticSphereLens3DPath sphere_path;
    Vec3 reconciled;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    RuntimeCausticPhotonTrace3D_DefaultSettings(&settings);

    sphere.ior = 1.2;
    sphere.absorptionDistance = 12.0;
    light.radius = 0.05;
    light.intensity = 3.0;
    light.color = vec3(1.0, 0.9, 0.7);
    sample.apertureU = -0.15;
    sample.lensU = 0.25;
    sample.lensV = -0.10;
    sample.sampleWeight = 0.5;
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_photon_trace_sphere_reference",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &sphere_path));
    assert_true("runtime_caustic_photon_trace_sphere",
                RuntimeCausticPhotonTrace3D_TraceSphereLens(&sphere,
                                                            &light,
                                                            &sample,
                                                            &settings,
                                                            1001u,
                                                            17u,
                                                            7,
                                                            11,
                                                            &trace));

    assert_true("runtime_caustic_photon_trace_valid", trace.valid);
    assert_true("runtime_caustic_photon_trace_event_counts",
                trace.eventCount == 3u && trace.dielectricEventCount == 2u);
    assert_true("runtime_caustic_photon_trace_emission_event",
                trace.events[0].kind == RUNTIME_CAUSTIC_PHOTON_EVENT_EMISSION);
    assert_true("runtime_caustic_photon_trace_dielectric_events",
                trace.events[1].kind == RUNTIME_CAUSTIC_PHOTON_EVENT_DIELECTRIC &&
                    trace.events[2].kind == RUNTIME_CAUSTIC_PHOTON_EVENT_DIELECTRIC);
    assert_true("runtime_caustic_photon_trace_entry_exit_ior",
                trace.dielectricEvents[0].etaFrom == 1.0 &&
                    trace.dielectricEvents[0].etaTo == sphere.ior &&
                    trace.dielectricEvents[1].etaFrom == sphere.ior &&
                    trace.dielectricEvents[1].etaTo == 1.0);
    assert_true("runtime_caustic_photon_trace_refracted_branches",
                trace.dielectricEvents[0].selectedBranch ==
                        RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED &&
                    trace.dielectricEvents[1].selectedBranch ==
                        RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED &&
                    trace.debug.refractedBranchCount == 2u);
    assert_true("runtime_caustic_photon_trace_branch_pdf_bounds",
                trace.dielectricEvents[0].branchPdf > 0.0 &&
                    trace.dielectricEvents[0].branchPdf <= 1.0 &&
                    trace.dielectricEvents[1].branchPdf > 0.0 &&
                    trace.dielectricEvents[1].branchPdf <= 1.0 &&
                    trace.finalState.pathPdf > 0.0);
    assert_vec3_close("runtime_caustic_photon_trace_exit_origin",
                      trace.postExitOrigin,
                      sphere_path.exitPosition,
                      1e-9);
    assert_vec3_close("runtime_caustic_photon_trace_exit_direction",
                      trace.postExitDirection,
                      sphere_path.exitDirection,
                      1e-9);
    assert_vec3_close("runtime_caustic_photon_trace_final_flux",
                      trace.finalState.throughput,
                      sphere_path.throughput,
                      1e-9);
    assert_close("runtime_caustic_photon_trace_inside_distance",
                 trace.insideDistance,
                 sphere_path.insideDistance,
                 1e-9);
    assert_true("runtime_caustic_photon_trace_no_map_storage",
                trace.debug.storedSurfaceFlux.x == 0.0 &&
                    trace.debug.storedVolumeFlux.x == 0.0);

    reconciled = vec3_add(trace.finalState.throughput, trace.debug.rejectedFlux);
    assert_vec3_close("runtime_caustic_photon_trace_flux_reconciles",
                      reconciled,
                      trace.debug.emittedFlux,
                      1e-9);
    return 0;
}

static int test_runtime_caustic_photon_trace_max_depth_rejects_before_storage(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticPhotonTraceSettings3D settings;
    RuntimeCausticPhotonTrace3D trace;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    RuntimeCausticPhotonTrace3D_DefaultSettings(&settings);
    settings.maxDepth = 1u;
    sample.lensU = 0.10;

    assert_true("runtime_caustic_photon_trace_max_depth_reject",
                !RuntimeCausticPhotonTrace3D_TraceSphereLens(&sphere,
                                                             &light,
                                                             &sample,
                                                             &settings,
                                                             2002u,
                                                             19u,
                                                             3,
                                                             5,
                                                             &trace));
    assert_true("runtime_caustic_photon_trace_max_depth_reason",
                trace.finalState.rejectReason == RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH &&
                    trace.debug.rejectedPhotonCount == 1u &&
                    trace.debug.lastRejectReason == RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
    assert_vec3_close("runtime_caustic_photon_trace_max_depth_rejected_flux",
                      trace.debug.rejectedFlux,
                      trace.debug.emittedFlux,
                      1e-9);
    return 0;
}

static RuntimeLightSet3D test_mesh_dielectric_emission_light_set(void) {
    RuntimeLightSet3D set;
    RuntimeLightSource3D light;

    RuntimeLightSet3D_Init(&set);
    RuntimeLightSource3D_Init(&light);
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.position = vec3(0.0, 2.0, 0.0);
    light.color = vec3(1.0, 0.85, 0.65);
    light.intensity = 3.0;
    assert_true("runtime_caustic_photon_trace_mesh_emit_light",
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

static int test_runtime_caustic_photon_trace_mesh_dielectric_path_maps(void) {
    RuntimeLightSet3D set = test_mesh_dielectric_emission_light_set();
    RuntimeCausticPhotonEmissionSettings3D emission_settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticLensShape3D shape = test_mesh_dielectric_shape();
    RuntimeTriangle3D triangle = test_mesh_dielectric_entry_triangle();
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D lens_sample;
    RuntimeCausticLensPath3D lens_path;
    RuntimeCausticPhotonTraceSettings3D trace_settings;
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticPhotonMap3D surface_map;
    RuntimeCausticPhotonMapQuery3D surface_query;
    RuntimeCausticPhotonMapQueryResult3D surface_result;
    RuntimeCausticBeamMap3D beam_map;
    RuntimeCausticBeamMapQuery3D beam_query;
    RuntimeCausticBeamMapQueryResult3D beam_result;
    Vec3 beam_midpoint;

    RuntimeCausticPhotonEmission3D_DefaultSettings(&emission_settings);
    emission_settings.sampleBudget = 1u;
    emission_settings.baseSeed = 99u;
    emission_settings.firstPhotonId = 8001u;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    RuntimeCausticPhotonMap3D_Init(&surface_map);
    RuntimeCausticBeamMap3D_Init(&beam_map);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&lens_sample);
    RuntimeCausticPhotonTrace3D_DefaultSettings(&trace_settings);
    RuntimeCausticPhotonMap3D_DefaultQuery(&surface_query);
    RuntimeCausticBeamMap3D_DefaultQuery(&beam_query);

    assert_true("runtime_caustic_photon_trace_mesh_emit_alloc",
                RuntimeCausticPhotonEmission3D_AllocateBatch(&batch, 1u));
    assert_true("runtime_caustic_photon_trace_mesh_emit",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(&batch,
                                                                &set,
                                                                &emission_settings,
                                                                NULL));
    light.position = set.lights[0].position;
    light.color = set.lights[0].color;
    light.intensity = set.lights[0].intensity;
    light.lightIndex = 0;
    lens_sample.lensU = 0.10;
    lens_sample.lensV = -0.05;
    lens_sample.receiverDistance = 1.25;
    assert_true("runtime_caustic_photon_trace_mesh_lens_path",
                RuntimeCausticLensTransport3D_SolveMeshDielectricPath(&shape,
                                                                      &triangle,
                                                                      &light,
                                                                      &lens_sample,
                                                                      &lens_path));
    assert_true("runtime_caustic_photon_trace_mesh_trace",
                RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(&lens_path,
                                                                    &batch.samples[0],
                                                                    &trace_settings,
                                                                    &trace));
    assert_true("runtime_caustic_photon_trace_mesh_valid",
                trace.valid && trace.eventCount == 3u &&
                    trace.dielectricEventCount == 2u);
    assert_true("runtime_caustic_photon_trace_mesh_identity",
                trace.sample.photonId == 8001u &&
                    trace.events[1].sceneObjectIndex == 31 &&
                    trace.events[1].primitiveIndex == 41);
    assert_true("runtime_caustic_photon_trace_mesh_branches",
                trace.debug.refractedBranchCount == 2u &&
                    trace.dielectricEvents[0].selectedBranch ==
                        RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED &&
                    trace.dielectricEvents[1].selectedBranch ==
                        RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED);
    assert_true("runtime_caustic_photon_trace_mesh_ledgers",
                trace.finalState.pathPdf > 0.0 &&
                    trace.finalState.throughput.x > 0.0 &&
                    trace.insideDistance > 0.0 &&
                    trace.receiverPlaneT > 0.0);

    assert_true("runtime_caustic_photon_trace_mesh_surface_alloc",
                RuntimeCausticPhotonMap3D_Allocate(&surface_map, 4u));
    assert_true("runtime_caustic_photon_trace_mesh_store_surface",
                RuntimeCausticPhotonMap3D_StoreTraceReceiver(&surface_map,
                                                             &trace,
                                                             vec3(0.0, 1.0, 0.0),
                                                             0.12,
                                                             71,
                                                             72,
                                                             73));
    surface_query.position = trace.receiverCrossing;
    surface_query.normal = vec3(0.0, 1.0, 0.0);
    surface_query.radius = 0.15;
    surface_query.sceneObjectIndex = 71;
    surface_query.primitiveIndex = 72;
    surface_query.triangleIndex = 73;
    assert_true("runtime_caustic_photon_trace_mesh_query_surface",
                RuntimeCausticPhotonMap3D_Query(&surface_map,
                                                &surface_query,
                                                &surface_result));
    assert_true("runtime_caustic_photon_trace_mesh_surface_flux",
                surface_result.contributingCount == 1u &&
                    surface_result.flux.x > 0.0);

    assert_true("runtime_caustic_photon_trace_mesh_beam_alloc",
                RuntimeCausticBeamMap3D_Allocate(&beam_map, 4u));
    assert_true("runtime_caustic_photon_trace_mesh_store_beam",
                RuntimeCausticBeamMap3D_StoreTraceSegment(&beam_map,
                                                          &trace,
                                                          0.04,
                                                          0.08,
                                                          0.9,
                                                          1.0,
                                                          3));
    beam_midpoint = vec3_add(trace.postExitOrigin,
                             vec3_scale(vec3_sub(trace.receiverCrossing,
                                                 trace.postExitOrigin),
                                        0.5));
    beam_query.position = beam_midpoint;
    beam_query.direction = trace.postExitDirection;
    beam_query.radius = 0.20;
    beam_query.mediumId = 3;
    assert_true("runtime_caustic_photon_trace_mesh_query_beam",
                RuntimeCausticBeamMap3D_Query(&beam_map, &beam_query, &beam_result));
    assert_true("runtime_caustic_photon_trace_mesh_beam_flux",
                beam_result.contributingCount == 1u && beam_result.flux.x > 0.0);

    RuntimeCausticBeamMap3D_Free(&beam_map);
    RuntimeCausticPhotonMap3D_Free(&surface_map);
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_caustic_photon_trace_mesh_rejects_non_mesh_path(void) {
    RuntimeCausticLensPath3D path;
    RuntimeCausticPhotonSample3D sample = {0};
    RuntimeCausticPhotonTrace3D trace;

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE;
    path.interfaceEventCount = 1u;
    sample.photonId = 9002u;
    sample.position = vec3(0.0, 1.0, 0.0);
    sample.direction = vec3(0.0, -1.0, 0.0);
    sample.flux = vec3(1.0, 1.0, 1.0);
    sample.emissionPdf = 1.0;

    assert_true("runtime_caustic_photon_trace_mesh_rejects_non_mesh",
                !RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(&path,
                                                                     &sample,
                                                                     NULL,
                                                                     &trace));
    assert_true("runtime_caustic_photon_trace_mesh_reject_reason",
                trace.finalState.rejectReason ==
                    RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM &&
                    trace.debug.rejectedPhotonCount == 1u &&
                    trace.debug.lastRejectReason ==
                        RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
    assert_vec3_close("runtime_caustic_photon_trace_mesh_reject_flux",
                      trace.debug.rejectedFlux,
                      sample.flux,
                      1e-9);
    return 0;
}

int run_test_runtime_caustic_photon_trace_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_trace_defaults();
    failures += test_runtime_caustic_photon_trace_sphere_lens_events_and_ledger();
    failures += test_runtime_caustic_photon_trace_max_depth_rejects_before_storage();
    failures += test_runtime_caustic_photon_trace_mesh_dielectric_path_maps();
    failures += test_runtime_caustic_photon_trace_mesh_rejects_non_mesh_path();
    return failures;
}
