#include "app/data_paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

static const char *k_default_input_root = "config";
static const char *k_default_output_root = "data/runtime";
static const char *k_default_shape_asset_dir = "config/objects";
static const char *k_default_import_dir = "import";
static const char *k_default_frame_root = "data/runtime/frames";
static const char *k_default_frame_dir = "data/runtime/frames/default";
static const char *k_default_video_output_root = "data/runtime/videos";
static const char *k_default_video_output_filename = "output.mp4";
static const char *k_default_video_output_path = "data/runtime/videos/output.mp4";

static bool dir_exists(const char *path) {
    DIR *d = NULL;
    if (!path || !path[0]) return false;
    d = opendir(path);
    if (!d) return false;
    closedir(d);
    return true;
}

static bool resolve_candidate_program_root(const char *candidate,
                                           char *out,
                                           size_t out_size) {
    char config_path[PATH_MAX];
    char resolved_root[PATH_MAX];
    const char *final_root = candidate;
    if (!candidate || !candidate[0] || !out || out_size == 0) return false;
    if (snprintf(config_path, sizeof(config_path), "%s/config", candidate) >= (int)sizeof(config_path)) {
        return false;
    }
    if (!dir_exists(config_path)) return false;
    if (realpath(candidate, resolved_root)) {
        final_root = resolved_root;
    }
    if (snprintf(out, out_size, "%s", final_root) >= (int)out_size) {
        return false;
    }
    return true;
}

static bool find_program_root_from_base(const char *base_dir,
                                        char *out,
                                        size_t out_size) {
    char candidate[PATH_MAX];
    if (!base_dir || !base_dir[0] || !out || out_size == 0) return false;

    if (snprintf(candidate, sizeof(candidate), "%s/ray_tracing", base_dir) < (int)sizeof(candidate) &&
        resolve_candidate_program_root(candidate, out, out_size)) {
        return true;
    }
    if (snprintf(candidate, sizeof(candidate), "%s/CodeWork/ray_tracing", base_dir) < (int)sizeof(candidate) &&
        resolve_candidate_program_root(candidate, out, out_size)) {
        return true;
    }
    if (snprintf(candidate, sizeof(candidate), "%s/Desktop/CodeWork/ray_tracing", base_dir) < (int)sizeof(candidate) &&
        resolve_candidate_program_root(candidate, out, out_size)) {
        return true;
    }
    return false;
}

static bool ray_tracing_find_program_root(char *out, size_t out_size) {
    char cwd[PATH_MAX];
    char cursor[PATH_MAX];
    const char *home = getenv("HOME");
    if (!out || out_size == 0) return false;

    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(cursor, sizeof(cursor), "%s", cwd);
        while (cursor[0]) {
            if (find_program_root_from_base(cursor, out, out_size)) {
                return true;
            }
            char *slash = strrchr(cursor, '/');
            if (!slash) break;
            if (slash == cursor) {
                cursor[1] = '\0';
                break;
            }
            *slash = '\0';
        }
    }

    if (home && home[0]) {
        if (find_program_root_from_base(home, out, out_size)) {
            return true;
        }
    }
    return false;
}

bool ray_tracing_find_stable_input_root(char *out, size_t out_size) {
    char program_root[PATH_MAX];
    char candidate[PATH_MAX];
    if (!out || out_size == 0) return false;
    if (!ray_tracing_find_program_root(program_root, sizeof(program_root))) return false;
    if (snprintf(candidate, sizeof(candidate), "%s/config", program_root) >= (int)sizeof(candidate)) {
        return false;
    }
    if (!dir_exists(candidate)) return false;
    if (snprintf(out, out_size, "%s", candidate) >= (int)out_size) return false;
    return true;
}

