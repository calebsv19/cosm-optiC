#include "config/config_file_io.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

bool config_io_directory_exists(const char* path) {
    struct stat st = {0};
    return (path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static bool config_io_ensure_directory_path(const char* path) {
    if (!path || !path[0]) return false;

    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1);

    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool config_io_ensure_parent_directory_for_file(const char* path) {
    if (!path || !path[0]) return false;
    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1);
    char* slash = strrchr(tmp, '/');
    if (!slash) return true;
    *slash = '\0';
    if (tmp[0] == '\0') return true;
    return config_io_ensure_directory_path(tmp);
}

FILE* config_io_open_read_with_fallback(const char* primary,
                                        const char* fallback,
                                        const char* legacy,
                                        const char** selected_path) {
    const char* candidates[] = {primary, fallback, legacy};
    if (selected_path) *selected_path = NULL;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        const char* path = candidates[i];
        if (!path || !path[0]) continue;
        FILE* file = fopen(path, "r");
        if (file) {
            if (selected_path) *selected_path = path;
            return file;
        }
    }
    return NULL;
}

struct json_object* config_io_parse_json_file(FILE* file,
                                              const char* lane_name,
                                              bool warn_on_empty) {
    const char* lane = (lane_name && lane_name[0]) ? lane_name : "config";
    struct json_object* config = NULL;
    char* buffer = NULL;
    long fsize = 0;
    size_t read_size = 0;

    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        printf("ERROR: Failed to seek %s file.\n", lane);
        fclose(file);
        return NULL;
    }
    fsize = ftell(file);
    if (fsize < 0) {
        printf("ERROR: Failed to get %s file size.\n", lane);
        fclose(file);
        return NULL;
    }
    rewind(file);

    if (fsize == 0) {
        if (warn_on_empty) {
            printf("Warning: %s file is empty.\n", lane);
        } else {
            printf("ERROR: %s file is empty.\n", lane);
        }
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)fsize + 1);
    if (!buffer) {
        printf("ERROR: Memory allocation failed for %s buffer.\n", lane);
        fclose(file);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)fsize, file);
    fclose(file);
    if (read_size != (size_t)fsize) {
        printf("ERROR: Failed to read %s file content.\n", lane);
        free(buffer);
        return NULL;
    }
    buffer[fsize] = '\0';

    config = json_tokener_parse(buffer);
    free(buffer);
    if (!config) {
        printf("ERROR: Failed to parse %s JSON.\n", lane);
        return NULL;
    }

    return config;
}
