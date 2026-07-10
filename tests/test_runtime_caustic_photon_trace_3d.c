#include "test_runtime_caustic_photon_trace_3d.h"

#include <math.h>

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

int run_test_runtime_caustic_photon_trace_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_trace_defaults();
    failures += test_runtime_caustic_photon_trace_sphere_lens_events_and_ledger();
    failures += test_runtime_caustic_photon_trace_max_depth_rejects_before_storage();
    return failures;
}
