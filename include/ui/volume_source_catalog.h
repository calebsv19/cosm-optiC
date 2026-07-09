#ifndef VOLUME_SOURCE_CATALOG_H
#define VOLUME_SOURCE_CATALOG_H

#include <limits.h>
#include <stddef.h>

typedef struct {
    char path[PATH_MAX];
    int kind;
} VolumeSourceCatalogEntry;

size_t volume_source_catalog_collect(VolumeSourceCatalogEntry *out_entries,
                                     size_t max_entries,
                                     const char *const *roots,
                                     size_t root_count,
                                     const char *selected_volume_path);

#endif
