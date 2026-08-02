#include "render/runtime_native_3d_async_render_bridge.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_ray_3d.h"

struct RuntimeNative3DAsyncRenderProgressBuffer {
    pthread_mutex_t mutex;
    bool mutexReady;
    bool valid;
    bool pending;
    uint64_t generation;
    uint64_t sequence;
    int hostWidth;
    int hostHeight;
    RuntimeNative3DAsyncRenderProgressRect rect;
    uint8_t* pixels;
    size_t byteCount;
    size_t capacity;
    bool tileProgressValid;
    uint64_t tileProgressGeneration;
    uint64_t tileProgressSequence;
    int startedSubpasses;
    int completedSubpasses;
    int totalSubpasses;
    size_t completedTilesInSubpass;
    size_t totalTilesInSubpass;
};

static RuntimeNative3DAsyncRenderAssessment runtime_native_3d_async_assessment(
    RuntimeNative3DAsyncRenderReadiness readiness,
    const char* reason) {
    RuntimeNative3DAsyncRenderAssessment assessment;
    memset(&assessment, 0, sizeof(assessment));
    assessment.readiness = readiness;
    assessment.reason = reason;
    assessment.ready =
        readiness == RUNTIME_NATIVE_3D_ASYNC_RENDER_READY_EXCLUSIVE_SINGLE_JOB;
    assessment.requiresExclusiveRenderContext = assessment.ready;
    return assessment;
}

const char* RuntimeNative3DAsyncRenderReadiness_Name(
    RuntimeNative3DAsyncRenderReadiness readiness) {
    switch (readiness) {
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_READY_EXCLUSIVE_SINGLE_JOB:
            return "ready_exclusive_single_job";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_INVALID_SNAPSHOT:
            return "blocked_invalid_snapshot";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_GENERATION_UNBOUND:
            return "blocked_generation_unbound";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_CANCEL_UNBOUND:
            return "blocked_cancel_unbound";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_PREPARED_FRAME_UNBOUND:
            return "blocked_prepared_frame_unbound";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_ACCELERATION_UNBOUND:
            return "blocked_acceleration_unbound";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_NON_TLAS_ROUTE:
            return "blocked_non_tlas_route";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_VOLUME:
            return "blocked_dynamic_volume";
        case RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_WATER:
            return "blocked_dynamic_water";
    }
    return "unknown";
}

RuntimeNative3DAsyncRenderAssessment RuntimeNative3DAsyncRender_AssessSnapshot(
    const RuntimeNative3DRenderRequestSnapshot* snapshot) {
    if (!snapshot || !snapshot->valid) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_INVALID_SNAPSHOT,
            "snapshot is missing or invalid");
    }
    if (!snapshot->generationBound || snapshot->generation == 0u) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_GENERATION_UNBOUND,
            "snapshot has no nonzero generation binding");
    }
    if (!snapshot->cancelTokenBound ||
        !snapshot->cancelToken.cancelRequested ||
        snapshot->cancelToken.generation != snapshot->generation ||
        snapshot->cancelGeneration != snapshot->generation) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_CANCEL_UNBOUND,
            "snapshot has no generation-matched cancellation token");
    }
    if (!snapshot->preparedFrameBound || !snapshot->preparedFrameValid) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_PREPARED_FRAME_UNBOUND,
            "snapshot has no valid prepared-frame identity");
    }
    if (!snapshot->sceneAccelerationBound) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_ACCELERATION_UNBOUND,
            "snapshot has no acceleration binding");
    }
    if (snapshot->traceRoute != RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_NON_TLAS_ROUTE,
            "only pure TLAS/BLAS routes are eligible for the first async bridge");
    }
    if (snapshot->volumeFrameSelectionDynamic) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_VOLUME,
            "dynamic volume frame selection still depends on frame-local state");
    }
    if (snapshot->waterSurfaceFrameSelectionDynamic) {
        return runtime_native_3d_async_assessment(
            RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_WATER,
            "dynamic water frame selection still depends on frame-local state");
    }
    return runtime_native_3d_async_assessment(
        RUNTIME_NATIVE_3D_ASYNC_RENDER_READY_EXCLUSIVE_SINGLE_JOB,
        "snapshot can use the async bridge under one exclusive render job");
}

RuntimeNative3DAsyncRenderProgressBuffer*
RuntimeNative3DAsyncRenderProgressBuffer_Create(void) {
    RuntimeNative3DAsyncRenderProgressBuffer* progress =
        (RuntimeNative3DAsyncRenderProgressBuffer*)calloc(1u, sizeof(*progress));
    if (!progress) return NULL;
    if (pthread_mutex_init(&progress->mutex, NULL) != 0) {
        free(progress);
        return NULL;
    }
    progress->mutexReady = true;
    return progress;
}

