#include <string.h>

#include "app/ray_tracing_deep_render_frame_request.h"
#include "app/ray_tracing_deep_render_session.h"
#include "render/runtime_native_3d_async_render_bridge.h"
#include "test_support.h"

static RuntimeNative3DPreparedFrame make_deep_render_prepared_frame(void) {
    RuntimeNative3DPreparedFrame frame;
    memset(&frame, 0, sizeof(frame));
    RuntimeScene3D_Init(&frame.scene);
    frame.width = 160;
    frame.height = 96;
    frame.valid = true;
    return frame;
}

static RuntimeNative3DRenderRequestSnapshot make_deep_render_snapshot(
    int frame_index,
    int frame_count,
    bool dynamic_volume,
    bool dynamic_water) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .outputWidth = 160,
        .outputHeight = 96,
        .renderWidth = 160,
        .renderHeight = 96,
        .hostWidth = 160,
        .hostHeight = 96,
        .frameIndex = frame_index,
        .frameCount = frame_count,
        .temporalFrames = 4,
        .tileSize = 16,
        .integratorId = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
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
        .traceContextCallbackBound = true,
        .volumeFrameSelectionDynamic = dynamic_volume,
        .waterSurfaceFrameSelectionDynamic = dynamic_water,
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);
    return snapshot;
}

static RayTracingDeepRenderFrameRequestDesc make_deep_render_request_desc(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    uint64_t generation,
    int local_frame_index,
    int absolute_frame_index,
    int frame_count,
    const char* output_root,
    const char* frame_directory,
    const char* final_frame_path) {
    static RuntimeCamera3D camera;
    static RuntimeLight3D light;
    RayTracingDeepRenderFrameRequestDesc desc;
    memset(&desc, 0, sizeof(desc));
    memset(&camera, 0, sizeof(camera));
    memset(&light, 0, sizeof(light));
    camera.position = vec3(1.0, 2.0, 3.0);
    camera.zoom = 1.0;
    camera.nearPlane = 0.01;
    light.position = vec3(4.0, 5.0, 6.0);
    light.radius = 0.5;
    light.intensity = 1.0;
    light.falloffDistance = 10.0;
    desc.generation = generation;
    desc.localFrameIndex = local_frame_index;
    desc.absoluteFrameIndex = absolute_frame_index;
    desc.frameCount = frame_count;
    desc.frameDurationSeconds = 1.0 / 30.0;
    desc.animationTimeSeconds = (double)absolute_frame_index / 30.0;
    desc.normalizedT = frame_count > 1 ? (double)local_frame_index / (double)(frame_count - 1) : 1.0;
    desc.camera = &camera;
    desc.light = &light;
    desc.renderSnapshot = snapshot;
    desc.outputRoot = output_root;
    desc.frameDirectory = frame_directory;
    desc.finalFramePath = final_frame_path;
    return desc;
}

static bool build_deep_render_request(RayTracingDeepRenderFrameRequest* request,
                                      uint64_t generation,
                                      int local_frame_index,
                                      int absolute_frame_index,
                                      int frame_count) {
    RuntimeNative3DPreparedFrame frame = make_deep_render_prepared_frame();
    RuntimeNative3DRenderRequestSnapshot snapshot =
        make_deep_render_snapshot(absolute_frame_index, frame_count, false, false);
    RayTracingDeepRenderFrameRequestDesc desc = make_deep_render_request_desc(
        &snapshot,
        generation,
        local_frame_index,
        absolute_frame_index,
        frame_count,
        "/tmp/deep-render",
        "/tmp/deep-render/frames",
        "/tmp/deep-render/frames/frame.bmp");
    RayTracingDeepRenderFrameRequestStatus status;
    frame.traceScene = &frame.scene;
    bool ok = RayTracingDeepRenderFrameRequest_Build(request, &desc, &frame, &status);
    assert_true("deep_request_helper_status",
                ok && status == RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY);
    if (!ok) RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}

