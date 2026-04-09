#include "app/data_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static const char *k_default_input_root = "config";
static const char *k_default_output_root = "data/runtime";
static const char *k_default_shape_asset_dir = "config/objects";
static const char *k_default_import_dir = "import";
static const char *k_default_frame_root = "data/runtime/frames";
static const char *k_default_frame_dir = "data/runtime/frames/default";
static const char *k_default_video_output_path = "data/runtime/videos/output.mp4";

static bool dir_exists(const char *path) {
    DIR *d = NULL;
    if (!path || !path[0]) return false;
    d = opendir(path);
    if (!d) return false;
    closedir(d);
    return true;
}

const char *ray_tracing_default_input_root(void) {
    return k_default_input_root;
}

const char *ray_tracing_default_output_root(void) {
    return k_default_output_root;
}

const char *ray_tracing_default_shape_asset_dir(void) {
    return k_default_shape_asset_dir;
}

const char *ray_tracing_default_import_dir(void) {
    return k_default_import_dir;
}

const char *ray_tracing_default_frame_root(void) {
    return k_default_frame_root;
}

const char *ray_tracing_default_frame_dir(void) {
    return k_default_frame_dir;
}

const char *ray_tracing_default_video_output_path(void) {
    return k_default_video_output_path;
}

const char *ray_tracing_env_input_root(void) {
    const char *env = getenv("RAY_TRACING_INPUT_ROOT");
    if (env && env[0]) return env;
    return k_default_input_root;
}

const char *ray_tracing_env_output_root(void) {
    const char *env = getenv("RAY_TRACING_OUTPUT_ROOT");
    if (env && env[0]) return env;
    return k_default_output_root;
}

bool ray_tracing_compose_path(const char *root,
                              const char *leaf,
                              char *out,
                              size_t out_size) {
    size_t root_len = 0;
    if (!root || !root[0] || !leaf || !leaf[0] || !out || out_size == 0) {
        return false;
    }
    root_len = strlen(root);
    if (root_len + 1 + strlen(leaf) + 1 > out_size) {
        return false;
    }
    if (root[root_len - 1] == '/') {
        snprintf(out, out_size, "%s%s", root, leaf);
    } else {
        snprintf(out, out_size, "%s/%s", root, leaf);
    }
    return true;
}

const char *ray_tracing_resolve_shape_asset_dir(const char *shape_asset_env,
                                                char *out,
                                                size_t out_size) {
    const char *input_root = ray_tracing_env_input_root();
    if (shape_asset_env && shape_asset_env[0]) {
        return shape_asset_env;
    }
    if (ray_tracing_compose_path(input_root, "objects", out, out_size) &&
        dir_exists(out)) {
        return out;
    }
    return k_default_shape_asset_dir;
}

const char *ray_tracing_resolve_import_dir(char *out, size_t out_size) {
    const char *input_root = ray_tracing_env_input_root();
    const char *import_override = getenv("RAY_TRACING_IMPORT_DIR");
    if (import_override && import_override[0]) {
        return import_override;
    }
    if (ray_tracing_compose_path(input_root, "import", out, out_size) &&
        dir_exists(out)) {
        return out;
    }
    return k_default_import_dir;
}

size_t ray_tracing_manifest_default_roots(const char ***out_roots) {
    static char input_volume_frames[PATH_MAX];
    static char input_scenes[PATH_MAX];
    static char input_shared_scenes[PATH_MAX];
    static char output_volume_frames[PATH_MAX];
    static const char *legacy_roots[] = {
        "export/volume_frames",
        "../physics_sim/export/volume_frames",
        "../ray_tracing/export/volume_frames",
        "../shared/assets/scenes"
    };
    static const char *roots[8];
    size_t count = 0;
    const char *input_root = ray_tracing_env_input_root();
    const char *output_root = ray_tracing_env_output_root();

    if (ray_tracing_compose_path(input_root, "export/volume_frames",
                                 input_volume_frames, sizeof(input_volume_frames))) {
        roots[count++] = input_volume_frames;
    }
    if (ray_tracing_compose_path(input_root, "scenes",
                                 input_scenes, sizeof(input_scenes))) {
        roots[count++] = input_scenes;
    }
    if (ray_tracing_compose_path(input_root, "assets/scenes",
                                 input_shared_scenes, sizeof(input_shared_scenes))) {
        roots[count++] = input_shared_scenes;
    }
    if (ray_tracing_compose_path(output_root, "volume_frames",
                                 output_volume_frames, sizeof(output_volume_frames))) {
        roots[count++] = output_volume_frames;
    }
    for (size_t i = 0; i < sizeof(legacy_roots) / sizeof(legacy_roots[0]); ++i) {
        roots[count++] = legacy_roots[i];
    }
    if (out_roots) {
        *out_roots = roots;
    }
    return count;
}
