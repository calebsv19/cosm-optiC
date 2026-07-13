#include <stdint.h>
#include <string.h>

#include "app/ray_tracing_deep_render_listener.h"
#include "test_support.h"

typedef struct DeepRenderPresentProbe {
    int calls;
    uint64_t generation;
    uint64_t sequence;
    int width;
    int height;
    uint8_t sampledPixel[4];
} DeepRenderPresentProbe;

static RayTracingDeepRenderSession make_listener_session(uint64_t generation) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderSession_Init(&session);
    session.state = RAY_TRACING_DEEP_RENDER_SESSION_RENDERING;
    session.generation = generation;
    session.frameRequestOwned = true;
    session.frameRequest.valid = true;
    session.frameRequest.preparedFrameOwned = true;
    session.frameRequest.generation = generation;
    return session;
}

static bool listener_present_probe(
    const RayTracingDeepRenderPresentationView* view,
    void* user_data) {
    DeepRenderPresentProbe* probe = (DeepRenderPresentProbe*)user_data;
    size_t offset = 0u;
    if (!view || !view->valid || !view->pixels || !probe) return false;
    probe->calls += 1;
    probe->generation = view->generation;
    probe->sequence = view->sequence;
    probe->width = view->hostWidth;
    probe->height = view->hostHeight;
    offset = (((size_t)1u * (size_t)view->hostWidth) + 1u) * 4u;
    if (offset + 4u <= view->byteCount) {
        memcpy(probe->sampledPixel, view->pixels + offset, 4u);
    }
    return true;
}

static RuntimeNative3DRenderRequestSnapshot make_listener_job_snapshot(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .outputWidth = 4,
        .outputHeight = 3,
        .renderWidth = 4,
        .renderHeight = 3,
        .hostWidth = 4,
        .hostHeight = 3,
        .frameIndex = 0,
        .frameCount = 1,
        .temporalFrames = 1,
        .tileSize = 2,
        .integratorId = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        .preparedFrameBound = true,
        .preparedFrameValid = true,
        .preparedFrameWidth = 4,
        .preparedFrameHeight = 3,
        .sceneAccelerationBound = true,
        .traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS,
        .tlasInstanceCount = 1u,
        .tlasNodeCount = 1u,
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);
    return snapshot;
}

static bool listener_job_success(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    (void)user_data;
    if (!snapshot || !cancel_token || !out_result) return false;
    out_result->completionValue = 17u;
    return true;
}

static bool listener_job_failure(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    (void)snapshot;
    (void)cancel_token;
    (void)user_data;
    (void)out_result;
    return false;
}

static int test_deep_render_listener_applies_matching_progress_and_retains_view(void) {
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderSession session = make_listener_session(21u);
    RuntimeNative3DAsyncRenderProgressBuffer* progress =
        RuntimeNative3DAsyncRenderProgressBuffer_Create();
    RayTracingDeepRenderListenerPollResult result;
    RuntimeNative3DAsyncRenderProgressRect rect = {
        .x = 1,
        .y = 1,
        .width = 2,
        .height = 1,
    };
    uint8_t host_pixels[4 * 3 * 4];
    DeepRenderPresentProbe present = {0};

    RayTracingDeepRenderListener_Init(&listener);
    memset(host_pixels, 0, sizeof(host_pixels));
    host_pixels[((1 * 4 + 1) * 4) + 0] = 10u;
    host_pixels[((1 * 4 + 1) * 4) + 1] = 20u;
    host_pixels[((1 * 4 + 1) * 4) + 2] = 30u;
    host_pixels[((1 * 4 + 1) * 4) + 3] = 255u;

    assert_true("deep_listener_progress_create", progress != NULL);
    if (!progress) return 0;
    assert_true("deep_listener_progress_publish",
                RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
                    progress, 21u, host_pixels, 4, 3, rect));
    assert_true("deep_listener_progress_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  progress,
                                                  NULL,
                                                  listener_present_probe,
                                                  &present,
                                                  &result));
    assert_true("deep_listener_progress_applied",
                result.status == RAY_TRACING_DEEP_RENDER_LISTENER_OK &&
                    result.progressApplied && result.presented &&
                    !result.staleProgressRejected && present.calls == 1 &&
                    present.generation == 21u && present.sequence == 1u &&
                    present.width == 4 && present.height == 3 &&
                    present.sampledPixel[0] == 10u &&
                    present.sampledPixel[1] == 20u &&
                    present.sampledPixel[2] == 30u &&
                    present.sampledPixel[3] == 255u);

    assert_true("deep_listener_retained_view_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  progress,
                                                  NULL,
                                                  listener_present_probe,
                                                  &present,
                                                  &result));
    assert_true("deep_listener_retained_view_presented",
                !result.progressApplied && result.presented && present.calls == 2);

    assert_true("deep_listener_stale_progress_publish",
                RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
                    progress, 20u, host_pixels, 4, 3, rect));
    assert_true("deep_listener_stale_progress_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  progress,
                                                  NULL,
                                                  listener_present_probe,
                                                  &present,
                                                  &result));
    assert_true("deep_listener_stale_progress_rejected",
                result.staleProgressRejected && !result.progressApplied &&
                    result.presented && present.calls == 3 &&
                    present.generation == 21u);

    session.generation = 22u;
    session.frameRequest.generation = 22u;
    assert_true("deep_listener_generation_switch_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  progress,
                                                  NULL,
                                                  listener_present_probe,
                                                  &present,
                                                  &result));
    assert_true("deep_listener_generation_switch_clears_old_view",
                result.staleProgressRejected && !result.presented &&
                    present.calls == 3 && listener.generation == 22u &&
                    !listener.displayValid);

    RayTracingDeepRenderListener_Destroy(&listener);
    RuntimeNative3DAsyncRenderProgressBuffer_Destroy(progress);
    return 0;
}

