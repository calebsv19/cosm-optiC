#include "platform/ray_tracing_folder_picker.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool write_script(const char *path, const char *body) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF || fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool setup_fixture(char *root, char *zenity, char *kdialog, char *marker) {
    char template[] = "/tmp/ray_tracing_folder_picker_XXXXXX";
    const char *zenity_script =
        "#!/bin/sh\n"
        "case \"$RAY_TRACING_FOLDER_PICKER_TEST_ZENITY\" in\n"
        "  selected) printf '%s\\n' \"$RAY_TRACING_FOLDER_PICKER_SELECTED_PATH\"; exit 0 ;;\n"
        "  cancelled) exit 1 ;;\n"
        "  unavailable) exit 127 ;;\n"
        "  *) exit 2 ;;\n"
        "esac\n";
    const char *kdialog_script =
        "#!/bin/sh\n"
        ": > \"$RAY_TRACING_FOLDER_PICKER_KDIALOG_MARKER\"\n"
        "printf '%s\\n' \"$RAY_TRACING_FOLDER_PICKER_KDIALOG_PATH\"\n";
    char *created = mkdtemp(template);

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
    char template[] = "/tmp/ray_tracing_folder_picker_empty_XXXXXX";
    char selected[PATH_MAX];
    char *root = mkdtemp(template);
    bool passed;
    if (!root) return false;
    (void)setenv("PATH", root, 1);
    passed = RayTracing_FolderPicker_Select("Unavailable", NULL, selected, sizeof(selected)) == RAY_TRACING_FOLDER_PICKER_UNAVAILABLE &&
             selected[0] == '\0';
    (void)rmdir(root);
    return passed;
}

int main(void) {
    const bool passed = test_zenity_selection() && test_kdialog_fallback() &&
                        test_cancel_does_not_fallback() && test_no_picker_available();
    fprintf(stdout, "ray_tracing_folder_picker_test: %s\n", passed ? "success" : "failed");
    return passed ? 0 : 1;
}
