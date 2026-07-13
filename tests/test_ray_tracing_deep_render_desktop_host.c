#include "app/ray_tracing_deep_render_desktop_host.h"
#include "test_support.h"

static int test_deep_render_desktop_selection_requires_explicit_opt_in(void) {
    assert_true(
        "deep_desktop_selection_deep_render_off",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            false, true, true, true, false) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED);
    assert_true(
        "deep_desktop_selection_async_off",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            true, false, true, true, false) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED);
    return 0;
}

static int test_deep_render_desktop_selection_accepts_native_tiled_3d(void) {
    assert_true(
        "deep_desktop_selection_native_tiled",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            true, true, true, true, false) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED);
    return 0;
}

static int test_deep_render_desktop_selection_preserves_fallbacks(void) {
    assert_true(
        "deep_desktop_selection_non_native_fallback",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            true, true, false, true, false) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE);
    assert_true(
        "deep_desktop_selection_non_tiled_fallback",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            true, true, true, false, false) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE);
    assert_true(
        "deep_desktop_selection_dynamic_fallback",
        RayTracingDeepRenderDesktopHost_AssessSelection(
            true, true, true, true, true) ==
            RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_DYNAMIC_DEPENDENCY);
    return 0;
}

int run_test_ray_tracing_deep_render_desktop_host_tests(void) {
    test_deep_render_desktop_selection_requires_explicit_opt_in();
    test_deep_render_desktop_selection_accepts_native_tiled_3d();
    test_deep_render_desktop_selection_preserves_fallbacks();
    return 0;
}
