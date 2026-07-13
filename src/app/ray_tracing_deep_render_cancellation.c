#include "app/ray_tracing_deep_render_cancellation.h"

#include <string.h>

static void deep_render_cancellation_result_init(
    RayTracingDeepRenderCancellationResult* result) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT;
    result->jobStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE;
    result->publishStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_NOT_READY;
}

static bool deep_render_cancellation_terminal_status(
    RuntimeNative3DAsyncRenderJobStatus status) {
    return status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED ||
           status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED ||
           status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED;
}

static bool deep_render_cancellation_terminal_publish_status(
    RuntimeNative3DAsyncRenderPublishStatus status) {
    return status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED ||
           status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION ||
           status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED ||
           status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED;
}

static bool deep_render_cancellation_requestable_session(
    const RayTracingDeepRenderSession* session) {
    return session &&
           (session->state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING ||
            session->state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
            session->state == RAY_TRACING_DEEP_RENDER_SESSION_SAVING ||
            session->state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING);
}

void RayTracingDeepRenderCancellation_Init(
    RayTracingDeepRenderCancellation* cancellation) {
    if (!cancellation) return;
    memset(cancellation, 0, sizeof(*cancellation));
}

void RayTracingDeepRenderCancellation_Reset(
    RayTracingDeepRenderCancellation* cancellation) {
    RayTracingDeepRenderCancellation_Init(cancellation);
}

bool RayTracingDeepRenderCancellation_Request(
    RayTracingDeepRenderCancellation* cancellation,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    RayTracingDeepRenderCancellationResult* out_result) {
    RayTracingDeepRenderCancellationResult result;
    const bool before_dispatch =
        session && session->state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
        !session->frameRequestOwned;

    deep_render_cancellation_result_init(&result);
    if (out_result) *out_result = result;
    if (!cancellation || !session || !out_result) return false;
    result.generation = session->generation;
    if (!deep_render_cancellation_requestable_session(session) ||
        session->generation == 0u) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_SESSION;
        *out_result = result;
        return false;
    }
    if (cancellation->requestIssued) {
        if (cancellation->generation != session->generation ||
            session->state != RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
            result.status =
                RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH;
            *out_result = result;
            return false;
        }
        result.status =
            cancellation->sessionCanceled
                ? RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED
                : RAY_TRACING_DEEP_RENDER_CANCELLATION_WAITING_FOR_TERMINAL;
        result.waitingForTerminal = !cancellation->sessionCanceled;
        result.sessionCanceled = cancellation->sessionCanceled;
        *out_result = result;
        return true;
    }
    if (!before_dispatch && !job) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT;
        *out_result = result;
        return false;
    }
    if (job) {
        result.jobStatus = RuntimeNative3DAsyncRenderJob_GetStatus(job);
        if (!before_dispatch &&
            result.jobStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE) {
            result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT;
            *out_result = result;
            return false;
        }
    }
    if (!RayTracingDeepRenderSession_RequestCancel(session)) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_TRANSITION_FAILED;
        *out_result = result;
        return false;
    }

    cancellation->generation = session->generation;
    cancellation->requestIssued = true;
    result.requestIssuedThisCall = true;
    if (before_dispatch) {
        if (!RayTracingDeepRenderSession_MarkCanceled(session)) {
            result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_TRANSITION_FAILED;
            *out_result = result;
            return false;
        }
        cancellation->terminalConsumed = true;
        cancellation->sessionCanceled = true;
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED;
        result.terminalConsumed = true;
        result.sessionCanceled = true;
        *out_result = result;
        return true;
    }

    RuntimeNative3DAsyncRenderJob_RequestCancel(job);
    result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_REQUESTED;
    result.waitingForTerminal = true;
    *out_result = result;
    return true;
}

bool RayTracingDeepRenderCancellation_Poll(
    RayTracingDeepRenderCancellation* cancellation,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderListenerPollResult* poll_result,
    RayTracingDeepRenderCancellationResult* out_result) {
    RayTracingDeepRenderCancellationResult result;

    deep_render_cancellation_result_init(&result);
    if (out_result) *out_result = result;
    if (!cancellation || !session || !job || !poll_result || !out_result) {
        return false;
    }
    result.generation = cancellation->generation;
    result.publishStatus = poll_result->publishStatus;
    result.jobStatus = RuntimeNative3DAsyncRenderJob_GetStatus(job);
    if (!cancellation->requestIssued || cancellation->generation == 0u ||
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_SESSION;
        *out_result = result;
        return false;
    }
    if (session->generation != cancellation->generation ||
        poll_result->generation != cancellation->generation) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH;
        *out_result = result;
        return false;
    }
    if (!poll_result->terminalObserved) {
        result.status =
            RAY_TRACING_DEEP_RENDER_CANCELLATION_WAITING_FOR_TERMINAL;
        result.waitingForTerminal = true;
        *out_result = result;
        return true;
    }
    if (poll_result->status != RAY_TRACING_DEEP_RENDER_LISTENER_OK ||
        !deep_render_cancellation_terminal_publish_status(
            poll_result->publishStatus) ||
        !poll_result->jobResult.valid) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT;
        *out_result = result;
        return false;
    }
    if (poll_result->jobResult.generation != cancellation->generation ||
        (poll_result->jobResult.snapshot.generationBound &&
         poll_result->jobResult.snapshot.generation != cancellation->generation)) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH;
        *out_result = result;
        return false;
    }
    if (!deep_render_cancellation_terminal_status(result.jobStatus)) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_JOB_NOT_TERMINAL;
        *out_result = result;
        return false;
    }
    if (!RuntimeNative3DAsyncRenderJob_Join(job)) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_JOIN_FAILED;
        *out_result = result;
        return false;
    }
    cancellation->terminalConsumed = true;
    cancellation->jobJoined = true;
    result.terminalConsumed = true;
    result.jobJoinedThisCall = true;
    if (!RayTracingDeepRenderSession_MarkCanceled(session)) {
        result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_TRANSITION_FAILED;
        *out_result = result;
        return false;
    }
    cancellation->sessionCanceled = true;
    result.status = RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED;
    result.sessionCanceled = true;
    *out_result = result;
    return true;
}

const char* RayTracingDeepRenderCancellationStatus_Name(
    RayTracingDeepRenderCancellationStatus status) {
    switch (status) {
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_IDLE: return "idle";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_REQUESTED: return "requested";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_WAITING_FOR_TERMINAL:
            return "waiting_for_terminal";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED:
            return "session_canceled";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT:
            return "invalid_argument";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_SESSION:
            return "invalid_session";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH:
            return "generation_mismatch";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_JOB_NOT_TERMINAL:
            return "job_not_terminal";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_JOIN_FAILED:
            return "join_failed";
        case RAY_TRACING_DEEP_RENDER_CANCELLATION_TRANSITION_FAILED:
            return "transition_failed";
    }
    return "unknown";
}
