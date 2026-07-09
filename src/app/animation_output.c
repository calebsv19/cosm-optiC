#include "app/animation_output.h"

#include "app/animation.h"
#include "app/data_paths.h"
#include "config/config_manager.h"
#include "export/render_metrics_dataset.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/ray_tracing2.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool AnimationOutputEnvEnabled(const char *name) {
    const char *v = getenv(name);
    if (!v || !v[0]) return false;
    return strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "TRUE") == 0 ||
           strcmp(v, "yes") == 0 || strcmp(v, "on") == 0;
}

void AnimationExportRenderMetricsDatasetIfEnabled(void) {
    RayTracingRuntimeRoute route;
    if (!AnimationOutputEnvEnabled("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET")) return;

    const char *out_path = getenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH");
    RayTracingRenderMetricsSnapshot snapshot = {0};
    route = RayTracingModeBackend_ResolveRoute();
    snapshot.frames_rendered = frameCounter;
    snapshot.loops_completed = loopCount;
    snapshot.runtime_seconds = currentTime;
    snapshot.scene_object_count = sceneSettings.objectCount;
    snapshot.scene_rays = sceneSettings.rays;
    snapshot.target_fps = animSettings.fps;
    snapshot.frame_duration_seconds = animSettings.frameDuration;
    snapshot.integrator_mode = animSettings.integratorMode;
    snapshot.integrator_mode_3d = route.integratorMode3D;
    snapshot.route_family = route.routeFamily;
    snapshot.integrator_uses_3d_catalog = route.integratorUses3DCatalog;
    snapshot.bounce_limit = animSettings.bounceLimit;
    snapshot.path_samples_per_pixel = animSettings.pathSamplesPerPixel;
    snapshot.path_max_depth = animSettings.pathMaxDepth;
    snapshot.use_tiled_renderer = animSettings.useTiledRenderer;
    snapshot.tile_size = animSettings.tileSize;
    snapshot.light_intensity = animSettings.lightIntensity;
    snapshot.cache_variance_cutoff = animSettings.cacheVarianceCutoff;
    snapshot.cache_halo_radius = animSettings.cacheHaloRadius;
    snapshot.environment_brightness = animSettings.environmentBrightness;
    snapshot.interactive_mode = animSettings.interactiveMode;
    snapshot.deep_render_mode = animSettings.deepRenderMode;
    snapshot.bounce_mode = animSettings.bounceMode;
    snprintf(snapshot.integrator_status_label,
             sizeof(snapshot.integrator_status_label),
             "%s",
             RayTracingModeBackend_IntegratorStatusLabel(&route));

    if (!ray_tracing_render_metrics_dataset_export_json(&snapshot, out_path)) {
        fprintf(stderr, "[render_metrics] failed to export dataset json\n");
        return;
    }

    if (out_path && out_path[0]) {
        printf("[render_metrics] dataset exported: %s\n", out_path);
    } else {
        printf("[render_metrics] dataset exported: data/runtime/render_metrics.dataset.json\n");
    }
}

static void EnsureDirectoryExists(const char* path) {
    if (!path || !path[0]) return;

    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);

    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) {
            return;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return;
    }
}

static bool AnimationResolveFrameOutputPath(int frameNumber,
                                            char* out_path,
                                            size_t out_path_size) {
    char frame_dir[PATH_MAX];
    if (!out_path || out_path_size == 0) return false;
    if (!ray_tracing_resolve_frame_output_dir(animSettings.frameDir, frame_dir, sizeof(frame_dir))) {
        return false;
    }
    return snprintf(out_path,
                    out_path_size,
                    "%s/frame_%04d.bmp",
                    frame_dir,
                    frameNumber) < (int)out_path_size;
}

bool AnimationFrameOutputExists(int frameNumber) {
    char filename[PATH_MAX];
    struct stat st;
    if (!AnimationResolveFrameOutputPath(frameNumber, filename, sizeof(filename))) {
        return false;
    }
    if (stat(filename, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode) && st.st_size > 0;
}

static bool AnimationPrepareFrameOutputPathForWrite(const char* filename) {
    struct stat st;
    if (!filename || !filename[0]) {
        return false;
    }
    if (lstat(filename, &st) != 0) {
        return errno == ENOENT;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "SaveFrame refused non-regular output path: %s\n", filename);
        return false;
    }
    if (unlink(filename) != 0) {
        fprintf(stderr, "SaveFrame failed to replace existing frame %s: %s\n",
                filename,
                strerror(errno));
        return false;
    }
    return true;
}

bool SaveFrame(int frameNumber) {
    char frame_dir[PATH_MAX];
    char filename[PATH_MAX];
    RayTracingRuntimeRoute route;
    if (!ray_tracing_resolve_frame_output_dir(animSettings.frameDir, frame_dir, sizeof(frame_dir)) ||
        !AnimationResolveFrameOutputPath(frameNumber, filename, sizeof(filename))) {
        fprintf(stderr, "SaveFrame failed to resolve frame path.\n");
        return false;
    }
#if USE_VULKAN
    EnsureDirectoryExists(frame_dir);
    if (!AnimationPrepareFrameOutputPathForWrite(filename)) {
        return false;
    }

    route = RayTracingModeBackend_ResolveRoute();
    if (route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D) {
        if (!ExportCurrentNative3DFrameBMP(filename)) {
            fprintf(stderr, "SaveFrame failed to export native 3D frame.\n");
            return false;
        }
        return true;
    }

    VkResult capture_result = vk_renderer_request_capture((VkRenderer*)renderer, filename);
    if (capture_result != VK_SUCCESS) {
        fprintf(stderr, "SaveFrame failed to request capture: %d\n", capture_result);
        return false;
    }
    return true;
#else
    EnsureDirectoryExists(frame_dir);
    if (!AnimationPrepareFrameOutputPathForWrite(filename)) {
        return false;
    }

    printf("Saving frame to: %s\n", filename);

    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;

    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                                0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) {
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }

    if (SDL_SaveBMP(surface, filename) != 0) {
        fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    } else {
        printf("Saved %s\n", filename);
    }

    SDL_FreeSurface(surface);
    return AnimationFrameOutputExists(frameNumber);
#endif
}
