#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

#include "app/ray_tracing_deep_render_cancellation.h"
#include "test_support.h"

typedef struct DeepRenderCancellationProbe {
    atomic_bool entered;
    atomic_bool release;
    atomic_bool sawCancel;
} DeepRenderCancellationProbe;

static RuntimeNative3DRenderRequestSnapshot make_cancellation_snapshot(
    int frame_index,
    int frame_count) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .outputWidth = 2,
        .outputHeight = 2,
        .renderWidth = 2,
        .renderHeight = 2,
        .hostWidth = 2,
        .hostHeight = 2,
        .frameIndex = frame_index,
        .frameCount = frame_count,
        .temporalFrames = 1,
        .tileSize = 2,
        .integratorId = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        .preparedFrameBound = true,
        .preparedFrameValid = true,
        .preparedFrameWidth = 2,
        .preparedFrameHeight = 2,
        .sceneAccelerationBound = true,
        .traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS,
        .tlasInstanceCount = 1u,
        .tlasNodeCount = 1u,
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);
    return snapshot;
}

static bool make_cancellation_session(RayTracingDeepRenderSession* session,
                                      uint64_t generation,
                                      bool adopt_request) {
    RayTracingDeepRenderSessionDesc desc = {
        .startFrameIndex = 4,
        .frameCount = 3,
        .initialGeneration = generation,
    };
    RayTracingDeepRenderFrameRequest request;
    if (!session) return false;
    RayTracingDeepRenderSession_Init(session);
    if (!RayTracingDeepRenderSession_Begin(session, &desc)) return false;
    if (!adopt_request) return true;
    RayTracingDeepRenderFrameRequest_Init(&request);
    request.valid = true;
    request.preparedFrameOwned = true;
    request.generation = generation;
    request.localFrameIndex = 0;
    request.absoluteFrameIndex = 4;
    request.frameCount = 3;
    request.renderSnapshot = make_cancellation_snapshot(4, 3);
    snprintf(request.outputRoot, sizeof(request.outputRoot), "/tmp/deep-s10e");
    snprintf(request.frameDirectory,
             sizeof(request.frameDirectory),
             "/tmp/deep-s10e/frames");
    snprintf(request.finalFramePath,
             sizeof(request.finalFramePath),
             "/tmp/deep-s10e/frames/frame_0004.bmp");
    return RayTracingDeepRenderSession_AdoptFrameRequest(session, &request);
}

static bool cancellation_worker_wait(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    DeepRenderCancellationProbe* probe =
        (DeepRenderCancellationProbe*)user_data;
    if (!snapshot || !cancel_token || !probe || !out_result) return false;
    atomic_store_explicit(&probe->entered, true, memory_order_release);
    while (!RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel_token)) {
        usleep(1000);
    }
    atomic_store_explicit(&probe->sawCancel, true, memory_order_release);
    while (!atomic_load_explicit(&probe->release, memory_order_acquire)) {
        usleep(1000);
    }
    out_result->cancelRequested = true;
    out_result->canceled = true;
    return false;
}

static bool cancellation_worker_success(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    (void)user_data;
    if (!snapshot || !cancel_token || !out_result) return false;
    out_result->completionValue = 1u;
    return true;
}

static bool cancellation_wait_for_status(
    RuntimeNative3DAsyncRenderJob* job,
    RuntimeNative3DAsyncRenderJobStatus expected) {
    int spins = 0;
    while (job && RuntimeNative3DAsyncRenderJob_GetStatus(job) != expected &&
           spins < 10000) {
        usleep(1000);
        spins += 1;
    }
    return job && RuntimeNative3DAsyncRenderJob_GetStatus(job) == expected;
}

static bool cancellation_start_job(RuntimeNative3DAsyncRenderJob* job,
                                   uint64_t generation,
                                   RuntimeNative3DAsyncRenderJobRunFn run_fn,
                                   void* user_data) {
    RuntimeNative3DAsyncRenderJobStartDesc desc = {
        .snapshot = make_cancellation_snapshot(4, 3),
        .generation = generation,
        .run_fn = run_fn,
        .user_data = user_data,
    };
    return RuntimeNative3DAsyncRenderJob_Start(job, &desc);
}