static int test_deep_render_frame_request_owns_immutable_inputs(void) {
    char output_root[128] = "/tmp/deep-render";
    char frame_directory[128] = "/tmp/deep-render/frames";
    char final_frame_path[128] = "/tmp/deep-render/frames/frame_0042.bmp";
    RuntimeNative3DPreparedFrame frame = make_deep_render_prepared_frame();
    RuntimeNative3DRenderRequestSnapshot snapshot =
        make_deep_render_snapshot(42, 2, false, false);
    RayTracingDeepRenderFrameRequestDesc desc = make_deep_render_request_desc(
        &snapshot, 100u, 0, 42, 2, output_root, frame_directory, final_frame_path);
    RayTracingDeepRenderFrameRequest request;
    RayTracingDeepRenderFrameRequestStatus status;
    atomic_bool cancel_requested = ATOMIC_VAR_INIT(false);
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {
        .cancelRequested = &cancel_requested,
        .generation = 100u,
    };
    RuntimeNative3DRenderRequestSnapshot dispatch_snapshot;
    RuntimeNative3DAsyncRenderAssessment assessment;

    RayTracingDeepRenderFrameRequest_Init(&request);
    frame.traceScene = &frame.scene;
    assert_true("deep_request_build",
                RayTracingDeepRenderFrameRequest_Build(&request, &desc, &frame, &status));
    assert_true("deep_request_ready",
                status == RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY && request.valid);
    assert_true("deep_request_adopts_frame",
                request.preparedFrameOwned && request.preparedFrame.valid && !frame.valid);
    assert_true("deep_request_rebinds_owned_scene",
                request.preparedFrame.traceScene == &request.preparedFrame.scene);
    output_root[0] = 'X';
    frame_directory[0] = 'Y';
    final_frame_path[0] = 'Z';
    assert_true("deep_request_copies_output_identity",
                strcmp(request.outputRoot, "/tmp/deep-render") == 0 &&
                    strcmp(request.frameDirectory, "/tmp/deep-render/frames") == 0 &&
                    strcmp(request.finalFramePath,
                           "/tmp/deep-render/frames/frame_0042.bmp") == 0);
    assert_true("deep_request_sanitizes_cancel_pointer",
                request.renderSnapshot.generationBound &&
                    request.renderSnapshot.generation == 100u &&
                    !request.renderSnapshot.cancelTokenBound &&
                    request.renderSnapshot.cancelToken.cancelRequested == NULL);
    assert_true("deep_request_dispatch_snapshot",
                RayTracingDeepRenderFrameRequest_BuildDispatchSnapshot(
                    &request, &cancel_token, &dispatch_snapshot, &status));
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&dispatch_snapshot);
    assert_true("deep_request_dispatch_ready",
                status == RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_READY &&
                    assessment.ready && dispatch_snapshot.cancelTokenBound &&
                    dispatch_snapshot.cancelGeneration == 100u);
    RayTracingDeepRenderFrameRequest_Destroy(&request);
    return 0;
}

static int test_deep_render_frame_request_rejects_unowned_dynamic_inputs(void) {
    RuntimeNative3DPreparedFrame volume_frame = make_deep_render_prepared_frame();
    RuntimeNative3DRenderRequestSnapshot volume_snapshot =
        make_deep_render_snapshot(8, 1, true, false);
    RayTracingDeepRenderFrameRequestDesc volume_desc = make_deep_render_request_desc(
        &volume_snapshot, 9u, 0, 8, 1, "/tmp/a", "/tmp/a/frames", "/tmp/a/f.bmp");
    RayTracingDeepRenderFrameRequest request;
    RayTracingDeepRenderFrameRequestStatus status;

    RayTracingDeepRenderFrameRequest_Init(&request);
    assert_true("deep_request_blocks_dynamic_volume",
                !RayTracingDeepRenderFrameRequest_Build(
                    &request, &volume_desc, &volume_frame, &status) &&
                    status == RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_VOLUME_UNOWNED &&
                    volume_frame.valid);
    RuntimeNative3DPreparedFrame_Free(&volume_frame);

    RuntimeNative3DPreparedFrame water_frame = make_deep_render_prepared_frame();
    RuntimeNative3DRenderRequestSnapshot water_snapshot =
        make_deep_render_snapshot(8, 1, false, true);
    RayTracingDeepRenderFrameRequestDesc water_desc = make_deep_render_request_desc(
        &water_snapshot, 10u, 0, 8, 1, "/tmp/a", "/tmp/a/frames", "/tmp/a/f.bmp");
    assert_true("deep_request_blocks_dynamic_water",
                !RayTracingDeepRenderFrameRequest_Build(
                    &request, &water_desc, &water_frame, &status) &&
                    status == RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_WATER_UNOWNED &&
                    water_frame.valid);
    RuntimeNative3DPreparedFrame_Free(&water_frame);
    return 0;
}

