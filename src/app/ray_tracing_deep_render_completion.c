#include "app/ray_tracing_deep_render_completion.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "render/pipeline/ray_tracing2_native3d_overlay.h"

static void deep_render_completion_set_result(
    RayTracingDeepRenderCompletionResult* result,
    RayTracingDeepRenderCompletionStatus status) {
    if (result) result->status = status;
}

static bool deep_render_completion_has_parent_ref(const char* path) {
    const char* segment = path;
    if (!path || !path[0]) return true;
    while (*segment) {
        const char* end = strchr(segment, '/');
        size_t length = end ? (size_t)(end - segment) : strlen(segment);
        if (length == 2u && segment[0] == '.' && segment[1] == '.') return true;
        if (!end) break;
        segment = end + 1;
    }
    return false;
}

static bool deep_render_completion_descendant_path(const char* parent,
                                                   const char* child,
                                                   bool allow_equal) {
    size_t parent_length = 0u;
    if (!parent || !parent[0] || !child || !child[0] ||
        deep_render_completion_has_parent_ref(parent) ||
        deep_render_completion_has_parent_ref(child)) {
        return false;
    }
    parent_length = strlen(parent);
    while (parent_length > 1u && parent[parent_length - 1u] == '/') {
        parent_length -= 1u;
    }
    if (strncmp(parent, child, parent_length) != 0) return false;
    if (child[parent_length] == '\0') return allow_equal;
    return child[parent_length] == '/';
}

static bool deep_render_completion_output_identity_valid(
    const RayTracingDeepRenderFrameRequest* request) {
    if (!request) return false;
    return deep_render_completion_descendant_path(request->outputRoot,
                                                  request->frameDirectory,
                                                  true) &&
           deep_render_completion_descendant_path(request->frameDirectory,
                                                  request->finalFramePath,
                                                  false);
}

static bool deep_render_completion_view_complete(
    const RayTracingDeepRenderFrameRequest* request,
    const RayTracingDeepRenderListenerPollResult* poll_result,
    const RayTracingDeepRenderPresentationView* view) {
    const RuntimeNative3DRenderRequestSnapshot* snapshot = NULL;
    if (!request || !poll_result || !view || !view->valid || !view->pixels ||
        view->generation != request->generation || view->sequence == 0u ||
        poll_result->generation != request->generation ||
        poll_result->jobResult.generation != request->generation) {
        return false;
    }
    snapshot = &request->renderSnapshot;
    if (view->hostWidth != snapshot->hostWidth ||
        view->hostHeight != snapshot->hostHeight ||
        view->hostWidth != poll_result->jobResult.snapshot.hostWidth ||
        view->hostHeight != poll_result->jobResult.snapshot.hostHeight) {
        return false;
    }
    if (view->dirtyRect.x != 0 || view->dirtyRect.y != 0 ||
        view->dirtyRect.width != view->hostWidth ||
        view->dirtyRect.height != view->hostHeight) {
        return false;
    }
    if ((size_t)view->hostWidth > SIZE_MAX / (size_t)view->hostHeight) return false;
    const size_t pixels = (size_t)view->hostWidth * (size_t)view->hostHeight;
    return pixels <= SIZE_MAX / 4u && view->byteCount == pixels * 4u;
}