bool ray_tracing_find_stable_output_root(char *out, size_t out_size) {
    char program_root[PATH_MAX];
    char candidate[PATH_MAX];
    if (!out || out_size == 0) return false;
    if (!ray_tracing_find_program_root(program_root, sizeof(program_root))) return false;
    if (snprintf(candidate, sizeof(candidate), "%s/data/runtime", program_root) >= (int)sizeof(candidate)) {
        return false;
    }
    if (snprintf(out, out_size, "%s", candidate) >= (int)out_size) return false;
    return true;
}

static void append_workspace_program_roots(const char *base_dir,
                                           const char **roots,
                                           size_t *count,
                                           size_t max_roots,
                                           char discovered[][PATH_MAX],
                                           size_t *discovered_count) {
    char candidate_samples[PATH_MAX];
    char candidate_runtime[PATH_MAX];
    char candidate_shared[PATH_MAX];
    char candidate_phys[PATH_MAX];
    const char *candidates[4];
    if (!base_dir || !base_dir[0] || !roots || !count || !discovered || !discovered_count) {
        return;
    }
    if (snprintf(candidate_samples,
                 sizeof(candidate_samples),
                 "%s/ray_tracing/config/samples",
                 base_dir) >= (int)sizeof(candidate_samples)) {
        return;
    }
    if (snprintf(candidate_runtime,
                 sizeof(candidate_runtime),
                 "%s/ray_tracing/data/runtime/scenes",
                 base_dir) >= (int)sizeof(candidate_runtime)) {
        return;
    }
    if (snprintf(candidate_shared,
                 sizeof(candidate_shared),
                 "%s/ray_tracing/third_party/codework_shared/assets/scenes",
                 base_dir) >= (int)sizeof(candidate_shared)) {
        return;
    }
    if (snprintf(candidate_phys,
                 sizeof(candidate_phys),
                 "%s/physics_sim/config/samples",
                 base_dir) >= (int)sizeof(candidate_phys)) {
        return;
    }

    candidates[0] = candidate_samples;
    candidates[1] = candidate_runtime;
    candidates[2] = candidate_shared;
    candidates[3] = candidate_phys;
    for (size_t i = 0; i < 4; ++i) {
        char resolved[PATH_MAX];
        const char *final_path = candidates[i];
        bool duplicate = false;
        if (!dir_exists(candidates[i])) continue;
        if (realpath(candidates[i], resolved)) {
            final_path = resolved;
        }
        for (size_t r = 0; r < *count; ++r) {
            if (roots[r] && strcmp(roots[r], final_path) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;
        if (*count >= max_roots) return;
        if (*discovered_count >= 16u) return;
        snprintf(discovered[*discovered_count],
                 PATH_MAX,
                 "%s",
                 final_path);
        roots[*count] = discovered[*discovered_count];
        *count += 1u;
        *discovered_count += 1u;
    }
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

const char *ray_tracing_default_video_output_root(void) {
    return k_default_video_output_root;
}

const char *ray_tracing_default_video_output_filename(void) {
    return k_default_video_output_filename;
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

const char *ray_tracing_env_video_output_root(void) {
    const char *env = getenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    if (env && env[0]) return env;
    return k_default_video_output_root;
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

bool ray_tracing_resolve_frame_output_dir(const char *configured_frame_dir,
                                          char *out,
                                          size_t out_size) {
    const char *candidate = configured_frame_dir;
    if (!out || out_size == 0) return false;
    if (!candidate || !candidate[0]) {
        candidate = k_default_frame_dir;
    }
    return snprintf(out, out_size, "%s", candidate) < (int)out_size;
}

bool ray_tracing_resolve_video_output_root(const char *configured_video_output_root,
                                           char *out,
                                           size_t out_size) {
    const char *candidate = configured_video_output_root;
    if (!out || out_size == 0) return false;
    if (!candidate || !candidate[0]) {
        candidate = ray_tracing_env_video_output_root();
    }
    return snprintf(out, out_size, "%s", candidate) < (int)out_size;
}

bool ray_tracing_resolve_video_output_path(const char *configured_video_output_root,
                                           char *out,
                                           size_t out_size) {
    char root[PATH_MAX];
    if (!out || out_size == 0) return false;
    if (!ray_tracing_resolve_video_output_root(configured_video_output_root, root, sizeof(root))) {
        return false;
    }
    return ray_tracing_compose_path(root, k_default_video_output_filename, out, out_size);
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
    static char input_root_direct[PATH_MAX];
    static char input_volume_frames[PATH_MAX];
    static char input_scenes[PATH_MAX];
    static char input_samples[PATH_MAX];
    static char input_shared_scenes[PATH_MAX];
    static char output_root_direct[PATH_MAX];
    static char output_volume_frames[PATH_MAX];
    static char output_runtime_scenes[PATH_MAX];
    static char discovered_workspace_roots[16][PATH_MAX];
    char cwd[PATH_MAX];
    char cursor[PATH_MAX];
    static const char *legacy_roots[] = {
        "export/volume_frames",
        "../physics_sim/export/volume_frames",
        "../ray_tracing/export/volume_frames",
        "../ray_tracing/data/runtime/scenes",
        "../ray_tracing/config/samples",
        "../physics_sim/config/samples",
        "third_party/codework_shared/assets/scenes"
    };
    static const char *roots[32];
    size_t count = 0;
    size_t discovered_count = 0;
    const char *input_root = ray_tracing_env_input_root();
    const char *output_root = ray_tracing_env_output_root();

    if (snprintf(input_root_direct, sizeof(input_root_direct), "%s", input_root) < (int)sizeof(input_root_direct)) {
        roots[count++] = input_root_direct;
    }
    if (ray_tracing_compose_path(input_root, "export/volume_frames",
                                 input_volume_frames, sizeof(input_volume_frames))) {
        roots[count++] = input_volume_frames;
    }
    if (ray_tracing_compose_path(input_root, "scenes",
                                 input_scenes, sizeof(input_scenes))) {
        roots[count++] = input_scenes;
    }
    if (ray_tracing_compose_path(input_root, "samples",
                                 input_samples, sizeof(input_samples))) {
        roots[count++] = input_samples;
    }
    if (ray_tracing_compose_path(input_root, "assets/scenes",
                                 input_shared_scenes, sizeof(input_shared_scenes))) {
        roots[count++] = input_shared_scenes;
    }
    if (snprintf(output_root_direct, sizeof(output_root_direct), "%s", output_root) < (int)sizeof(output_root_direct)) {
        roots[count++] = output_root_direct;
    }
    if (ray_tracing_compose_path(output_root, "volume_frames",
                                 output_volume_frames, sizeof(output_volume_frames))) {
        roots[count++] = output_volume_frames;
    }
    if (ray_tracing_compose_path(output_root, "scenes",
                                 output_runtime_scenes, sizeof(output_runtime_scenes))) {
        roots[count++] = output_runtime_scenes;
    }
    for (size_t i = 0; i < sizeof(legacy_roots) / sizeof(legacy_roots[0]); ++i) {
        roots[count++] = legacy_roots[i];
    }
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(cursor, sizeof(cursor), "%s", cwd);
        while (cursor[0] && count < (sizeof(roots) / sizeof(roots[0]))) {
            append_workspace_program_roots(cursor,
                                           roots,
                                           &count,
                                           sizeof(roots) / sizeof(roots[0]),
                                           discovered_workspace_roots,
                                           &discovered_count);

            char *slash = strrchr(cursor, '/');
            if (!slash) break;
            if (slash == cursor) {
                cursor[1] = '\0';
                append_workspace_program_roots(cursor,
                                               roots,
                                               &count,
                                               sizeof(roots) / sizeof(roots[0]),
                                               discovered_workspace_roots,
                                               &discovered_count);
                break;
            }
            *slash = '\0';
        }
    }
    if (out_roots) {
        *out_roots = roots;
    }
    return count;
}
