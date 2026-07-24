#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_durable_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/ray_tracing_recovery_authority.h"

static bool durable_parent_directory(const char *path,
                                     char *out_directory,
                                     size_t out_directory_size) {
    const char *separator = NULL;
    size_t length = 0u;
    if (!path || !path[0] || !out_directory || out_directory_size == 0u) return false;
    separator = strrchr(path, '/');
    if (!separator) {
        if (out_directory_size < 2u) return false;
        memcpy(out_directory, ".", 2u);
        return true;
    }
    length = (size_t)(separator - path);
    if (length == 0u) {
        if (out_directory_size < 2u) return false;
        memcpy(out_directory, "/", 2u);
        return true;
    }
    if (length >= out_directory_size) return false;
    memcpy(out_directory, path, length);
    out_directory[length] = '\0';
    return true;
}

static bool durable_ensure_directory(const char *path) {
    char working[PATH_MAX];
    size_t length = 0u;
    if (!path || !path[0]) return false;
    length = strlen(path);
    if (length >= sizeof(working)) return false;
    memcpy(working, path, length + 1u);
    for (size_t index = 1u; index < length; ++index) {
        if (working[index] != '/') continue;
        working[index] = '\0';
        if (working[0] && mkdir(working, 0700) != 0 && errno != EEXIST) return false;
        working[index] = '/';
    }
    return mkdir(working, 0700) == 0 || errno == EEXIST;
}

static bool durable_ensure_parent_directory(const char *path) {
    char directory[PATH_MAX];
    return durable_parent_directory(path, directory, sizeof(directory)) &&
           durable_ensure_directory(directory);
}

static bool durable_sync_regular_fd(int fd) {
    if (fd < 0) return false;
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    if (fcntl(fd, F_FULLFSYNC) == 0) return true;
#endif
    return fsync(fd) == 0;
}

static bool durable_sync_parent_directory(const char *path) {
    char directory[PATH_MAX];
    int flags = O_RDONLY;
    int fd = -1;
    bool ok = false;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    if (!durable_parent_directory(path, directory, sizeof(directory))) return false;
    fd = open(directory, flags);
    if (fd < 0) return false;
    ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok;
}

static bool durable_remove_stale_temporary_file(const char *path) {
    struct stat status;
    if (lstat(path, &status) != 0) return errno == ENOENT;
    if (!S_ISREG(status.st_mode)) return false;
    return unlink(path) == 0;
}

static bool durable_make_temporary_path(const char *target_path,
                                        char *out_path,
                                        size_t out_path_size) {
    int written = 0;
    if (!target_path || !target_path[0] || !out_path || out_path_size == 0u) return false;
    written = snprintf(out_path,
                       out_path_size,
                       "%s.tmp.%ld",
                       target_path,
                       (long)getpid());
    return written > 0 && written < (int)out_path_size;
}

bool ray_tracing_durable_output_begin(RayTracingDurableOutput *output,
                                      const char *target_path) {
    int fd = -1;
    if (!output || !target_path || !target_path[0]) return false;
    memset(output, 0, sizeof(*output));
    if (snprintf(output->target_path,
                 sizeof(output->target_path),
                 "%s",
                 target_path) >= (int)sizeof(output->target_path) ||
        !durable_make_temporary_path(target_path,
                                     output->temporary_path,
                                     sizeof(output->temporary_path)) ||
        !durable_ensure_parent_directory(target_path) ||
        !durable_remove_stale_temporary_file(output->temporary_path)) {
        memset(output, 0, sizeof(*output));
        return false;
    }
    fd = open(output->temporary_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        memset(output, 0, sizeof(*output));
        return false;
    }
    output->stream = fdopen(fd, "wb");
    if (!output->stream) {
        close(fd);
        (void)unlink(output->temporary_path);
        memset(output, 0, sizeof(*output));
        return false;
    }
    return true;
}

bool ray_tracing_durable_output_commit(RayTracingDurableOutput *output) {
    int fd = -1;
    bool ok = true;
    if (!output || !output->stream || !output->target_path[0] ||
        !output->temporary_path[0]) {
        return false;
    }
    fd = fileno(output->stream);
    if (ferror(output->stream) != 0 || fflush(output->stream) != 0 ||
        !durable_sync_regular_fd(fd)) {
        ok = false;
    }
    if (fclose(output->stream) != 0) ok = false;
    output->stream = NULL;
    if (ok && !ray_tracing_output_fence_validate_environment()) ok = false;
    if (ok && rename(output->temporary_path, output->target_path) != 0) ok = false;
    if (ok && !durable_sync_parent_directory(output->target_path)) ok = false;
    if (!ok) (void)unlink(output->temporary_path);
    memset(output, 0, sizeof(*output));
    return ok;
}

