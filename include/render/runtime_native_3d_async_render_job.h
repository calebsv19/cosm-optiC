#ifndef RENDER_RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_H
#define RENDER_RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_native_3d_render_request_snapshot.h"

typedef struct RuntimeNative3DAsyncRenderJob RuntimeNative3DAsyncRenderJob;

typedef enum RuntimeNative3DAsyncRenderJobStatus {
    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE = 0,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED,
} RuntimeNative3DAsyncRenderJobStatus;

typedef enum RuntimeNative3DAsyncRenderPublishStatus {
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_NOT_READY = 0,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_ALREADY_PUBLISHED,
} RuntimeNative3DAsyncRenderPublishStatus;

typedef struct RuntimeNative3DAsyncRenderJobResult {
    bool valid;
    bool succeeded;
    bool canceled;
    bool cancelRequested;
    bool staleGeneration;
    bool published;
    uint64_t generation;
    uint64_t completionValue;
    RuntimeNative3DRenderRequestSnapshot snapshot;
} RuntimeNative3DAsyncRenderJobResult;

typedef bool (*RuntimeNative3DAsyncRenderJobRunFn)(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result);

typedef struct RuntimeNative3DAsyncRenderJobStartDesc {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    uint64_t generation;
    RuntimeNative3DAsyncRenderJobRunFn run_fn;
    void* user_data;
} RuntimeNative3DAsyncRenderJobStartDesc;

RuntimeNative3DAsyncRenderJob* RuntimeNative3DAsyncRenderJob_Create(void);
void RuntimeNative3DAsyncRenderJob_Destroy(RuntimeNative3DAsyncRenderJob* job);

bool RuntimeNative3DAsyncRenderJob_Start(
    RuntimeNative3DAsyncRenderJob* job,
    const RuntimeNative3DAsyncRenderJobStartDesc* desc);
void RuntimeNative3DAsyncRenderJob_RequestCancel(RuntimeNative3DAsyncRenderJob* job);
bool RuntimeNative3DAsyncRenderJob_Join(RuntimeNative3DAsyncRenderJob* job);
bool RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(
    RuntimeNative3DAsyncRenderJob* job);

RuntimeNative3DAsyncRenderJobStatus RuntimeNative3DAsyncRenderJob_GetStatus(
    RuntimeNative3DAsyncRenderJob* job);
RuntimeNative3DAsyncRenderPublishStatus RuntimeNative3DAsyncRenderJob_TryPublish(
    RuntimeNative3DAsyncRenderJob* job,
    uint64_t current_generation,
    RuntimeNative3DAsyncRenderJobResult* out_result);

#endif
