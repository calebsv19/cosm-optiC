#ifndef APP_RAY_TRACING_DEEP_RENDER_CANCELLATION_H
#define APP_RAY_TRACING_DEEP_RENDER_CANCELLATION_H

#include <stdbool.h>
#include <stdint.h>

#include "app/ray_tracing_deep_render_listener.h"

typedef enum RayTracingDeepRenderCancellationStatus {
    RAY_TRACING_DEEP_RENDER_CANCELLATION_IDLE = 0,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_REQUESTED,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_WAITING_FOR_TERMINAL,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_SESSION,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_JOB_NOT_TERMINAL,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_JOIN_FAILED,
    RAY_TRACING_DEEP_RENDER_CANCELLATION_TRANSITION_FAILED,
} RayTracingDeepRenderCancellationStatus;

typedef struct RayTracingDeepRenderCancellationResult {
    RayTracingDeepRenderCancellationStatus status;
    uint64_t generation;
    bool requestIssuedThisCall;
    bool waitingForTerminal;
    bool terminalConsumed;
    bool jobJoinedThisCall;
    bool sessionCanceled;
    RuntimeNative3DAsyncRenderJobStatus jobStatus;
    RuntimeNative3DAsyncRenderPublishStatus publishStatus;
} RayTracingDeepRenderCancellationResult;

typedef struct RayTracingDeepRenderCancellation {
    uint64_t generation;
    bool requestIssued;
    bool terminalConsumed;
    bool jobJoined;
    bool sessionCanceled;
} RayTracingDeepRenderCancellation;

void RayTracingDeepRenderCancellation_Init(
    RayTracingDeepRenderCancellation* cancellation);
void RayTracingDeepRenderCancellation_Reset(
    RayTracingDeepRenderCancellation* cancellation);

/* Main-thread request path. A missing job is valid only before frame dispatch. */
bool RayTracingDeepRenderCancellation_Request(
    RayTracingDeepRenderCancellation* cancellation,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    RayTracingDeepRenderCancellationResult* out_result);

/*
 * Main-thread tick after listener polling. This never joins a running job;
 * retained progress may continue to present while cancellation is pending.
 */
bool RayTracingDeepRenderCancellation_Poll(
    RayTracingDeepRenderCancellation* cancellation,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderListenerPollResult* poll_result,
    RayTracingDeepRenderCancellationResult* out_result);

const char* RayTracingDeepRenderCancellationStatus_Name(
    RayTracingDeepRenderCancellationStatus status);

#endif
