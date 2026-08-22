#include "config/core/config_output_identity.h"

#include <stdio.h>
#include <string.h>

static bool config_output_identity_is_descendant(const char *parent,
                                                 const char *child) {
    size_t parent_length;
    if (!parent || !parent[0] || !child || !child[0] ||
        strstr(parent, "..") || strstr(child, "..")) {
        return false;
    }
    parent_length = strlen(parent);
    while (parent_length > 1u && parent[parent_length - 1u] == '/') {
        parent_length -= 1u;
    }
    if (parent_length == 1u && parent[0] == '/') {
        return child[0] == '/' && child[1] != '\0';
    }
    return strncmp(parent, child, parent_length) == 0 &&
           child[parent_length] == '/';
}

bool config_output_identity_reconcile_root(const char *output_root,
                                           const char *frame_directory,
                                           char *reconciled_root,
                                           size_t reconciled_root_size) {
    char candidate[4096];
    char *separator;
    size_t frame_length;
    if (!output_root || !output_root[0] || !frame_directory ||
        !frame_directory[0] || !reconciled_root || reconciled_root_size == 0u ||
        strstr(frame_directory, "..")) {
        return false;
    }
    if (config_output_identity_is_descendant(output_root, frame_directory)) {
        return false;
    }
    if (snprintf(candidate, sizeof(candidate), "%s", frame_directory) >=
        (int)sizeof(candidate)) {
        return false;
    }
    frame_length = strlen(candidate);
    while (frame_length > 1u && candidate[frame_length - 1u] == '/') {
        candidate[--frame_length] = '\0';
    }
    separator = strrchr(candidate, '/');
    if (!separator || separator == candidate) {
        return false;
    }
    *separator = '\0';
    return snprintf(reconciled_root, reconciled_root_size, "%s", candidate) <
           (int)reconciled_root_size;
}
