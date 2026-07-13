#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_file_io.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

int ray_tracing_headless_validate_render_output_root(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight) {
    if (!request || !preflight) return 2;
    if (request->output_root[0]) return 0;
    memset(preflight, 0, sizeof(*preflight));
    preflight->scene_acceleration_stats =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    preflight->request_loaded = true;
    snprintf(preflight->diagnostics,
             sizeof(preflight->diagnostics),
             "output.root required for render");
    return 8;
}

int ray_tracing_headless_prepare_frame_directory_and_buffer(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    uint8_t **out_pixels) {
    if (!request || !preflight || !out_pixels) return 2;
    *out_pixels = NULL;
    if (snprintf(preflight->frame_dir,
                 sizeof(preflight->frame_dir),
                 "%s/frames",
                 request->output_root) >= (int)sizeof(preflight->frame_dir)) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "frame directory path too long");
        return 8;
    }
    if (!config_io_ensure_directory_exists(preflight->frame_dir)) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "failed to create frame directory");
        return 8;
    }
    *out_pixels = (uint8_t *)calloc((size_t)request->width * (size_t)request->height,
                                    (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    if (!*out_pixels) {
        snprintf(preflight->diagnostics,
                 sizeof(preflight->diagnostics),
                 "failed to allocate frame buffer");
        return 8;
    }
    return 0;
}

void ray_tracing_headless_initial_light_point(double *out_x, double *out_y) {
    if (out_x) *out_x = 0.0;
    if (out_y) *out_y = 0.0;
    if (sceneSettings.bezierPath.numPoints >= 1) {
        if (out_x) *out_x = sceneSettings.bezierPath.points[0].x;
        if (out_y) *out_y = sceneSettings.bezierPath.points[0].y;
    }
}

void ray_tracing_headless_reset_render_trace_state(void) {
    RuntimeTriangleBVH3D_ResetTraceStats();
    RuntimeSceneAcceleration3D_ResetTraceStats();
    RuntimeRay3D_ResetRouteStats();
    RuntimeRenderTraceCostLedger3D_SetEnabledFromEnvironment();
    RuntimeRenderTraceCostLedger3D_Reset();
}
