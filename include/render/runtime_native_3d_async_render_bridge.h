#ifndef RENDER_RUNTIME_NATIVE_3D_ASYNC_RENDER_BRIDGE_H
#define RENDER_RUNTIME_NATIVE_3D_ASYNC_RENDER_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_native_3d_render_request_snapshot.h"

typedef enum RuntimeNative3DAsyncRenderReadiness {
    RUNTIME_NATIVE_3D_ASYNC_RENDER_READY_EXCLUSIVE_SINGLE_JOB = 0,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_INVALID_SNAPSHOT,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_GENERATION_UNBOUND,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_CANCEL_UNBOUND,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_PREPARED_FRAME_UNBOUND,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_ACCELERATION_UNBOUND,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_NON_TLAS_ROUTE,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_VOLUME,
    RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_WATER,
} RuntimeNative3DAsyncRenderReadiness;

typedef struct RuntimeNative3DAsyncRenderAssessment {
    RuntimeNative3DAsyncRenderReadiness readiness;
    bool ready;
    bool requiresExclusiveRenderContext;
    const char* reason;
} RuntimeNative3DAsyncRenderAssessment;

typedef struct RuntimeNative3DAsyncRenderProgressRect {
    int x;
    int y;
    int width;
    int height;
} RuntimeNative3DAsyncRenderProgressRect;

typedef struct RuntimeNative3DAsyncRenderProgressSnapshot {
    bool valid;
    bool staleGeneration;
    uint64_t generation;
    uint64_t sequence;
    int hostWidth;
    int hostHeight;
    RuntimeNative3DAsyncRenderProgressRect rect;
    size_t byteCount;
} RuntimeNative3DAsyncRenderProgressSnapshot;

typedef struct RuntimeNative3DAsyncRenderProgressBuffer
    RuntimeNative3DAsyncRenderProgressBuffer;

RuntimeNative3DAsyncRenderAssessment RuntimeNative3DAsyncRender_AssessSnapshot(
    const RuntimeNative3DRenderRequestSnapshot* snapshot);
const char* RuntimeNative3DAsyncRenderReadiness_Name(
    RuntimeNative3DAsyncRenderReadiness readiness);

RuntimeNative3DAsyncRenderProgressBuffer*
RuntimeNative3DAsyncRenderProgressBuffer_Create(void);
void RuntimeNative3DAsyncRenderProgressBuffer_Destroy(
    RuntimeNative3DAsyncRenderProgressBuffer* progress);
void RuntimeNative3DAsyncRenderProgressBuffer_Reset(
    RuntimeNative3DAsyncRenderProgressBuffer* progress);

bool RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t generation,
    const uint8_t* host_buffer,
    int host_width,
    int host_height,
    RuntimeNative3DAsyncRenderProgressRect rect);

bool RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(
    RuntimeNative3DAsyncRenderProgressBuffer* progress,
    uint64_t current_generation,
    RuntimeNative3DAsyncRenderProgressSnapshot* out_snapshot,
    uint8_t* out_pixels,
    size_t out_pixel_capacity,
    size_t* out_required_bytes);

#endif
