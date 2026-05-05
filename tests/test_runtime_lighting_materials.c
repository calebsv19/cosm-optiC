#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

int run_test_runtime_lighting_materials_tests(void) {
    int before = test_support_failures();

    run_test_runtime_lighting_materials_payload_suite();
    run_test_runtime_lighting_materials_direct_light_suite();
    run_test_runtime_lighting_materials_transport_suite();
    return test_support_failures() - before;
}

int run_test_runtime_lighting_materials_payload_tests(void) {
    int before = test_support_failures();
    run_test_runtime_lighting_materials_payload_suite();
    return test_support_failures() - before;
}
