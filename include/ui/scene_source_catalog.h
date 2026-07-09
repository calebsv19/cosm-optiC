#ifndef SCENE_SOURCE_CATALOG_H
#define SCENE_SOURCE_CATALOG_H

#include <limits.h>
#include <stddef.h>

typedef struct {
    char path[PATH_MAX];
    int source;
} SceneSourceCatalogEntry;

size_t scene_source_catalog_collect(SceneSourceCatalogEntry *out_entries,
                                    size_t max_entries,
                                    const char *const *roots,
                                    size_t root_count,
                                    const char *fluid_manifest_path,
                                    const char *runtime_scene_path);

#endif
