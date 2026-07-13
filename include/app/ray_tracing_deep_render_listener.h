#ifndef APP_RAY_TRACING_DEEP_RENDER_LISTENER_H
#define APP_RAY_TRACING_DEEP_RENDER_LISTENER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/ray_tracing_deep_render_session.h"
#include "render/runtime_native_3d_async_render_bridge.h"
#include "render/runtime_native_3d_async_render_job.h"

typedef enum RayTracingDeepRenderListenerStatus {
    RAY_TRACING_DEEP_RENDER_LISTENER_OK = 0,
    RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_ARGUMENT,
    RAY_TRACING_DEEP_RENDER_LISTENER_INVALID_SESSION,
    RAY_TRACING_DEEP_RENDER_LISTENER_ALLOCATION_FAILED,
    RAY_TRACING_DEEP_RENDER_LISTENER_PROGRESS_INVALID,
    RAY_TRACING_DEEP_RENDER_LISTENER_PRESENT_FAILED,
} RayTracingDeepRenderListenerStatus;

typedef struct RayTracingDeepRenderPresentationView {
    bool valid;
    uint64_t generation;
    uint64_t sequence;
    int hostWidth;
    int hostHeight;
    RuntimeNative3DAsyncRenderProgressRect dirtyRect;
    const uint8_t* pixels;
    size_t byteCount;
} RayTracingDeepRenderPresentationView;

typedef bool (*RayTracingDeepRenderPresentFn)(
    const RayTracingDeepRenderPresentationView* view,
    void* user_data);

typedef struct RayTracingDeepRenderListenerPollResult {
    RayTracingDeepRenderListenerStatus status;
    uint64_t generation;
    bool progressApplied;
    bool staleProgressRejected;
    bool presented;
    bool terminalObserved;
    RuntimeNative3DAsyncRenderProgressSnapshot progress;
    RuntimeNative3DAsyncRenderJobStatus jobStatus;
    RuntimeNative3DAsyncRenderPublishStatus publishStatus;
    RuntimeNative3DAsyncRenderJobResult jobResult;
} RayTracingDeepRenderListenerPollResult;

typedef struct RayTracingDeepRenderListener {
    uint8_t* displayPixels;
    uint8_t* progressScratch;
    size_t displayCapacity;
    size_t progressScratchCapacity;
    uint64_t generation;
    uint64_t lastSequence;
    int hostWidth;
    int hostHeight;
    RuntimeNative3DAsyncRenderProgressRect dirtyRect;
    bool displayValid;
} RayTracingDeepRenderListener;

/* Poll and present are desktop/main-thread operations and never join the job. */
void RayTracingDeepRenderListener_Init(RayTracingDeepRenderListener* listener);
void RayTracingDeepRenderListener_Destroy(RayTracingDeepRenderListener* listener);

bool RayTracingDeepRenderListener_Poll(
    RayTracingDeepRenderListener* listener,
    const RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    RuntimeNative3DAsyncRenderJob* job,
    RayTracingDeepRenderPresentFn present_fn,
    void* present_user_data,
    RayTracingDeepRenderListenerPollResult* out_result);

RayTracingDeepRenderPresentationView RayTracingDeepRenderListener_GetView(
    const RayTracingDeepRenderListener* listener);

const char* RayTracingDeepRenderListenerStatus_Name(
    RayTracingDeepRenderListenerStatus status);

#endif
