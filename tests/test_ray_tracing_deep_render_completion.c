#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/ray_tracing_deep_render_completion.h"
#include "test_support.h"

typedef struct DeepRenderCommitProbe {
    bool writeResult;
    bool verifyResult;
    int writeCalls;
    int verifyCalls;
    char path[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    uint8_t firstPixel[4];
} DeepRenderCommitProbe;

static RuntimeNative3DRenderRequestSnapshot make_completion_snapshot(
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

static bool make_completion_session(RayTracingDeepRenderSession* session,
                                    uint64_t generation,
                                    int start_frame,
                                    int frame_count,
                                    const char* output_root,
                                    const char* frame_directory,
                                    const char* final_path) {
    RayTracingDeepRenderSessionDesc session_desc = {
        .startFrameIndex = start_frame,
        .frameCount = frame_count,
        .initialGeneration = generation,
    };
    RayTracingDeepRenderFrameRequest request;
    if (!session || !output_root || !frame_directory || !final_path) return false;
    RayTracingDeepRenderSession_Init(session);
    if (!RayTracingDeepRenderSession_Begin(session, &session_desc)) return false;
    RayTracingDeepRenderFrameRequest_Init(&request);
    request.valid = true;
    request.preparedFrameOwned = true;
    request.generation = generation;
    request.localFrameIndex = 0;
    request.absoluteFrameIndex = start_frame;
    request.frameCount = frame_count;
    request.renderSnapshot = make_completion_snapshot(start_frame, frame_count);
    snprintf(request.outputRoot, sizeof(request.outputRoot), "%s", output_root);
    snprintf(request.frameDirectory,
             sizeof(request.frameDirectory),
             "%s",
             frame_directory);
    snprintf(request.finalFramePath, sizeof(request.finalFramePath), "%s", final_path);
    return RayTracingDeepRenderSession_AdoptFrameRequest(session, &request);
}

static bool completion_worker_success(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    (void)user_data;
    if (!snapshot || !cancel_token || !out_result) return false;
    out_result->completionValue = 4u;
    return true;
}

static bool completion_worker_failure(
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

static bool completion_wait_for_terminal(RuntimeNative3DAsyncRenderJob* job) {
    int spins = 0;
    if (!job) return false;
    while (RuntimeNative3DAsyncRenderJob_GetStatus(job) ==
               RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING &&
           spins < 10000) {
        usleep(1000);
        spins += 1;
    }
    return RuntimeNative3DAsyncRenderJob_GetStatus(job) !=
           RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING;
}

static bool completion_make_poll(
    RayTracingDeepRenderListener* listener,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    uint64_t job_generation,
    RuntimeNative3DAsyncRenderJobRunFn run_fn,
    RayTracingDeepRenderListenerPollResult* out_poll) {
    RuntimeNative3DAsyncRenderJobStartDesc start = {
        .snapshot = make_completion_snapshot(session->currentAbsoluteFrameIndex,
                                             session->frameCount),
        .generation = job_generation,
        .run_fn = run_fn,
    };
    return RuntimeNative3DAsyncRenderJob_Start(job, &start) &&
           completion_wait_for_terminal(job) &&
           RayTracingDeepRenderListener_Poll(listener,
                                             session,
                                             NULL,
                                             job,
                                             NULL,
                                             NULL,
                                             out_poll);
}

static RayTracingDeepRenderPresentationView make_completion_view(
    uint64_t generation,
    uint8_t* pixels,
    bool complete) {
    RayTracingDeepRenderPresentationView view = {
        .valid = true,
        .generation = generation,
        .sequence = 3u,
        .hostWidth = 2,
        .hostHeight = 2,
        .dirtyRect = {
            .x = 0,
            .y = 0,
            .width = complete ? 2 : 1,
            .height = 2,
        },
        .pixels = pixels,
        .byteCount = 2u * 2u * 4u,
    };
    return view;
}

static bool completion_probe_write(
    const char* path,
    const RayTracingDeepRenderPresentationView* view,
    void* user_data) {
    DeepRenderCommitProbe* probe = (DeepRenderCommitProbe*)user_data;
    if (!path || !view || !view->pixels || !probe) return false;
    probe->writeCalls += 1;
    snprintf(probe->path, sizeof(probe->path), "%s", path);
    memcpy(probe->firstPixel, view->pixels, sizeof(probe->firstPixel));
    return probe->writeResult;
}

static bool completion_probe_verify(const char* path,
                                    int expected_width,
                                    int expected_height,
                                    void* user_data) {
    DeepRenderCommitProbe* probe = (DeepRenderCommitProbe*)user_data;
    if (!path || !probe) return false;
    probe->verifyCalls += 1;
    return probe->verifyResult && expected_width == 2 && expected_height == 2 &&
           strcmp(path, probe->path) == 0;
}

static int test_deep_render_completion_advances_exactly_one_verified_frame(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult poll;
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {10u, 20u, 30u, 255u};
    RayTracingDeepRenderPresentationView view =
        make_completion_view(50u, pixels, true);
    DeepRenderCommitProbe probe = {
        .writeResult = true,
        .verifyResult = true,
    };
    RayTracingDeepRenderCompletionDesc desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &probe,
    };

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_completion_session_create",
                make_completion_session(&session,
                                        50u,
                                        7,
                                        2,
                                        "/tmp/deep-s10d",
                                        "/tmp/deep-s10d/frames",
                                        "/tmp/deep-s10d/frames/frame_0007.bmp"));
    assert_true("deep_completion_job_create", job != NULL);
    if (!job) return 0;
    assert_true("deep_completion_matching_poll",
                completion_make_poll(&listener,
                                     &session,
                                     job,
                                     50u,
                                     completion_worker_success,
                                     &poll));
    assert_true("deep_completion_process",
                RayTracingDeepRenderCompletion_Process(&session,
                                                       job,
                                                       &poll,
                                                       &view,
                                                       &desc,
                                                       &result));
    assert_true("deep_completion_frame_advanced",
                result.status == RAY_TRACING_DEEP_RENDER_COMPLETION_FRAME_ADVANCED &&
                    result.terminalConsumed && result.jobJoined &&
                    result.outputWritten && result.outputVerified &&
                    result.frameAdvanced && !result.sessionCompleted &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
                    session.generation == 51u &&
                    session.currentAbsoluteFrameIndex == 8 &&
                    session.completedFrameCount == 1 &&
                    !session.frameRequestOwned && probe.writeCalls == 1 &&
                    probe.verifyCalls == 1 &&
                    strcmp(probe.path,
                           "/tmp/deep-s10d/frames/frame_0007.bmp") == 0 &&
                    probe.firstPixel[0] == 10u);
    assert_true("deep_completion_duplicate_rejected",
                !RayTracingDeepRenderCompletion_Process(&session,
                                                        job,
                                                        &poll,
                                                        &view,
                                                        &desc,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_INVALID_SESSION &&
                    probe.writeCalls == 1 && probe.verifyCalls == 1);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_completion_finishes_last_frame(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult poll;
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView view =
        make_completion_view(70u, pixels, true);
    DeepRenderCommitProbe probe = {
        .writeResult = true,
        .verifyResult = true,
    };
    RayTracingDeepRenderCompletionDesc desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &probe,
    };

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_completion_final_session_create",
                make_completion_session(&session,
                                        70u,
                                        12,
                                        1,
                                        "/tmp/deep-s10d-final",
                                        "/tmp/deep-s10d-final/frames",
                                        "/tmp/deep-s10d-final/frames/frame_0012.bmp"));
    assert_true("deep_completion_final_job_create", job != NULL);
    if (!job) return 0;
    assert_true("deep_completion_final_poll",
                completion_make_poll(&listener,
                                     &session,
                                     job,
                                     70u,
                                     completion_worker_success,
                                     &poll));
    assert_true("deep_completion_final_process",
                RayTracingDeepRenderCompletion_Process(&session,
                                                       job,
                                                       &poll,
                                                       &view,
                                                       &desc,
                                                       &result));
    assert_true("deep_completion_session_completed",
                result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_SESSION_COMPLETED &&
                    result.sessionCompleted && result.frameAdvanced &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED &&
                    session.completedFrameCount == 1 &&
                    !session.frameRequestOwned);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_completion_rejects_stale_and_incomplete_results(void) {
    RayTracingDeepRenderSession stale_session;
    RayTracingDeepRenderSession incomplete_session;
    RayTracingDeepRenderListener stale_listener;
    RayTracingDeepRenderListener incomplete_listener;
    RuntimeNative3DAsyncRenderJob* stale_job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJob* incomplete_job =
        RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult stale_poll;
    RayTracingDeepRenderListenerPollResult incomplete_poll;
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView complete_view =
        make_completion_view(81u, pixels, true);
    RayTracingDeepRenderPresentationView incomplete_view =
        make_completion_view(90u, pixels, false);
    DeepRenderCommitProbe probe = {
        .writeResult = true,
        .verifyResult = true,
    };
    RayTracingDeepRenderCompletionDesc desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &probe,
    };

    RayTracingDeepRenderListener_Init(&stale_listener);
    RayTracingDeepRenderListener_Init(&incomplete_listener);
    assert_true("deep_completion_stale_session_create",
                make_completion_session(&stale_session,
                                        81u,
                                        1,
                                        1,
                                        "/tmp/deep-stale",
                                        "/tmp/deep-stale/frames",
                                        "/tmp/deep-stale/frames/frame.bmp"));
    assert_true("deep_completion_incomplete_session_create",
                make_completion_session(&incomplete_session,
                                        90u,
                                        2,
                                        1,
                                        "/tmp/deep-incomplete",
                                        "/tmp/deep-incomplete/frames",
                                        "/tmp/deep-incomplete/frames/frame.bmp"));
    assert_true("deep_completion_reject_jobs_create", stale_job && incomplete_job);
    if (!stale_job || !incomplete_job) return 0;
    assert_true("deep_completion_stale_poll",
                completion_make_poll(&stale_listener,
                                     &stale_session,
                                     stale_job,
                                     80u,
                                     completion_worker_success,
                                     &stale_poll));
    assert_true("deep_completion_stale_rejected",
                !RayTracingDeepRenderCompletion_Process(&stale_session,
                                                        stale_job,
                                                        &stale_poll,
                                                        &complete_view,
                                                        &desc,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_STALE_TERMINAL &&
                    result.jobJoined && probe.writeCalls == 0 &&
                    stale_session.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    stale_session.failure ==
                        RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_REQUEST_MISMATCH);
    assert_true("deep_completion_incomplete_poll",
                completion_make_poll(&incomplete_listener,
                                     &incomplete_session,
                                     incomplete_job,
                                     90u,
                                     completion_worker_success,
                                     &incomplete_poll));
    assert_true("deep_completion_incomplete_rejected",
                !RayTracingDeepRenderCompletion_Process(&incomplete_session,
                                                        incomplete_job,
                                                        &incomplete_poll,
                                                        &incomplete_view,
                                                        &desc,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_FINAL_IMAGE_INCOMPLETE &&
                    result.jobJoined && probe.writeCalls == 0 &&
                    incomplete_session.state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    incomplete_session.failure ==
                        RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
    RuntimeNative3DAsyncRenderJob_Destroy(stale_job);
    RuntimeNative3DAsyncRenderJob_Destroy(incomplete_job);
    RayTracingDeepRenderListener_Destroy(&stale_listener);
    RayTracingDeepRenderListener_Destroy(&incomplete_listener);
    RayTracingDeepRenderSession_Reset(&stale_session);
    RayTracingDeepRenderSession_Reset(&incomplete_session);
    return 0;
}

static int test_deep_render_completion_save_and_verify_failures_do_not_advance(void) {
    RayTracingDeepRenderSession save_session;
    RayTracingDeepRenderSession verify_session;
    RayTracingDeepRenderListener save_listener;
    RayTracingDeepRenderListener verify_listener;
    RuntimeNative3DAsyncRenderJob* save_job = RuntimeNative3DAsyncRenderJob_Create();
    RuntimeNative3DAsyncRenderJob* verify_job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult save_poll;
    RayTracingDeepRenderListenerPollResult verify_poll;
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView save_view =
        make_completion_view(100u, pixels, true);
    RayTracingDeepRenderPresentationView verify_view =
        make_completion_view(110u, pixels, true);
    DeepRenderCommitProbe save_probe = {
        .writeResult = false,
        .verifyResult = true,
    };
    DeepRenderCommitProbe verify_probe = {
        .writeResult = true,
        .verifyResult = false,
    };
    RayTracingDeepRenderCompletionDesc save_desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &save_probe,
    };
    RayTracingDeepRenderCompletionDesc verify_desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &verify_probe,
    };

    RayTracingDeepRenderListener_Init(&save_listener);
    RayTracingDeepRenderListener_Init(&verify_listener);
    assert_true("deep_completion_save_session_create",
                make_completion_session(&save_session,
                                        100u,
                                        4,
                                        2,
                                        "/tmp/deep-save",
                                        "/tmp/deep-save/frames",
                                        "/tmp/deep-save/frames/frame.bmp"));
    assert_true("deep_completion_verify_session_create",
                make_completion_session(&verify_session,
                                        110u,
                                        5,
                                        2,
                                        "/tmp/deep-verify",
                                        "/tmp/deep-verify/frames",
                                        "/tmp/deep-verify/frames/frame.bmp"));
    assert_true("deep_completion_failure_jobs_create", save_job && verify_job);
    if (!save_job || !verify_job) return 0;
    assert_true("deep_completion_save_poll",
                completion_make_poll(&save_listener,
                                     &save_session,
                                     save_job,
                                     100u,
                                     completion_worker_success,
                                     &save_poll));
    assert_true("deep_completion_save_failure",
                !RayTracingDeepRenderCompletion_Process(&save_session,
                                                        save_job,
                                                        &save_poll,
                                                        &save_view,
                                                        &save_desc,
                                                        &result) &&
                    result.status == RAY_TRACING_DEEP_RENDER_COMPLETION_SAVE_FAILED &&
                    result.writeAttempted && !result.outputWritten &&
                    save_probe.writeCalls == 1 && save_probe.verifyCalls == 0 &&
                    save_session.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    save_session.completedFrameCount == 0);
    assert_true("deep_completion_verify_poll",
                completion_make_poll(&verify_listener,
                                     &verify_session,
                                     verify_job,
                                     110u,
                                     completion_worker_success,
                                     &verify_poll));
    assert_true("deep_completion_verify_failure",
                !RayTracingDeepRenderCompletion_Process(&verify_session,
                                                        verify_job,
                                                        &verify_poll,
                                                        &verify_view,
                                                        &verify_desc,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_VERIFY_FAILED &&
                    result.outputWritten && !result.outputVerified &&
                    verify_probe.writeCalls == 1 && verify_probe.verifyCalls == 1 &&
                    verify_session.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    verify_session.completedFrameCount == 0);
    RuntimeNative3DAsyncRenderJob_Destroy(save_job);
    RuntimeNative3DAsyncRenderJob_Destroy(verify_job);
    RayTracingDeepRenderListener_Destroy(&save_listener);
    RayTracingDeepRenderListener_Destroy(&verify_listener);
    RayTracingDeepRenderSession_Reset(&save_session);
    RayTracingDeepRenderSession_Reset(&verify_session);
    return 0;
}

static int test_deep_render_completion_rejects_output_outside_frame_directory(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RuntimeNative3DAsyncRenderJob* job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult poll;
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView view =
        make_completion_view(115u, pixels, true);
    DeepRenderCommitProbe probe = {
        .writeResult = true,
        .verifyResult = true,
    };
    RayTracingDeepRenderCompletionDesc desc = {
        .write_fn = completion_probe_write,
        .verify_fn = completion_probe_verify,
        .user_data = &probe,
    };

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_completion_output_identity_session_create",
                make_completion_session(&session,
                                        115u,
                                        5,
                                        1,
                                        "/tmp/deep-output-root",
                                        "/tmp/deep-output-root/frames",
                                        "/tmp/deep-output-other/frame.bmp"));
    assert_true("deep_completion_output_identity_job_create", job != NULL);
    if (!job) return 0;
    assert_true("deep_completion_output_identity_poll",
                completion_make_poll(&listener,
                                     &session,
                                     job,
                                     115u,
                                     completion_worker_success,
                                     &poll));
    assert_true("deep_completion_output_identity_rejected",
                !RayTracingDeepRenderCompletion_Process(&session,
                                                        job,
                                                        &poll,
                                                        &view,
                                                        &desc,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_OUTPUT_IDENTITY_INVALID &&
                    result.jobJoined && probe.writeCalls == 0 &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    session.failure == RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_SAVE);
    RuntimeNative3DAsyncRenderJob_Destroy(job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_completion_reports_render_failure_and_defers_cancel(void) {
    RayTracingDeepRenderSession failed_session;
    RayTracingDeepRenderSession canceled_session;
    RayTracingDeepRenderListener listener;
    RuntimeNative3DAsyncRenderJob* failed_job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult failed_poll;
    RayTracingDeepRenderListenerPollResult canceled_poll = {
        .generation = 130u,
        .terminalObserved = true,
        .publishStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED,
        .jobResult = {
            .valid = true,
            .generation = 130u,
            .canceled = true,
            .cancelRequested = true,
        },
    };
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView failed_view =
        make_completion_view(120u, pixels, true);
    RayTracingDeepRenderPresentationView canceled_view =
        make_completion_view(130u, pixels, true);

    RayTracingDeepRenderListener_Init(&listener);
    assert_true("deep_completion_failed_session_create",
                make_completion_session(&failed_session,
                                        120u,
                                        6,
                                        1,
                                        "/tmp/deep-failed",
                                        "/tmp/deep-failed/frames",
                                        "/tmp/deep-failed/frames/frame.bmp"));
    assert_true("deep_completion_canceled_session_create",
                make_completion_session(&canceled_session,
                                        130u,
                                        7,
                                        1,
                                        "/tmp/deep-canceled",
                                        "/tmp/deep-canceled/frames",
                                        "/tmp/deep-canceled/frames/frame.bmp"));
    assert_true("deep_completion_failed_job_create", failed_job != NULL);
    if (!failed_job) return 0;
    assert_true("deep_completion_failed_poll",
                completion_make_poll(&listener,
                                     &failed_session,
                                     failed_job,
                                     120u,
                                     completion_worker_failure,
                                     &failed_poll));
    assert_true("deep_completion_render_failure",
                !RayTracingDeepRenderCompletion_Process(&failed_session,
                                                        failed_job,
                                                        &failed_poll,
                                                        &failed_view,
                                                        NULL,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_RENDER_FAILED &&
                    result.jobJoined &&
                    failed_session.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED);
    assert_true("deep_completion_cancel_deferred",
                !RayTracingDeepRenderCompletion_Process(&canceled_session,
                                                        NULL,
                                                        &canceled_poll,
                                                        &canceled_view,
                                                        NULL,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_CANCELED_TERMINAL &&
                    !result.jobJoined &&
                    canceled_session.state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
                    canceled_session.frameRequestOwned);
    RuntimeNative3DAsyncRenderJob_Destroy(failed_job);
    RayTracingDeepRenderListener_Destroy(&listener);
    RayTracingDeepRenderSession_Reset(&failed_session);
    RayTracingDeepRenderSession_Reset(&canceled_session);
    return 0;
}

static int test_deep_render_completion_never_joins_a_running_or_idle_job(void) {
    RayTracingDeepRenderSession session;
    RuntimeNative3DAsyncRenderJob* idle_job = RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderListenerPollResult poll = {
        .generation = 140u,
        .terminalObserved = true,
        .jobStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED,
        .publishStatus = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED,
        .jobResult = {
            .valid = true,
            .succeeded = true,
            .published = true,
            .generation = 140u,
        },
    };
    RayTracingDeepRenderCompletionResult result;
    uint8_t pixels[16] = {0u};
    RayTracingDeepRenderPresentationView view =
        make_completion_view(140u, pixels, true);

    poll.jobResult.snapshot = make_completion_snapshot(8, 1);
    assert_true("deep_completion_idle_session_create",
                make_completion_session(&session,
                                        140u,
                                        8,
                                        1,
                                        "/tmp/deep-idle",
                                        "/tmp/deep-idle/frames",
                                        "/tmp/deep-idle/frames/frame.bmp"));
    assert_true("deep_completion_idle_job_create", idle_job != NULL);
    if (!idle_job) return 0;
    assert_true("deep_completion_idle_job_rejected",
                !RayTracingDeepRenderCompletion_Process(&session,
                                                        idle_job,
                                                        &poll,
                                                        &view,
                                                        NULL,
                                                        &result) &&
                    result.status ==
                        RAY_TRACING_DEEP_RENDER_COMPLETION_JOB_NOT_TERMINAL &&
                    !result.jobJoined &&
                    session.state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING &&
                    session.frameRequestOwned);
    RuntimeNative3DAsyncRenderJob_Destroy(idle_job);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_completion_default_bmp_writer_and_verifier(void) {
    char root_template[] = "/tmp/ray_tracing_s10d_bmp_XXXXXX";
    char* root = mkdtemp(root_template);
    char path[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    char frame_dir[RAY_TRACING_DEEP_RENDER_PATH_MAX];
    uint8_t pixels[16] = {
        1u, 2u, 3u, 255u,
        4u, 5u, 6u, 255u,
        7u, 8u, 9u, 255u,
        10u, 11u, 12u, 255u,
    };
    RayTracingDeepRenderPresentationView view =
        make_completion_view(1u, pixels, true);

    assert_true("deep_completion_bmp_tmp_root", root != NULL);
    if (!root) return 0;
    snprintf(frame_dir, sizeof(frame_dir), "%s/frames", root);
    snprintf(path, sizeof(path), "%s/frame_0001.bmp", frame_dir);
    assert_true("deep_completion_bmp_write",
                RayTracingDeepRenderCompletion_WriteFrameBMP(path, &view, NULL));
    assert_true("deep_completion_bmp_verify",
                RayTracingDeepRenderCompletion_VerifyFrameBMP(path, 2, 2, NULL));
    pixels[0] = 99u;
    assert_true("deep_completion_bmp_atomic_replace",
                RayTracingDeepRenderCompletion_WriteFrameBMP(path, &view, NULL) &&
                    RayTracingDeepRenderCompletion_VerifyFrameBMP(path, 2, 2, NULL));
    assert_true("deep_completion_bmp_wrong_dimensions_rejected",
                !RayTracingDeepRenderCompletion_VerifyFrameBMP(path, 3, 2, NULL));
    unlink(path);
    rmdir(frame_dir);
    rmdir(root);
    return 0;
}

int run_test_ray_tracing_deep_render_completion_tests(void) {
    test_deep_render_completion_advances_exactly_one_verified_frame();
    test_deep_render_completion_finishes_last_frame();
    test_deep_render_completion_rejects_stale_and_incomplete_results();
    test_deep_render_completion_save_and_verify_failures_do_not_advance();
    test_deep_render_completion_rejects_output_outside_frame_directory();
    test_deep_render_completion_reports_render_failure_and_defers_cancel();
    test_deep_render_completion_never_joins_a_running_or_idle_job();
    test_deep_render_completion_default_bmp_writer_and_verifier();
    return test_support_failures();
}
