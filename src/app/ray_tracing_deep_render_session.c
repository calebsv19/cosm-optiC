#include "app/ray_tracing_deep_render_session.h"

#include <limits.h>
#include <string.h>

static bool deep_render_session_active_state(RayTracingDeepRenderSessionState state) {
    return state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_SAVING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING;
}

static void deep_render_session_release_request(RayTracingDeepRenderSession* session) {
    if (!session || !session->frameRequestOwned) return;
    RayTracingDeepRenderFrameRequest_Destroy(&session->frameRequest);
    session->frameRequestOwned = false;
}

void RayTracingDeepRenderSession_Init(RayTracingDeepRenderSession* session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_IDLE;
}

void RayTracingDeepRenderSession_Reset(RayTracingDeepRenderSession* session) {
    if (!session) return;
    deep_render_session_release_request(session);
    RayTracingDeepRenderSession_Init(session);
}

bool RayTracingDeepRenderSession_Begin(
    RayTracingDeepRenderSession* session,
    const RayTracingDeepRenderSessionDesc* desc) {
    if (!session || !desc || session->state != RAY_TRACING_DEEP_RENDER_SESSION_IDLE ||
        desc->startFrameIndex < 0 || desc->frameCount <= 0 ||
        desc->frameCount > INT_MAX - desc->startFrameIndex ||
        desc->initialGeneration == 0u) {
        if (session && session->state == RAY_TRACING_DEEP_RENDER_SESSION_IDLE) {
            session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_INVALID_START;
        }
        return false;
    }
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_PREPARING;
    session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_NONE;
    session->generation = desc->initialGeneration;
    session->startFrameIndex = desc->startFrameIndex;
    session->currentLocalFrameIndex = 0;
    session->currentAbsoluteFrameIndex = desc->startFrameIndex;
    session->frameCount = desc->frameCount;
    session->completedFrameCount = 0;
    return true;
}

bool RayTracingDeepRenderSession_AdoptFrameRequest(
    RayTracingDeepRenderSession* session,
    RayTracingDeepRenderFrameRequest* request) {
    if (!session || !request ||
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_PREPARING ||
        session->frameRequestOwned || !request->valid ||
        request->generation != session->generation ||
        request->localFrameIndex != session->currentLocalFrameIndex ||
        request->absoluteFrameIndex != session->currentAbsoluteFrameIndex ||
        request->frameCount != session->frameCount) {
        if (session && session->state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING) {
            session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH;
        }
        return false;
    }
    if (!RayTracingDeepRenderFrameRequest_Move(&session->frameRequest, request)) {
        session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH;
        return false;
    }
    session->frameRequestOwned = true;
    session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_NONE;
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_RENDERING;
    return true;
}

const RayTracingDeepRenderFrameRequest* RayTracingDeepRenderSession_CurrentRequest(
    const RayTracingDeepRenderSession* session) {
    if (!session || !session->frameRequestOwned) return NULL;
    return &session->frameRequest;
}

bool RayTracingDeepRenderSession_MarkRenderSucceeded(
    RayTracingDeepRenderSession* session) {
    if (!session || session->state != RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
        !session->frameRequestOwned) {
        return false;
    }
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_SAVING;
    return true;
}

bool RayTracingDeepRenderSession_MarkFrameSaved(
    RayTracingDeepRenderSession* session) {
    if (!session || session->state != RAY_TRACING_DEEP_RENDER_SESSION_SAVING ||
        !session->frameRequestOwned) {
        return false;
    }
    deep_render_session_release_request(session);
    session->completedFrameCount += 1;
    if (session->completedFrameCount >= session->frameCount) {
        session->state = RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED;
        return true;
    }
    if (session->generation == UINT64_MAX) {
        session->failure = RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_GENERATION_EXHAUSTED;
        session->state = RAY_TRACING_DEEP_RENDER_SESSION_FAILED;
        return false;
    }
    session->generation += 1u;
    session->currentLocalFrameIndex += 1;
    session->currentAbsoluteFrameIndex += 1;
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_PREPARING;
    return true;
}

bool RayTracingDeepRenderSession_RequestCancel(
    RayTracingDeepRenderSession* session) {
    if (!session) return false;
    if (session->state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) return true;
    if (session->state != RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
        session->state != RAY_TRACING_DEEP_RENDER_SESSION_SAVING) {
        return false;
    }
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_CANCELING;
    return true;
}

bool RayTracingDeepRenderSession_MarkCanceled(
    RayTracingDeepRenderSession* session) {
    if (!session || session->state != RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        return false;
    }
    deep_render_session_release_request(session);
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_CANCELED;
    return true;
}

bool RayTracingDeepRenderSession_MarkFailed(
    RayTracingDeepRenderSession* session,
    RayTracingDeepRenderSessionFailure failure) {
    if (!session || !deep_render_session_active_state(session->state) ||
        failure == RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_NONE) {
        return false;
    }
    deep_render_session_release_request(session);
    session->failure = failure;
    session->state = RAY_TRACING_DEEP_RENDER_SESSION_FAILED;
    return true;
}

RayTracingDeepRenderSessionSnapshot RayTracingDeepRenderSession_GetSnapshot(
    const RayTracingDeepRenderSession* session) {
    RayTracingDeepRenderSessionSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    if (!session) return snapshot;
    snapshot.state = session->state;
    snapshot.failure = session->failure;
    snapshot.generation = session->generation;
    snapshot.startFrameIndex = session->startFrameIndex;
    snapshot.currentLocalFrameIndex = session->currentLocalFrameIndex;
    snapshot.currentAbsoluteFrameIndex = session->currentAbsoluteFrameIndex;
    snapshot.frameCount = session->frameCount;
    snapshot.completedFrameCount = session->completedFrameCount;
    snapshot.frameRequestOwned = session->frameRequestOwned;
    return snapshot;
}

const char* RayTracingDeepRenderSessionState_Name(
    RayTracingDeepRenderSessionState state) {
    switch (state) {
        case RAY_TRACING_DEEP_RENDER_SESSION_IDLE: return "idle";
        case RAY_TRACING_DEEP_RENDER_SESSION_PREPARING: return "preparing";
        case RAY_TRACING_DEEP_RENDER_SESSION_RENDERING: return "rendering";
        case RAY_TRACING_DEEP_RENDER_SESSION_SAVING: return "saving";
        case RAY_TRACING_DEEP_RENDER_SESSION_CANCELING: return "canceling";
        case RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED: return "completed";
        case RAY_TRACING_DEEP_RENDER_SESSION_CANCELED: return "canceled";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILED: return "failed";
    }
    return "unknown";
}

const char* RayTracingDeepRenderSessionFailure_Name(
    RayTracingDeepRenderSessionFailure failure) {
    switch (failure) {
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_NONE: return "none";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_INVALID_START: return "invalid_start";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_INVALID_TRANSITION: return "invalid_transition";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH: return "request_mismatch";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER: return "render";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE: return "save";
        case RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_GENERATION_EXHAUSTED: return "generation_exhausted";
    }
    return "unknown";
}
