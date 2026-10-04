#include "test_support.h"
#include "test_runtime_caustic_volume_cache_3d.h"
#include <math.h>
#include <stdio.h>

static int failures;
int test_support_failures(void) { return failures; }
void assert_true(const char* name, bool condition) {
    if (!condition) { fprintf(stderr, "FAIL %s\n", name); ++failures; }
}
void assert_close(const char* name, double actual, double expected, double tolerance) {
    if (!isfinite(actual) || !isfinite(expected) || fabs(actual-expected)>tolerance) {
        fprintf(stderr, "FAIL %s: %.17g expected %.17g\n", name, actual, expected);
        ++failures;
    }
}
int main(void) {
    run_test_runtime_caustic_volume_cache_3d_tests();
    if (!failures) puts("Placed caustic cache numerical and sanitizer checks PASS");
    return failures ? 1 : 0;
}
