#include "test_runtime_caustic_sphere_lens_3d.h"

#include <math.h>

#include "render/runtime_caustic_sphere_lens_3d.h"
#include "test_support.h"

static void assert_vec3_close(const char* name, Vec3 actual, Vec3 expected, double epsilon) {
    assert_close(name, actual.x, expected.x, epsilon);
    assert_close(name, actual.y, expected.y, epsilon);
    assert_close(name, actual.z, expected.z, epsilon);
}

static int test_runtime_caustic_sphere_lens_center_path(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticSphereLens3DPath path;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_sphere_lens_center_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &path));
    assert_true("runtime_caustic_sphere_lens_center_valid", path.valid);
    assert_vec3_close("runtime_caustic_sphere_lens_center_entry",
                      path.entryPosition,
                      vec3(0.0, 0.0, 1.0),
                      1e-6);
    assert_vec3_close("runtime_caustic_sphere_lens_center_exit",
                      path.exitPosition,
                      vec3(0.0, 0.0, -1.0),
                      1e-6);
    assert_true("runtime_caustic_sphere_lens_center_inside_distance",
                path.insideDistance > 1.99 && path.insideDistance < 2.01);
    assert_true("runtime_caustic_sphere_lens_center_exit_direction",
                path.exitDirection.z < -0.999);
    assert_true("runtime_caustic_sphere_lens_center_receiver",
                path.exitReceiverT > 0.0 && fabs(path.receiverCrossing.z + 2.0) < 1e-6);
    assert_true("runtime_caustic_sphere_lens_center_throughput",
                path.throughput.x > 0.0 && path.throughput.y > 0.0 &&
                    path.throughput.z > 0.0);
    return 0;
}

static int test_runtime_caustic_sphere_lens_off_axis_path_bends(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticSphereLens3DPath path;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    light.radius = 0.08;
    sample.apertureU = -0.5;
    sample.lensU = 0.45;
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_sphere_lens_off_axis_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &path));
    assert_true("runtime_caustic_sphere_lens_off_axis_valid", path.valid);
    assert_true("runtime_caustic_sphere_lens_off_axis_entry_spread",
                fabs(path.entryPosition.x) > 0.10 || fabs(path.entryPosition.y) > 0.10);
    assert_true("runtime_caustic_sphere_lens_off_axis_exit_spread",
                fabs(path.exitPosition.x) > 0.05 || fabs(path.exitPosition.y) > 0.05);
    assert_true("runtime_caustic_sphere_lens_off_axis_direction_spread",
                fabs(path.exitDirection.x) > 0.01 || fabs(path.exitDirection.y) > 0.01);
    assert_true("runtime_caustic_sphere_lens_off_axis_receiver_crossing",
                path.exitReceiverT > 0.0 && fabs(path.receiverCrossing.z + 2.0) < 1e-6);
    return 0;
}

static int test_runtime_caustic_sphere_lens_absorption_tints_throughput(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticSphereLens3DPath clear_path;
    RuntimeCausticSphereLens3DPath tinted_path;
    RuntimeCausticSphereLens3DPath weighted_path;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    assert_true("runtime_caustic_sphere_lens_absorption_clear_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &clear_path));

    sphere.tint = vec3(0.5, 0.75, 1.0);
    sphere.absorptionDistance = 1.0;
    assert_true("runtime_caustic_sphere_lens_absorption_tinted_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &tinted_path));
    assert_true("runtime_caustic_sphere_lens_absorption_reduces_r",
                tinted_path.throughput.x < clear_path.throughput.x);
    assert_true("runtime_caustic_sphere_lens_absorption_reduces_g",
                tinted_path.throughput.y < clear_path.throughput.y);
    assert_true("runtime_caustic_sphere_lens_absorption_reduces_b",
                tinted_path.throughput.z < clear_path.throughput.z);
    assert_true("runtime_caustic_sphere_lens_absorption_tint_order",
                tinted_path.throughput.x < tinted_path.throughput.y &&
                    tinted_path.throughput.y < tinted_path.throughput.z);

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    sample.sampleWeight = 2.0;
    assert_true("runtime_caustic_sphere_lens_weighted_solve",
                RuntimeCausticSphereLens3D_SolvePath(&sphere, &light, &sample, &weighted_path));
    assert_true("runtime_caustic_sphere_lens_weight_allows_pdf_scale",
                weighted_path.throughput.x > clear_path.throughput.x * 1.99 &&
                    weighted_path.throughput.x < clear_path.throughput.x * 2.01);
    return 0;
}

static int test_runtime_caustic_sphere_lens_refract_reports_tir(void) {
    Vec3 refracted = vec3(0.0, 0.0, 0.0);
    bool tir = false;
    bool ok = RuntimeCausticSphereLens3D_Refract(vec3(0.99, 0.0, 0.10),
                                                 vec3(0.0, 0.0, 1.0),
                                                 1.5,
                                                 1.0,
                                                 &refracted,
                                                 &tir);
    assert_true("runtime_caustic_sphere_lens_tir_rejects", !ok);
    assert_true("runtime_caustic_sphere_lens_tir_flag", tir);
    assert_vec3_close("runtime_caustic_sphere_lens_tir_no_direction",
                      refracted,
                      vec3(0.0, 0.0, 0.0),
                      1e-9);
    return 0;
}

int run_test_runtime_caustic_sphere_lens_3d_tests(void) {
    test_runtime_caustic_sphere_lens_center_path();
    test_runtime_caustic_sphere_lens_off_axis_path_bends();
    test_runtime_caustic_sphere_lens_absorption_tints_throughput();
    test_runtime_caustic_sphere_lens_refract_reports_tir();
    return 0;
}
