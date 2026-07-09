#include "test_config_animation_internal.h"
#include "test_support.h"

int run_test_config_animation_tests(void) {
    int before = test_support_failures();

    run_test_config_animation_source_volume_suite();
    run_test_config_animation_settings_export_suite();
    return test_support_failures() - before;
}