static int test_deep_render_listener_reports_terminal_without_session_mutation(void) {
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderSession session = make_listener_session(31u);
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJobStartDesc start = {
        .snapshot = make_listener_job_snapshot(),
        .generation = 31u,
        .run_fn = listener_job_success,
    };
    RayTracingDeepRenderListenerPollResult result;

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_listener_terminal_job_create", job != NULL);
    if (!job) return 0;
    assert_true("deep_listener_terminal_job_start",
                RuntimeNative3DAsyncRenderJob_Start(job, &start));
    assert_true("deep_listener_terminal_fixture_join",
                RuntimeNative3DAsyncRenderJob_Join(job));
    assert_true("deep_listener_terminal_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &result));
    assert_true("deep_listener_terminal_published",
                result.terminalObserved &&
                    result.publishStatus ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED &&
                    result.jobResult.valid && result.jobResult.published &&
                    result.jobResult.generation == 31u &&
                    result.jobResult.completionValue == 17u);
    assert_true("deep_listener_terminal_does_not_advance_session",
                session.state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
                    session.generation == 31u && session.frameRequestOwned);
    assert_true("deep_listener_terminal_publish_once_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &result));
    assert_true("deep_listener_terminal_publish_once",
                !result.terminalObserved &&
                    result.publishStatus ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_ALREADY_PUBLISHED);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    return 0;
}

static int test_deep_render_listener_rejects_stale_terminal_and_reports_failure(void) {
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderSession session = make_listener_session(42u);
    RuntimeNative3DAsyncRenderJob* stale_job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJob* failed_job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJobStartDesc stale_start = {
        .snapshot = make_listener_job_snapshot(),
        .generation = 41u,
        .run_fn = listener_job_success,
    };
    RuntimeNative3DAsyncRenderJobStartDesc failed_start = {
        .snapshot = make_listener_job_snapshot(),
        .generation = 42u,
        .run_fn = listener_job_failure,
    };
    RayTracingDeepRenderListenerPollResult result;

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_listener_terminal_jobs_create", stale_job && failed_job);
    if (!stale_job || !failed_job) {
        RuntimeNative3DAsyncRenderJob_Destroy(stale_job);
        RuntimeNative3DAsyncRenderJob_Destroy(failed_job);
        return 0;
    }
    assert_true("deep_listener_stale_job_start",
                RuntimeNative3DAsyncRenderJob_Start(stale_job, &stale_start));
    assert_true("deep_listener_stale_job_join",
                RuntimeNative3DAsyncRenderJob_Join(stale_job));
    assert_true("deep_listener_stale_terminal_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  stale_job,
                                                  NULL,
                                                  NULL,
                                                  &result));
    assert_true("deep_listener_stale_terminal_rejected",
                result.terminalObserved &&
                    result.publishStatus ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION &&
                    result.jobResult.staleGeneration &&
                    !result.jobResult.published);

    assert_true("deep_listener_failed_job_start",
                RuntimeNative3DAsyncRenderJob_Start(failed_job, &failed_start));
    assert_true("deep_listener_failed_job_join",
                RuntimeNative3DAsyncRenderJob_Join(failed_job));
    assert_true("deep_listener_failed_terminal_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  failed_job,
                                                  NULL,
                                                  NULL,
                                                  &result));
    assert_true("deep_listener_failed_terminal_reported",
                result.terminalObserved &&
                    result.publishStatus ==
                        RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED &&
                    result.jobStatus == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED);

    RuntimeNative3DAsyncRenderJob_Destroy(stale_job);
    RuntimeNative3DAsyncRenderJob_Destroy(failed_job);
    RayTracingDeepRenderListener_Destroy(&listener);
    return 0;
}

int run_test_ray_tracing_deep_render_listener_tests(void) {
    test_deep_render_listener_applies_matching_progress_and_retains_view();
    test_deep_render_listener_reports_terminal_without_session_mutation();
    test_deep_render_listener_rejects_stale_terminal_and_reports_failure();
    return test_support_failures();
}