void RuntimeNative3DAsyncRenderProgressBuffer_Destroy(
    RuntimeNative3DAsyncRenderProgressBuffer* progress) {
    if (!progress) return;
    if (progress->mutexReady) {
        pthread_mutex_destroy(&progress->mutex);
    }
    free(progress->pixels);
    free(progress);
}

void RuntimeNative3DAsyncRenderProgressBuffer_Reset(
    RuntimeNative3DAsyncRenderProgressBuffer* progress) {
    if (!progress) return;
    pthread_mutex_lock(&progress->mutex);
    progress->valid = false;
    progress->pending = false;
    progress->generation = 0u;
    progress->sequence = 0u;
    progress->hostWidth = 0;
    progress->hostHeight = 0;
    memset(&progress->rect, 0, sizeof(progress->rect));
    progress->byteCount = 0u;
    progress->tileProgressValid = false;
    progress->tileProgressGeneration = 0u;
    progress->tileProgressSequence = 0u;
    progress->startedSubpasses = 0;
    progress->completedSubpasses = 0;
    progress->totalSubpasses = 0;
    progress->completedTilesInSubpass = 0u;
    progress->totalTilesInSubpass = 0u;
    pthread_mutex_unlock(&progress->mutex);
}

static bool runtime_native_3d_async_progress_rect_valid(
    int host_width,
    int host_height,
    RuntimeNative3DAsyncRenderProgressRect rect) {
    if (host_width <= 0 || host_height <= 0 || rect.width <= 0 || rect.height <= 0) {
        return false;
    }
    if (rect.x < 0 || rect.y < 0) return false;
    if (rect.x > host_width || rect.y > host_height) return false;
    if (rect.width > host_width - rect.x) return false;
    if (rect.height > host_height - rect.y) return false;
    return true;
}

bool RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t generation,
    const uint8_t* host_buffer,
    int host_width,
    int host_height,
    RuntimeNative3DAsyncRenderProgressRect rect) {
    size_t stride;
    size_t full_required;
    size_t row_bytes;
    uint8_t* pixels = NULL;
    RuntimeNative3DAsyncRenderProgressRect pending;
    bool identity_changed = false;

    if (!progress || generation == 0u || !host_buffer ||
        !runtime_native_3d_async_progress_rect_valid(host_width, host_height, rect)) {
        return false;
    }

    stride = (size_t)host_width * 4u;
    full_required = stride * (size_t)host_height;
    row_bytes = (size_t)rect.width * 4u;

    pthread_mutex_lock(&progress->mutex);
    identity_changed = progress->generation != generation ||
                       progress->hostWidth != host_width ||
                       progress->hostHeight != host_height;
    if (full_required > progress->capacity) {
        pixels = (uint8_t*)realloc(progress->pixels, full_required);
        if (!pixels) {
            pthread_mutex_unlock(&progress->mutex);
            return false;
        }
        progress->pixels = pixels;
        progress->capacity = full_required;
    }
    if (identity_changed) {
        memset(progress->pixels, 0, full_required);
        for (size_t offset = 3u; offset < full_required; offset += 4u) {
            progress->pixels[offset] = 255u;
        }
        progress->valid = false;
        progress->pending = false;
        progress->generation = generation;
        progress->hostWidth = host_width;
        progress->hostHeight = host_height;
        memset(&progress->rect, 0, sizeof(progress->rect));
        progress->byteCount = 0u;
    }
    for (int y = 0; y < rect.height; ++y) {
        const size_t src_offset =
            ((size_t)(rect.y + y) * stride) + ((size_t)rect.x * 4u);
        const size_t dst_offset = src_offset;
        memcpy(progress->pixels + dst_offset, host_buffer + src_offset, row_bytes);
    }
    pending = rect;
    if (progress->pending) {
        const int min_x = progress->rect.x < rect.x ? progress->rect.x : rect.x;
        const int min_y = progress->rect.y < rect.y ? progress->rect.y : rect.y;
        const int max_x =
            progress->rect.x + progress->rect.width > rect.x + rect.width
                ? progress->rect.x + progress->rect.width
                : rect.x + rect.width;
        const int max_y =
            progress->rect.y + progress->rect.height > rect.y + rect.height
                ? progress->rect.y + progress->rect.height
                : rect.y + rect.height;
        pending = (RuntimeNative3DAsyncRenderProgressRect){
            .x = min_x,
            .y = min_y,
            .width = max_x - min_x,
            .height = max_y - min_y,
        };
    }
    progress->valid = true;
    progress->pending = true;
    progress->generation = generation;
    progress->sequence += 1u;
    progress->hostWidth = host_width;
    progress->hostHeight = host_height;
    progress->rect = pending;
    progress->byteCount =
        (size_t)pending.width * 4u * (size_t)pending.height;
    pthread_mutex_unlock(&progress->mutex);
    return true;
}

