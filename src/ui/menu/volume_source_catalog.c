#include "ui/volume_source_catalog.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "import/fluid_volume_import_3d.h"

static bool file_exists_regular(const char *path) {
    struct stat st;
    if (!path || !*path) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static void add_catalog_entry(VolumeSourceCatalogEntry *entries,
                              size_t max_entries,
                              size_t *entry_count,
                              const char *path,
                              int kind) {
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

    entries[*entry_count].kind = kind;
    strncpy(entries[*entry_count].path, use_path, sizeof(entries[*entry_count].path) - 1);
    entries[*entry_count].path[sizeof(entries[*entry_count].path) - 1] = '\0';
    *entry_count += 1u;
}

static void maybe_add_supported_path(VolumeSourceCatalogEntry *entries,
                                     size_t max_entries,
                                     size_t *entry_count,
                                     const char *path) {
    RuntimeVolume3DSourceKind kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    if (!path || !*path) return;
    if (!fluid_volume_import_3d_classify_path(path, &kind)) return;
    add_catalog_entry(entries, max_entries, entry_count, path, (int)kind);
}

static void scan_root(VolumeSourceCatalogEntry *entries,
                      size_t max_entries,
                      size_t *entry_count,
                      const char *root) {
    DIR *dir = NULL;
    struct dirent *ent = NULL;
    char path_buf[PATH_MAX];

    if (!entries || !entry_count || !root || !*root) return;

    maybe_add_supported_path(entries, max_entries, entry_count, root);
    snprintf(path_buf, sizeof(path_buf), "%s/manifest.json", root);
    maybe_add_supported_path(entries, max_entries, entry_count, path_buf);
    snprintf(path_buf, sizeof(path_buf), "%s/scene_bundle.json", root);
    maybe_add_supported_path(entries, max_entries, entry_count, path_buf);

    dir = opendir(root);
    if (!dir) return;

    while ((ent = readdir(dir)) != NULL) {
        struct stat st;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(path_buf, sizeof(path_buf), "%s/%s", root, ent->d_name);
        if (stat(path_buf, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            DIR *sub = NULL;
            struct dirent *sub_ent = NULL;
            char nested_path[PATH_MAX];

            snprintf(nested_path, sizeof(nested_path), "%s/manifest.json", path_buf);
            maybe_add_supported_path(entries, max_entries, entry_count, nested_path);
            snprintf(nested_path, sizeof(nested_path), "%s/scene_bundle.json", path_buf);
            maybe_add_supported_path(entries, max_entries, entry_count, nested_path);

            sub = opendir(path_buf);
            if (!sub) continue;
            while ((sub_ent = readdir(sub)) != NULL) {
                struct stat sub_st;
                if (strcmp(sub_ent->d_name, ".") == 0 || strcmp(sub_ent->d_name, "..") == 0) continue;
                snprintf(nested_path, sizeof(nested_path), "%s/%s", path_buf, sub_ent->d_name);
                if (stat(nested_path, &sub_st) != 0 || !S_ISREG(sub_st.st_mode)) continue;
                maybe_add_supported_path(entries, max_entries, entry_count, nested_path);
            }
            closedir(sub);
        } else if (S_ISREG(st.st_mode)) {
            maybe_add_supported_path(entries, max_entries, entry_count, path_buf);
        }
    }

    closedir(dir);
}

size_t volume_source_catalog_collect(VolumeSourceCatalogEntry *out_entries,
                                     size_t max_entries,
                                     const char *const *roots,
                                     size_t root_count,
                                     const char *selected_volume_path) {
    size_t entry_count = 0;
    if (!out_entries || max_entries == 0u) return 0u;

    for (size_t i = 0; i < root_count; ++i) {
        scan_root(out_entries, max_entries, &entry_count, roots[i]);
        if (entry_count >= max_entries) break;
    }

    maybe_add_supported_path(out_entries, max_entries, &entry_count, selected_volume_path);
    return entry_count;
}
