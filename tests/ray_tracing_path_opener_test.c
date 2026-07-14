#include "platform/ray_tracing_path_opener.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void expect_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "ray_tracing_path_opener_test: failed: %s\n", name);
        failures += 1;
    }
}

static bool write_script(const char *path, const char *body) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF || fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool setup_fixture(char *root, char *directory, char *xdg_open, char *gio, char *log_path) {
    char root_template[] = "/tmp/ray_tracing_path_opener_XXXXXX";
    const char *xdg_script =
        "#!/bin/sh\n"
        "printf '%s\\n' \"$@\" > \"$RAY_TRACING_PATH_OPENER_LOG\"\n"
        "case \"$RAY_TRACING_PATH_OPENER_XDG\" in\n"
        "  opened) exit 0 ;; unavailable) exit 127 ;; *) exit 2 ;; esac\n";
    const char *gio_script =
        "#!/bin/sh\n"
        "printf '%s\\n' \"$@\" > \"$RAY_TRACING_PATH_OPENER_LOG\"\n"
        "exit 0\n";
    char *created_root = mkdtemp(root_template);
    if (!created_root) {
        perror("ray_tracing_path_opener_test: mkdtemp");
        return false;
    }
    if (snprintf(root, PATH_MAX, "%s", created_root) >= PATH_MAX ||
        snprintf(directory, PATH_MAX, "%s/output", root) >= PATH_MAX ||
        snprintf(xdg_open, PATH_MAX, "%s/xdg-open", root) >= PATH_MAX ||
        snprintf(gio, PATH_MAX, "%s/gio", root) >= PATH_MAX ||
        snprintf(log_path, PATH_MAX, "%s/args.log", root) >= PATH_MAX) {
        fprintf(stderr, "ray_tracing_path_opener_test: fixture path too long\n");
        return false;
    }
    if (mkdir(directory, 0700) != 0) {
        perror("ray_tracing_path_opener_test: mkdir");
        return false;
    }
    if (!write_script(xdg_open, xdg_script) || !write_script(gio, gio_script)) {
        perror("ray_tracing_path_opener_test: write fixture");
        return false;
    }
    return true;
}

static bool read_text(const char *path, char *out, size_t out_size) {
    FILE *file = fopen(path, "r");
    size_t count = 0u;
    if (!file || !out || out_size == 0u) return false;
    count = fread(out, 1u, out_size - 1u, file);
    out[count] = '\0';
    (void)fclose(file);
    return true;
}

static void cleanup_fixture(const char *root, const char *directory, const char *xdg_open, const char *gio, const char *log_path) {
    (void)unlink(xdg_open); (void)unlink(gio); (void)unlink(log_path); (void)rmdir(directory); (void)rmdir(root);
}

static void test_xdg_open_receives_directory(void) {
    char root[PATH_MAX], directory[PATH_MAX], xdg_open[PATH_MAX], gio[PATH_MAX], log_path[PATH_MAX], args[PATH_MAX];
    bool ready = setup_fixture(root, directory, xdg_open, gio, log_path);
    expect_true("opener_xdg_fixture", ready);
    if (ready) {
        expect_true("opener_xdg_path", setenv("PATH", root, 1) == 0);
        expect_true("opener_xdg_log", setenv("RAY_TRACING_PATH_OPENER_LOG", log_path, 1) == 0);
        expect_true("opener_xdg_mode", setenv("RAY_TRACING_PATH_OPENER_XDG", "opened", 1) == 0);
        expect_true("opener_xdg_result", RayTracing_PathOpener_OpenDirectory(directory) == RAY_TRACING_PATH_OPENER_OPENED);
        expect_true("opener_xdg_args", read_text(log_path, args, sizeof(args)) &&
                    strncmp(args, directory, strlen(directory)) == 0 && args[strlen(directory)] == '\n');
        cleanup_fixture(root, directory, xdg_open, gio, log_path);
    }
}

static void test_gio_fallback_receives_open_and_directory(void) {
    char root[PATH_MAX], directory[PATH_MAX], xdg_open[PATH_MAX], gio[PATH_MAX], log_path[PATH_MAX], args[PATH_MAX];
    bool ready = setup_fixture(root, directory, xdg_open, gio, log_path);
    expect_true("opener_gio_fixture", ready);
    if (ready) {
        (void)setenv("PATH", root, 1); (void)setenv("RAY_TRACING_PATH_OPENER_LOG", log_path, 1);
        (void)setenv("RAY_TRACING_PATH_OPENER_XDG", "unavailable", 1);
        expect_true("opener_gio_result", RayTracing_PathOpener_OpenDirectory(directory) == RAY_TRACING_PATH_OPENER_OPENED);
        expect_true("opener_gio_args", read_text(log_path, args, sizeof(args)) &&
                    strncmp(args, "open\n", strlen("open\n")) == 0 && strstr(args, directory) != NULL);
        cleanup_fixture(root, directory, xdg_open, gio, log_path);
    }
}

static void test_invalid_directory_does_not_run_opener(void) {
    expect_true("opener_invalid_path", RayTracing_PathOpener_OpenDirectory("/path/that/does/not/exist") ==
                RAY_TRACING_PATH_OPENER_INVALID_PATH);
}

int main(void) {
    test_xdg_open_receives_directory();
    test_gio_fallback_receives_open_and_directory();
    test_invalid_directory_does_not_run_opener();
    return failures == 0 ? 0 : 1;
}
