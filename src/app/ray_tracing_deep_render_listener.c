#include "app/ray_tracing_deep_render_listener.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEEP_RENDER_PIXEL_STRIDE 4u

typedef enum DeepRenderProgressApplyStatus {
    DEEP_RENDER_PROGRESS_NONE = 0,
    DEEP_RENDER_PROGRESS_APPLIED,
    DEEP_RENDER_PROGRESS_STALE,
    DEEP_RENDER_PROGRESS_ALLOCATION_FAILED,
    DEEP_RENDER_PROGRESS_INVALID,
} DeepRenderProgressApplyStatus;

static bool deep_render_listener_active_session(
    const RayTracingDeepRenderSession* session,
    uint64_t* out_generation) {
    const RayTracingDeepRenderFrameRequest* request = NULL;
    if (!session || !out_generation) return false;
    if (session->state != RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_SAVING &&
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        return false;
    }
    request = RayTracingDeepRenderSession_CurrentRequest(session);
    if (!request || session->generation == 0u ||
        request->generation != session->generation) {
        return false;
    }
    *out_generation = session->generation;
    return true;
}

static bool deep_render_listener_ensure_bytes(uint8_t** buffer,
                                              size_t* capacity,
                                              size_t required) {
    uint8_t* resized = NULL;
    if (!buffer || !capacity || required == 0u) return false;
    if (*buffer && *capacity >= required) return true;
    resized = (uint8_t*)realloc(*buffer, required);
    if (!resized) return false;
    *buffer = resized;
    *capacity = required;
    return true;
}

static bool deep_render_listener_checked_bytes(int width,
                                               int height,
                                               size_t* out_bytes) {
    size_t pixels = 0u;
    if (!out_bytes || width <= 0 || height <= 0) return false;
    if ((size_t)width > SIZE_MAX / (size_t)height) return false;
    pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / DEEP_RENDER_PIXEL_STRIDE) return false;
    *out_bytes = pixels * DEEP_RENDER_PIXEL_STRIDE;
    return true;
}

static bool deep_render_listener_progress_valid(
    const RuntimeNative3DAsyncRenderProgressSnapshot* progress,
    size_t* out_display_bytes,
    size_t* out_rect_bytes) {
    size_t display_bytes = 0u;
    size_t rect_bytes = 0u;
    if (!progress || !progress->valid || progress->generation == 0u ||
        progress->rect.x < 0 || progress->rect.y < 0 ||
        progress->rect.width <= 0 || progress->rect.height <= 0 ||
        progress->rect.x > progress->hostWidth ||
        progress->rect.y > progress->hostHeight ||
        progress->rect.width > progress->hostWidth - progress->rect.x ||
        progress->rect.height > progress->hostHeight - progress->rect.y ||
        !deep_render_listener_checked_bytes(progress->hostWidth,
                                            progress->hostHeight,
                                            &display_bytes) ||
        !deep_render_listener_checked_bytes(progress->rect.width,
                                            progress->rect.height,
                                            &rect_bytes) ||
        rect_bytes != progress->byteCount) {
        return false;
    }
    if (out_display_bytes) *out_display_bytes = display_bytes;
    if (out_rect_bytes) *out_rect_bytes = rect_bytes;
    return true;
}

static void deep_render_listener_bind_generation(
    RayTracingDeepRenderListener* listener,
    uint64_t generation) {
    if (!listener || listener->generation == generation) return;
    listener->generation = generation;
    listener->lastSequence = 0u;
    listener->hostWidth = 0;
    listener->hostHeight = 0;
    memset(&listener->dirtyRect, 0, sizeof(listener->dirtyRect));
    listener->displayValid = false;
}

static void deep_render_listener_clear_display(uint8_t* pixels,
                                               size_t byte_count) {
    if (!pixels || byte_count == 0u) return;
    memset(pixels, 0, byte_count);
    for (size_t offset = 3u; offset < byte_count; offset += DEEP_RENDER_PIXEL_STRIDE) {
        pixels[offset] = 255u;
    }
}

