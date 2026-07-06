#include "test_runtime_caustic_lens_transport_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "test_support.h"

static void assert_vec3_close(const char* name, Vec3 actual, Vec3 expected, double epsilon) {
    assert_close(name, actual.x, expected.x, epsilon);
    assert_close(name, actual.y, expected.y, epsilon);
    assert_close(name, actual.z, expected.z, epsilon);
}

static int test_runtime_caustic_lens_transport_defaults_are_bounded(void) {
    RuntimeCausticLensShape3D shape;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultPath(&path);

    assert_true("runtime_caustic_lens_transport_default_shape_kind",
                shape.kind == RUNTIME_CAUSTIC_LENS_SHAPE_NONE);
    assert_true("runtime_caustic_lens_transport_default_shape_label",
                strcmp(RuntimeCausticLensTransport3D_ShapeKindLabel(shape.kind),
                       "none") == 0);
    assert_true("runtime_caustic_lens_transport_sphere_shape_label",
                strcmp(RuntimeCausticLensTransport3D_ShapeKindLabel(
                           RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE),
                       "sphere") == 0);
    assert_true("runtime_caustic_lens_transport_default_shape_identity",
                shape.sceneObjectIndex == -1 && shape.primitiveIndex == -1);
    assert_vec3_close("runtime_caustic_lens_transport_default_axis",
                      shape.axis,
                      vec3(0.0, 0.0, 1.0),
                      1e-9);
    assert_true("runtime_caustic_lens_transport_default_light",
                light.intensity == 1.0 && light.radius > 0.0 && light.lightIndex == -1);
    assert_true("runtime_caustic_lens_transport_default_path_weight",
                path.sampleWeight == 1.0 && path.pathPdf == 1.0);
    assert_vec3_close("runtime_caustic_lens_transport_default_path_throughput",
                      path.throughput,
                      vec3(1.0, 1.0, 1.0),
                      1e-9);
    return 0;
}

static int test_runtime_caustic_lens_transport_refract_and_fresnel(void) {
    Vec3 refracted = vec3(0.0, 0.0, 0.0);
    bool tir = true;
    double fresnel = RuntimeCausticLensTransport3D_FresnelSchlick(vec3(0.0, 0.0, -1.0),
                                                                  vec3(0.0, 0.0, 1.0),
                                                                  1.0,
                                                                  1.5);

    assert_close("runtime_caustic_lens_transport_normal_fresnel", fresnel, 0.04, 1e-6);
    assert_true("runtime_caustic_lens_transport_normal_refract",
                RuntimeCausticLensTransport3D_Refract(vec3(0.0, 0.0, -1.0),
                                                      vec3(0.0, 0.0, 1.0),
                                                      1.0,
                                                      1.5,
                                                      &refracted,
                                                      &tir));
    assert_true("runtime_caustic_lens_transport_normal_not_tir", !tir);
    assert_vec3_close("runtime_caustic_lens_transport_normal_refracted",
                      refracted,
                      vec3(0.0, 0.0, -1.0),
                      1e-9);
    return 0;
}

static int test_runtime_caustic_lens_transport_tir_reports_no_direction(void) {
    Vec3 refracted = vec3(0.0, 0.0, 0.0);
    bool tir = false;

    assert_true("runtime_caustic_lens_transport_tir_rejects",
                !RuntimeCausticLensTransport3D_Refract(vec3(0.99, 0.0, 0.10),
                                                       vec3(0.0, 0.0, 1.0),
                                                       1.5,
                                                       1.0,
                                                       &refracted,
                                                       &tir));
    assert_true("runtime_caustic_lens_transport_tir_flag", tir);
    assert_vec3_close("runtime_caustic_lens_transport_tir_direction",
                      refracted,
                      vec3(0.0, 0.0, 0.0),
                      1e-9);
    return 0;
}

