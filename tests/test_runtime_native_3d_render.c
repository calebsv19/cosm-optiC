#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "render/runtime_native_3d_progress_hud.h"
#include "render/runtime_native_3d_render.h"
#include "test_runtime_native_3d_render.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_support.h"

uint8_t native3d_test_pixel_r(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base];
}

uint8_t native3d_test_pixel_g(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base + 1u];
}

uint8_t native3d_test_pixel_b(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base + 2u];
}

bool native3d_test_pixels_match_rgb_only(const uint8_t* a,
                                         const uint8_t* b,
                                         size_t pixel_count) {
    if (!a || !b) return false;
    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        if (a[base] != b[base] ||
            a[base + 1u] != b[base + 1u] ||
            a[base + 2u] != b[base + 2u]) {
            return false;
        }
    }
    return true;
}

static int test_runtime_native_3d_progress_hud_eta_learns_completed_frame(void) {
    RuntimeNative3DProgressHUDSnapshot snapshot = {0};

    RuntimeNative3DProgressHUD_Reset();
    RuntimeNative3DProgressHUD_BeginFrame(RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 4, 0, 3);
    SDL_Delay(2);
    RuntimeNative3DProgressHUD_UpdateTemporal(1, 1, 4);
    RuntimeNative3DProgressHUD_Snapshot(&snapshot);
    assert_true("runtime_native_3d_progress_hud_subpass_eta_ready", snapshot.estimateReady);
    assert_true("runtime_native_3d_progress_hud_subpass_eta_positive",
                snapshot.estimatedRemainingSeconds > 0.0);
    assert_true("runtime_native_3d_progress_hud_subpass_frame_eta_positive",
                snapshot.estimatedFrameRemainingSeconds > 0.0);
    assert_true("runtime_native_3d_progress_hud_subpass_batch_eta_includes_future_frames",
                snapshot.estimatedBatchRemainingSeconds > snapshot.estimatedFrameRemainingSeconds);
    assert_close("runtime_native_3d_progress_hud_primary_eta_is_frame_eta",
                 snapshot.estimatedRemainingSeconds,
                 snapshot.estimatedFrameRemainingSeconds,
                 1e-9);

    SDL_Delay(2);
    RuntimeNative3DProgressHUD_CompleteFrame();
    RuntimeNative3DProgressHUD_Snapshot(&snapshot);
    assert_true("runtime_native_3d_progress_hud_complete_eta_ready", snapshot.estimateReady);
    assert_true("runtime_native_3d_progress_hud_complete_eta_zero",
                snapshot.estimatedRemainingSeconds == 0.0);
    assert_true("runtime_native_3d_progress_hud_complete_frame_eta_zero",
                snapshot.estimatedFrameRemainingSeconds == 0.0);
    assert_true("runtime_native_3d_progress_hud_complete_batch_eta_zero",
                snapshot.estimatedBatchRemainingSeconds == 0.0);

    RuntimeNative3DProgressHUD_BeginFrame(RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 4, 1, 3);
    RuntimeNative3DProgressHUD_Snapshot(&snapshot);
    assert_true("runtime_native_3d_progress_hud_next_frame_eta_ready", snapshot.estimateReady);
    assert_true("runtime_native_3d_progress_hud_next_frame_eta_positive",
                snapshot.estimatedRemainingSeconds > 0.0);
    assert_true("runtime_native_3d_progress_hud_next_frame_batch_eta_includes_final_frame",
                snapshot.estimatedBatchRemainingSeconds > snapshot.estimatedFrameRemainingSeconds);

    RuntimeNative3DProgressHUD_Reset();
    RuntimeNative3DProgressHUD_Snapshot(&snapshot);
    assert_true("runtime_native_3d_progress_hud_reset_eta_cleared", !snapshot.estimateReady);
    return 0;
}

int run_test_runtime_native_3d_render_tests(void) {
    int before = test_support_failures();

    test_runtime_native_3d_progress_hud_eta_learns_completed_frame();
    run_test_runtime_native_3d_render_live_suite();
    run_test_runtime_native_3d_render_prepared_suite();
    return test_support_failures() - before;
}