static DeepRenderProgressApplyStatus deep_render_listener_apply_progress(
    RayTracingDeepRenderListener* listener,
    RuntimeNative3DAsyncRenderProgressBuffer* progress_buffer,
    RayTracingDeepRenderListenerPollResult* result) {
    RuntimeNative3DAsyncRenderProgressSnapshot progress = {0};
    size_t required = 0u;
    size_t display_bytes = 0u;
    size_t rect_bytes = 0u;
    size_t row_bytes = 0u;
    bool copied = false;

    if (!listener || !progress_buffer || !result) return DEEP_RENDER_PROGRESS_NONE;
    copied = RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(
        progress_buffer,
        listener->generation,
        &progress,
        listener->progressScratch,
        listener->progressScratchCapacity,
        &required);
    result->progress = progress;
    if (!progress.valid) return DEEP_RENDER_PROGRESS_NONE;
    if (progress.staleGeneration) return DEEP_RENDER_PROGRESS_STALE;
    if (!deep_render_listener_progress_valid(&progress, &display_bytes, &rect_bytes)) {
        return DEEP_RENDER_PROGRESS_INVALID;
    }
    if (listener->displayValid && progress.sequence == listener->lastSequence) {
        return DEEP_RENDER_PROGRESS_NONE;
    }
    if (!copied) {
        if (required == 0u ||
            !deep_render_listener_ensure_bytes(&listener->progressScratch,
                                               &listener->progressScratchCapacity,
                                               required)) {
            return DEEP_RENDER_PROGRESS_ALLOCATION_FAILED;
        }
        copied = RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(
            progress_buffer,
            listener->generation,
            &progress,
            listener->progressScratch,
            listener->progressScratchCapacity,
            &required);
        result->progress = progress;
        if (!copied) {
            if (progress.valid && progress.staleGeneration) {
                return DEEP_RENDER_PROGRESS_STALE;
            }
            return DEEP_RENDER_PROGRESS_NONE;
        }
        if (!deep_render_listener_progress_valid(&progress,
                                                 &display_bytes,
                                                 &rect_bytes)) {
            return DEEP_RENDER_PROGRESS_INVALID;
        }
    }
    if (!deep_render_listener_ensure_bytes(&listener->displayPixels,
                                           &listener->displayCapacity,
                                           display_bytes)) {
        return DEEP_RENDER_PROGRESS_ALLOCATION_FAILED;
    }
    if (!listener->displayValid || listener->hostWidth != progress.hostWidth ||
        listener->hostHeight != progress.hostHeight) {
        deep_render_listener_clear_display(listener->displayPixels, display_bytes);
    }
    row_bytes = (size_t)progress.rect.width * DEEP_RENDER_PIXEL_STRIDE;
    for (int y = 0; y < progress.rect.height; ++y) {
        const size_t dst_offset =
            (((size_t)(progress.rect.y + y) * (size_t)progress.hostWidth) +
             (size_t)progress.rect.x) * DEEP_RENDER_PIXEL_STRIDE;
        const size_t src_offset = (size_t)y * row_bytes;
        memcpy(listener->displayPixels + dst_offset,
               listener->progressScratch + src_offset,
               row_bytes);
    }
    listener->lastSequence = progress.sequence;
    listener->hostWidth = progress.hostWidth;
    listener->hostHeight = progress.hostHeight;
    listener->dirtyRect = progress.rect;
    listener->displayValid = true;
    result->progress = progress;
    result->progressApplied = true;
    return DEEP_RENDER_PROGRESS_APPLIED;
}

void RayTracingDeepRenderListener_Init(RayTracingDeepRenderListener* listener) {
    if (!listener) return;
    memset(listener, 0, sizeof(*listener));
}

void RayTracingDeepRenderListener_Destroy(RayTracingDeepRenderListener* listener) {
    if (!listener) return;
    free(listener->displayPixels);
    free(listener->progressScratch);
    RayTracingDeepRenderListener_Init(listener);
}

