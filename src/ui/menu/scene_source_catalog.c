#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "ui/scene_source_catalog.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"

#define SCENE_SOURCE_CATALOG_MAX_SCAN_DEPTH 2

static void add_catalog_entry(SceneSourceCatalogEntry *entries,
                              size_t max_entries,
                              size_t *entry_count,
                              const char *path,
                              int source);

static bool file_exists_regular(const char *path) {
    struct stat st;
    if (!path || !*path) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool path_has_json_extension(const char *path) {
    size_t len = 0;
    if (!path || !*path) return false;
    len = strlen(path);
    return len >= 5 && strcmp(path + len - 5, ".json") == 0;
}

static bool path_is_manifest_lane_file(const char *path) {
    const char *filename;
    if (!path || !*path) return false;
    filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    return strcmp(filename, "manifest.json") == 0 ||
           strcmp(filename, "scene_bundle.json") == 0;
}

static bool path_is_runtime_scene_catalog_candidate(const char *path) {
    const char *filename;
    if (!path_has_json_extension(path)) return false;
    if (path_is_manifest_lane_file(path)) return false;
    filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    if (strcmp(filename, "scene_authoring.json") == 0) return false;
    if (strcmp(filename, "scene_runtime.json") == 0) return true;
    return strstr(filename, "runtime") != NULL;
}

static void maybe_add_runtime_scene_in_dir(SceneSourceCatalogEntry *entries,
                                           size_t max_entries,
                                           size_t *entry_count,
                                           const char *dir) {
    char runtime_path[PATH_MAX];
    if (!dir || !dir[0]) return;
    if (snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", dir) >=
        (int)sizeof(runtime_path)) {
        return;
    }
    if (path_is_runtime_scene_catalog_candidate(runtime_path)) {
        add_catalog_entry(entries, max_entries, entry_count, runtime_path, SCENE_SOURCE_RUNTIME_SCENE);
    }
}

static void add_catalog_entry(SceneSourceCatalogEntry *entries,
                              size_t max_entries,
                              size_t *entry_count,
                              const char *path,
                              int source) {
    char resolved[PATH_MAX];
    const char *use_path = path;
    if (!entries || !entry_count || !path || !*path) return;
    if (*entry_count >= max_entries) return;

    if (realpath(path, resolved)) {
        use_path = resolved;
    }
    if (!file_exists_regular(use_path)) return;

    for (size_t i = 0; i < *entry_count; ++i) {
        if (strcmp(entries[i].path, use_path) == 0) {
            return;
        }
    }

    SceneSourceCatalogEntry *entry = &entries[*entry_count];
    strncpy(entry->path, use_path, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';
    entry->source = animation_config_scene_source_clamp(source);
    *entry_count += 1;
}

static void scan_manifest_root(SceneSourceCatalogEntry *entries,
                               size_t max_entries,
                               size_t *entry_count,
                               const char *root,
                               int depth) {
    DIR *dir;
    struct dirent *ent = NULL;
    char path_buf[PATH_MAX];
    if (!entries || !entry_count || !root || !*root) return;

    snprintf(path_buf, sizeof(path_buf), "%s/manifest.json", root);
    add_catalog_entry(entries, max_entries, entry_count, path_buf, SCENE_SOURCE_FLUID_MANIFEST);
    snprintf(path_buf, sizeof(path_buf), "%s/scene_bundle.json", root);
    add_catalog_entry(entries, max_entries, entry_count, path_buf, SCENE_SOURCE_FLUID_MANIFEST);
    maybe_add_runtime_scene_in_dir(entries, max_entries, entry_count, root);

    dir = opendir(root);
    if (!dir) return;

    while ((ent = readdir(dir)) != NULL) {
        struct stat st;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (snprintf(path_buf, sizeof(path_buf), "%s/%s", root, ent->d_name) >= (int)sizeof(path_buf)) {
            continue;
        }
        if (stat(path_buf, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            char manifest_path[PATH_MAX];
            char bundle_path[PATH_MAX];
            snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", path_buf);
            add_catalog_entry(entries, max_entries, entry_count, manifest_path, SCENE_SOURCE_FLUID_MANIFEST);
            snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", path_buf);
            add_catalog_entry(entries, max_entries, entry_count, bundle_path, SCENE_SOURCE_FLUID_MANIFEST);
            maybe_add_runtime_scene_in_dir(entries, max_entries, entry_count, path_buf);
            if (depth < SCENE_SOURCE_CATALOG_MAX_SCAN_DEPTH && *entry_count < max_entries) {
                scan_manifest_root(entries, max_entries, entry_count, path_buf, depth + 1);
            }
        } else if (S_ISREG(st.st_mode) && path_is_manifest_lane_file(path_buf)) {
            add_catalog_entry(entries, max_entries, entry_count, path_buf, SCENE_SOURCE_FLUID_MANIFEST);
        } else if (S_ISREG(st.st_mode) && path_is_runtime_scene_catalog_candidate(path_buf)) {
            add_catalog_entry(entries, max_entries, entry_count, path_buf, SCENE_SOURCE_RUNTIME_SCENE);
        }
        if (*entry_count >= max_entries) break;
    }

    closedir(dir);
}

size_t scene_source_catalog_collect(SceneSourceCatalogEntry *out_entries,
                                    size_t max_entries,
                                    const char *const *roots,
                                    size_t root_count,
                                    const char *fluid_manifest_path,
                                    const char *runtime_scene_path) {
    size_t entry_count = 0;
    if (!out_entries || max_entries == 0) return 0;

    for (size_t i = 0; i < root_count; ++i) {
        scan_manifest_root(out_entries, max_entries, &entry_count, roots[i], 0);
        if (entry_count >= max_entries) break;
    }

    add_catalog_entry(out_entries,
                      max_entries,
                      &entry_count,
                      fluid_manifest_path,
                      SCENE_SOURCE_FLUID_MANIFEST);
    if (path_is_runtime_scene_catalog_candidate(runtime_scene_path)) {
        add_catalog_entry(out_entries,
                          max_entries,
                          &entry_count,
                          runtime_scene_path,
                          SCENE_SOURCE_RUNTIME_SCENE);
    }
    return entry_count;
}
