#include "platform/ray_tracing_folder_picker.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool write_script(const char *path, const char *body) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF || fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static char *make_fixture_directory(char *template, size_t template_size, const char *prefix) {
    const char *tmpdir = getenv("TMPDIR");

    if (!tmpdir || !tmpdir[0]) tmpdir = "/tmp";
    if (snprintf(template, template_size, "%s/%sXXXXXX", tmpdir, prefix) >= (int)template_size) {
        return NULL;
    }
    return mkdtemp(template);
}

static bool setup_fixture(char *root, char *zenity, char *kdialog, char *marker) {
    char template[PATH_MAX];
    const char *zenity_script =
        "#!/bin/sh\n"
        "if [ \"$RAY_TRACING_FOLDER_PICKER_EXPECT_FILE\" = file ]; then\n"
        "  saw_file=0; saw_directory=0\n"
        "  for arg in \"$@\"; do\n"
        "    [ \"$arg\" = --file-selection ] && saw_file=1\n"
        "    [ \"$arg\" = --directory ] && saw_directory=1\n"
        "  done\n"
        "  [ \"$saw_file\" = 1 ] && [ \"$saw_directory\" = 0 ] || exit 9\n"
        "fi\n"
        "case \"$RAY_TRACING_FOLDER_PICKER_TEST_ZENITY\" in\n"
        "  selected) [ \"$RAY_TRACING_FOLDER_PICKER_DELAY\" = yes ] && /bin/sleep 0.05; printf '%s\\n' \"$RAY_TRACING_FOLDER_PICKER_SELECTED_PATH\"; exit 0 ;;\n"
        "  cancelled) exit 1 ;;\n"
        "  unavailable) exit 127 ;;\n"
        "  *) exit 2 ;;\n"
        "esac\n";
    const char *kdialog_script =
        "#!/bin/sh\n"
        "if [ \"$RAY_TRACING_FOLDER_PICKER_EXPECT_FILE\" = file ]; then\n"
        "  [ \"$1\" = --getopenfilename ] || exit 9\n"
        "fi\n"
        ": > \"$RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER\"\n"
        "printf '%s\\n' \"$RAY_TRACING_FOLDER_PICKER_KDIALOG_PATH\"\n";
    char *created = make_fixture_directory(template, sizeof(template), "ray_tracing_folder_picker_");

    if (!created || snprintf(root, PATH_MAX, "%s", created) >= PATH_MAX ||
        snprintf(zenity, PATH_MAX, "%s/zenity", root) >= PATH_MAX ||
        snprintf(kdialog, PATH_MAX, "%s/kdialog", root) >= PATH_MAX ||
        snprintf(marker, PATH_MAX, "%s/kdialog-ran", root) >= PATH_MAX) {
        return false;
    }
    return write_script(zenity, zenity_script) && write_script(kdialog, kdialog_script);
}

static void remove_fixture(const char *root, const char *zenity, const char *kdialog, const char *marker) {
    (void)unlink(zenity);
    (void)unlink(kdialog);
    (void)unlink(marker);
    (void)rmdir(root);
}