RayTracingDeepRenderPresentationView RayTracingDeepRenderListener_GetView(
    const RayTracingDeepRenderListener* listener) {
    RayTracingDeepRenderPresentationView view = {0};
    size_t byte_count = 0u;
    if (!listener || !listener->displayValid || !listener->displayPixels ||
        !deep_render_listener_checked_bytes(listener->hostWidth,
                                            listener->hostHeight,
                                            &byte_count)) {
        return view;
    }
    view.valid = true;
    view.generation = listener->generation;
    view.sequence = listener->lastSequence;
    view.hostWidth = listener->hostWidth;
    view.hostHeight = listener->hostHeight;
    view.dirtyRect = listener->dirtyRect;
    view.pixels = listener->displayPixels;
    view.byteCount = byte_count;
    return view;
}

bool RayTracingDeepRenderListener_Poll(
    RayTracingDeepRenderListener* listener,
    const RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    RuntimeNative3DAsyncRenderJob* job,
    RayTracingDeepRenderPresentFn present_fn,
    void* present_user_data,
    RayTracingDeepRenderListenerPollResult* out_result) {
    RayTracingDeepRenderListenerPollResult result = {0};
    DeepRenderProgressApplyStatus progress_status;
    RayTracingDeepRenderPresentationView view;
    uint64_t generation = 0u;

    result.status = RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_ARGUMENT;
    result.jobStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE;
    result.publishStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_NOT_READY;
    if (out_result) *out_result = result;
    if (!listener || !session || !out_result) return false;
    if (!deep_render_listener_active_session(session, &generation)) {
        result.status = RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_SESSION;
        *out_result = result;
        return false;
    }
    result.status = RAY_TRACING_DEEP_RENDER_LISTENER_OK;
    result.generation = generation;
    deep_render_listener_bind_generation(listener, generation);

    progress_status = deep_render_listener_apply_progress(listener, progress, &result);
    if (progress_status == DEEP_RENDER_PROGRESS_STALE) {
        result.staleProgressRejected = true;
    } else if (progress_status == DEEP_RENDER_PROGRESS_ALLOCATION_FAILED) {
        result.status = RAY_TRACING_DEEP_RENDER_LISTENER_ALLOCATION_FAILED;
        *out_result = result;
        return false;
    } else if (progress_status == DEEP_RENDER_PROGRESS_INVALID) {
        result.status = RAY_TRACING_DEEP_RENDER_LISTENER_PROGRESS_INVALID;
        *out_result = result;
        return false;
    }

    view = RayTracingDeepRenderListener_GetView(listener);
    if (view.valid && present_fn) {
        if (!present_fn(&view, present_user_data)) {
            result.status = RAY_TRACING_DEEP_RENDER_LISTENER_PRESENT_FAILED;
            *out_result = result;
            return false;
        }
        result.presented = true;
    }

    if (job) {
        result.jobStatus = RuntimeNative3DAsyncRenderJob_GetStatus(job);
        if (result.jobStatus != RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE &&
            result.jobStatus != RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING) {
            result.publishStatus = RuntimeNative3DAsyncRenderJob_TryPublish(
                job, generation, &result.jobResult);
            result.terminalObserved =
                result.publishStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED ||
                result.publishStatus ==
                    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION ||
                result.publishStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED ||
                result.publishStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED;
        }
    }
    *out_result = result;
    return true;
}

const char* RayTracingDeepRenderListenerStatus_Name(
    RayTracingDeepRenderListenerStatus status) {
    switch (status) {
        case RAY_TRACING_DEEP_RENDER_LISTENER_OK: return "ok";
        case RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_ARGUMENT:
            return "invalid_argument";
        case RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_SESSION:
            return "invalid_session";
        case RAY_TRACING_DEEP_RENDER_LISTENER_ALLOCATION_FAILED:
            return "allocation_failed";
        case RAY_TRACING_DEEP_RENDER_LISTENER_PROGRESS_INVALID:
            return "progress_invalid";
        case RAY_TRACING_DEEP_RENDER_LISTENER_PRESENT_FAILED:
            return "present_failed";
    }
    return "unknown";
}
