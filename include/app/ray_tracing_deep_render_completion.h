#ifndef APP_RAY_TRACING_DEEP_RENDER_COMPLETION_H
#define APP_RAY_TRACING_DEEP_RENDER_COMPLETION_H

#include <stdbool.h>

#include "app/ray_tracing_deep_render_listener.h"

typedef enum RayTracingDeepRenderCompletionStatus {
    RAY_TRACING_DEEP_RENDER_COMPLETION_NO_TERMINAL = 0,
    RAY_TRACING_DEEP_RENDER_COMPLETION_FRAME_ADVANCED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_SESSION_COMPLETED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_ARGUMENT,
    RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_SESSION,
    RAY_TRACING_DEEP_RENDER_COMPLETION_STALE_TERMINAL,
    RAY_TRACING_DEEP_RENDER_COMPLETION_CANCELED_TERMINAL,
    RAY_TRACING_DEEP_RENDER_COMPLETION_RENDER_FAILED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_JOB_NOT_TERMINAL,
    RAY_TRACING_DEEP_RENDER_COMPLETION_JOIN_FAILED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_FINAL_IMAGE_INCOMPLETE,
    RAY_TRACING_DEEP_RENDER_COMPLETION_OUTPUT_IDENTITY_INVALID,
    RAY_TRACING_DEEP_RENDER_COMPLETION_SAVE_FAILED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_VERIFY_FAILED,
    RAY_TRACING_DEEP_RENDER_COMPLETION_SEQUENCE_FAILED,
} RayTracingDeepRenderCompletionStatus;

typedef bool (*RayTracingDeepRenderFrameWriteFn)(
    const char* path,
    const RayTracingDeepRenderPresentationView* view,
    void* user_data);

typedef bool (*RayTracingDeepRenderFrameVerifyFn)(
    const char* path,
    int expected_width,
    int expected_height,
    void* user_data);

typedef struct RayTracingDeepRenderCompletionDesc {
    RayTracingDeepRenderFrameWriteFn write_fn;
    RayTracingDeepRenderFrameVerifyFn verify_fn;
    void* user_data;
} RayTracingDeepRenderCompletionDesc;

typedef struct RayTracingDeepRenderCompletionResult {
    RayTracingDeepRenderCompletionStatus status;
    uint64_t generation;
    int absoluteFrameIndex;
    bool terminalConsumed;
    bool jobJoined;
    bool writeAttempted;
    bool outputWritten;
    bool outputVerified;
    bool frameAdvanced;
    bool sessionCompleted;
} RayTracingDeepRenderCompletionResult;

bool RayTracingDeepRenderCompletion_Process(
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderListenerPollResult* poll_result,
    const RayTracingDeepRenderPresentationView* final_view,
    const RayTracingDeepRenderCompletionDesc* desc,
    RayTracingDeepRenderCompletionResult* out_result);

bool RayTracingDeepRenderCompletion_WriteFrameBMP(
    const char* path,
    const RayTracingDeepRenderPresentationView* view,
    void* user_data);

bool RayTracingDeepRenderCompletion_VerifyFrameBMP(
    const char* path,
    int expected_width,
    int expected_height,
    void* user_data);

const char* RayTracingDeepRenderCompletionStatus_Name(
    RayTracingDeepRenderCompletionStatus status);

#endif