static int test_deep_render_cancellation_waits_without_blocking_then_reaps(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderCancellationResult result;
    RayTracingDeepRenderListenerPollResult poll;
    RayTracingDeepRenderListenerPollResult false_terminal = {
        .status = RAY_TRACING_DEEP_RENDER_LISTENER_OK,
        .generation = 60u,
        .terminalObserved = true,
        .jobStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED,
        .publishStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED,
        .jobResult = {
            .valid = true,
            .canceled = true,
            .cancelRequested = true,
            .generation = 60u,
        },
    };
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    DeepRenderCancellationProbe probe;

    atomic_init(&probe.entered, false);
    atomic_init(&probe.release, false);
    atomic_init(&probe.sawCancel, false);
    RayTracingDeepRenderListener_Init(&listener);
    RayTracingDeepRenderCancellation_Init(&cancellation);
    assert_true("deep_cancel_session_create",
                make_cancellation_session(&session, 60u, true));
    assert_true("deep_cancel_job_create", job != NULL);
    if (!job) return 0;
    assert_true("deep_cancel_job_start",
                cancellation_start_job(job, 60u, cancellation_worker_wait, &probe));
    assert_true("deep_cancel_request",
                RayTracingDeepRenderCancellation_Request(&cancellation,
                                                         &session,
                                                         job,
                                                         &result));
    assert_true("deep_cancel_requested_once",
                result.status == RAY_TRACING_DEEP_RENDER_CANCELLATION_REQUESTED &&
                    result.requestIssuedThisCall && result.waitingForTerminal &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING);
    assert_true("deep_cancel_request_idempotent",
                RayTracingDeepRenderCancellation_Request(&cancellation,
                                                         &session,
                                                         job,
                                                         &result) &&
                    !result.requestIssuedThisCall && result.waitingForTerminal);
    assert_true("deep_cancel_running_job_not_joined",
                !RayTracingDeepRenderCancellation_Poll(&cancellation,
                                                       &session,
                                                       job,
                                                       &false_terminal,
                                                       &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_JOB_NOT_TERMINAL &&
                    !result.jobJoinedThisCall && session.frameRequestOwned);
    assert_true("deep_cancel_listener_running_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &poll));
    assert_true("deep_cancel_nonblocking_wait",
                RayTracingDeepRenderCancellation_Poll(&cancellation,
                                                      &session,
                                                      job,
                                                      &poll,
                                                      &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_WAITING_FOR_TERMINAL &&
                    !result.jobJoinedThisCall && session.frameRequestOwned);

    atomic_store_explicit(&probe.release, true, memory_order_release);
    assert_true("deep_cancel_worker_terminal",
                cancellation_wait_for_status(
                    job, RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED));
    assert_true("deep_cancel_listener_terminal_poll",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &poll) &&
                    poll.terminalObserved);
    assert_true("deep_cancel_terminal_reap",
                RayTracingDeepRenderCancellation_Poll(&cancellation,
                                                      &session,
                                                      job,
                                                      &poll,
                                                      &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED &&
                    result.terminalConsumed && result.jobJoinedThisCall &&
                    result.sessionCanceled &&
                    atomic_load_explicit(&probe.sawCancel, memory_order_acquire) &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELED &&
                    !session.frameRequestOwned && session.completedFrameCount == 0);

    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_cancellation_discards_completed_race(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderCancellationResult result;
    RayTracingDeepRenderListenerPollResult poll;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();

    RayTracingDeepRenderListener_Init(&listener);
    RayTracingDeepRenderCancellation_Init(&cancellation);
    assert_true("deep_cancel_race_session",
                make_cancellation_session(&session, 70u, true));
    assert_true("deep_cancel_race_job", job != NULL);
    if (!job) return 0;
    assert_true("deep_cancel_race_start",
                cancellation_start_job(job, 70u, cancellation_worker_success, NULL));
    assert_true("deep_cancel_race_terminal",
                cancellation_wait_for_status(
                    job, RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED));
    assert_true("deep_cancel_race_request",
                RayTracingDeepRenderCancellation_Request(&cancellation,
                                                         &session,
                                                         job,
                                                         &result));
    assert_true("deep_cancel_race_listener",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &poll) &&
                    poll.terminalObserved && poll.jobResult.succeeded);
    assert_true("deep_cancel_race_discarded",
                RayTracingDeepRenderCancellation_Poll(&cancellation,
                                                      &session,
                                                      job,
                                                      &poll,
                                                      &result) &&
                    result.sessionCanceled && result.jobJoinedThisCall &&
                    session.completedFrameCount == 0 &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELED);

    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_cancellation_rejects_stale_terminal(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderCancellationResult result;
    RayTracingDeepRenderListenerPollResult poll;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();

    RayTracingDeepRenderListener_Init(&listener);
    RayTracingDeepRenderCancellation_Init(&cancellation);
    assert_true("deep_cancel_stale_session",
                make_cancellation_session(&session, 80u, true));
    assert_true("deep_cancel_stale_job", job != NULL);
    if (!job) return 0;
    assert_true("deep_cancel_stale_start",
                cancellation_start_job(job, 81u, cancellation_worker_success, NULL));
    assert_true("deep_cancel_stale_terminal",
                cancellation_wait_for_status(
                    job, RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED));
    assert_true("deep_cancel_stale_request",
                RayTracingDeepRenderCancellation_Request(&cancellation,
                                                         &session,
                                                         job,
                                                         &result));
    assert_true("deep_cancel_stale_listener",
                RayTracingDeepRenderListener_Poll(&listener,
                                                  &session,
                                                  NULL,
                                                  job,
                                                  NULL,
                                                  NULL,
                                                  &poll) &&
                    poll.terminalObserved);
    assert_true("deep_cancel_stale_rejected",
                !RayTracingDeepRenderCancellation_Poll(&cancellation,
                                                       &session,
                                                       job,
                                                       &poll,
                                                       &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_GENERATION_MISMATCH &&
                    !result.jobJoinedThisCall && session.frameRequestOwned &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING);
    assert_true("deep_cancel_stale_cleanup_join",
                RuntimeNative3DAsyncRenderJob_Join(job));
    assert_true("deep_cancel_stale_cleanup_session",
                RayTracingDeepRenderSession_MarkCanceled(&session));
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_cancellation_before_dispatch_is_immediate(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderCancellationResult result;

    RayTracingDeepRenderCancellation_Init(&cancellation);
    assert_true("deep_cancel_predispatch_session",
                make_cancellation_session(&session, 90u, false));
    assert_true("deep_cancel_predispatch_request",
                RayTracingDeepRenderCancellation_Request(&cancellation,
                                                         &session,
                                                         NULL,
                                                         &result));
    assert_true("deep_cancel_predispatch_complete",
                result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_SESSION_CANCELED &&
                    result.requestIssuedThisCall && result.terminalConsumed &&
                    result.sessionCanceled &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELED &&
                    !session.frameRequestOwned);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_cancellation_rejects_idle_dispatched_job(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderCancellationResult result;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();

    RayTracingDeepRenderCancellation_Init(&cancellation);
    assert_true("deep_cancel_idle_session",
                make_cancellation_session(&session, 100u, true));
    assert_true("deep_cancel_idle_job", job != NULL);
    if (!job) return 0;
    assert_true("deep_cancel_idle_rejected",
                !RayTracingDeepRenderCancellation_Request(&cancellation,
                                                          &session,
                                                          job,
                                                          &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_CANCELLATION_INVALID_ARGUMENT &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
                    session.frameRequestOwned && !cancellation.requestIssued);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

int run_test_ray_tracing_deep_render_cancellation_tests(void) {
    test_deep_render_cancellation_waits_without_blocking_then_reaps();
    test_deep_render_cancellation_discards_completed_race();
    test_deep_render_cancellation_rejects_stale_terminal();
    test_deep_render_cancellation_before_dispatch_is_immediate();
    test_deep_render_cancellation_rejects_idle_dispatched_job();
    return 0;
}
