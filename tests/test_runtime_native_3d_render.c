#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_native_3d_render.h"
#include "test_runtime_native_3d_render.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_support.h"

uint8_t native3d_test_pixel_r(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base];
}

uint8_t native3d_test_pixel_g(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base + 1u];
}

uint8_t native3d_test_pixel_b(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base + 2u];
}

bool native3d_test_pixels_match_rgb_only(const uint8_t* a,
                                         const uint8_t* b,
                                         size_t pixel_count) {
    if (!a || !b) return false;
    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        if (a[base] != b[base] ||
            a[base + 1u] != b[base + 1u] ||
            a[base + 2u] != b[base + 2u]) {
            return false;
        }
    }
    return true;
}

int run_test_runtime_native_3d_render_tests(void) {
    int before = test_support_failures();

    run_test_runtime_native_3d_render_live_suite();
    run_test_runtime_native_3d_render_prepared_suite();
    return test_support_failures() - before;
}
