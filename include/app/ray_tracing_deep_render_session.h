#ifndef APP_RAY_TRACING_DEEP_RENDER_SESSION_H
#define APP_RAY_TRACING_DEEP_RENDER_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "app/ray_tracing_deep_render_frame_request.h"

typedef enum RayTracingDeepRenderSessionState {
    RAY_TRACING_DEEP_RENDER_SESSION_IDLE = 0,
    RAY_TRACING_DEEP_RENDER_SESSION_PREPARING,
    RAY_TRACING_DEEP_RENDER_SESSION_RENDERING,
    RAY_TRACING_DEEP_RENDER_SESSION_SAVING,
    RAY_TRACING_DEEP_RENDER_SESSION_CANCELING,
    RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED,
    RAY_TRACING_DEEP_RENDER_SESSION_CANCELED,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILED,
} RayTracingDeepRenderSessionState;

typedef enum RayTracingDeepRenderSessionFailure {
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_NONE = 0,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_INVALID_START,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_INVALID_TRANSITION,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE,
    RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_GENERATION_EXHAUSTED,
} RayTracingDeepRenderSessionFailure;

typedef struct RayTracingDeepRenderSessionDesc {
    int startFrameIndex;
    int frameCount;
    uint64_t initialGeneration;
} RayTracingDeepRenderSessionDesc;

typedef struct RayTracingDeepRenderSessionSnapshot {
    RayTracingDeepRenderSessionState state;
    RayTracingDeepRenderSessionFailure failure;
    uint64_t generation;
    int startFrameIndex;
    int currentLocalFrameIndex;
    int currentAbsoluteFrameIndex;
    int frameCount;
    int completedFrameCount;
    bool frameRequestOwned;
} RayTracingDeepRenderSessionSnapshot;

typedef struct RayTracingDeepRenderSession {
    RayTracingDeepRenderSessionState state;
    RayTracingDeepRenderSessionFailure failure;
    uint64_t generation;
    int startFrameIndex;
    int currentLocalFrameIndex;
    int currentAbsoluteFrameIndex;
    int frameCount;
    int completedFrameCount;
    bool frameRequestOwned;
    RayTracingDeepRenderFrameRequest frameRequest;
} RayTracingDeepRenderSession;

/* The desktop/main thread is the sole mutator of this session contract. */
void RayTracingDeepRenderSession_Init(RayTracingDeepRenderSession* session);
void RayTracingDeepRenderSession_Reset(RayTracingDeepRenderSession* session);
bool RayTracingDeepRenderSession_Begin(
    RayTracingDeepRenderSession* session,
    const RayTracingDeepRenderSessionDesc* desc);

bool RayTracingDeepRenderSession_AdoptFrameRequest(
    RayTracingDeepRenderSession* session,
    RayTracingDeepRenderFrameRequest* request);
const RayTracingDeepRenderFrameRequest* RayTracingDeepRenderSession_CurrentRequest(
    const RayTracingDeepRenderSession* session);

bool RayTracingDeepRenderSession_MarkRenderSucceeded(
    RayTracingDeepRenderSession* session);
bool RayTracingDeepRenderSession_MarkFrameSaved(
    RayTracingDeepRenderSession* session);
bool RayTracingDeepRenderSession_RequestCancel(
    RayTracingDeepRenderSession* session);
bool RayTracingDeepRenderSession_MarkCanceled(
    RayTracingDeepRenderSession* session);
bool RayTracingDeepRenderSession_MarkFailed(
    RayTracingDeepRenderSession* session,
    RayTracingDeepRenderSessionFailure failure);

RayTracingDeepRenderSessionSnapshot RayTracingDeepRenderSession_GetSnapshot(
    const RayTracingDeepRenderSession* session);
const char* RayTracingDeepRenderSessionState_Name(
    RayTracingDeepRenderSessionState state);
const char* RayTracingDeepRenderSessionFailure_Name(
    RayTracingDeepRenderSessionFailure failure);

#endif