static bool test_zenity_selection(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)unsetenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE");
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "selected", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_SELECTED_PATH", "/tmp/selected folder", 1);
    passed = RayTracing_FolderPicker_Select("Choose root", "/tmp/start", selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED &&
             strcmp(selected, "/tmp/selected folder") == 0 && access(marker, F_OK) != 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_kdialog_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "unavailable", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER", marker, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_PATH", "/tmp/kdialog selection", 1);
    passed = RayTracing_FolderPicker_Select("Fallback", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED &&
             strcmp(selected, "/tmp/kdialog selection") == 0 && access(marker, F_OK) == 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_cancel_does_not_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "cancelled", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER", marker, 1);
    passed = RayTracing_FolderPicker_Select("Cancel", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_CANCELLED &&
             selected[0] == '\0' && access(marker, F_OK) != 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_no_picker_available(void) {
    char template[PATH_MAX];
    char selected[PATH_MAX];
    char *root = make_fixture_directory(template, sizeof(template), "ray_tracing_folder_picker_empty_");
    bool passed;
    if (!root) return false;
    (void)setenv("PATH", root, 1);
    passed = RayTracing_FolderPicker_Select("Unavailable", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_UNAVAILABLE &&
             selected[0] == '\0';
    (void)rmdir(root);
    return passed;
}

static bool test_file_zenity_selection(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE", "file", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "selected", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_SELECTED_PATH", "/tmp/texture manifest.json", 1);
    passed = RayTracing_FilePicker_Select("Choose manifest", "/tmp/starting.json", selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED &&
             strcmp(selected, "/tmp/texture manifest.json") == 0 && access(marker, F_OK) != 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_file_kdialog_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE", "file", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "unavailable", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER", marker, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_PATH", "/tmp/fallback manifest.json", 1);
    passed = RayTracing_FilePicker_Select("Choose manifest", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_SELECTED &&
             strcmp(selected, "/tmp/fallback manifest.json") == 0 && access(marker, F_OK) == 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_file_cancel_does_not_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX], selected[PATH_MAX];
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE", "file", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "cancelled", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER", marker, 1);
    passed = RayTracing_FilePicker_Select("Choose manifest", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_CANCELLED &&
             selected[0] == '\0' && access(marker, F_OK) != 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_file_picker_unavailable(void) {
    char template[PATH_MAX];
    char selected[PATH_MAX];
    char *root = make_fixture_directory(template, sizeof(template), "ray_tracing_file_picker_empty_");
    bool passed;
    if (!root) return false;
    (void)setenv("PATH", root, 1);
    passed = RayTracing_FilePicker_Select("Unavailable", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_UNAVAILABLE &&
             selected[0] == '\0';
    (void)rmdir(root);
    return passed;
}

static RayTracingFolderPickerResult poll_until_terminal(
    RayTracingFolderPickerRequest *request,
    char *out_path,
    size_t out_path_size,
    bool *out_saw_pending) {
    RayTracingFolderPickerResult result = RAY_TRACING_FOLDER_PICKER_PENDING;
    int attempts = 0;
    if (out_saw_pending) *out_saw_pending = false;
    while (result == RAY_TRACING_FOLDER_PICKER_PENDING && attempts++ < 2000) {
        result = RayTracing_FolderPicker_Poll(request, out_path, out_path_size);
        if (result == RAY_TRACING_FOLDER_PICKER_PENDING) {
            if (out_saw_pending) *out_saw_pending = true;
            (void)usleep(1000u);
        }
    }
    return result;
}

static bool test_async_selection_is_nonblocking(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX];
    char selected[PATH_MAX] = "unchanged";
    RayTracingFolderPickerRequest request;
    RayTracingFolderPickerResult result;
    bool saw_pending = false;
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)unsetenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE");
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "selected", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_SELECTED_PATH", "/tmp/async selected", 1);
    (void)setenv("RAY_TRACING_FOLDER_PICKER_DELAY", "yes", 1);
    RayTracing_FolderPicker_RequestInit(&request);
    if (!RayTracing_FolderPicker_Begin(&request, "Async", "relative/missing")) {
        remove_fixture(root, zenity, kdialog, marker);
        return false;
    }
    result = RayTracing_FolderPicker_Poll(&request, selected, sizeof(selected));
    passed = result == RAY_TRACING_FOLDER_PICKER_PENDING &&
             strcmp(selected, "unchanged") == 0;
    result = poll_until_terminal(&request, selected, sizeof(selected), &saw_pending);
    passed = passed && saw_pending &&
             result == RAY_TRACING_FOLDER_PICKER_SELECTED &&
             strcmp(selected, "/tmp/async selected") == 0;
    (void)unsetenv("RAY_TRACING_FOLDER_PICKER_DELAY");
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_async_cancel_and_failure_do_not_mutate(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], marker[PATH_MAX];
    char selected[PATH_MAX] = "preserve-me";
    RayTracingFolderPickerRequest request;
    RayTracingFolderPickerResult result;
    bool passed = false;
    if (!setup_fixture(root, zenity, kdialog, marker)) return false;
    (void)setenv("PATH", root, 1);
    (void)unsetenv("RAY_TRACING_FOLDER_PICKER_EXPECT_FILE");
    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "cancelled", 1);
    RayTracing_FolderPicker_RequestInit(&request);
    if (!RayTracing_FolderPicker_Begin(&request, "Cancel async", NULL)) {
        remove_fixture(root, zenity, kdialog, marker);
        return false;
    }
    result = poll_until_terminal(&request, selected, sizeof(selected), NULL);
    passed = result == RAY_TRACING_FOLDER_PICKER_CANCELLED &&
             strcmp(selected, "preserve-me") == 0;

    (void)setenv("RAY_TRACING_FOLDER_PICKER_TEST_ZENITY", "failed", 1);
    RayTracing_FolderPicker_RequestInit(&request);
    if (!RayTracing_FolderPicker_Begin(&request, "Fail async", NULL)) {
        remove_fixture(root, zenity, kdialog, marker);
        return false;
    }
    result = poll_until_terminal(&request, selected, sizeof(selected), NULL);
    passed = passed && result == RAY_TRACING_FOLDER_PICKER_FAILED &&
             strcmp(selected, "preserve-me") == 0;
    remove_fixture(root, zenity, kdialog, marker);
    return passed;
}

static bool test_initial_directory_resolution(void) {
    char template[PATH_MAX];
    char *root = make_fixture_directory(template, sizeof(template), "ray_tracing_picker_resolve_");
    char existing[PATH_MAX];
    char file_path[PATH_MAX];
    char resolved[PATH_MAX];
    char expected[PATH_MAX];
    FILE *file = NULL;
    bool passed = false;
    if (!root ||
        snprintf(existing, sizeof(existing), "%s/existing", root) >= (int)sizeof(existing) ||
        snprintf(file_path, sizeof(file_path), "%s/manifest.json", existing) >= (int)sizeof(file_path) ||
        mkdir(existing, 0700) != 0) {
        return false;
    }
    file = fopen(file_path, "w");
    if (!file || fputs("{}\n", file) == EOF) {
        if (file) (void)fclose(file);
        (void)unlink(file_path);
        (void)rmdir(existing);
        (void)rmdir(root);
        return false;
    }
    if (fclose(file) != 0 || !realpath(existing, expected)) {
        (void)unlink(file_path);
        (void)rmdir(existing);
        (void)rmdir(root);
        return false;
    }
    (void)setenv("RAY_TRACING_RUNTIME_DIR", root, 1);
    passed =
        RayTracing_FolderPicker_ResolveInitialDirectory("existing/missing/leaf",
                                                        resolved,
                                                        sizeof(resolved)) &&
        strcmp(resolved, expected) == 0 &&
        RayTracing_FolderPicker_ResolveInitialDirectory(file_path,
                                                        resolved,
                                                        sizeof(resolved)) &&
        strcmp(resolved, expected) == 0;
    resolved[0] = 'x';
    passed = passed &&
             !RayTracing_FolderPicker_ResolveInitialDirectory(NULL,
                                                              resolved,
                                                              sizeof(resolved)) &&
             resolved[0] == '\0';
    (void)unsetenv("RAY_TRACING_RUNTIME_DIR");
    (void)unlink(file_path);
    (void)rmdir(existing);
    (void)rmdir(root);
    return passed;
}

int main(void) {
    const bool zenity_selection = test_zenity_selection();
    const bool kdialog_fallback = test_kdialog_fallback();
    const bool cancellation = test_cancel_does_not_fallback();
    const bool unavailable = test_no_picker_available();
    const bool file_zenity_selection = test_file_zenity_selection();
    const bool file_kdialog_fallback = test_file_kdialog_fallback();
    const bool file_cancellation = test_file_cancel_does_not_fallback();
    const bool file_unavailable = test_file_picker_unavailable();
    const bool async_selection = test_async_selection_is_nonblocking();
    const bool async_no_mutation = test_async_cancel_and_failure_do_not_mutate();
    const bool initial_resolution = test_initial_directory_resolution();
    const bool passed = zenity_selection && kdialog_fallback && cancellation && unavailable &&
                        file_zenity_selection && file_kdialog_fallback && file_cancellation &&
                        file_unavailable && async_selection && async_no_mutation &&
                        initial_resolution;
    fprintf(stderr,
            "ray_tracing_folder_picker_test cases: zenity=%d kdialog=%d cancel=%d unavailable=%d file_zenity=%d file_kdialog=%d file_cancel=%d file_unavailable=%d async=%d no_mutation=%d resolve=%d\n",
            zenity_selection,
            kdialog_fallback,
            cancellation,
            unavailable,
            file_zenity_selection,
            file_kdialog_fallback,
            file_cancellation,
            file_unavailable,
            async_selection,
            async_no_mutation,
            initial_resolution);
    fprintf(stdout, "ray_tracing_folder_picker_test: %s\n", passed ? "success" : "failed");
    return passed ? 0 : 1;
}
