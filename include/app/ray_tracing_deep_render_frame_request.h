#ifndef APP_RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_H
#define APP_RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_native_3d_render_request_snapshot.h"
#include "render/runtime_scene_3d.h"

#define RAY_TRACING_DEEP_RENDER_PATH_MAX 4096

typedef enum RayTracingDeepRenderFrameRequestStatus {
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY = 0,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_INVALID_ARGUMENT,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_UNBOUND,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_FRAME_RANGE_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_TIMING_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CAMERA_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_LIGHT_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_OUTPUT_IDENTITY_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_RENDER_SNAPSHOT_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_MISMATCH,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_INVALID,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_EXTERNAL_BACKING,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_ACCELERATION_UNBOUND,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_NON_TLAS_ROUTE,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_VOLUME_UNOWNED,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_WATER_UNOWNED,
    RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CANCEL_BINDING_INVALID,
} RayTracingDeepRenderFrameRequestStatus;

typedef struct RayTracingDeepRenderFrameRequestDesc {
    uint64_t generation;
    int localFrameIndex;
    int absoluteFrameIndex;
    int frameCount;
    double frameDurationSeconds;
    double animationTimeSeconds;
    double normalizedT;
    const RuntimeCamera3D* camera;
    const RuntimeLight3D* light;
    const RuntimeNative3DRenderRequestSnapshot* renderSnapshot;
    const char* outputRoot;
    const char* frameDirectory;
    const char* finalFramePath;
} RayTracingDeepRenderFrameRequestDesc;

/*
 * Move-only app contract. Build adopts prepared_frame on success. Callers must
 * use Move rather than copying this struct because it owns prepared-frame data.
 */
typedef struct RayTracingDeepRenderFrameRequest {
    bool valid;
    bool preparedFrameOwned;
    uint64_t generation;
    int localFrameIndex;
    int absoluteFrameIndex;
    int frameCount;
    double frameDurationSeconds;
    double animationTimeSeconds;
    double normalizedT;
    RuntimeCamera3D camera;
    RuntimeLight3D light;
    RuntimeNative3DRenderRequestSnapshot renderSnapshot;
    RuntimeNative3DPreparedFrame preparedFrame;
    char outputRoot[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    char frameDirectory[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    char finalFramePath[RAY_TRACING_DEEP_RENDER_PATH_MAX];
} RayTracingDeepRenderFrameRequest;

void RayTracingDeepRenderFrameRequest_Init(
    RayTracingDeepRenderFrameRequest* request);
void RayTracingDeepRenderFrameRequest_Destroy(
    RayTracingDeepRenderFrameRequest* request);

bool RayTracingDeepRenderFrameRequest_Build(
    RayTracingDeepRenderFrameRequest* out_request,
    const RayTracingDeepRenderFrameRequestDesc* desc,
    RuntimeNative3DPreparedFrame* prepared_frame,
    RayTracingDeepRenderFrameRequestStatus* out_status);

bool RayTracingDeepRenderFrameRequest_Move(
    RayTracingDeepRenderFrameRequest* out_request,
    RayTracingDeepRenderFrameRequest* source_request);

bool RayTracingDeepRenderFrameRequest_BuildDispatchSnapshot(
    const RayTracingDeepRenderFrameRequest* request,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    RuntimeNative3DRenderRequestSnapshot* out_snapshot,
    RayTracingDeepRenderFrameRequestStatus* out_status);

const char* RayTracingDeepRenderFrameRequestStatus_Name(
    RayTracingDeepRenderFrameRequestStatus status);

#endif