static int test_deep_render_session_advances_one_owned_frame_at_a_time(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderSessionDesc desc = {
        .startFrameIndex = 42,
        .frameCount = 2,
        .initialGeneration = 100u,
    };
    RayTracingDeepRenderFrameRequest first;
    RayTracingDeepRenderFrameRequest second;
    RayTracingDeepRenderSessionSnapshot snapshot;

    RayTracingDeepRenderSession_Init(&session);
    assert_true("deep_session_begin", RayTracingDeepRenderSession_Begin(&session, &desc));
    snapshot = RayTracingDeepRenderSession_GetSnapshot(&session);
    assert_true("deep_session_initial_preparing",
                snapshot.state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
                    snapshot.currentAbsoluteFrameIndex == 42 && snapshot.generation == 100u);
    RayTracingDeepRenderFrameRequest_Init(&first);
    if (!build_deep_render_request(&first, 100u, 0, 42, 2)) return 0;
    assert_true("deep_session_adopt_first",
                RayTracingDeepRenderSession_AdoptFrameRequest(&session, &first));
    assert_true("deep_session_first_moved", !first.valid && !first.preparedFrameOwned);
    assert_true("deep_session_rendering_owned",
                RayTracingDeepRenderSession_CurrentRequest(&session) != NULL &&
                    RayTracingDeepRenderSession_GetSnapshot(&session).state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_RENDERING);
    assert_true("deep_session_first_render_done",
                RayTracingDeepRenderSession_MarkRenderSucceeded(&session));
    assert_true("deep_session_first_saved",
                RayTracingDeepRenderSession_MarkFrameSaved(&session));
    snapshot = RayTracingDeepRenderSession_GetSnapshot(&session);
    assert_true("deep_session_advances_exactly_one",
                snapshot.state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
                    snapshot.completedFrameCount == 1 &&
                    snapshot.currentLocalFrameIndex == 1 &&
                    snapshot.currentAbsoluteFrameIndex == 43 &&
                    snapshot.generation == 101u && !snapshot.frameRequestOwned);

    RayTracingDeepRenderFrameRequest_Init(&second);
    if (!build_deep_render_request(&second, 101u, 1, 43, 2)) return 0;
    assert_true("deep_session_adopt_second",
                RayTracingDeepRenderSession_AdoptFrameRequest(&session, &second));
    assert_true("deep_session_second_render_done",
                RayTracingDeepRenderSession_MarkRenderSucceeded(&session));
    assert_true("deep_session_second_saved",
                RayTracingDeepRenderSession_MarkFrameSaved(&session));
    snapshot = RayTracingDeepRenderSession_GetSnapshot(&session);
    assert_true("deep_session_completed",
                snapshot.state == RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED &&
                    snapshot.completedFrameCount == 2 && !snapshot.frameRequestOwned);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_session_rejects_stale_and_cancels_owned_request(void) {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderSessionDesc desc = {
        .startFrameIndex = 5,
        .frameCount = 1,
        .initialGeneration = 20u,
    };
    RayTracingDeepRenderFrameRequest stale;
    RayTracingDeepRenderFrameRequest current;

    RayTracingDeepRenderSession_Init(&session);
    assert_true("deep_session_cancel_begin",
                RayTracingDeepRenderSession_Begin(&session, &desc));
    RayTracingDeepRenderFrameRequest_Init(&stale);
    if (!build_deep_render_request(&stale, 19u, 0, 5, 1)) return 0;
    assert_true("deep_session_rejects_stale_request",
                !RayTracingDeepRenderSession_AdoptFrameRequest(&session, &stale) &&
                    stale.valid && stale.preparedFrameOwned &&
                    RayTracingDeepRenderSession_GetSnapshot(&session).state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_PREPARING);
    RayTracingDeepRenderFrameRequest_Destroy(&stale);
    RayTracingDeepRenderFrameRequest_Init(&current);
    if (!build_deep_render_request(&current, 20u, 0, 5, 1)) return 0;
    assert_true("deep_session_cancel_adopt",
                RayTracingDeepRenderSession_AdoptFrameRequest(&session, &current));
    assert_true("deep_session_cancel_request",
                RayTracingDeepRenderSession_RequestCancel(&session));
    assert_true("deep_session_canceling_retains_request",
                RayTracingDeepRenderSession_CurrentRequest(&session) != NULL &&
                    RayTracingDeepRenderSession_GetSnapshot(&session).state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_CANCELING);
    assert_true("deep_session_cancel_terminal",
                RayTracingDeepRenderSession_MarkCanceled(&session));
    assert_true("deep_session_canceled_releases_request",
                RayTracingDeepRenderSession_CurrentRequest(&session) == NULL &&
                    RayTracingDeepRenderSession_GetSnapshot(&session).state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_CANCELED);
    RayTracingDeepRenderSession_Reset(&session);
    return 0;
}

static int test_deep_render_session_failure_and_generation_exhaustion(void) {
    RayTracingDeepRenderSession failed_session;
    RayTracingDeepRenderSessionDesc failed_desc = {
        .startFrameIndex = 3,
        .frameCount = 1,
        .initialGeneration = 30u,
    };
    RayTracingDeepRenderFrameRequest failed_request;
    RayTracingDeepRenderSessionSnapshot snapshot;

    RayTracingDeepRenderSession_Init(&failed_session);
    assert_true("deep_session_failure_begin",
                RayTracingDeepRenderSession_Begin(&failed_session, &failed_desc));
    assert_true("deep_session_invalid_save_transition",
                !RayTracingDeepRenderSession_MarkFrameSaved(&failed_session) &&
                    RayTracingDeepRenderSession_GetSnapshot(&failed_session).state ==
                        RAY_TRACING_DEEP_RENDER_SESSION_PREPARING);
    RayTracingDeepRenderFrameRequest_Init(&failed_request);
    if (!build_deep_render_request(&failed_request, 30u, 0, 3, 1)) return 0;
    assert_true("deep_session_failure_adopt",
                RayTracingDeepRenderSession_AdoptFrameRequest(
                    &failed_session, &failed_request));
    assert_true("deep_session_mark_render_failed",
                RayTracingDeepRenderSession_MarkFailed(
                    &failed_session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER));
    snapshot = RayTracingDeepRenderSession_GetSnapshot(&failed_session);
    assert_true("deep_session_failed_releases_request",
                snapshot.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    snapshot.failure == RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER &&
                    !snapshot.frameRequestOwned);
    RayTracingDeepRenderSession_Reset(&failed_session);

    RayTracingDeepRenderSession exhausted_session;
    RayTracingDeepRenderSessionDesc exhausted_desc = {
        .startFrameIndex = 10,
        .frameCount = 2,
        .initialGeneration = UINT64_MAX,
    };
    RayTracingDeepRenderFrameRequest exhausted_request;
    RayTracingDeepRenderSession_Init(&exhausted_session);
    assert_true("deep_session_exhaustion_begin",
                RayTracingDeepRenderSession_Begin(&exhausted_session, &exhausted_desc));
    RayTracingDeepRenderFrameRequest_Init(&exhausted_request);
    if (!build_deep_render_request(&exhausted_request, UINT64_MAX, 0, 10, 2)) return 0;
    assert_true("deep_session_exhaustion_adopt",
                RayTracingDeepRenderSession_AdoptFrameRequest(
                    &exhausted_session, &exhausted_request));
    assert_true("deep_session_exhaustion_render_done",
                RayTracingDeepRenderSession_MarkRenderSucceeded(&exhausted_session));
    assert_true("deep_session_exhaustion_blocks_wrap",
                !RayTracingDeepRenderSession_MarkFrameSaved(&exhausted_session));
    snapshot = RayTracingDeepRenderSession_GetSnapshot(&exhausted_session);
    assert_true("deep_session_exhaustion_failed",
                snapshot.state == RAY_TRACING_DEEP_RENDER_SESSION_FAILED &&
                    snapshot.failure ==
                        RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_GENERATION_EXHAUSTED &&
                    snapshot.completedFrameCount == 1 && !snapshot.frameRequestOwned);
    RayTracingDeepRenderSession_Reset(&exhausted_session);
    return 0;
}

int run_test_ray_tracing_deep_render_session_tests(void) {
    test_deep_render_frame_request_owns_immutable_inputs();
    test_deep_render_frame_request_rejects_unowned_dynamic_inputs();
    test_deep_render_session_advances_one_owned_frame_at_a_time();
    test_deep_render_session_rejects_stale_and_cancels_owned_request();
    test_deep_render_session_failure_and_generation_exhaustion();
    return test_support_failures();
}
