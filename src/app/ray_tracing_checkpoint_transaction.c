#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_checkpoint_transaction.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool parent_directory(const char* path,
                             char* out_directory,
                             size_t out_directory_size) {
    const char* separator = NULL;
    size_t length = 0u;
    if (!path || !path[0] || !out_directory || out_directory_size == 0u) {
        return false;
    }
    separator = strrchr(path, '/');
    if (!separator) {
        return snprintf(out_directory, out_directory_size, ".") == 1;
    }
    length = (size_t)(separator - path);
    if (length == 0u) {
        return snprintf(out_directory, out_directory_size, "/") == 1;
    }
    if (length >= out_directory_size) return false;
    memcpy(out_directory, path, length);
    out_directory[length] = '\0';
    return true;
}

static bool ensure_directory(const char* path) {
    char working[PATH_MAX];
    size_t length = 0u;
    if (!path || !path[0]) return false;
    length = strlen(path);
    if (length >= sizeof(working)) return false;
    memcpy(working, path, length + 1u);
    for (size_t i = 1u; i < length; ++i) {
        if (working[i] != '/') continue;
        working[i] = '\0';
        if (working[0] && mkdir(working, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        working[i] = '/';
    }
    return mkdir(working, 0700) == 0 || errno == EEXIST;
}

static bool ensure_parent(const char* path) {
    char directory[PATH_MAX];
    return parent_directory(path, directory, sizeof(directory)) &&
           ensure_directory(directory);
}

static bool sync_fd(int fd) {
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    if (fcntl(fd, F_FULLFSYNC) == 0) return true;
#endif
    return fd >= 0 && fsync(fd) == 0;
}

static bool sync_parent(const char* path) {
    char directory[PATH_MAX];
    int flags = O_RDONLY;
    int fd = -1;
    bool ok = false;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    if (!parent_directory(path, directory, sizeof(directory))) return false;
    fd = open(directory, flags);
    if (fd < 0) return false;
    ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok;
}

static bool injection_matches(const char* stage, uint64_t generation) {
    const char* requested_stage =
        getenv("RAY_TRACING_TEST_CHECKPOINT_EXIT_STAGE");
    const char* requested_generation =
        getenv("RAY_TRACING_TEST_CHECKPOINT_EXIT_GENERATION");
    char* end = NULL;
    unsigned long long parsed = 0u;
    if (!requested_stage || strcmp(requested_stage, stage) != 0) return false;
    if (!requested_generation || !requested_generation[0]) return true;
    parsed = strtoull(requested_generation, &end, 10);
    return end && *end == '\0' && (uint64_t)parsed == generation;
}

static void inject_if_requested(const char* stage, uint64_t generation) {
    if (injection_matches(stage, generation)) _exit(87);
}

bool ray_tracing_checkpoint_transaction_begin(
    RayTracingCheckpointTransaction* transaction,
    const char* target_path,
    uint64_t generation) {
    int fd = -1;
    struct stat status;
    if (!transaction || !target_path || !target_path[0] || generation == 0u) {
        return false;
    }
    memset(transaction, 0, sizeof(*transaction));
    if (snprintf(transaction->targetPath,
                 sizeof(transaction->targetPath),
                 "%s",
                 target_path) >= (int)sizeof(transaction->targetPath) ||
        snprintf(transaction->temporaryPath,
                 sizeof(transaction->temporaryPath),
                 "%s.tmp.%ld",
                 target_path,
                 (long)getpid()) >= (int)sizeof(transaction->temporaryPath) ||
        !ensure_parent(target_path)) {
        return false;
    }
    if (lstat(transaction->temporaryPath, &status) == 0) {
        if (!S_ISREG(status.st_mode) || unlink(transaction->temporaryPath) != 0) {
            return false;
        }
    } else if (errno != ENOENT) {
        return false;
    }
    transaction->generation = generation;
    inject_if_requested("before_write", generation);
    fd = open(transaction->temporaryPath, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return false;
    transaction->stream = fdopen(fd, "wb");
    if (!transaction->stream) {
        close(fd);
        (void)unlink(transaction->temporaryPath);
        memset(transaction, 0, sizeof(*transaction));
        return false;
    }
    return true;
}

void ray_tracing_checkpoint_transaction_reached_temporary_write(
    RayTracingCheckpointTransaction* transaction) {
    if (transaction) {
        inject_if_requested("during_temporary_write", transaction->generation);
    }
}

bool ray_tracing_checkpoint_transaction_commit(
    RayTracingCheckpointTransaction* transaction) {
    int fd = -1;
    bool ok = true;
    if (!transaction || !transaction->stream || !transaction->targetPath[0] ||
        !transaction->temporaryPath[0]) {
        return false;
    }
    fd = fileno(transaction->stream);
    if (ferror(transaction->stream) != 0 ||
        fflush(transaction->stream) != 0 || !sync_fd(fd)) {
        ok = false;
    }
    if (fclose(transaction->stream) != 0) ok = false;
    transaction->stream = NULL;
    if (ok) inject_if_requested("after_file_sync", transaction->generation);
    if (ok && rename(transaction->temporaryPath, transaction->targetPath) != 0) {
        ok = false;
    }
    if (ok) inject_if_requested("after_rename", transaction->generation);
    if (ok) inject_if_requested("before_directory_sync", transaction->generation);
    if (ok && !sync_parent(transaction->targetPath)) ok = false;
    if (!ok) (void)unlink(transaction->temporaryPath);
    memset(transaction, 0, sizeof(*transaction));
    return ok;
}

void ray_tracing_checkpoint_transaction_abort(
    RayTracingCheckpointTransaction* transaction) {
    if (!transaction) return;
    if (transaction->stream) fclose(transaction->stream);
    if (transaction->temporaryPath[0]) {
        (void)unlink(transaction->temporaryPath);
    }
    memset(transaction, 0, sizeof(*transaction));
}