bool RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t current_generation,
    RuntimeNative3DAsyncRenderProgressSnapshot* out_snapshot,
    uint8_t* out_pixels,
    size_t out_pixel_capacity,
    size_t* out_required_bytes) {
    RuntimeNative3DAsyncRenderProgressSnapshot snapshot;
    bool copied = false;

    if (out_snapshot) memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (out_required_bytes) *out_required_bytes = 0u;
    if (!progress) return false;

    pthread_mutex_lock(&progress->mutex);
    memset(&snapshot, 0, sizeof(snapshot));
    if (!progress->valid) {
        pthread_mutex_unlock(&progress->mutex);
        return false;
    }
    snapshot.valid = true;
    snapshot.generation = progress->generation;
    snapshot.sequence = progress->sequence;
    snapshot.hostWidth = progress->hostWidth;
    snapshot.hostHeight = progress->hostHeight;
    snapshot.rect = progress->rect;
    snapshot.byteCount = progress->byteCount;
    if (current_generation != progress->generation) {
        snapshot.staleGeneration = true;
    } else if (!progress->pending) {
        pthread_mutex_unlock(&progress->mutex);
        return false;
    } else if (out_pixels && out_pixel_capacity >= progress->byteCount) {
        const size_t source_stride = (size_t)progress->hostWidth * 4u;
        const size_t row_bytes = (size_t)progress->rect.width * 4u;
        for (int y = 0; y < progress->rect.height; ++y) {
            const size_t source_offset =
                ((size_t)(progress->rect.y + y) * source_stride) +
                ((size_t)progress->rect.x * 4u);
            memcpy(out_pixels + ((size_t)y * row_bytes),
                   progress->pixels + source_offset,
                   row_bytes);
        }
        copied = true;
        progress->pending = false;
    }
    if (out_required_bytes) {
        *out_required_bytes = progress->byteCount;
    }
    if (out_snapshot) {
        *out_snapshot = snapshot;
    }
    pthread_mutex_unlock(&progress->mutex);
    return copied;
}

bool RuntimeNative3DAsyncRenderProgressBuffer_PublishTileProgress(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t generation,
    const RuntimeNative3DTileSchedulerProgress* tile_progress) {
    if (!progress || generation == 0u || !tile_progress) return false;
    pthread_mutex_lock(&progress->mutex);
    progress->tileProgressValid = true;
    progress->tileProgressGeneration = generation;
    progress->tileProgressSequence += 1u;
    progress->startedSubpasses = tile_progress->startedSubpasses;
    progress->completedSubpasses = tile_progress->completedSubpasses;
    progress->totalSubpasses = tile_progress->totalSubpasses;
    progress->completedTilesInSubpass = tile_progress->completedTilesInSubpass;
    progress->totalTilesInSubpass = tile_progress->totalTilesInSubpass;
    pthread_mutex_unlock(&progress->mutex);
    return true;
}

bool RuntimeNative3DAsyncRenderProgressBuffer_CopyLatestTileProgress(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t current_generation,
    RuntimeNative3DAsyncRenderTileProgressSnapshot* out_snapshot) {
    RuntimeNative3DAsyncRenderTileProgressSnapshot snapshot = {0};
    if (out_snapshot) memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (!progress || !out_snapshot) return false;
    pthread_mutex_lock(&progress->mutex);
    if (progress->tileProgressValid) {
        snapshot.valid = true;
        snapshot.generation = progress->tileProgressGeneration;
        snapshot.sequence = progress->tileProgressSequence;
        snapshot.startedSubpasses = progress->startedSubpasses;
        snapshot.completedSubpasses = progress->completedSubpasses;
        snapshot.totalSubpasses = progress->totalSubpasses;
        snapshot.completedTilesInSubpass = progress->completedTilesInSubpass;
        snapshot.totalTilesInSubpass = progress->totalTilesInSubpass;
        snapshot.staleGeneration = current_generation != snapshot.generation;
    }
    pthread_mutex_unlock(&progress->mutex);
    *out_snapshot = snapshot;
    return snapshot.valid && !snapshot.staleGeneration;
}