static int test_runtime_caustic_lens_transport_records_interface_events(void) {
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry;
    RuntimeCausticLensInterfaceEvent3D exit_event;

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry);
    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);

    entry.position = vec3(0.0, 0.0, 1.0);
    entry.normal = vec3(0.0, 0.0, 1.0);
    entry.incidentDirection = vec3(0.0, 0.0, -1.0);
    entry.outgoingDirection = vec3(0.0, 0.0, -1.0);
    entry.etaFrom = 1.0;
    entry.etaTo = 1.5;
    entry.fresnel = 0.04;
    entry.refracted = true;

    exit_event.position = vec3(0.0, 0.0, -1.0);
    exit_event.normal = vec3(0.0, 0.0, -1.0);
    exit_event.incidentDirection = vec3(0.0, 0.0, -1.0);
    exit_event.outgoingDirection = vec3(0.0, 0.0, -1.0);
    exit_event.etaFrom = 1.5;
    exit_event.etaTo = 1.0;
    exit_event.fresnel = 0.04;
    exit_event.distanceInMedium = 2.0;
    exit_event.refracted = true;

    assert_true("runtime_caustic_lens_transport_append_entry",
                RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry));
    assert_true("runtime_caustic_lens_transport_append_exit",
                RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event));
    assert_true("runtime_caustic_lens_transport_event_count",
                path.interfaceEventCount == 2u);
    assert_close("runtime_caustic_lens_transport_inside_distance",
                 path.insideDistance,
                 2.0,
                 1e-9);
    assert_true("runtime_caustic_lens_transport_event_identity",
                path.events[0].etaFrom == 1.0 && path.events[1].etaTo == 1.0);
    return 0;
}

static int test_runtime_caustic_lens_transport_throughput_helpers(void) {
    Vec3 throughput = vec3(10.0, 8.0, 6.0);
    Vec3 transmitted =
        RuntimeCausticLensTransport3D_ApplyInterfaceTransmission(throughput, 0.25);
    Vec3 absorbed = RuntimeCausticLensTransport3D_ApplyAbsorptionTint(transmitted,
                                                                      vec3(0.5, 0.75, 1.0),
                                                                      2.0,
                                                                      2.0);
    const double absorption = exp(-1.0);

    assert_vec3_close("runtime_caustic_lens_transport_transmission",
                      transmitted,
                      vec3(7.5, 6.0, 4.5),
                      1e-9);
    assert_close("runtime_caustic_lens_transport_absorbed_r",
                 absorbed.x,
                 7.5 * 0.5 * absorption,
                 1e-9);
    assert_close("runtime_caustic_lens_transport_absorbed_g",
                 absorbed.y,
                 6.0 * 0.75 * absorption,
                 1e-9);
    assert_close("runtime_caustic_lens_transport_absorbed_b",
                 absorbed.z,
                 4.5 * absorption,
                 1e-9);
    return 0;
}

static int test_runtime_caustic_lens_transport_sphere_adapter_matches_solver(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticSphereLens3DPath sphere_path;
    RuntimeCausticLensPath3D lens_path;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    light.radius = 0.08;
    sample.apertureU = -0.35;
    sample.lensU = 0.42;
    sample.sampleWeight = 1.2;
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_lens_transport_sphere_legacy_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &sphere_path));
    assert_true("runtime_caustic_lens_transport_sphere_adapter_solve",
                RuntimeCausticLensTransport3D_SolveSpherePath(&sphere,
                                                              &light,
                                                              &sample,
                                                              7,
                                                              11,
                                                              &lens_path));
    assert_true("runtime_caustic_lens_transport_sphere_adapter_valid", lens_path.valid);
    assert_true("runtime_caustic_lens_transport_sphere_adapter_kind",
                lens_path.shapeKind == RUNTIME_CAUSTIC_LENS_SHAPE_SPHERE);
    assert_true("runtime_caustic_lens_transport_sphere_adapter_identity",
                lens_path.sceneObjectIndex == 7 && lens_path.primitiveIndex == 11);
    assert_true("runtime_caustic_lens_transport_sphere_adapter_events",
                lens_path.interfaceEventCount == 2u);
    assert_vec3_close("runtime_caustic_lens_transport_sphere_adapter_exit_origin",
                      lens_path.postExitOrigin,
                      sphere_path.exitPosition,
                      1e-9);
    assert_vec3_close("runtime_caustic_lens_transport_sphere_adapter_exit_dir",
                      lens_path.postExitDirection,
                      sphere_path.exitDirection,
                      1e-9);
    assert_vec3_close("runtime_caustic_lens_transport_sphere_adapter_throughput",
                      lens_path.throughput,
                      sphere_path.throughput,
                      1e-9);
    assert_vec3_close("runtime_caustic_lens_transport_sphere_adapter_receiver",
                      lens_path.receiverCrossing,
                      sphere_path.receiverCrossing,
                      1e-9);
    assert_close("runtime_caustic_lens_transport_sphere_adapter_inside",
                 lens_path.insideDistance,
                 sphere_path.insideDistance,
                 1e-9);
    assert_close("runtime_caustic_lens_transport_sphere_adapter_entry_fresnel",
                 lens_path.events[0].fresnel,
                 sphere_path.entryFresnel,
                 1e-9);
    assert_close("runtime_caustic_lens_transport_sphere_adapter_exit_fresnel",
                 lens_path.events[1].fresnel,
                 sphere_path.exitFresnel,
                 1e-9);
    return 0;
}

