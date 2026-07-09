#include "app/ray_tracing_request_utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void RayTracingRequestSetDiag(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}

bool RayTracingCopyString(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

bool RayTracingReadTextFile(const char *path, char **out_text) {
    FILE *file = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;

    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

void RayTracingDirnameOf(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!out_dir || out_dir_size == 0u) return;
    out_dir[0] = '\0';
    if (!path || !path[0]) return;
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out_dir, out_dir_size, ".");
        return;
    }
    len = (size_t)(slash - path);
    if (len == 0u) {
        snprintf(out_dir, out_dir_size, "/");
        return;
    }
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
}

bool RayTracingPathIsAbsolute(const char *path) {
    return path && path[0] == '/';
}

bool RayTracingResolveRequestPath(const char *request_dir,
                                  const char *path,
                                  char *out_path,
                                  size_t out_path_size) {
    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (RayTracingPathIsAbsolute(path)) {
        return RayTracingCopyString(out_path, out_path_size, path);
    }
    if (!request_dir || !request_dir[0] || strcmp(request_dir, ".") == 0) {
        return RayTracingCopyString(out_path, out_path_size, path);
    }
    if (snprintf(out_path, out_path_size, "%s/%s", request_dir, path) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool RayTracingNormalizeResolvedPath(const char *path, char *out_path, size_t out_path_size) {
    char working[PATH_MAX];
    char result[PATH_MAX];
    char *segments[PATH_MAX / 2u];
    size_t segment_count = 0u;
    bool absolute = false;
    char *cursor = NULL;

    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (snprintf(working, sizeof(working), "%s", path) >= (int)sizeof(working)) {
        out_path[0] = '\0';
        return false;
    }

    absolute = working[0] == '/';
    cursor = working;
    while (*cursor) {
        char *segment = cursor;
        while (*cursor && *cursor != '/') cursor += 1;
        if (*cursor == '/') {
            *cursor = '\0';
            cursor += 1;
        }
        if (segment[0] == '\0' || strcmp(segment, ".") == 0) {
            continue;
        }
        if (strcmp(segment, "..") == 0) {
            if (segment_count > 0u && strcmp(segments[segment_count - 1u], "..") != 0) {
                segment_count -= 1u;
                continue;
            }
            if (absolute) {
                continue;
            }
        }
        if (segment_count >= (sizeof(segments) / sizeof(segments[0]))) {
            out_path[0] = '\0';
            return false;
        }
        segments[segment_count++] = segment;
    }

    result[0] = '\0';
    if (absolute) {
        snprintf(result, sizeof(result), "/");
    }
    for (size_t i = 0u; i < segment_count; ++i) {
        size_t len = strlen(result);
        int written = 0;
        if (absolute && len == 1u && result[0] == '/') {
            written = snprintf(result + len, sizeof(result) - len, "%s", segments[i]);
        } else if (len == 0u) {
            written = snprintf(result, sizeof(result), "%s", segments[i]);
        } else {
            written = snprintf(result + len, sizeof(result) - len, "/%s", segments[i]);
        }
        if (written < 0 || (size_t)written >= sizeof(result) - len) {
            out_path[0] = '\0';
            return false;
        }
    }
    if (result[0] == '\0') {
        snprintf(result, sizeof(result), "%s", absolute ? "/" : ".");
    }
    return RayTracingCopyString(out_path, out_path_size, result);
}

bool RayTracingResolveExistingRequestPath(const char *request_dir,
                                          const char *path,
                                          char *out_path,
                                          size_t out_path_size) {
    char resolved[PATH_MAX];
    char canonical[PATH_MAX];
    if (!RayTracingResolveRequestPath(request_dir, path, resolved, sizeof(resolved))) {
        return false;
    }
    if (!realpath(resolved, canonical)) {
        return false;
    }
    return RayTracingCopyString(out_path, out_path_size, canonical);
}

bool RayTracingResolveRequestInputPath(const char *request_dir,
                                       const char *path,
                                       char *out_path,
                                       size_t out_path_size) {
    char resolved[PATH_MAX];
    char canonical[PATH_MAX];
    if (!RayTracingResolveRequestPath(request_dir, path, resolved, sizeof(resolved))) {
        return false;
    }
    if (realpath(resolved, canonical)) {
        return RayTracingCopyString(out_path, out_path_size, canonical);
    }
    return RayTracingNormalizeResolvedPath(resolved, out_path, out_path_size);
}

bool RayTracingResolveRequestOutputPath(const char *request_dir,
                                        const char *path,
                                        char *out_path,
                                        size_t out_path_size) {
    char resolved[PATH_MAX];
    if (!RayTracingResolveRequestPath(request_dir, path, resolved, sizeof(resolved))) {
        return false;
    }
    return RayTracingNormalizeResolvedPath(resolved, out_path, out_path_size);
}

bool RayTracingEnvGetInt(const char *name, int *out_value) {
    const char *raw = getenv(name);
    char *end = NULL;
    long parsed = 0;
    if (out_value) *out_value = 0;
    if (!name || !out_value || !raw || !raw[0]) return false;
    parsed = strtol(raw, &end, 10);
    if (end == raw || (end && *end != '\0') || parsed < -2147483647L ||
        parsed > 2147483647L) {
        return false;
    }
    *out_value = (int)parsed;
    return true;
}

bool RayTracingJsonGetObject(json_object *owner, const char *key, json_object **out_obj) {
    json_object *obj = NULL;
    if (out_obj) *out_obj = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_object)) {
        return false;
    }
    if (out_obj) *out_obj = obj;
    return true;
}

