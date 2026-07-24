#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_temporal_checkpoint.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define RTCK_REFERENCE_MAGIC "RTCKPT1"

#pragma pack(push, 1)
typedef struct RayTracingCheckpointReferenceHeaderV1 {
    char magic[8];
    uint32_t schemaVersion;
    uint32_t headerSize;
    uint32_t tileHeaderSize;
    uint32_t adaptivePixelSize;
    uint32_t adaptiveSummarySize;
    int32_t frameIndex;
    int32_t width;
    int32_t height;
    int32_t tileSize;
    int32_t temporalFrames;
    int32_t integratorId;
    int32_t completedSubpasses;
    uint32_t tileCount;
    char digests[6][RAY_TRACING_SHA256_HEX_SIZE];
} RayTracingCheckpointReferenceHeaderV1;

typedef struct RayTracingCheckpointReferenceHeader {
    RayTracingCheckpointReferenceHeaderV1 base;
    uint64_t generation;
    int32_t activeSubpass;
    uint32_t completedTilesInSubpass;
    uint32_t totalTilesInSubpass;
    uint64_t previousWriteNanoseconds;
} RayTracingCheckpointReferenceHeader;
#pragma pack(pop)

static bool regular_file(const char* path) {
    struct stat status;
    return path && lstat(path, &status) == 0 && S_ISREG(status.st_mode);
}

bool ray_tracing_temporal_checkpoint_latest_reference(
    const char* checkpoint_root,
    int frame_index,
    char* out_path,
    size_t out_path_size,
    char out_sha256[RAY_TRACING_SHA256_HEX_SIZE],
    int* out_completed_subpasses,
    int* out_active_subpass,
    size_t* out_completed_tiles_in_subpass) {
    char directory_path[PATH_MAX];
    char best_path[PATH_MAX] = "";
    DIR* directory = NULL;
    struct dirent* entry = NULL;
    uint64_t best_generation = 0u;
    RayTracingCheckpointReferenceHeader header;
    FILE* file = NULL;
    if (!checkpoint_root || !out_path || out_path_size == 0u || !out_sha256 ||
        snprintf(directory_path,
                 sizeof(directory_path),
                 "%s/frame_%04d",
                 checkpoint_root,
                 frame_index) >= (int)sizeof(directory_path)) {
        return false;
    }
    directory = opendir(directory_path);
    if (!directory) return false;
    while ((entry = readdir(directory)) != NULL) {
        unsigned long long generation = 0u;
        int consumed = 0;
        char path[PATH_MAX];
        if (sscanf(entry->d_name,
                   "generation_%20llu.rtck%n",
                   &generation,
                   &consumed) != 1 ||
            entry->d_name[consumed] != '\0' ||
            (uint64_t)generation <= best_generation ||
            snprintf(path,
                     sizeof(path),
                     "%s/%s",
                     directory_path,
                     entry->d_name) >= (int)sizeof(path) ||
            !regular_file(path)) {
            continue;
        }
        file = fopen(path, "rb");
        if (!file) continue;
        if (fread(&header, 1u, sizeof(header), file) == sizeof(header) &&
            memcmp(header.base.magic,
                   RTCK_REFERENCE_MAGIC,
                   sizeof(header.base.magic)) == 0 &&
            header.base.schemaVersion ==
                RAY_TRACING_TEMPORAL_CHECKPOINT_SCHEMA_VERSION &&
            header.base.headerSize == sizeof(header) &&
            header.generation == (uint64_t)generation) {
            best_generation = (uint64_t)generation;
            snprintf(best_path, sizeof(best_path), "%s", path);
        }
        fclose(file);
        file = NULL;
    }
    closedir(directory);
    if (best_generation == 0u || !ray_tracing_sha256_file(best_path, out_sha256) ||
        snprintf(out_path, out_path_size, "%s", best_path) >=
            (int)out_path_size) {
        return false;
    }
    file = fopen(best_path, "rb");
    if (!file) return false;
    if (fread(&header, 1u, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }
    fclose(file);
    if (out_completed_subpasses) {
        *out_completed_subpasses = header.base.completedSubpasses;
    }
    if (out_active_subpass) *out_active_subpass = header.activeSubpass;
    if (out_completed_tiles_in_subpass) {
        *out_completed_tiles_in_subpass = header.completedTilesInSubpass;
    }
    return true;
}
