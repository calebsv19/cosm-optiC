/* Actual tile scheduler proof after an accepted progress prefix. */
static bool l3_cancel_on_tile_progress(const RuntimeNative3DTileSchedulerProgress *p,void *user) {
    if(p->completedTilesInSubpass>0) atomic_store_explicit((atomic_bool*)user,true,memory_order_release);
    return true;
}
static int test_l3_tile_cancel_after_progress(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeScene3D scene;
    RuntimeNative3DRenderStats stats = {0};
    atomic_bool cancel_requested = ATOMIC_VAR_INIT(false);
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {0};
    RuntimeNative3DTileSchedulerControl scheduler_control = {0};
    uint8_t pixel_buffer[32 * 32 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    memset(pixel_buffer, 0x7c, sizeof(pixel_buffer));
    animSettings.tileSize = 16;
    animSettings.renderScale3D = 1;
    animSettings.disneyDenoiseEnabled = false;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();

    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, -4.0, 1.5);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = -0.1;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 3.0);
    scene.light.radius = 0.2;
    scene.light.intensity = 1.0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 32, 32, &frame.projector);
    assert_true("runtime_native_3d_tiled_cancel_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    frame.scene = scene;
    frame.width = 32;
    frame.height = 32;
    frame.valid = true;
    frame.tileOccupancyConservativeAllTiles = true;
    cancel_token.cancelRequested = &cancel_requested;
    cancel_token.generation = 17u;
    scheduler_control.cancelToken = &cancel_token;

    ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
        pixel_buffer,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        &frame,
        1,
        NULL,
        NULL,
        l3_cancel_on_tile_progress,
        &cancel_requested,
        NULL,
        &scheduler_control,
        &stats);
    assert_true("runtime_native_3d_tiled_cancel_render_reports_false", !ok);
    assert_true("runtime_native_3d_tiled_cancel_token_bound",
                stats.temporalTileSchedulerCancelTokenBound == 1);
    assert_true("runtime_native_3d_tiled_cancel_generation",
                stats.temporalTileSchedulerCancelGeneration == 17u);
    assert_true("l3_cancel_has_accepted_tiles",stats.temporalCompletedTileJobCount>0);
    assert_true("l3_cancel_blocks_final_resolve",stats.temporalFinalFullResolveCount==0);
    assert_true("l3_cancel_after_progress_observed",stats.temporalTileSchedulerCancelRequestedCount>0);
    assert_true("l3_cancel_pool_joined",stats.temporalTileSchedulerWorkerCancelShutdownCount+stats.temporalTileSchedulerWorkerDrainShutdownCount==1);
    RuntimeNative3DPreparedFrame_Free(&frame);
    animSettings = saved_anim;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    return 0;
}