static int test_runtime_caustic_lens_transport_cylinder_provider_solves_path(void) {
    RuntimeCausticLensShape3D cylinder;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&cylinder);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);

    cylinder.kind = RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER;
    cylinder.sceneObjectIndex = 9;
    cylinder.primitiveIndex = 12;
    cylinder.center = vec3(0.0, 0.0, 0.0);
    cylinder.axis = vec3(0.0, 0.0, 1.0);
    cylinder.radius = 0.5;
    cylinder.height = 1.4;
    cylinder.payload.opticalIor = 1.45;
    cylinder.payload.bsdf.ior = 1.45;
    cylinder.payload.baseColorR = 1.0;
    cylinder.payload.baseColorG = 1.0;
    cylinder.payload.baseColorB = 1.0;
    cylinder.payload.absorptionDistance = 8.0;

    light.position = vec3(0.0, -2.0, 0.0);
    light.radius = 0.04;
    light.intensity = 3.0;
    light.color = vec3(1.0, 0.95, 0.85);
    light.lightIndex = 2;

    sample.apertureU = 0.15;
    sample.apertureV = -0.10;
    sample.lensU = 0.35;
    sample.lensV = 0.20;
    sample.sampleWeight = 0.75;
    sample.receiverDistance = 2.5;

    assert_true("runtime_caustic_lens_transport_cylinder_solve",
                RuntimeCausticLensTransport3D_SolveCylinderPath(&cylinder,
                                                                &light,
                                                                &sample,
                                                                &path));
    assert_true("runtime_caustic_lens_transport_cylinder_valid", path.valid);
    assert_true("runtime_caustic_lens_transport_cylinder_kind",
                path.shapeKind == RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER);
    assert_true("runtime_caustic_lens_transport_cylinder_identity",
                path.sceneObjectIndex == 9 && path.primitiveIndex == 12);
    assert_true("runtime_caustic_lens_transport_cylinder_events",
                path.interfaceEventCount == 2u);
    assert_true("runtime_caustic_lens_transport_cylinder_entry_exit_spread",
                vec3_length(vec3_sub(path.events[0].position,
                                     path.events[1].position)) > 0.1);
    assert_true("runtime_caustic_lens_transport_cylinder_entry_ior",
                path.events[0].etaFrom == 1.0 && path.events[0].etaTo > 1.0);
    assert_true("runtime_caustic_lens_transport_cylinder_exit_ior",
                path.events[1].etaFrom > 1.0 && path.events[1].etaTo == 1.0);
    assert_true("runtime_caustic_lens_transport_cylinder_fresnel_bounds",
                path.events[0].fresnel >= 0.0 && path.events[0].fresnel <= 1.0 &&
                    path.events[1].fresnel >= 0.0 && path.events[1].fresnel <= 1.0);
    assert_true("runtime_caustic_lens_transport_cylinder_post_exit",
                vec3_length(path.postExitDirection) > 0.9);
    assert_true("runtime_caustic_lens_transport_cylinder_inside_distance",
                path.insideDistance > 0.1);
    assert_close("runtime_caustic_lens_transport_cylinder_sample_weight",
                 path.sampleWeight,
                 0.75,
                 1e-9);
    assert_close("runtime_caustic_lens_transport_cylinder_pdf",
                 path.pathPdf,
                 1.0 / 0.75,
                 1e-9);
    assert_true("runtime_caustic_lens_transport_cylinder_throughput",
                path.throughput.x > 0.0 &&
                    path.throughput.y > 0.0 &&
                    path.throughput.z > 0.0);
    assert_true("runtime_caustic_lens_transport_cylinder_receiver",
                vec3_length(vec3_sub(path.receiverCrossing,
                                     path.postExitOrigin)) > 2.0);
    return 0;
}

static int test_runtime_caustic_lens_transport_cylinder_rejects_wrong_shape(void) {
    RuntimeCausticLensShape3D shape;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_PRISM;
    shape.payload.opticalIor = 1.5;

    assert_true("runtime_caustic_lens_transport_cylinder_reject_shape",
                !RuntimeCausticLensTransport3D_SolveCylinderPath(&shape,
                                                                 &light,
                                                                 &sample,
                                                                 &path));
    assert_true("runtime_caustic_lens_transport_cylinder_reject_invalid_path",
                !path.valid);
    return 0;
}

