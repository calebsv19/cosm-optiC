#include "config/core/config_runtime_paths.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "app/data_paths.h"
#include "config/config_file_io.h"
#include "config/config_manager.h"

#define FRAME_DIR_LEGACY_PREFIX "Animations/"

void config_runtime_paths_normalize_frame_dir(void) {
    const char *frame_root = ray_tracing_default_frame_root();
    const char *default_frame_dir = ray_tracing_default_frame_dir();
    if (animSettings.frameDir[0] == '\0') {
        strncpy(animSettings.frameDir, default_frame_dir, sizeof(animSettings.frameDir) - 1);
        animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
        return;
    }

    {
        size_t legacy_prefix_len = strlen(FRAME_DIR_LEGACY_PREFIX);
        if (strncmp(animSettings.frameDir, FRAME_DIR_LEGACY_PREFIX, legacy_prefix_len) != 0) {
            return;
        }

        {
            const char *suffix = animSettings.frameDir + legacy_prefix_len;
            char mapped[sizeof(animSettings.frameDir)];
            int written;
            if (!suffix[0]) suffix = "default";

            written = snprintf(mapped, sizeof(mapped), "%s/%s", frame_root, suffix);
            if (written <= 0 || (size_t)written >= sizeof(mapped)) {
                strncpy(animSettings.frameDir, default_frame_dir, sizeof(animSettings.frameDir) - 1);
                animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
                return;
            }

            strncpy(animSettings.frameDir, mapped, sizeof(animSettings.frameDir) - 1);
            animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
        }
    }
}

void config_runtime_paths_normalize_data_roots(void) {
    const char *default_input_root = ray_tracing_default_input_root();
    const char *default_output_root = ray_tracing_default_output_root();
    if (animSettings.inputRoot[0] == '\0') {
        strncpy(animSettings.inputRoot, default_input_root, sizeof(animSettings.inputRoot) - 1);
        animSettings.inputRoot[sizeof(animSettings.inputRoot) - 1] = '\0';
    }
    if (animSettings.outputRoot[0] == '\0') {
        strncpy(animSettings.outputRoot, default_output_root, sizeof(animSettings.outputRoot) - 1);
        animSettings.outputRoot[sizeof(animSettings.outputRoot) - 1] = '\0';
    }
}

void config_runtime_paths_normalize_video_output_root(void) {
    const char *default_video_root = ray_tracing_default_video_output_root();
    if (animSettings.videoOutputRoot[0] == '\0') {
        strncpy(animSettings.videoOutputRoot,
                default_video_root,
                sizeof(animSettings.videoOutputRoot) - 1);
        animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    }
}

bool config_runtime_paths_validate_root(char *target,
                                        size_t target_size,
                                        const char *default_path,
                                        const char *label,
                                        bool is_output_root,
                                        bool create_if_missing) {
    char stable_root[PATH_MAX];
    bool corrected = false;
    if (!target || target_size == 0 || !default_path || !default_path[0]) return false;
    if (target[0] == '\0') {
        fprintf(stderr,
                "[startup] %s root is empty; using default '%s'.\n",
                label ? label : "data",
                default_path);
        snprintf(target, target_size, "%s", default_path);
        corrected = true;
    }
    if (create_if_missing && !config_io_directory_exists(target)) {
        if (!config_io_ensure_directory_exists(target)) {
            fprintf(stderr,
                    "[startup] %s root '%s' could not be created; using default '%s'.\n",
                    label ? label : "data",
                    target,
                    default_path);
            snprintf(target, target_size, "%s", default_path);
            corrected = true;
        }
    }
    if (!config_io_directory_exists(target)) {
        bool has_stable_root = is_output_root
                                   ? ray_tracing_find_stable_output_root(stable_root, sizeof(stable_root))
                                   : ray_tracing_find_stable_input_root(stable_root, sizeof(stable_root));
        if (has_stable_root) {
            fprintf(stderr,
                    "[startup] %s root '%s' missing; using stable workspace root '%s'.\n",
                    label ? label : "data",
                    target,
                    stable_root);
            snprintf(target, target_size, "%s", stable_root);
            corrected = true;
        }
    }
    if (!config_io_directory_exists(target)) {
        fprintf(stderr,
                "[startup] %s root '%s' missing; using default '%s'.\n",
                label ? label : "data",
                target,
                default_path);
        snprintf(target, target_size, "%s", default_path);
        corrected = true;
    }
    if (create_if_missing && !config_io_directory_exists(target)) {
        if (!config_io_ensure_directory_exists(target)) {
            fprintf(stderr,
                    "[startup] %s default root '%s' is unavailable after fallback.\n",
                    label ? label : "data",
                    target);
        }
    }
    return corrected;
}
