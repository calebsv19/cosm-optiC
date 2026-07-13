#include "app/ray_tracing_deep_render_frame_request.h"

#include <math.h>
#include <string.h>

#include "render/runtime_native_3d_async_render_bridge.h"

static void deep_render_frame_request_set_status(
    RayTracingDeepRenderFrameRequestStatus* out_status,
    RayTracingDeepRenderFrameRequestStatus status) {
    if (out_status) {
        *out_status = status;
    }
}

static bool deep_render_frame_request_copy_path(char* dst,
                                                size_t dst_size,
                                                const char* src) {
    size_t length = 0u;
    if (!dst || dst_size == 0u || !src || !src[0]) return false;
    length = strlen(src);
    if (length >= dst_size) return false;
    memcpy(dst, src, length + 1u);
    return true;
}

static bool deep_render_frame_request_vec3_finite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool deep_render_frame_request_camera_valid(const RuntimeCamera3D* camera) {
    return camera && deep_render_frame_request_vec3_finite(camera->position) &&
           isfinite(camera->rotation) && isfinite(camera->lookPitch) &&
           isfinite(camera->zoom) && camera->zoom > 0.0 &&
           isfinite(camera->nearPlane) && camera->nearPlane > 0.0;
}

static bool deep_render_frame_request_light_valid(const RuntimeLight3D* light) {
    return light && deep_render_frame_request_vec3_finite(light->position) &&
           isfinite(light->radius) && light->radius >= 0.0 &&
           isfinite(light->intensity) && light->intensity >= 0.0 &&
           isfinite(light->falloffDistance) && light->falloffDistance >= 0.0;
}

void RayTracingDeepRenderFrameRequest_Init(
    RayTracingDeepRenderFrameRequest* request) {
    if (!request) return;
    memset(request, 0, sizeof(*request));
}

void RayTracingDeepRenderFrameRequest_Destroy(
    RayTracingDeepRenderFrameRequest* request) {
    if (!request) return;
    if (request->preparedFrameOwned) {
        RuntimeNative3DPreparedFrame_Free(&request->preparedFrame);
    }
    memset(request, 0, sizeof(*request));
}

bool RayTracingDeepRenderFrameRequest_Build(
    RayTracingDeepRenderFrameRequest* out_request,
    const RayTracingDeepRenderFrameRequestDesc* desc,
    RuntimeNative3DPreparedFrame* prepared_frame,
    RayTracingDeepRenderFrameRequestStatus* out_status) {
    RayTracingDeepRenderFrameRequest request;
    const RuntimeNative3DRenderRequestSnapshot* snapshot = NULL;
    bool trace_scene_uses_owned_scene = false;

    deep_render_frame_request_set_status(
        out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_INVALID_ARGUMENT);
    if (!out_request) return false;
    RayTracingDeepRenderFrameRequest_Init(out_request);
    RayTracingDeepRenderFrameRequest_Init(&request);
    if (!desc || !prepared_frame) return false;
    if (desc->generation == 0u) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_UNBOUND);
        return false;
    }
    if (desc->localFrameIndex < 0 || desc->absoluteFrameIndex < 0 ||
        desc->frameCount <= 0 || desc->localFrameIndex >= desc->frameCount) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_FRAME_RANGE_INVALID);
        return false;
    }
    if (!isfinite(desc->frameDurationSeconds) || desc->frameDurationSeconds <= 0.0 ||
        !isfinite(desc->animationTimeSeconds) || desc->animationTimeSeconds < 0.0 ||
        !isfinite(desc->normalizedT) || desc->normalizedT < 0.0 ||
        desc->normalizedT > 1.0) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_TIMING_INVALID);
        return false;
    }
    if (!deep_render_frame_request_camera_valid(desc->camera)) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CAMERA_INVALID);
        return false;
    }
    if (!deep_render_frame_request_light_valid(desc->light)) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_LIGHT_INVALID);
        return false;
    }
    if (!deep_render_frame_request_copy_path(request.outputRoot,
                                             sizeof(request.outputRoot),
                                             desc->outputRoot) ||
        !deep_render_frame_request_copy_path(request.frameDirectory,
                                             sizeof(request.frameDirectory),
                                             desc->frameDirectory) ||
        !deep_render_frame_request_copy_path(request.finalFramePath,
                                             sizeof(request.finalFramePath),
                                             desc->finalFramePath)) {
        deep_render_frame_request_set_status(
            out_status,
            RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_OUTPUT_IDENTITY_INVALID);
        return false;
    }

    snapshot = desc->renderSnapshot;
    if (!snapshot || !snapshot->valid) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_RENDER_SNAPSHOT_INVALID);
        return false;
    }
    if ((snapshot->generationBound && snapshot->generation != desc->generation) ||
        (snapshot->cancelTokenBound &&
         (snapshot->cancelToken.generation != desc->generation ||
          snapshot->cancelGeneration != desc->generation))) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_MISMATCH);
        return false;
    }
    if (snapshot->frameIndex != desc->absoluteFrameIndex ||
        snapshot->frameCount != desc->frameCount) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_FRAME_RANGE_INVALID);
        return false;
    }
    if (!prepared_frame->valid || !snapshot->preparedFrameBound ||
        !snapshot->preparedFrameValid || snapshot->preparedFrameWidth != prepared_frame->width ||
        snapshot->preparedFrameHeight != prepared_frame->height) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_INVALID);
        return false;
    }
    if (prepared_frame->traceScene && prepared_frame->traceScene != &prepared_frame->scene) {
        deep_render_frame_request_set_status(
            out_status,
            RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_EXTERNAL_BACKING);
        return false;
    }
    if (!snapshot->sceneAccelerationBound) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_ACCELERATION_UNBOUND);
        return false;
    }
    if (snapshot->traceRoute != RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_NON_TLAS_ROUTE);
        return false;
    }
    if (snapshot->volumeFrameSelectionDynamic) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_VOLUME_UNOWNED);
        return false;
    }
    if (snapshot->waterSurfaceFrameSelectionDynamic) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_WATER_UNOWNED);
        return false;
    }

    request.generation = desc->generation;
    request.localFrameIndex = desc->localFrameIndex;
    request.absoluteFrameIndex = desc->absoluteFrameIndex;
    request.frameCount = desc->frameCount;
    request.frameDurationSeconds = desc->frameDurationSeconds;
    request.animationTimeSeconds = desc->animationTimeSeconds;
    request.normalizedT = desc->normalizedT;
    request.camera = *desc->camera;
    request.light = *desc->light;
    request.renderSnapshot = *snapshot;
    request.renderSnapshot.generationBound = true;
    request.renderSnapshot.generation = desc->generation;
    request.renderSnapshot.cancelTokenBound = false;
    memset(&request.renderSnapshot.cancelToken, 0, sizeof(request.renderSnapshot.cancelToken));
    request.renderSnapshot.cancelGeneration = 0u;

    trace_scene_uses_owned_scene = prepared_frame->traceScene == &prepared_frame->scene;
    request.preparedFrame = *prepared_frame;
    request.preparedFrame.traceScene = NULL;
    request.preparedFrameOwned = true;
    request.valid = true;
    memset(prepared_frame, 0, sizeof(*prepared_frame));
    *out_request = request;
    if (trace_scene_uses_owned_scene) {
        out_request->preparedFrame.traceScene = &out_request->preparedFrame.scene;
    }
    deep_render_frame_request_set_status(
        out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY);
    return true;
}

