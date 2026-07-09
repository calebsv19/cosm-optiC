#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "ui/menu_scene_project_summary.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config_manager.h"

static bool file_exists_regular(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool dir_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool join_path(const char *a, const char *b, char *out, size_t out_size) {
    if (!a || !a[0] || !b || !b[0] || !out || out_size == 0u) return false;
    return snprintf(out, out_size, "%s/%s", a, b) < (int)out_size;
}

static bool dirname_of(const char *path, char *out, size_t out_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out || out_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return false;
    len = (size_t)(slash - path);
    if (len >= out_size) return false;
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

static const char *basename_of(const char *path) {
    const char *slash = NULL;
    if (!path || !path[0]) return "";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool has_suffix(const char *value, const char *suffix) {
    size_t value_len = 0u;
    size_t suffix_len = 0u;
    if (!value || !suffix) return false;
    value_len = strlen(value);
    suffix_len = strlen(suffix);
    return value_len >= suffix_len &&
           strcmp(value + value_len - suffix_len, suffix) == 0;
}

static int count_runtime_mesh_assets(const char *root) {
    DIR *dir = NULL;
    struct dirent *ent = NULL;
    int count = 0;
    if (!root || !root[0]) return 0;
    dir = opendir(root);
    if (!dir) return 0;
    while ((ent = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!has_suffix(ent->d_name, ".runtime.json")) continue;
        if (!join_path(root, ent->d_name, path, sizeof(path))) continue;
        if (file_exists_regular(path)) {
            ++count;
        }
    }
    closedir(dir);
    return count;
}

static void set_summary_text(RayTracingMenuSceneProjectSummary *summary) {
    char mesh_label[32];
    if (!summary) return;

    if (!summary->selected_runtime_scene) {
        snprintf(summary->label, sizeof(summary->label), "No scene project");
        snprintf(summary->detail, sizeof(summary->detail), "Select a runtime scene");
        return;
    }
    if (!summary->project_detected) {
        snprintf(summary->label, sizeof(summary->label), "Loose runtime scene");
        snprintf(summary->detail, sizeof(summary->detail), "%s", basename_of(summary->project_root));
        return;
    }

    if (summary->mesh_asset_count > 0) {
        snprintf(mesh_label, sizeof(mesh_label), "mesh:%d", summary->mesh_asset_count);
    } else {
        snprintf(mesh_label, sizeof(mesh_label), "mesh:missing");
    }

    snprintf(summary->label,
             sizeof(summary->label),
             "Scene Project: %s",
             basename_of(summary->project_root));
    snprintf(summary->detail,
             sizeof(summary->detail),
             "scene:%s author:%s %s cache:%s bundle:%s render:%s",
             summary->has_scene_runtime ? "ok" : "missing",
             summary->has_scene_authoring ? "ok" : "missing",
             mesh_label,
             summary->has_physics_cache_manifest ? "ok" : "none",
             summary->has_physics_scene_bundle ? "ok" : "none",
             summary->has_render_request ? "ok" : "none");
}

bool ray_tracing_menu_scene_project_summary_for_runtime_scene(
    const char *runtime_scene_path,
    RayTracingMenuSceneProjectSummary *out_summary) {
    RayTracingMenuSceneProjectSummary summary;
    char project_root[PATH_MAX];
    char path[PATH_MAX];
    int mesh_count = 0;

    if (!out_summary) return false;
    memset(&summary, 0, sizeof(summary));

    if (!runtime_scene_path || !runtime_scene_path[0]) {
        set_summary_text(&summary);
        *out_summary = summary;
        return false;
    }

    summary.selected_runtime_scene = true;
    if (!dirname_of(runtime_scene_path, project_root, sizeof(project_root))) {
        snprintf(summary.project_root, sizeof(summary.project_root), "%s", runtime_scene_path);
        set_summary_text(&summary);
        *out_summary = summary;
        return false;
    }
    snprintf(summary.project_root, sizeof(summary.project_root), "%s", project_root);

    if (!join_path(project_root, "scene_project.json", path, sizeof(path)) ||
        !file_exists_regular(path)) {
        set_summary_text(&summary);
        *out_summary = summary;
        return false;
    }

    summary.project_detected = true;
    summary.has_scene_project = true;
    summary.has_scene_runtime = file_exists_regular(runtime_scene_path);

    if (join_path(project_root, "scene_authoring.json", path, sizeof(path))) {
        summary.has_scene_authoring = file_exists_regular(path);
    }
    if (join_path(project_root, "assets/mesh_assets", path, sizeof(path)) &&
        dir_exists(path)) {
        mesh_count = count_runtime_mesh_assets(path);
        summary.has_mesh_assets = mesh_count > 0;
        summary.mesh_asset_count = mesh_count;
    }
    if (join_path(project_root, "physics_sim/active_cache_manifest.json", path, sizeof(path))) {
        summary.has_physics_cache_manifest = file_exists_regular(path);
    }
    if (!summary.has_physics_cache_manifest &&
        join_path(project_root, "physics_sim/cache_manifest.json", path, sizeof(path))) {
        summary.has_physics_cache_manifest = file_exists_regular(path);
    }
    if (join_path(project_root, "assets/physics/active/scene_bundle.json", path, sizeof(path))) {
        summary.has_physics_scene_bundle = file_exists_regular(path);
    }
    if (!summary.has_physics_scene_bundle &&
        join_path(project_root, "assets/physics/scene_bundle.json", path, sizeof(path))) {
        summary.has_physics_scene_bundle = file_exists_regular(path);
    }
    if (join_path(project_root, "ray_tracing/render_request.json", path, sizeof(path))) {
        summary.has_render_request = file_exists_regular(path);
    }
    set_summary_text(&summary);
    *out_summary = summary;
    return true;
}

bool ray_tracing_menu_scene_project_summary_current(RayTracingMenuSceneProjectSummary *out_summary) {
    if (!out_summary) return false;
    if (animation_config_scene_source_clamp(animSettings.sceneSource) != SCENE_SOURCE_RUNTIME_SCENE ||
        animSettings.runtimeScenePath[0] == '\0') {
        RayTracingMenuSceneProjectSummary summary;
        memset(&summary, 0, sizeof(summary));
        set_summary_text(&summary);
        *out_summary = summary;
        return false;
    }
    return ray_tracing_menu_scene_project_summary_for_runtime_scene(animSettings.runtimeScenePath,
                                                                    out_summary);
}
