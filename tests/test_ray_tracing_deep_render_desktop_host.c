#include "app/ray_tracing_deep_render_desktop_host.h"
#include "test_support.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static bool test_directory_is_empty(const char* path) {
    DIR* directory = opendir(path);
    struct dirent* entry = NULL;
    if (!directory) return false;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            (void)closedir(directory);
            return false;
        }
    }
    (void)closedir(directory);
    return true;
}

static int test_deep_render_desktop_output_probe_creates_and_cleans(void) {
    char root_template[] = "/tmp/optic-output-probe-XXXXXX";
    char nested_path[PATH_MAX];
    char frames_path[PATH_MAX];
    char diagnostic[256];
    char* root = mkdtemp(root_template);

    assert_true("deep_desktop_probe_temp_root", root != NULL);
    if (!root) return 0;
    (void)snprintf(frames_path, sizeof(frames_path), "%s/frames", root);
    (void)snprintf(nested_path, sizeof(nested_path), "%s/frames/scene", root);

    assert_true(
        "deep_desktop_probe_success",
        RayTracingDeepRenderDesktopHost_ProbeOutputDirectory(
            nested_path, 17u, diagnostic, sizeof(diagnostic)));
    assert_true("deep_desktop_probe_created_directory", path_exists(nested_path));
    assert_true("deep_desktop_probe_removed_sentinel",
                test_directory_is_empty(nested_path));
    assert_true("deep_desktop_probe_success_diagnostic",
                strcmp(diagnostic, "passed") == 0);

    (void)rmdir(nested_path);
    (void)rmdir(frames_path);
    (void)rmdir(root);
    return 0;
}

static int test_deep_render_desktop_output_probe_rejects_file_path(void) {
    char file_template[] = "/tmp/optic-output-probe-file-XXXXXX";
    char diagnostic[256];
    int fd = mkstemp(file_template);

    assert_true("deep_desktop_probe_file_fixture", fd >= 0);
    if (fd < 0) return 0;
    (void)close(fd);

    assert_true(
        "deep_desktop_probe_rejects_file_path",
        !RayTracingDeepRenderDesktopHost_ProbeOutputDirectory(
            file_template, 18u, diagnostic, sizeof(diagnostic)));
    assert_true("deep_desktop_probe_failure_diagnostic",
                strstr(diagnostic, "create frame directory") != NULL);
    assert_true("deep_desktop_probe_preserves_existing_file",
                path_exists(file_template));

    (void)unlink(file_template);
    return 0;
}

int run_test_ray_tracing_deep_render_desktop_host_tests(void) {
    test_deep_render_desktop_selection_requires_explicit_opt_in();
    test_deep_render_desktop_selection_accepts_native_tiled_3d();
    test_deep_render_desktop_selection_preserves_fallbacks();
    test_deep_render_desktop_output_probe_creates_and_cleans();
    test_deep_render_desktop_output_probe_rejects_file_path();
    return 0;
}