bool RayTracingDeepRenderFrameRequest_Move(
    RayTracingDeepRenderFrameRequest* out_request,
    RayTracingDeepRenderFrameRequest* source_request) {
    bool trace_scene_uses_owned_scene = false;
    if (!out_request || !source_request || out_request == source_request ||
        !source_request->valid || !source_request->preparedFrameOwned) {
        return false;
    }
    trace_scene_uses_owned_scene =
        source_request->preparedFrame.traceScene == &source_request->preparedFrame.scene;
    *out_request = *source_request;
    if (trace_scene_uses_owned_scene) {
        out_request->preparedFrame.traceScene = &out_request->preparedFrame.scene;
    }
    memset(source_request, 0, sizeof(*source_request));
    return true;
}

bool RayTracingDeepRenderFrameRequest_BuildDispatchSnapshot(
    const RayTracingDeepRenderFrameRequest* request,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    RuntimeNative3DRenderRequestSnapshot* out_snapshot,
    RayTracingDeepRenderFrameRequestStatus* out_status) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DAsyncRenderAssessment assessment;

    deep_render_frame_request_set_status(
        out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_INVALID_ARGUMENT);
    if (!out_snapshot) return false;
    RuntimeNative3DRenderRequestSnapshot_Init(out_snapshot);
    if (!request || !request->valid || !request->preparedFrameOwned) return false;
    if (!cancel_token || !cancel_token->cancelRequested ||
        cancel_token->generation != request->generation) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CANCEL_BINDING_INVALID);
        return false;
    }
    snapshot = request->renderSnapshot;
    snapshot.cancelTokenBound = true;
    snapshot.cancelToken = *cancel_token;
    snapshot.cancelGeneration = request->generation;
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    if (!assessment.ready) {
        deep_render_frame_request_set_status(
            out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_RENDER_SNAPSHOT_INVALID);
        return false;
    }
    *out_snapshot = snapshot;
    deep_render_frame_request_set_status(
        out_status, RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY);
    return true;
}

const char* RayTracingDeepRenderFrameRequestStatus_Name(
    RayTracingDeepRenderFrameRequestStatus status) {
    switch (status) {
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY: return "ready";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_INVALID_ARGUMENT: return "invalid_argument";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_UNBOUND: return "generation_unbound";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_FRAME_RANGE_INVALID: return "frame_range_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_TIMING_INVALID: return "timing_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CAMERA_INVALID: return "camera_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_LIGHT_INVALID: return "light_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_OUTPUT_IDENTITY_INVALID: return "output_identity_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_RENDER_SNAPSHOT_INVALID: return "render_snapshot_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_GENERATION_MISMATCH: return "generation_mismatch";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_INVALID: return "prepared_frame_invalid";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_PREPARED_FRAME_EXTERNAL_BACKING: return "prepared_frame_external_backing";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_ACCELERATION_UNBOUND: return "acceleration_unbound";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_NON_TLAS_ROUTE: return "non_tlas_route";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_VOLUME_UNOWNED: return "dynamic_volume_unowned";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_WATER_UNOWNED: return "dynamic_water_unowned";
        case RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_CANCEL_BINDING_INVALID: return "cancel_binding_invalid";
    }
    return "unknown";
}
