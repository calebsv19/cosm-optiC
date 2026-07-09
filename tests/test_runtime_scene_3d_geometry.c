#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_runtime_scene_3d_geometry.h"
#include "test_runtime_scene_3d_geometry_internal.h"
#include "test_support.h"

bool test_runtime_scene_3d_geometry_trace_enabled(void) {
    const char* env = getenv("TEST_RUNTIME_SCENE_3D_GEOMETRY_TRACE");
    return env && env[0] && strcmp(env, "0") != 0;
}

void test_runtime_scene_3d_geometry_trace(const char* name) {
    if (!test_runtime_scene_3d_geometry_trace_enabled() || !name) return;
    fprintf(stderr, "TEST CASE: %s\n", name);
    fflush(stderr);
}

int run_test_runtime_scene_3d_geometry_tests(void) {
    int before = test_support_failures();

    run_test_runtime_scene_3d_geometry_builder_suite();
    run_test_runtime_scene_3d_geometry_trace_suite();
    return test_support_failures() - before;
}
