#include "ui/scene_catalog_labels.h"
#include <stdio.h>
#include <string.h>
static const char *suffix(const char *path, const char *end, unsigned components) {
    const char *p = end;
    while (p > path) {
        --p;
        if (*p == '/' && --components == 0) return p+1;
    }
    return path;
}
void scene_catalog_directory_label(const char *path, const char *const *paths,
                                   size_t count, char *out, size_t capacity) {
    if (!out || !capacity) return;
    out[0] = '\0';
    if (!path || !*path) return;
    const char *end = strrchr(path,'/');
    if (!end) { snprintf(out,capacity,"Current directory"); return; }
    const char *start = end;
    for (unsigned depth=1;;++depth) {
        start = suffix(path,end,depth);
        size_t len = (size_t)(end-start);
        int unique = 1;
        for (size_t i=0;i<count;++i) {
            const char *other = paths[i];
            if (!other || !strcmp(path,other)) continue;
            const char *other_end = strrchr(other,'/');
            if (!other_end) continue;
            /* Distinct files in the same directory naturally share provenance. */
            if ((size_t)(other_end-other) == (size_t)(end-path) &&
                !strncmp(path,other,(size_t)(end-path))) continue;
            const char *other_start = suffix(other,other_end,depth);
            if ((size_t)(other_end-other_start) == len && !strncmp(start,other_start,len)) { unique=0; break; }
        }
        if (unique || start == path) break;
    }
    snprintf(out,capacity,"%.*s",(int)(end-start),start);
}