static int test_runtime_caustic_lens_transport_prism_provider_solves_path(void) {
    RuntimeCausticLensShape3D prism;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&prism);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);

    prism.kind = RUNTIME_CAUSTIC_LENS_SHAPE_PRISM;
    prism.sceneObjectIndex = 13;
    prism.primitiveIndex = 17;
    prism.center = vec3(0.0, 0.0, 0.0);
    prism.axis = vec3(0.0, 0.0, 1.0);
    prism.radius = 0.55;
    prism.height = 1.2;
    prism.payload.opticalIor = 1.52;
    prism.payload.bsdf.ior = 1.52;
    prism.payload.baseColorR = 1.0;
    prism.payload.baseColorG = 0.96;
    prism.payload.baseColorB = 0.88;
    prism.payload.absorptionDistance = 10.0;

    light.position = vec3(0.0, -2.0, 0.0);
    light.radius = 0.03;
    light.intensity = 2.5;
    light.color = vec3(1.0, 0.95, 0.85);

    sample.apertureU = 0.0;
    sample.apertureV = 0.0;
    sample.lensU = 0.10;
    sample.lensV = 0.0;
    sample.sampleWeight = 0.8;
    sample.receiverDistance = 2.4;

    assert_true("runtime_caustic_lens_transport_prism_solve",
                RuntimeCausticLensTransport3D_SolvePrismPath(&prism,
                                                             &light,
                                                             &sample,
                                                             &path));
    assert_true("runtime_caustic_lens_transport_prism_valid", path.valid);
    assert_true("runtime_caustic_lens_transport_prism_kind",
                path.shapeKind == RUNTIME_CAUSTIC_LENS_SHAPE_PRISM);
    assert_true("runtime_caustic_lens_transport_prism_identity",
                path.sceneObjectIndex == 13 && path.primitiveIndex == 17);
    assert_true("runtime_caustic_lens_transport_prism_events",
                path.interfaceEventCount == 2u);
    assert_true("runtime_caustic_lens_transport_prism_entry_ior",
                path.events[0].etaFrom == 1.0 && path.events[0].etaTo > 1.0);
    assert_true("runtime_caustic_lens_transport_prism_exit_ior",
                path.events[1].etaFrom > 1.0 && path.events[1].etaTo == 1.0);
    assert_true("runtime_caustic_lens_transport_prism_deflects",
                fabs(path.postExitDirection.x) > 0.05);
    assert_true("runtime_caustic_lens_transport_prism_inside_distance",
                path.insideDistance > 0.1);
    assert_close("runtime_caustic_lens_transport_prism_sample_weight",
                 path.sampleWeight,
                 0.8,
                 1e-9);
    assert_true("runtime_caustic_lens_transport_prism_throughput",
                path.throughput.x > 0.0 &&
                    path.throughput.y > 0.0 &&
                    path.throughput.z > 0.0);
    return 0;
}

static int test_runtime_caustic_lens_transport_prism_rejects_wrong_shape(void) {
    RuntimeCausticLensShape3D shape;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER;
    shape.payload.opticalIor = 1.5;

    assert_true("runtime_caustic_lens_transport_prism_reject_shape",
                !RuntimeCausticLensTransport3D_SolvePrismPath(&shape,
                                                              &light,
                                                              &sample,
                                                              &path));
    assert_true("runtime_caustic_lens_transport_prism_reject_invalid_path",
                !path.valid);
    return 0;
}