void ray_tracing_durable_output_abort(RayTracingDurableOutput *output) {
    if (!output) return;
    if (output->stream) fclose(output->stream);
    if (output->temporary_path[0]) (void)unlink(output->temporary_path);
    memset(output, 0, sizeof(*output));
}

bool ray_tracing_durable_prepare_external_write(const char *target_path,
                                                char *out_temporary_path,
                                                size_t out_temporary_path_size) {
    if (!target_path || !target_path[0] || !out_temporary_path ||
        out_temporary_path_size == 0u ||
        !durable_make_temporary_path(target_path,
                                     out_temporary_path,
                                     out_temporary_path_size) ||
        !durable_ensure_parent_directory(target_path) ||
        !durable_remove_stale_temporary_file(out_temporary_path)) {
        return false;
    }
    return true;
}

bool ray_tracing_durable_commit_external_write(const char *temporary_path,
                                               const char *target_path) {
    struct stat status;
    int flags = O_RDONLY;
    int fd = -1;
    bool ok = false;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    if (!temporary_path || !temporary_path[0] || !target_path || !target_path[0] ||
        lstat(temporary_path, &status) != 0 || !S_ISREG(status.st_mode)) {
        return false;
    }
    fd = open(temporary_path, flags);
    if (fd < 0) return false;
    ok = durable_sync_regular_fd(fd);
    if (close(fd) != 0) ok = false;
    if (ok && !ray_tracing_output_fence_validate_environment()) ok = false;
    if (ok && rename(temporary_path, target_path) != 0) ok = false;
    if (ok && !durable_sync_parent_directory(target_path)) ok = false;
    if (!ok) (void)unlink(temporary_path);
    return ok;
}

static uint16_t durable_read_le16(const unsigned char *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
}

static uint32_t durable_read_le32(const unsigned char *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

bool ray_tracing_durable_validate_bmp(const char *path,
                                      int expected_width,
                                      int expected_height) {
    unsigned char header[54];
    struct stat status;
    FILE *file = NULL;
    uint32_t declared_size = 0u;
    uint32_t pixel_offset = 0u;
    uint32_t dib_size = 0u;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 0u;
    uint16_t bits_per_pixel = 0u;
    uint32_t compression = 0u;
    uint64_t row_bytes = 0u;
    uint64_t pixel_bytes = 0u;
    bool valid = false;

    if (!path || !path[0] || lstat(path, &status) != 0 ||
        !S_ISREG(status.st_mode) || status.st_size < (off_t)sizeof(header)) {
        return false;
    }
    file = fopen(path, "rb");
    if (!file) return false;
    if (fread(header, 1u, sizeof(header), file) != sizeof(header)) goto done;
    if (header[0] != 'B' || header[1] != 'M') goto done;
    declared_size = durable_read_le32(&header[2]);
    pixel_offset = durable_read_le32(&header[10]);
    dib_size = durable_read_le32(&header[14]);
    width = (int32_t)durable_read_le32(&header[18]);
    height = (int32_t)durable_read_le32(&header[22]);
    planes = durable_read_le16(&header[26]);
    bits_per_pixel = durable_read_le16(&header[28]);
    compression = durable_read_le32(&header[30]);
    if (declared_size < sizeof(header) || (off_t)declared_size > status.st_size ||
        pixel_offset < sizeof(header) || pixel_offset >= declared_size ||
        dib_size < 40u || width <= 0 || height == 0 || planes != 1u ||
        (bits_per_pixel != 24u && bits_per_pixel != 32u) ||
        (compression != 0u && compression != 3u)) {
        goto done;
    }
    if (expected_width > 0 && width != expected_width) goto done;
    if (expected_height > 0 &&
        (height == INT32_MIN || abs(height) != expected_height)) {
        goto done;
    }
    row_bytes = ((((uint64_t)(uint32_t)width * (uint64_t)bits_per_pixel) + 31u) / 32u) * 4u;
    pixel_bytes = row_bytes * (uint64_t)(height < 0 ? -(int64_t)height : (int64_t)height);
    if ((uint64_t)pixel_offset + pixel_bytes > (uint64_t)declared_size) goto done;
    valid = true;

done:
    fclose(file);
    return valid;
}