static bool deep_render_completion_ensure_directory(const char* path) {
    char buffer[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    size_t length = 0u;
    if (!path || !path[0]) return false;
    length = strlen(path);
    if (length >= sizeof(buffer)) return false;
    memcpy(buffer, path, length + 1u);
    for (size_t i = 1u; i < length; ++i) {
        if (buffer[i] != '/') continue;
        buffer[i] = '\0';
        if (buffer[0] && mkdir(buffer, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        buffer[i] = '/';
    }
    return mkdir(buffer, 0700) == 0 || errno == EEXIST;
}

static bool deep_render_completion_prepare_output_path(const char* path) {
    char directory[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    char* separator = NULL;
    struct stat st;
    size_t length = 0u;
    if (!path || !path[0] || deep_render_completion_has_parent_ref(path)) return false;
    length = strlen(path);
    if (length >= sizeof(directory)) return false;
    memcpy(directory, path, length + 1u);
    separator = strrchr(directory, '/');
    if (!separator || separator == directory || !separator[1]) return false;
    *separator = '\0';
    if (!deep_render_completion_ensure_directory(directory)) return false;
    if (lstat(path, &st) != 0) return errno == ENOENT;
    return S_ISREG(st.st_mode);
}

bool RayTracingDeepRenderCompletion_WriteFrameBMP(
    const char* path,
    const RayTracingDeepRenderPresentationView* view,
    void* user_data) {
    char temporary_path[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    struct stat st;
    int written = 0;
    bool exported = false;
    (void)user_data;
    if (!path || !view || !view->valid || !view->pixels ||
        !deep_render_completion_prepare_output_path(path)) {
        return false;
    }
    written = snprintf(temporary_path,
                       sizeof(temporary_path),
                       "%s.tmp.%ld.%llu",
                       path,
                       (long)getpid(),
                       (unsigned long long)view->generation);
    if (written <= 0 || written >= (int)sizeof(temporary_path)) return false;
    if (lstat(temporary_path, &st) == 0) {
        if (!S_ISREG(st.st_mode) || unlink(temporary_path) != 0) return false;
    } else if (errno != ENOENT) {
        return false;
    }
    exported = RayTracing2Native3DOverlay_ExportFrameBMP(temporary_path,
                                                         view->hostWidth,
                                                         view->hostHeight,
                                                         view->pixels,
                                                         NULL);
    if (!exported) {
        (void)unlink(temporary_path);
        return false;
    }
    if (rename(temporary_path, path) != 0) {
        (void)unlink(temporary_path);
        return false;
    }
    return true;
}

bool RayTracingDeepRenderCompletion_VerifyFrameBMP(
    const char* path,
    int expected_width,
    int expected_height,
    void* user_data) {
    SDL_Surface* surface = NULL;
    struct stat st;
    bool valid = false;
    (void)user_data;
    if (!path || !path[0] || expected_width <= 0 || expected_height <= 0 ||
        lstat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        return false;
    }
    surface = SDL_LoadBMP(path);
    if (!surface) return false;
    valid = surface->w == expected_width && surface->h == expected_height;
    SDL_FreeSurface(surface);
    return valid;
}

bool RayTracingDeepRenderCompletion_Process(
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderListenerPollResult* poll_result,
    const RayTracingDeepRenderPresentationView* final_view,
    const RayTracingDeepRenderCompletionDesc* desc,
    RayTracingDeepRenderCompletionResult* out_result) {
    RayTracingDeepRenderCompletionResult result = {0};
    const RayTracingDeepRenderFrameRequest* request = NULL;
    RayTracingDeepRenderFrameWriteFn write_fn =
        desc && desc->write_fn ? desc->write_fn
                              : RayTracingDeepRenderCompletion_WriteFrameBMP;
    RayTracingDeepRenderFrameVerifyFn verify_fn =
        desc && desc->verify_fn ? desc->verify_fn
                               : RayTracingDeepRenderCompletion_VerifyFrameBMP;
    void* user_data = desc ? desc->user_data : NULL;
    RuntimeNative3DAsyncRenderJobStatus current_job_status;

    result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_ARGUMENT;
    if (out_result) *out_result = result;
    if (!session || !poll_result || !out_result) return false;
    request = RayTracingDeepRenderSession_CurrentRequest(session);
    if (!request || session->state != RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
        session->generation == 0u || request->generation != session->generation) {
        deep_render_completion_set_result(
            &result, RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_SESSION);
        *out_result = result;
        return false;
    }
    result.generation = session->generation;
    result.absoluteFrameIndex = request->absoluteFrameIndex;
    if (!poll_result->terminalObserved) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_NO_TERMINAL;
        *out_result = result;
        return true;
    }
    result.terminalConsumed = true;
    if (poll_result->publishStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED ||
        poll_result->jobResult.canceled || poll_result->jobResult.cancelRequested) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_CANCELED_TERMINAL;
        *out_result = result;
        return false;
    }
    if (!job) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_ARGUMENT;
        *out_result = result;
        return false;
    }
    current_job_status = RuntimeNative3DAsyncRenderJob_GetStatus(job);
    if (current_job_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE ||
        current_job_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_JOB_NOT_TERMINAL;
        *out_result = result;
        return false;
    }
    if (!RuntimeNative3DAsyncRenderJob_Join(job)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_JOIN_FAILED;
        *out_result = result;
        return false;
    }
    result.jobJoined = true;
    if (poll_result->publishStatus ==
            RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION ||
        poll_result->generation != session->generation ||
        poll_result->jobResult.generation != session->generation) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_STALE_TERMINAL;
        *out_result = result;
        return false;
    }
    if (poll_result->publishStatus != RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED ||
        !poll_result->jobResult.valid || !poll_result->jobResult.published ||
        !poll_result->jobResult.succeeded) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_RENDER_FAILED;
        *out_result = result;
        return false;
    }
    if (!deep_render_completion_view_complete(request, poll_result, final_view)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_FINAL_IMAGE_INCOMPLETE;
        *out_result = result;
        return false;
    }
    if (!deep_render_completion_output_identity_valid(request)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_OUTPUT_IDENTITY_INVALID;
        *out_result = result;
        return false;
    }
    if (!RayTracingDeepRenderSession_MarkRenderSucceeded(session)) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_SEQUENCE_FAILED;
        *out_result = result;
        return false;
    }
    result.writeAttempted = true;
    if (!write_fn(request->finalFramePath, final_view, user_data)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_SAVE_FAILED;
        *out_result = result;
        return false;
    }
    result.outputWritten = true;
    if (!verify_fn(request->finalFramePath,
                   final_view->hostWidth,
                   final_view->hostHeight,
                   user_data)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE);
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_VERIFY_FAILED;
        *out_result = result;
        return false;
    }
    result.outputVerified = true;
    if (!RayTracingDeepRenderSession_MarkFrameSaved(session)) {
        result.status = RAY_TRACING_DEEP_RENDER_COMPLETION_SEQUENCE_FAILED;
        *out_result = result;
        return false;
    }
    result.frameAdvanced = true;
    result.sessionCompleted =
        session->state == RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED;
    result.status = result.sessionCompleted
                        ? RAY_TRACING_DEEP_RENDER_COMPLETION_SESSION_COMPLETED
                        : RAY_TRACING_DEEP_RENDER_COMPLETION_FRAME_ADVANCED;
    *out_result = result;
    return true;
}

const char* RayTracingDeepRenderCompletionStatus_Name(
    RayTracingDeepRenderCompletionStatus status) {
    switch (status) {
        case RAY_TRACING_DEEP_RENDER_COMPLETION_NO_TERMINAL: return "no_terminal";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_FRAME_ADVANCED: return "frame_advanced";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_SESSION_COMPLETED:
            return "session_completed";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_ARGUMENT:
            return "invalid_argument";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_SESSION:
            return "invalid_session";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_STALE_TERMINAL:
            return "stale_terminal";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_CANCELED_TERMINAL:
            return "canceled_terminal";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_RENDER_FAILED:
            return "render_failed";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_JOB_NOT_TERMINAL:
            return "job_not_terminal";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_JOIN_FAILED: return "join_failed";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_FINAL_IMAGE_INCOMPLETE:
            return "final_image_incomplete";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_OUTPUT_IDENTITY_INVALID:
            return "output_identity_invalid";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_SAVE_FAILED: return "save_failed";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_VERIFY_FAILED: return "verify_failed";
        case RAY_TRACING_DEEP_RENDER_COMPLETION_SEQUENCE_FAILED:
            return "sequence_failed";
    }
    return "unknown";
}
