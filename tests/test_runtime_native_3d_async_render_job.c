#include <stdint.h>
#include <unistd.h>

#include "render/runtime_native_3d_async_render_job.h"
#include "test_support.h"

typedef struct AsyncRenderJobProbe {
    volatile bool entered;
    volatile bool release;
    bool saw_generation_bound;
    bool saw_cancel_bound;
    bool saw_cancel_requested;
    uint64_t saw_generation;
    uint64_t completion_value;
} AsyncRenderJobProbe;

static bool async_render_job_probe_success(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    AsyncRenderJobProbe* probe = (AsyncRenderJobProbe*)user_data;
    if (!snapshot || !cancel_token || !probe || !out_result) {
        return false;
    }
    probe->entered = true;
    probe->saw_generation_bound = snapshot->generationBound;
    probe->saw_cancel_bound = snapshot->cancelTokenBound &&
                              snapshot->cancelToken.cancelRequested ==
                                  cancel_token->cancelRequested;
    probe->saw_generation = snapshot->generation;
    out_result->completionValue = probe->completion_value;
    return true;
}

static bool async_render_job_probe_wait_for_cancel(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    AsyncRenderJobProbe* probe = (AsyncRenderJobProbe*)user_data;
    int spins = 0;
    (void)snapshot;
    if (!cancel_token || !cancel_token->cancelRequested || !probe || !out_result) {
        return false;
    }
    probe->entered = true;
    while (!*cancel_token->cancelRequested && spins < 10000) {
        usleep(1000);
        spins++;
    }
    probe->saw_cancel_requested = *cancel_token->cancelRequested;
    out_result->cancelRequested = probe->saw_cancel_requested;
    out_result->canceled = probe->saw_cancel_requested;
    return !probe->saw_cancel_requested;
}

static RuntimeNative3DRenderRequestSnapshot make_async_job_snapshot(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .generationBound = false,
        .generation = 0u,
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
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);
    return snapshot;
}

static int test_async_render_job_publishes_matching_generation_once(void) {
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJobResult result;
    AsyncRenderJobProbe probe = {
        .completion_value = 99u,
    };
    RuntimeNative3DAsyncRenderJobStartDesc desc = {
        .snapshot = make_async_job_snapshot(),
        .generation = 42u,
        .run_fn = async_render_job_probe_success,
        .user_data = &probe,
    };
    RuntimeNative3DAsyncRenderPublishStatus publish_status;

    assert_true("runtime_native_3d_async_job_create", job != NULL);
    if (!job) return 0;
    assert_true("runtime_native_3d_async_job_start",
                RuntimeNative3DAsyncRenderJob_Start(job, &desc));
    assert_true("runtime_native_3d_async_job_join",
                RuntimeNative3DAsyncRenderJob_Join(job));
    assert_true("runtime_native_3d_async_job_status_completed",
                RuntimeNative3DAsyncRenderJob_GetStatus(job) ==
                    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED);
    publish_status = RuntimeNative3DAsyncRenderJob_TryPublish(job, 42u, &result);
    assert_true("runtime_native_3d_async_job_publish_status",
                publish_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED);
    assert_true("runtime_native_3d_async_job_publish_generation",
                result.valid && result.published && result.generation == 42u &&
                    result.snapshot.generationBound && result.snapshot.generation == 42u);
    assert_true("runtime_native_3d_async_job_snapshot_cancel_bound",
                probe.entered && probe.saw_generation_bound && probe.saw_cancel_bound &&
                    probe.saw_generation == 42u &&
                    result.snapshot.cancelTokenBound &&
                    result.snapshot.cancelGeneration == 42u);
    assert_true("runtime_native_3d_async_job_completion_value",
                result.completionValue == 99u);
    publish_status = RuntimeNative3DAsyncRenderJob_TryPublish(job, 42u, &result);
    assert_true("runtime_native_3d_async_job_publish_once",
                publish_status ==
                    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_ALREADY_PUBLISHED);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    return 0;
}

static int test_async_render_job_rejects_stale_generation_publish(void) {
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJobResult result;
    AsyncRenderJobProbe probe = {
        .completion_value = 11u,
    };
    RuntimeNative3DAsyncRenderJobStartDesc desc = {
        .snapshot = make_async_job_snapshot(),
        .generation = 51u,
        .run_fn = async_render_job_probe_success,
        .user_data = &probe,
    };
    RuntimeNative3DAsyncRenderPublishStatus publish_status;

    assert_true("runtime_native_3d_async_job_stale_create", job != NULL);
    if (!job) return 0;
    assert_true("runtime_native_3d_async_job_stale_start",
                RuntimeNative3DAsyncRenderJob_Start(job, &desc));
    assert_true("runtime_native_3d_async_job_stale_join",
                RuntimeNative3DAsyncRenderJob_Join(job));
    publish_status = RuntimeNative3DAsyncRenderJob_TryPublish(job, 52u, &result);
    assert_true("runtime_native_3d_async_job_stale_rejected",
                publish_status ==
                    RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION);
    assert_true("runtime_native_3d_async_job_stale_result",
                result.valid && result.staleGeneration && !result.published &&
                    result.generation == 51u &&
                    result.snapshot.generation == 51u);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    return 0;
}

static int test_async_render_job_shutdown_requests_cancel_before_join(void) {
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJobResult result;
    AsyncRenderJobProbe probe = {0};
    RuntimeNative3DAsyncRenderJobStartDesc desc = {
        .snapshot = make_async_job_snapshot(),
        .generation = 77u,
        .run_fn = async_render_job_probe_wait_for_cancel,
        .user_data = &probe,
    };
    RuntimeNative3DAsyncRenderPublishStatus publish_status;
    int spins = 0;

    assert_true("runtime_native_3d_async_job_cancel_create", job != NULL);
    if (!job) return 0;
    assert_true("runtime_native_3d_async_job_cancel_start",
                RuntimeNative3DAsyncRenderJob_Start(job, &desc));
    while (!probe.entered && spins < 10000) {
        usleep(1000);
        spins++;
    }
    assert_true("runtime_native_3d_async_job_cancel_entered", probe.entered);
    assert_true("runtime_native_3d_async_job_cancel_shutdown",
                RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(job));
    assert_true("runtime_native_3d_async_job_cancel_seen", probe.saw_cancel_requested);
    assert_true("runtime_native_3d_async_job_cancel_status",
                RuntimeNative3DAsyncRenderJob_GetStatus(job) ==
                    RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED);
    publish_status = RuntimeNative3DAsyncRenderJob_TryPublish(job, 77u, &result);
    assert_true("runtime_native_3d_async_job_cancel_not_published",
                publish_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED &&
                    result.canceled && result.cancelRequested && !result.published);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    return 0;
}

int run_test_runtime_native_3d_async_render_job_tests(void) {
    test_async_render_job_publishes_matching_generation_once();
    test_async_render_job_rejects_stale_generation_publish();
    test_async_render_job_shutdown_requests_cancel_before_join();
    return test_support_failures();
}