static int test_runtime_caustic_lens_transport_bowl_provider_solves_curved_path(void) {
    RuntimeCausticLensShape3D bowl;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;
    double entry_tilt = 0.0;

    RuntimeCausticLensTransport3D_DefaultShape(&bowl);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);

    bowl.kind = RUNTIME_CAUSTIC_LENS_SHAPE_BOWL;
    bowl.sceneObjectIndex = 19;
    bowl.primitiveIndex = 23;
    bowl.center = vec3(0.0, 0.0, 0.0);
    bowl.axis = vec3(0.0, 0.0, 1.0);
    bowl.radius = 0.62;
    bowl.height = 0.34;
    bowl.payload.opticalIor = 1.333;
    bowl.payload.bsdf.ior = 1.333;
    bowl.payload.baseColorR = 0.86;
    bowl.payload.baseColorG = 0.95;
    bowl.payload.baseColorB = 1.0;
    bowl.payload.absorptionDistance = 9.0;

    light.position = vec3(0.0, -2.2, 0.0);
    light.radius = 0.025;
    light.intensity = 2.0;
    light.color = vec3(0.85, 0.95, 1.0);

    sample.apertureU = 0.0;
    sample.apertureV = 0.0;
    sample.lensU = 0.42;
    sample.lensV = -0.18;
    sample.sampleWeight = 0.7;
    sample.receiverDistance = 2.8;

    assert_true("runtime_caustic_lens_transport_bowl_solve",
                RuntimeCausticLensTransport3D_SolveBowlPath(&bowl,
                                                            &light,
                                                            &sample,
                                                            &path));
    assert_true("runtime_caustic_lens_transport_bowl_valid", path.valid);
    assert_true("runtime_caustic_lens_transport_bowl_kind",
                path.shapeKind == RUNTIME_CAUSTIC_LENS_SHAPE_BOWL);
    assert_true("runtime_caustic_lens_transport_bowl_identity",
                path.sceneObjectIndex == 19 && path.primitiveIndex == 23);
    assert_true("runtime_caustic_lens_transport_bowl_events",
                path.interfaceEventCount == 2u);
    assert_true("runtime_caustic_lens_transport_bowl_entry_ior",
                path.events[0].etaFrom == 1.0 && path.events[0].etaTo > 1.0);
    assert_true("runtime_caustic_lens_transport_bowl_exit_ior",
                path.events[1].etaFrom > 1.0 && path.events[1].etaTo == 1.0);
    entry_tilt = sqrt(path.events[0].normal.x * path.events[0].normal.x +
                      path.events[0].normal.z * path.events[0].normal.z);
    assert_true("runtime_caustic_lens_transport_bowl_curved_entry_normal",
                entry_tilt > 0.04);
    assert_true("runtime_caustic_lens_transport_bowl_flat_exit_normal",
                fabs(path.events[1].normal.x) < 1.0e-9 &&
                    fabs(path.events[1].normal.z) < 1.0e-9);
    assert_true("runtime_caustic_lens_transport_bowl_exit_changes_direction",
                vec3_length(vec3_sub(path.postExitDirection,
                                     path.events[0].incidentDirection)) > 0.02);
    assert_true("runtime_caustic_lens_transport_bowl_inside_distance",
                path.insideDistance > 0.05);
    assert_close("runtime_caustic_lens_transport_bowl_sample_weight",
                 path.sampleWeight,
                 0.7,
                 1e-9);
    assert_true("runtime_caustic_lens_transport_bowl_throughput",
                path.throughput.x > 0.0 &&
                    path.throughput.y > 0.0 &&
                    path.throughput.z > 0.0);
    return 0;
}

static int test_runtime_caustic_lens_transport_bowl_rejects_wrong_shape(void) {
    RuntimeCausticLensShape3D shape;
    RuntimeCausticLensLightSample3D light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;

    RuntimeCausticLensTransport3D_DefaultShape(&shape);
    RuntimeCausticLensTransport3D_DefaultLightSample(&light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_PRISM;
    shape.payload.opticalIor = 1.333;

    assert_true("runtime_caustic_lens_transport_bowl_reject_shape",
                !RuntimeCausticLensTransport3D_SolveBowlPath(&shape,
                                                             &light,
                                                             &sample,
                                                             &path));
    assert_true("runtime_caustic_lens_transport_bowl_reject_invalid_path",
                !path.valid);
    return 0;
}

int run_test_runtime_caustic_lens_transport_3d_tests(void) {
    test_runtime_caustic_lens_transport_defaults_are_bounded();
    test_runtime_caustic_lens_transport_refract_and_fresnel();
    test_runtime_caustic_lens_transport_tir_reports_no_direction();
    test_runtime_caustic_lens_transport_records_interface_events();
    test_runtime_caustic_lens_transport_throughput_helpers();
    test_runtime_caustic_lens_transport_sphere_adapter_matches_solver();
    test_runtime_caustic_lens_transport_cylinder_provider_solves_path();
    test_runtime_caustic_lens_transport_cylinder_rejects_wrong_shape();
    test_runtime_caustic_lens_transport_prism_provider_solves_path();
    test_runtime_caustic_lens_transport_prism_rejects_wrong_shape();
    test_runtime_caustic_lens_transport_bowl_provider_solves_curved_path();
    test_runtime_caustic_lens_transport_bowl_rejects_wrong_shape();
    return 0;
}