bool RayTracingJsonGetString(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

bool RayTracingJsonGetInt(json_object *owner, const char *key, int *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_int(obj);
    return true;
}

bool RayTracingJsonGetDouble(json_object *owner, const char *key, double *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0.0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_double(obj);
    return true;
}

bool RayTracingJsonGetBool(json_object *owner, const char *key, bool *out_value) {
    json_object *obj = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_boolean)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_boolean(obj) != 0;
    return true;
}

bool RayTracingJsonGetDoubleAny(json_object *owner,
                                const char *key_a,
                                const char *key_b,
                                double *out_value) {
    if (!owner || !out_value) return false;
    if (key_a && RayTracingJsonGetDouble(owner, key_a, out_value)) return true;
    if (key_b && RayTracingJsonGetDouble(owner, key_b, out_value)) return true;
    return false;
}

bool RayTracingJsonGetIntAny(json_object *owner,
                             const char *key_a,
                             const char *key_b,
                             int *out_value) {
    if (!owner || !out_value) return false;
    if (key_a && RayTracingJsonGetInt(owner, key_a, out_value)) return true;
    if (key_b && RayTracingJsonGetInt(owner, key_b, out_value)) return true;
    return false;
}

void RayTracingJsonWriteString(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    if (!file) return;
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static double ray_tracing_clamp_progress(double progress_ratio) {
    if (progress_ratio < 0.0) return 0.0;
    if (progress_ratio > 1.0) return 1.0;
    return progress_ratio;
}

double RayTracingProgressRatioCompleted(int frames_completed,
                                        int frame_count,
                                        int temporal_subpasses_completed,
                                        int temporal_subpasses_total) {
    double progress_ratio = 0.0;
    if (temporal_subpasses_total > 0) {
        progress_ratio =
            (double)temporal_subpasses_completed / (double)temporal_subpasses_total;
    } else if (frame_count > 0) {
        progress_ratio = (double)frames_completed / (double)frame_count;
    }
    return ray_tracing_clamp_progress(progress_ratio);
}

double RayTracingProgressRatioActive(int frames_completed,
                                     int frame_count,
                                     int temporal_subpasses_started,
                                     int temporal_subpasses_completed,
                                     int temporal_subpasses_total,
                                     size_t completed_tiles_in_subpass,
                                     size_t total_tiles_in_subpass) {
    double progress_ratio =
        RayTracingProgressRatioCompleted(frames_completed,
                                         frame_count,
                                         temporal_subpasses_completed,
                                         temporal_subpasses_total);
    if (temporal_subpasses_total > 0 &&
        total_tiles_in_subpass > 0u &&
        temporal_subpasses_started > temporal_subpasses_completed) {
        progress_ratio +=
            ((double)completed_tiles_in_subpass / (double)total_tiles_in_subpass) /
            (double)temporal_subpasses_total;
    }
    return ray_tracing_clamp_progress(progress_ratio);
}
