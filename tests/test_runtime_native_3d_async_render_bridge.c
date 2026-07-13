#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_native_3d_async_render_bridge.h"
#include "test_support.h"

static RuntimeNative3DRenderRequestSnapshot make_bridge_snapshot(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    static atomic_bool cancel_requested = ATOMIC_VAR_INIT(false);
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {
        .cancelRequested = &cancel_requested,
        .generation = 9u,
    };
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .generationBound = true,
        .generation = 9u,
        .outputWidth = 160,
        .outputHeight = 96,
        .renderWidth = 160,
        .renderHeight = 96,
        .hostWidth = 160,
        .hostHeight = 96,
        .frameIndex = 1,
        .frameCount = 8,
        .temporalFrames = 1,
        .tileSize = 16,
        .integratorId = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        .preparedFrameBound = true,
        .preparedFrameValid = true,
        .preparedFrameWidth = 160,
        .preparedFrameHeight = 96,
        .preparedPrimitiveCount = 3u,
        .preparedTriangleCount = 26u,
        .sceneAccelerationBound = true,
        .traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS,
        .tlasInstanceCount = 3u,
        .tlasNodeCount = 5u,
        .cancelToken = &cancel_token,
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);
    return snapshot;
}

static int test_runtime_native_3d_async_bridge_assesses_static_tlas_ready(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot = make_bridge_snapshot();
    RuntimeNative3DAsyncRenderAssessment assessment =
        RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    assert_true("runtime_native_3d_async_bridge_ready",
                assessment.ready &&
                    assessment.requiresExclusiveRenderContext &&
                    assessment.readiness ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_READY_EXCLUSIVE_SINGLE_JOB);
    assert_true("runtime_native_3d_async_bridge_ready_name",
                strcmp(RuntimeNative3DAsyncRenderReadiness_Name(assessment.readiness),
                       "ready_exclusive_single_job") == 0);
    return 0;
}

static int test_runtime_native_3d_async_bridge_blocks_unsafe_routes(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot = make_bridge_snapshot();
    RuntimeNative3DAsyncRenderAssessment assessment;

    snapshot.cancelTokenBound = false;
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    assert_true("runtime_native_3d_async_bridge_blocks_cancel",
                !assessment.ready &&
                    assessment.readiness ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_CANCEL_UNBOUND);

    snapshot = make_bridge_snapshot();
    snapshot.traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH;
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    assert_true("runtime_native_3d_async_bridge_blocks_flattened",
                !assessment.ready &&
                    assessment.readiness ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_NON_TLAS_ROUTE);

    snapshot = make_bridge_snapshot();
    snapshot.waterSurfaceFrameSelectionDynamic = true;
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    assert_true("runtime_native_3d_async_bridge_blocks_dynamic_water",
                !assessment.ready &&
                    assessment.readiness ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_WATER);

    snapshot = make_bridge_snapshot();
    snapshot.volumeFrameSelectionDynamic = true;
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    assert_true("runtime_native_3d_async_bridge_blocks_dynamic_volume",
                !assessment.ready &&
                    assessment.readiness ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_BLOCKED_DYNAMIC_VOLUME);
    return 0;
}

static int test_runtime_native_3d_async_bridge_progress_deep_copies_dirty_rect(void) {
    RuntimeNative3DAsyncRenderProgressBuffer* progress =
        RuntimeNative3DAsyncRenderProgressBuffer_Create();
    uint8_t host_buffer[4 * 4 * 4];
    uint8_t copied[2 * 2 * 4];
    RuntimeNative3DAsyncRenderProgressSnapshot snapshot;
    size_t required = 0u;
    RuntimeNative3DAsyncRenderProgressRect rect = {
        .x = 1,
        .y = 1,
        .width = 2,
        .height = 2,
    };

    assert_true("runtime_native_3d_async_bridge_progress_create", progress != NULL);
    if (!progress) return 0;
    for (size_t i = 0; i < sizeof(host_buffer); ++i) {
        host_buffer[i] = (uint8_t)(i + 1u);
    }

    assert_true("runtime_native_3d_async_bridge_progress_publish",
                RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
                    progress,
                    12u,
                    host_buffer,
                    4,
                    4,
                    rect));
    memset(copied, 0, sizeof(copied));
    assert_true("runtime_native_3d_async_bridge_progress_copy",
                RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(progress,
                                                                    12u,
                                                                    &snapshot,
                                                                    copied,
                                                                    sizeof(copied),
                                                                    &required));
    assert_true("runtime_native_3d_async_bridge_progress_snapshot",
                snapshot.valid && !snapshot.staleGeneration &&
                    snapshot.generation == 12u && snapshot.sequence == 1u &&
                    snapshot.byteCount == sizeof(copied) &&
                    required == sizeof(copied));
    assert_true("runtime_native_3d_async_bridge_progress_bytes_row0",
                memcmp(copied, host_buffer + (((size_t)1u * 4u + 1u) * 4u), 8u) == 0);
    assert_true("runtime_native_3d_async_bridge_progress_bytes_row1",
                memcmp(copied + 8u, host_buffer + (((size_t)2u * 4u + 1u) * 4u), 8u) == 0);

    memset(copied, 0, sizeof(copied));
    assert_true("runtime_native_3d_async_bridge_progress_stale_rejects_copy",
                !RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(progress,
                                                                     13u,
                                                                     &snapshot,
                                                                     copied,
                                                                     sizeof(copied),
                                                                     &required) &&
                    snapshot.valid && snapshot.staleGeneration);
    RuntimeNative3DAsyncRenderProgressBuffer_Destroy(progress);
    return 0;
}

int run_test_runtime_native_3d_async_render_bridge_tests(void) {
    test_runtime_native_3d_async_bridge_assesses_static_tlas_ready();
    test_runtime_native_3d_async_bridge_blocks_unsafe_routes();
    test_runtime_native_3d_async_bridge_progress_deep_copies_dirty_rect();
    return test_support_failures();
}
