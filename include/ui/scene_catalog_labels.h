#ifndef RAY_TRACING_SCENE_CATALOG_LABELS_H
#define RAY_TRACING_SCENE_CATALOG_LABELS_H
#include <stddef.h>
void scene_catalog_directory_label(const char *path, const char *const *paths,
                                   size_t count, char *out, size_t capacity);
#endif
