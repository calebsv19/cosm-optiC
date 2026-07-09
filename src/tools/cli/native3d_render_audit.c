#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_resolution.h"

static const char* route_family_name(RayTracingRouteFamily family) {
    switch (family) {
        case RAY_TRACING_ROUTE_CANONICAL_2D:
            return "canonical_2d";
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            return "compat_3d_fallback";
        case RAY_TRACING_ROUTE_NATIVE_3D:
            return "native_3d";
        default:
            return "unknown";
    }
}

static size_t count_nonzero_pixels(const uint8_t* pixels,
                                   int width,
                                   int height,
                                   uint8_t* out_max_r,
                                   uint8_t* out_max_g,
                                   uint8_t* out_max_b,
                                   int* out_first_x,
                                   int* out_first_y) {
    size_t nonzero = 0u;
    uint8_t max_r = 0u;
    uint8_t max_g = 0u;
    uint8_t max_b = 0u;
    int first_x = -1;
    int first_y = -1;

    if (!pixels || width <= 0 || height <= 0) {
        return 0u;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t base =
                ((size_t)y * (size_t)width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const uint8_t r = pixels[base];
            const uint8_t g = pixels[base + 1u];
            const uint8_t b = pixels[base + 2u];
            if (r > max_r) max_r = r;
            if (g > max_g) max_g = g;
            if (b > max_b) max_b = b;
            if (r > 0u || g > 0u || b > 0u) {
                nonzero += 1u;
                if (first_x < 0) {
                    first_x = x;
                    first_y = y;
                }
            }
        }
    }

    if (out_max_r) *out_max_r = max_r;
    if (out_max_g) *out_max_g = max_g;
    if (out_max_b) *out_max_b = max_b;
    if (out_first_x) *out_first_x = first_x;
    if (out_first_y) *out_first_y = first_y;
    return nonzero;
}

int main(void) {
    RayTracingRuntimeRoute route;
    RuntimeNative3DRenderStats stats = {0};
    Point light_point = {0.0, 0.0};
    int render_width = 0;
    int render_height = 0;
    uint8_t* pixels = NULL;
    uint8_t max_r = 0u;
    uint8_t max_g = 0u;
    uint8_t max_b = 0u;
    int first_x = -1;
    int first_y = -1;
    size_t nonzero_pixels = 0u;
    bool apply_ok = false;
    bool render_ok = false;

    LoadAllSettings();
    apply_ok = AnimationApplyActiveSceneSource();
    route = RayTracingModeBackend_ResolveRoute();

    if (sceneSettings.bezierPath.numPoints >= 1) {
        light_point = sceneSettings.bezierPath.points[0];
    }

    printf("native3d_audit:\n");
    printf("  scene_source=%d\n", animation_config_scene_source_clamp(animSettings.sceneSource));
    printf("  runtime_scene_path=%s\n", animSettings.runtimeScenePath[0] ? animSettings.runtimeScenePath : "(empty)");
    printf("  fluid_manifest=%s\n", animSettings.fluidManifest[0] ? animSettings.fluidManifest : "(empty)");
    printf("  apply_active_scene_source=%s\n", apply_ok ? "true" : "false");
    printf("  space_mode=%d\n", animation_config_space_mode_clamp(animSettings.spaceMode));
    printf("  integrator3d=%d\n", animSettings.integratorMode3D);
    printf("  route_family=%s\n", route_family_name(route.routeFamily));
    printf("  route_integrator3d=%d\n", route.integratorMode3D);
    printf("  route_tile_preview=%s\n", route.tilePreviewEnabled ? "true" : "false");
    printf("  route_scaffold_primitive_count=%d\n", route.scaffoldPrimitiveCount);
    printf("  scene_object_count=%d\n", sceneSettings.objectCount);
    printf("  camera=(%.3f, %.3f, %.3f) zoom=%.3f rot=%.3f\n",
           sceneSettings.camera.x,
           sceneSettings.camera.y,
           sceneSettings.cameraZ,
           sceneSettings.camera.zoom,
           sceneSettings.camera.rotation);
    printf("  light=(%.3f, %.3f, %.3f) intensity=%.3f decay=%.3f falloff=%d\n",
           light_point.x,
           light_point.y,
           animSettings.lightHeight,
           animSettings.lightIntensity,
           animSettings.forwardDecay,
           animSettings.forwardFalloffMode);

    if (!RayTracingModeBackend_IsNative3D(&route)) {
        printf("  native_route_ready=false\n");
        return 2;
    }

    if (!RuntimeNative3DResolveScaledDimensions(sceneSettings.windowWidth,
                                                sceneSettings.windowHeight,
                                                animSettings.renderScale3D,
                                                &render_width,
                                                &render_height)) {
        fprintf(stderr, "native3d_audit: failed to resolve scaled dimensions\n");
        return 3;
    }

    pixels = (uint8_t*)calloc((size_t)render_width * (size_t)render_height,
                              (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    if (!pixels) {
        fprintf(stderr, "native3d_audit: failed to allocate pixel buffer\n");
        return 4;
    }

    render_ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        pixels,
        route.integratorMode3D,
        render_width,
        render_height,
        0.0,
        light_point.x,
        light_point.y,
        NULL,
        animSettings.temporalFrames3D,
        &stats);

    printf("  render_width=%d\n", render_width);
    printf("  render_height=%d\n", render_height);
    printf("  render_ok=%s\n", render_ok ? "true" : "false");
    printf("  stats.hit_pixels=%d\n", stats.hitPixelCount);
    printf("  stats.visible_pixels=%d\n", stats.visiblePixelCount);
    printf("  stats.secondary_rays=%d\n", stats.secondaryRayCount);
    printf("  stats.secondary_hits=%d\n", stats.secondaryHitCount);
    printf("  stats.max_radiance=%.9f\n", stats.maxRadiance);
    printf("  stats.max_bounce_radiance=%.9f\n", stats.maxBounceRadiance);
    printf("  stats.total_bounce_radiance=%.9f\n", stats.totalBounceRadiance);

    nonzero_pixels = count_nonzero_pixels(pixels,
                                          render_width,
                                          render_height,
                                          &max_r,
                                          &max_g,
                                          &max_b,
                                          &first_x,
                                          &first_y);
    printf("  nonzero_pixels=%zu\n", nonzero_pixels);
    printf("  first_nonzero=(%d,%d)\n", first_x, first_y);
    printf("  max_rgb=(%u,%u,%u)\n", (unsigned)max_r, (unsigned)max_g, (unsigned)max_b);

    free(pixels);
    return render_ok ? 0 : 5;
}
