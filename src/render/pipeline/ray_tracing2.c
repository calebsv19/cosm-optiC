#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "render/fast_rng.h"
#include "render/uniform_grid.h"
#include "render/ray_types.h"
#include "render/integrator_common.h"
#include "render/material_bsdf.h"
#include "render/ray_tracing2_preview.h"
#include "render/surface_mesh.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/integrators/direct_light_integrator.h"
#include "render/irradiance_cache.h"
#include "render/timer_hud_api.h"
#include "render/fluid_overlay.h"
#include "import/fluid_import.h"
#include "core_space.h"
#include "engine/Render/render_pipeline.h"
#include "render/fluid/fluid_state.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include "app/runtime_time.h"
#include "camera/camera.h"
#include "render/space_mode_adapter.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"
#include "geo/shape_asset.h"
#include "geo/shape_adapter.h"
#include "import/shape_import.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include "vk_renderer.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
Uint8* pixelBuffer = NULL;  // Global but uninitialized
static Uint8* tilePreviewBuffer = NULL;
static Uint8* native3DRenderBuffer = NULL;
static Uint8* native3DPreviewBuffer = NULL;
static size_t native3DRenderBufferCapacity = 0u;
static size_t native3DPreviewBufferCapacity = 0u;
float* energyBuffer = NULL;
float* directEnergyBufferCPU = NULL;
static uint32_t s_native3DSampleSequence = 1U;
static TileGrid tileGrid = {0};
static UniformGrid uniformGrid = {0};
static IrradianceCache irradianceCache = {0};
static MaterialBSDF* materialTable = NULL;
static int materialCapacity = 0;
static SurfaceMesh surfaceMesh = {0};
static TriangleMesh triangleMesh = {0};

static float* reflectionForwardBuffer = NULL;
static bool BuildReflectionCache(const IntegratorContext* ctx,
                                 const LightSource* light);
static int BuildMaterialTable(void);
static void RenderHybridTilesPreview(SDL_Renderer* renderer,
                                     IntegratorContext* ctx,
                                     const LightSource* light,
                                     const CameraIntegratorSettings* settings,
                                     double camX,
                                     double camY);
static bool RenderNative3DTilesPreview(SDL_Renderer* renderer,
                                       Uint8* host_buffer,
                                       int host_width,
                                       int host_height,
                                       Uint8* render_buffer,
                                       int render_width,
                                       int render_height,
                                       TileGrid* grid,
                                       RayTracing3DIntegratorId integrator_id,
                                       double normalized_t,
                                       double light_x,
                                       double light_y,
                                       const RuntimeNative3DSamplingContext* sampling,
                                       bool present_progress,
                                       RuntimeNative3DRenderStats* out_stats);
static bool BuildNative3DLiveSceneForOverlay(RuntimeScene3D* out_scene,
                                             double normalized_t,
                                             double light_x,
                                             double light_y);
typedef struct {
    int centerX;
    int centerY;
    int radius;
} Native3DLightMarkerScreenInfo;
static bool PresentNative3DTilePreviewFrame(SDL_Renderer* renderer,
                                            const IntegratorContext* preview_ctx,
                                            const IntegratorTile* dirty_tile,
                                            const Uint8* preview_buffer,
                                            bool reset_dirty_preview);
static bool PresentNative3DTilePreviewFrameTimed(SDL_Renderer* renderer,
                                                 const IntegratorContext* preview_ctx,
                                                 const IntegratorTile* dirty_tile,
                                                 const Uint8* preview_buffer,
                                                 bool reset_dirty_preview);
static bool ShouldPresentNative3DTileSubpassPreview(int subpass,
                                                    int temporal_frames);
static bool ResolveNative3DHostDirtyTile(const IntegratorTile* render_tile,
                                         int render_width,
                                         int render_height,
                                         int host_width,
                                         int host_height,
                                         IntegratorTile* out_host_tile);
static bool ResolveNative3DLightMarkerScreenInfo(int width,
                                                 int height,
                                                 double normalized_t,
                                                 double light_x,
                                                 double light_y,
                                                 Native3DLightMarkerScreenInfo* out_info);

static bool EnsureNative3DRenderBuffer(size_t pixel_count) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;
    if (pixel_count == 0u) {
        return false;
    }
    if (native3DRenderBuffer && native3DRenderBufferCapacity >= pixel_count) {
        return true;
    }
    byte_count = pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(native3DRenderBuffer, byte_count * sizeof(Uint8));
    if (!resized) {
        return false;
    }
    native3DRenderBuffer = resized;
    native3DRenderBufferCapacity = pixel_count;
    return true;
}

static bool EnsureNative3DPreviewBuffer(size_t pixel_count) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;
    if (pixel_count == 0u) {
        return false;
    }
    if (native3DPreviewBuffer && native3DPreviewBufferCapacity >= pixel_count) {
        return true;
    }
    byte_count = pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(native3DPreviewBuffer, byte_count * sizeof(Uint8));
    if (!resized) {
        return false;
    }
    native3DPreviewBuffer = resized;
    native3DPreviewBufferCapacity = pixel_count;
    return true;
}
static void DrawResolvedNative3DLightMarker(SDL_Renderer* renderer,
                                            const Native3DLightMarkerScreenInfo* marker);
static void __attribute__((unused)) DrawFilledCircleToARGBSurface(SDL_Surface* surface,
                                                                  int center_x,
                                                                  int center_y,
                                                                  int radius,
                                                                  uint32_t argb_color);

static RuntimeNative3DSamplingContext NextNative3DSamplingContext(void) {
    RuntimeNative3DSamplingContext sampling = {0};
    sampling.sampleSequence = s_native3DSampleSequence++;
    if (sampling.sampleSequence == 0U) {
        sampling.sampleSequence = s_native3DSampleSequence++;
    }
    return sampling;
}

static int ResolveNative3DTemporalFrames(RayTracing3DIntegratorId integrator_id) {
    int frames = animSettings.temporalFrames3D;
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT) {
        return 1;
    }
    if (frames < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (frames > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return frames;
}

static RuntimeNative3DSamplingContext ResolveNative3DSubpassSampling(
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t subpass_index) {
    RuntimeNative3DSamplingContext resolved = {0};
    if (sampling) {
        resolved = *sampling;
    }
    resolved.sampleSequence += subpass_index;
    if (resolved.sampleSequence == 0U) {
        resolved.sampleSequence = subpass_index + 1U;
    }
    return resolved;
}

static void LogNative3DRenderStatsIfNeeded(RayTracing3DIntegratorId integrator_id,
                                           const RuntimeNative3DRenderStats* stats) {
    double avg_bounce = 0.0;
    if (!stats) return;
    if (integrator_id != RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE) return;
    if (stats->bouncePixelCount > 0) {
        avg_bounce = stats->totalBounceRadiance / (double)stats->bouncePixelCount;
    }
    printf("[native3d] diffuse frame hits=%d visible=%d bounce_pixels=%d secondary=%d hits2=%d lit2=%d max_r=%.4f max_b=%.4f avg_b=%.4f\n",
           stats->hitPixelCount,
           stats->visiblePixelCount,
           stats->bouncePixelCount,
           stats->secondaryRayCount,
           stats->secondaryHitCount,
           stats->secondaryContributingHitCount,
           stats->maxRadiance,
           stats->maxBounceRadiance,
           avg_bounce);
}
static double ResolveNative3DLightMarkerWorldRadius(const RuntimeScene3D* scene);
static void __attribute__((unused)) DrawNative3DLightMarker(SDL_Renderer* renderer,
                                                            int width,
                                                            int height,
                                                            double normalized_t,
                                                            double light_x,
                                                            double light_y);

static FluidManifest g_fluidManifest = {0};
static FluidFrame g_fluidFrame = {0};
static int g_loadedFrameIndex = -1;
static FluidGridBounds g_grid = {0};
static bool g_manifestLoaded = false;
static CoreSpaceDesc g_fluidSpaceDesc = {0};
static bool g_fluidSpaceValid = false;

static bool BuildNative3DLiveSceneForOverlay(RuntimeScene3D* out_scene,
                                             double normalized_t,
                                             double light_x,
                                             double light_y) {
    RuntimeScene3D scene;
    RuntimeCamera3D sampled_camera = {0};

    if (!out_scene) return false;

    RuntimeScene3D_Init(&scene);
    if (!RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, normalized_t)) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    scene.light.position = vec3(light_x, light_y, animSettings.lightHeight);
    scene.light.radius = (scene.light.radius > 0.0) ? scene.light.radius : 10.0;
    scene.light.intensity = animSettings.lightIntensity;
    scene.light.falloffDistance = animSettings.forwardDecay;
    scene.light.falloffMode = animSettings.forwardFalloffMode;
    scene.hasLight = true;

    scene.camera.position = vec3(sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.cameraZ);
    scene.camera.rotation = sceneSettings.camera.rotation;
    scene.camera.zoom = (sceneSettings.camera.zoom > 0.0) ? sceneSettings.camera.zoom : 1.0;
    scene.camera.nearPlane = (scene.camera.nearPlane > 0.0) ? scene.camera.nearPlane : 0.1;
    scene.camera.lookPitch = 0.0;
    if (!animSettings.interactiveMode &&
        RuntimeScene3DSampleAuthoredCamera(normalized_t, &sampled_camera)) {
        scene.camera.lookPitch = sampled_camera.lookPitch;
    }
    scene.hasCamera = true;

    if (scene.primitiveCount <= 0 ||
        scene.triangleMesh.triangleCount <= 0 ||
        !scene.hasLight ||
        !scene.hasCamera) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    *out_scene = scene;
    return true;
}

static double ResolveNative3DLightMarkerWorldRadius(const RuntimeScene3D* scene) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;
    int i = 0;
    double span_max = 0.0;
    double radius = 0.0;

    if (!scene || scene->triangleMesh.triangleCount <= 0) {
        return 0.12;
    }

    for (i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* tri = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {tri->p0, tri->p1, tri->p2};
        int p = 0;
        for (p = 0; p < 3; ++p) {
            const Vec3 point = points[p];
            if (!seeded) {
                min_x = max_x = point.x;
                min_y = max_y = point.y;
                min_z = max_z = point.z;
                seeded = true;
            } else {
                if (point.x < min_x) min_x = point.x;
                if (point.x > max_x) max_x = point.x;
                if (point.y < min_y) min_y = point.y;
                if (point.y > max_y) max_y = point.y;
                if (point.z < min_z) min_z = point.z;
                if (point.z > max_z) max_z = point.z;
            }
        }
    }

    if (!seeded) {
        return 0.12;
    }

    span_max = fmax(max_x - min_x, fmax(max_y - min_y, max_z - min_z));
    if (!(span_max > 0.0) || !isfinite(span_max)) {
        return 0.12;
    }

    radius = span_max * 0.015;
    if (radius < 0.05) radius = 0.05;
    if (radius > 0.25) radius = 0.25;
    return radius;
}

static bool RuntimeNative3DLightMarkerPathVisible(const RuntimeScene3D* scene,
                                                  Vec3 origin,
                                                  Vec3 light_position,
                                                  double t_min,
                                                  double light_distance) {
    Ray3D ray = {0};
    double remaining_distance = 0.0;
    int skip_count = 0;
    static const int kMaxTransparentMarkerSkips = 16;

    if (!scene) return false;
    if (!(light_distance > t_min)) return false;

    ray = RuntimeRay3D_Make(origin, vec3_sub(light_position, origin));
    remaining_distance = light_distance;
    while (skip_count < kMaxTransparentMarkerSkips &&
           remaining_distance > t_min) {
        HitInfo3D blocker_hit = {0};
        RuntimeMaterialPayload3D payload = {0};

        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &ray,
                                             t_min,
                                             remaining_distance - 1e-4,
                                             &blocker_hit)) {
            return true;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&blocker_hit, &payload) ||
            !(payload.transparency > 0.0)) {
            return false;
        }
        remaining_distance -= blocker_hit.t;
        ray = RuntimeRay3D_MakeOffset(blocker_hit.position,
                                      blocker_hit.normal,
                                      ray.direction,
                                      1e-4);
        skip_count += 1;
    }

    return remaining_distance > t_min;
}

static bool ResolveNative3DLightMarkerScreenInfo(int width,
                                                 int height,
                                                 double normalized_t,
                                                 double light_x,
                                                 double light_y,
                                                 Native3DLightMarkerScreenInfo* out_info) {
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    Vec3 light_position = vec3(0.0, 0.0, 0.0);
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    double screen_x = 0.0;
    double screen_y = 0.0;
    double camera_depth = 0.0;
    bool inside = false;
    double light_radius_world = 0.0;
    double light_distance = 0.0;
    double radius_pixels = 0.0;
    int resolved_radius = 0;
    Native3DLightMarkerScreenInfo info = {0};

    if (width <= 0 || height <= 0 || !out_info) return false;
    if (!BuildNative3DLiveSceneForOverlay(&scene, normalized_t, light_x, light_y)) return false;

    if (!RuntimeCameraProjector3D_Build(&scene.camera, width, height, &projector)) {
        RuntimeScene3D_Free(&scene);
        return false;
    }
    light_position = scene.light.position;
    if (!RuntimeCameraProjector3D_ProjectPoint(&projector,
                                               light_position,
                                               &screen_x,
                                               &screen_y,
                                               &camera_depth,
                                               &inside)) {
        RuntimeScene3D_Free(&scene);
        return false;
    }
    if (!inside || camera_depth <= projector.nearPlane) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    to_light = vec3_sub(light_position, projector.origin);
    light_distance = vec3_length(to_light);
    if (light_distance <= projector.nearPlane) {
        RuntimeScene3D_Free(&scene);
        return false;
    }
    if (!RuntimeNative3DLightMarkerPathVisible(&scene,
                                               projector.origin,
                                               light_position,
                                               projector.nearPlane,
                                               light_distance)) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    light_radius_world = ResolveNative3DLightMarkerWorldRadius(&scene);
    radius_pixels = light_radius_world *
                    ((double)width / (2.0 * projector.tanHalfFovX * camera_depth));
    if (!isfinite(radius_pixels) || radius_pixels <= 0.0) {
        radius_pixels = 2.0;
    }
    resolved_radius = (int)lround(radius_pixels);
    if (resolved_radius < 2) resolved_radius = 2;
    if (resolved_radius > 24) resolved_radius = 24;

    info.centerX = (int)lround(screen_x);
    info.centerY = (int)lround(screen_y);
    info.radius = resolved_radius;
    *out_info = info;
    RuntimeScene3D_Free(&scene);
    return true;
}

static void __attribute__((unused)) DrawFilledCircleToARGBSurface(SDL_Surface* surface,
                                                                  int center_x,
                                                                  int center_y,
                                                                  int radius,
                                                                  uint32_t argb_color) {
    int min_y = 0;
    int max_y = 0;
    if (!surface || radius <= 0) return;

    min_y = center_y - radius;
    max_y = center_y + radius;
    for (int y = min_y; y <= max_y; ++y) {
        int dy = y - center_y;
        int dx_limit = 0;
        if (y < 0 || y >= surface->h) continue;
        dx_limit = (int)floor(sqrt((double)(radius * radius - dy * dy)));
        for (int x = center_x - dx_limit; x <= center_x + dx_limit; ++x) {
            uint32_t* row = NULL;
            if (x < 0 || x >= surface->w) continue;
            row = (uint32_t*)((uint8_t*)surface->pixels + ((size_t)y * surface->pitch));
            row[x] = argb_color;
        }
    }
}

static void DrawResolvedNative3DLightMarker(SDL_Renderer* renderer,
                                            const Native3DLightMarkerScreenInfo* marker) {
    if (!renderer || !marker) return;
    if (marker->radius <= 0) return;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    RenderCircle(renderer,
                 marker->centerX,
                 marker->centerY,
                 marker->radius,
                 true);
}

static void __attribute__((unused)) DrawNative3DLightMarker(SDL_Renderer* renderer,
                                                            int width,
                                                            int height,
                                                            double normalized_t,
                                                            double light_x,
                                                            double light_y) {
    Native3DLightMarkerScreenInfo marker = {0};

    if (!renderer || width <= 0 || height <= 0) return;
    if (!ResolveNative3DLightMarkerScreenInfo(width,
                                              height,
                                              normalized_t,
                                              light_x,
                                              light_y,
                                              &marker)) {
        return;
    }

    DrawResolvedNative3DLightMarker(renderer, &marker);
}

bool ExportCurrentNative3DFrameBMP(const char* filename) {
    SDL_Surface* surface = NULL;
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    const Uint8* source = native3DPreviewBuffer ? native3DPreviewBuffer : pixelBuffer;

    if (!filename || !filename[0] || !source || width <= 0 || height <= 0) {
        return false;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
        return false;
    }

    for (int y = 0; y < height; ++y) {
        uint32_t* row = (uint32_t*)((uint8_t*)surface->pixels + ((size_t)y * surface->pitch));
        size_t base = (size_t)y * (size_t)width;
        for (int x = 0; x < width; ++x) {
            if (source == native3DPreviewBuffer) {
                size_t idx = (base + (size_t)x) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
                row[x] = ((uint32_t)source[idx + 3u] << 24) |
                         ((uint32_t)source[idx] << 16) |
                         ((uint32_t)source[idx + 1u] << 8) |
                         (uint32_t)source[idx + 2u];
            } else {
                uint8_t b = source[base + (size_t)x];
                row[x] = ((uint32_t)0xFF << 24) | ((uint32_t)b << 16) |
                         ((uint32_t)b << 8) | (uint32_t)b;
            }
        }
    }

    if (SDL_SaveBMP(surface, filename) != 0) {
        fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_FreeSurface(surface);
    return true;
}

#if USE_VULKAN
static SDL_Surface* g_luma_surface = NULL;
static int g_luma_w = 0;
static int g_luma_h = 0;
static SDL_Surface* g_abgr_surface = NULL;
static int g_abgr_w = 0;
static int g_abgr_h = 0;

static SDL_Surface* get_luma_surface(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    if (!g_luma_surface || g_luma_w != width || g_luma_h != height) {
        if (g_luma_surface) {
            SDL_FreeSurface(g_luma_surface);
        }
        g_luma_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
        if (!g_luma_surface) {
            g_luma_w = 0;
            g_luma_h = 0;
            return NULL;
        }
        g_luma_w = width;
        g_luma_h = height;
    }
    return g_luma_surface;
}

static void draw_luminance_buffer(SDL_Renderer* renderer,
                                  const Uint8* buffer,
                                  int width,
                                  int height) {
    if (!renderer || !buffer || width <= 0 || height <= 0) return;
    SDL_Surface* surface = get_luma_surface(width, height);
    if (!surface) return;

    uint8_t* dst = (uint8_t*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)(dst + y * pitch);
        size_t base = (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) {
            uint8_t b = buffer[base + (size_t)x];
            row[x] = ((uint32_t)0xFF << 24) | ((uint32_t)b << 16) |
                     ((uint32_t)b << 8) | (uint32_t)b;
        }
    }

    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer,
                                                   surface,
                                                   &texture,
                                                   VK_FILTER_NEAREST) != VK_SUCCESS) {
        return;
    }
    SDL_Rect dst_rect = {0, 0, width, height};
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
}

static SDL_Surface* get_abgr_surface(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    if (!g_abgr_surface || g_abgr_w != width || g_abgr_h != height) {
        if (g_abgr_surface) {
            SDL_FreeSurface(g_abgr_surface);
        }
        g_abgr_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
        if (!g_abgr_surface) {
            g_abgr_w = 0;
            g_abgr_h = 0;
            return NULL;
        }
        g_abgr_w = width;
        g_abgr_h = height;
    }
    return g_abgr_surface;
}

static void draw_abgr_buffer(SDL_Renderer* renderer,
                             const Uint8* buffer,
                             int width,
                             int height) {
    if (!renderer || !buffer || width <= 0 || height <= 0) return;
    SDL_Surface* surface = get_abgr_surface(width, height);
    if (!surface) return;

    uint8_t* dst = (uint8_t*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)(dst + y * pitch);
        size_t base = (size_t)y * (size_t)width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = 0; x < width; x++) {
            size_t idx = base + (size_t)x * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            row[x] = ((uint32_t)buffer[idx + 3u] << 24) |
                     ((uint32_t)buffer[idx] << 16) |
                     ((uint32_t)buffer[idx + 1u] << 8) |
                     (uint32_t)buffer[idx + 2u];
        }
    }

    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer,
                                                   surface,
                                                   &texture,
                                                   VK_FILTER_NEAREST) != VK_SUCCESS) {
        return;
    }
    SDL_Rect dst_rect = {0, 0, width, height};
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
}
#endif

static bool LoadShapelibReplacement(const char *asset_path, ShapeAsset *asset) {
    if (!asset) return false;
    // Derive base name (without extension)
    char base[256] = {0};
    const char *path = asset_path ? asset_path : asset->name;
    if (path) {
        const char *slash = strrchr(path, '/');
        const char *fname = slash ? slash + 1 : path;
        strncpy(base, fname, sizeof(base) - 1);
    }
    if (base[0] == '\0' && asset->name) {
        const char *slash = strrchr(asset->name, '/');
        const char *fname = slash ? slash + 1 : asset->name;
        strncpy(base, fname, sizeof(base) - 1);
    }
    if (base[0] == '\0') return false;
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    const char *candidates[] = {
        "import", "../line_drawing/export", "../physics_sim/import"
    };
    char tryPath[512];
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        snprintf(tryPath, sizeof(tryPath), "%s/%s.json", candidates[i], base);
        ShapeDocument doc = {0};
        if (shape_import_load(tryPath, &doc) && doc.shapeCount > 0) {
            ShapeAsset replacement = {0};
            if (shape_asset_from_shapelib_shape(&doc.shapes[0], 0.1f, &replacement)) {
                shape_asset_free(asset);
                *asset = replacement;
                ShapeDocument_Free(&doc);
                return true;
            }
        }
        ShapeDocument_Free(&doc);
    }
    return false;
}

static void AddImportedObject(const FluidImportShape *imp) {
    if (!imp || !imp->path) return;
    // Position in world using normalized coords against grid bounds.
    double min_x = g_grid.valid ? g_grid.min_x : 0.0;
    double min_y = g_grid.valid ? g_grid.min_y : 0.0;
    double max_x = g_grid.valid ? g_grid.max_x : (double)sceneSettings.windowWidth;
    double max_y = g_grid.valid ? g_grid.max_y : (double)sceneSettings.windowHeight;
    double world_x = min_x + (max_x - min_x) * imp->pos_x_norm;
    double world_y = min_y + (max_y - min_y) * imp->pos_y_norm;

    ShapeAsset asset = {0};
    bool loaded = shape_asset_load_file(imp->path, &asset);
    if (loaded) {
        LoadShapelibReplacement(imp->path, &asset);
    }
    double angle = imp->rotation_deg * M_PI / 180.0;
    double scale = (imp->scale > 0.0f) ? imp->scale : 1.0;
    if (g_fluidSpaceValid) {
        float asset_max_dim = 1.0f;
        if (loaded) {
            ShapeAssetBounds bnds;
            if (shape_asset_bounds(&asset, &bnds) && bnds.valid) {
                float dx = bnds.max_x - bnds.min_x;
                float dy = bnds.max_y - bnds.min_y;
                asset_max_dim = (dx > dy) ? dx : dy;
                if (asset_max_dim <= 0.0001f) asset_max_dim = 1.0f;
            }
        }
        CoreSpaceImport import_in;
        CoreSpaceWorldTransform world_out;
        memset(&import_in, 0, sizeof(import_in));
        memset(&world_out, 0, sizeof(world_out));
        import_in.pos_x_raw = imp->pos_x_norm;
        import_in.pos_y_raw = imp->pos_y_norm;
        import_in.rotation_deg = imp->rotation_deg;
        import_in.scale = (imp->scale > 0.0f) ? imp->scale : 1.0f;
        import_in.asset_max_dim = asset_max_dim;
        if (core_space_import_to_world(&g_fluidSpaceDesc, &import_in, &world_out).code == CORE_OK) {
            world_x = world_out.x;
            world_y = world_out.y;
            scale = world_out.scale;
        }
    }
    if (loaded) {
        int before = sceneSettings.objectCount;
        ShapeToSceneOptions opts = {.scale = scale, .offset_x = world_x, .offset_y = world_y};
        shape_asset_append_to_scene(&asset, &opts);
        int after = sceneSettings.objectCount;
        for (int i = before; i < after; ++i) {
            SceneObject *obj = &sceneSettings.sceneObjects[i];
            obj->rotation = angle;
            obj->dirty = true;
        }
        shape_asset_free(&asset);
    } else {
        // Fallback: place a single polygon placeholder
        if (sceneSettings.objectCount >= MAX_OBJECTS) return;
        SceneObject *obj = &sceneSettings.sceneObjects[sceneSettings.objectCount++];
        InitObject(obj, OBJECT_POLYGON, 0, 0, 0, 0, NULL, 0);
        strncpy(obj->type, "polygon", sizeof(obj->type) - 1);
        obj->x = world_x;
        obj->y = world_y;
        obj->rotation = angle;
        obj->scale = scale;
        obj->dirty = true;
        printf("[fluid] warning: could not load asset %s\n", imp->path);
    }
}

static void ApplyMaterialOverrides(MaterialBSDF* material) {
    if (!material) return;
    if (animSettings.bsdfModel == 0) {
        material->specWeight = 0.0;
        material->diffuseWeight = 1.0;
        material->weightSum = 1.0;
    } else {
        material->weightSum = material->diffuseWeight + material->specWeight;
        if (material->weightSum <= 1e-4) {
            material->diffuseWeight = 1.0;
            material->specWeight = 0.0;
            material->weightSum = 1.0;
        }
    }
}

void InitRayTracingScene(void) {
    printf("DEBUG: Initializing Ray Tracing Scene\n");

    if (sceneSettings.objectCount == 0) {
        printf("WARNING: No scene objects detected! Rendering empty scene.\n");
    } else {
        printf("INFO: Loaded %d scene objects for ray tracing.\n", sceneSettings.objectCount);
        for (int i = 0; i < sceneSettings.objectCount; i++) {
            printf("  → Object %d: Type: %s, X: %.2f, Y: %.2f\n",
                   i, sceneSettings.sceneObjects[i].type,
                   (double)sceneSettings.sceneObjects[i].x,
                   (double)sceneSettings.sceneObjects[i].y);
        }
    }

    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate memory for pixel buffer!\n");
            exit(1);
        }
    }
    if (tilePreviewBuffer == NULL) {
        tilePreviewBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!tilePreviewBuffer) {
            printf("ERROR: Failed to allocate memory for tile preview buffer!\n");
            exit(1);
        }
    }

    if (energyBuffer == NULL) {
        energyBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!energyBuffer) {
            printf("ERROR: Failed to allocate memory for energy buffer!\n");
            exit(1);
        }
    }

    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
    memset(tilePreviewBuffer, 0, pixelCount * sizeof(Uint8));
    memset(energyBuffer, 0, pixelCount * sizeof(float));

    if (sceneSettings.bezierPath.numPoints > 0) {
        light.x = sceneSettings.bezierPath.points[0].x;
        light.y = sceneSettings.bezierPath.points[0].y;
        printf("INFO: Light source initialized from Bézier Path at (%.2f, %.2f)\n", light.x, light.y);
    } else {
        light.x = 100;
        light.y = 100;
        printf("WARNING: Bézier Path is uninitialized! Using default light position (100, 100)\n");
    }

    SurfaceMeshInit(&surfaceMesh);
    TriangleMeshInit(&triangleMesh);

    // Load fluid manifest if specified and enabled.
    if (AnimationUseFluidScene()) {
        if (fluid_manifest_load(animSettings.fluidManifest, &g_fluidManifest)) {
            printf("[fluid] Loaded manifest: %zu frames (%ux%u)\n",
                   g_fluidManifest.count,
                   g_fluidManifest.grid_w,
                   g_fluidManifest.grid_h);
            g_grid.valid = true;
            g_grid.min_x = g_fluidManifest.origin_x;
            g_grid.min_y = g_fluidManifest.origin_y;
            g_grid.max_x = g_fluidManifest.origin_x + g_fluidManifest.cell_size * (float)g_fluidManifest.grid_w;
            g_grid.max_y = g_fluidManifest.origin_y + g_fluidManifest.cell_size * (float)g_fluidManifest.grid_h;
            g_manifestLoaded = true;
            g_fluidSpaceValid = false;
            if (core_space_desc_default_from_grid((int)g_fluidManifest.grid_w,
                                                  (int)g_fluidManifest.grid_h,
                                                  g_fluidManifest.origin_x,
                                                  g_fluidManifest.origin_y,
                                                  g_fluidManifest.cell_size,
                                                  &g_fluidSpaceDesc).code == CORE_OK) {
                if (g_fluidManifest.space_author_window_w > 0) {
                    g_fluidSpaceDesc.author_window_w = g_fluidManifest.space_author_window_w;
                }
                if (g_fluidManifest.space_author_window_h > 0) {
                    g_fluidSpaceDesc.author_window_h = g_fluidManifest.space_author_window_h;
                }
                if (g_fluidManifest.space_desired_fit > 0.0f) {
                    g_fluidSpaceDesc.desired_fit = g_fluidManifest.space_desired_fit;
                }
                g_fluidSpaceValid = true;
            }
            // Fit camera to grid bounds
            double grid_w_world = g_grid.max_x - g_grid.min_x;
            double grid_h_world = g_grid.max_y - g_grid.min_y;
            sceneSettings.camera.x = g_grid.min_x + grid_w_world * 0.5;
            sceneSettings.camera.y = g_grid.min_y + grid_h_world * 0.5;
            double zoom_x = (grid_w_world > 1e-4) ? ((double)sceneSettings.windowWidth / grid_w_world) : 1.0;
            double zoom_y = (grid_h_world > 1e-4) ? ((double)sceneSettings.windowHeight / grid_h_world) : 1.0;
            sceneSettings.camera.zoom = fmin(zoom_x, zoom_y) * 0.9;
            sceneSettings.objectCount = 0;
            // Preload first frame if available.
            if (g_fluidManifest.count > 0) {
                int idx = g_fluidFrameIndex;
                if (idx < 0) idx = 0;
                if (idx >= (int)g_fluidManifest.count) idx = (int)g_fluidManifest.count - 1;
                const char *path = g_fluidManifest.paths[idx];
                if (path && fluid_frame_load(path, &g_fluidFrame)) {
                    g_fluidFrameIndex = idx;
                    g_loadedFrameIndex = idx;
                    printf("[fluid] Loaded frame %d from %s\n", idx, path);
                }
            }

            // Populate imported objects from manifest.
            for (size_t i = 0; i < g_fluidManifest.import_count; ++i) {
                AddImportedObject(&g_fluidManifest.imports[i]);
            }
        } else {
            printf("[fluid] Failed to load manifest: %s\n", animSettings.fluidManifest);
            g_fluidSpaceValid = false;
        }
    } else if (animSettings.fluidManifest[0] == '\0' &&
               strlen(animSettings.frameDir) > 0 &&
               (strstr(animSettings.frameDir, ".vf2d") || strstr(animSettings.frameDir, ".pack"))) {
        // If a direct frame path was provided in config, attempt single-frame load.
        const char *path = animSettings.frameDir;
        if (fluid_frame_load_single(path, &g_fluidFrame)) {
            g_loadedFrameIndex = 0;
            g_grid.valid = true;
            g_grid.min_x = g_grid.min_y = 0.0f;
            g_grid.max_x = (float)g_fluidFrame.w;
            g_grid.max_y = (float)g_fluidFrame.h;
            printf("[fluid] Loaded single frame from %s\n", path);
        }
        g_fluidSpaceValid = false;
    } else {
        g_fluidSpaceValid = false;
    }
}

void CleanupRayTracing(void) {
    if (pixelBuffer != NULL) {
        free(pixelBuffer);
        pixelBuffer = NULL;
    }
    if (tilePreviewBuffer != NULL) {
        free(tilePreviewBuffer);
        tilePreviewBuffer = NULL;
    }
    if (native3DRenderBuffer != NULL) {
        free(native3DRenderBuffer);
        native3DRenderBuffer = NULL;
        native3DRenderBufferCapacity = 0u;
    }
    if (native3DPreviewBuffer != NULL) {
        free(native3DPreviewBuffer);
        native3DPreviewBuffer = NULL;
        native3DPreviewBufferCapacity = 0u;
    }
    RayTracingPreview_ShutdownNative3DDirtyRect();
    if (energyBuffer != NULL) {
        free(energyBuffer);
        energyBuffer = NULL;
    }
    if (directEnergyBufferCPU != NULL) {
        free(directEnergyBufferCPU);
        directEnergyBufferCPU = NULL;
    }
    TileGridFree(&tileGrid);
    UniformGridFree(&uniformGrid);
    IrradianceCacheClear(&irradianceCache);
    if (reflectionForwardBuffer) {
        free(reflectionForwardBuffer);
        reflectionForwardBuffer = NULL;
    }
    if (materialTable) {
        free(materialTable);
        materialTable = NULL;
        materialCapacity = 0;
    }
    SurfaceMeshFree(&surfaceMesh);
    TriangleMeshFree(&triangleMesh);

    fluid_frame_free(&g_fluidFrame);
    fluid_manifest_free(&g_fluidManifest);
    g_fluidSpaceValid = false;
}

void SetLightPosition(double x, double y) {
    light.x = x;
    light.y = y;
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RayTracingViewCarrier viewCarrier = RayTracingModeBackend_BuildViewCarrier(&sceneSettings.camera,
                                                                                WIDTH,
                                                                                HEIGHT,
                                                                                &route);
    RayTracingPrimitivePrepPlan primitivePrepPlan =
        RayTracingModeBackend_BuildPrimitivePrepPlan(&route, sceneSettings.objectCount);
    bool useTiles = route.useTiles;
    int tileSize = route.tileSize;
    double gridCellSize = fmax(4.0, (animSettings.tileSize > 0 ? animSettings.tileSize : 16));
    double camera_origin_x = viewCarrier.originX;
    double camera_origin_y = viewCarrier.originY;
    bool native3D = RayTracingModeBackend_IsNative3D(&route);

    // Clamp camera to grid if fluid bounds are known.
    if (g_grid.valid) {
        if (sceneSettings.camera.x < g_grid.min_x) sceneSettings.camera.x = g_grid.min_x;
        if (sceneSettings.camera.x > g_grid.max_x) sceneSettings.camera.x = g_grid.max_x;
        if (sceneSettings.camera.y < g_grid.min_y) sceneSettings.camera.y = g_grid.min_y;
        if (sceneSettings.camera.y > g_grid.max_y) sceneSettings.camera.y = g_grid.max_y;
    }

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate pixel buffer during render.\n");
            return;
        }
    }
    if (tilePreviewBuffer == NULL) {
        tilePreviewBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!tilePreviewBuffer) {
            printf("ERROR: Failed to allocate tile preview buffer during render.\n");
            return;
        }
    }

    if (!useTiles) {
        if (energyBuffer == NULL) {
            energyBuffer = (float*)malloc(pixelCount * sizeof(float));
            if (!energyBuffer) {
                printf("ERROR: Failed to allocate energy buffer during render.\n");
                return;
            }
        }
        memset(energyBuffer, 0, pixelCount * sizeof(float));
    } else {
        TileGridEnsure(&tileGrid, WIDTH, HEIGHT, tileSize);
        TileGridClear(&tileGrid);
    }
    if (!useTiles) {
        if (directEnergyBufferCPU == NULL) {
            directEnergyBufferCPU = (float*)malloc(pixelCount * sizeof(float));
            if (!directEnergyBufferCPU) {
                printf("ERROR: Failed to allocate direct energy buffer during render.\n");
                return;
            }
        }
        memset(directEnergyBufferCPU, 0, pixelCount * sizeof(float));
    } else {
        // Ensure forward-only buffers are not reused in tiled mode
        if (directEnergyBufferCPU) memset(directEnergyBufferCPU, 0, pixelCount * sizeof(float));
    }
    
    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer
    memset(tilePreviewBuffer, 0, pixelCount * sizeof(Uint8));

    if (native3D) {
        RuntimeNative3DRenderStats nativeStats = {0};
        RuntimeNative3DSamplingContext nativeSampling = NextNative3DSamplingContext();
        int blurRadius = 0;
        bool nativeRenderOk = false;
        double normalized_t = AnimationCurrentNormalizedT();
        int renderScale = RuntimeNative3DClampRenderScale(animSettings.renderScale3D);
        int renderWidth = WIDTH;
        int renderHeight = HEIGHT;
        size_t renderPixelCount = 0u;
        size_t nativePreviewByteCount =
            pixelCount * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;

        if (!RuntimeNative3DResolveScaledDimensions(WIDTH,
                                                    HEIGHT,
                                                    renderScale,
                                                    &renderWidth,
                                                    &renderHeight)) {
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            return;
        }
        renderPixelCount = (size_t)renderWidth * (size_t)renderHeight;
        if (!EnsureNative3DRenderBuffer(renderPixelCount) ||
            !EnsureNative3DPreviewBuffer(pixelCount)) {
            printf("ERROR: Failed to allocate native 3D render buffer during render.\n");
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            return;
        }
        RuntimeNative3DFillPixelBufferEnvironment(native3DRenderBuffer, renderPixelCount);
        RuntimeNative3DFillPixelBufferEnvironment(native3DPreviewBuffer,
                                                 nativePreviewByteCount /
                                                     (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);

        ts_start_timer("Buffer Calc");
        if (useTiles && tileGrid.tiles && tileGrid.count > 0) {
            TileGridEnsure(&tileGrid, renderWidth, renderHeight, tileSize);
            TileGridClear(&tileGrid);
            nativeRenderOk = RenderNative3DTilesPreview(renderer,
                                                        native3DPreviewBuffer,
                                                        WIDTH,
                                                        HEIGHT,
                                                        native3DRenderBuffer,
                                                        renderWidth,
                                                        renderHeight,
                                                        &tileGrid,
                                                        route.integratorMode3D,
                                                        normalized_t,
                                                        light.x,
                                                        light.y,
                                                        &nativeSampling,
                                                        route.tilePreviewEnabled,
                                                        &nativeStats);
        } else {
            nativeRenderOk = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
                native3DRenderBuffer,
                route.integratorMode3D,
                renderWidth,
                renderHeight,
                normalized_t,
                light.x,
                light.y,
                &nativeSampling,
                ResolveNative3DTemporalFrames(route.integratorMode3D),
                &nativeStats);
        }
        ts_stop_timer("Buffer Calc");
        if (!nativeRenderOk) {
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            RuntimeNative3DFillPixelBufferEnvironment(
                native3DPreviewBuffer,
                nativePreviewByteCount / (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
        } else {
            LogNative3DRenderStatsIfNeeded(route.integratorMode3D, &nativeStats);
        }

        if (animSettings.blurMode == 1) blurRadius = 1;
        else if (animSettings.blurMode == 2) blurRadius = 2;
        else if (animSettings.blurMode == 3) blurRadius = 3;

        ts_start_timer("Buffer Present");
        if (nativeRenderOk) {
            if (blurRadius > 0) {
                RayTracingPreview_ApplySeparableBlurABGR(native3DRenderBuffer,
                                                         renderWidth,
                                                         renderHeight,
                                                         blurRadius);
            }
            RuntimeNative3DUpscaleNearestABGR(native3DRenderBuffer,
                                              renderWidth,
                                              renderHeight,
                                              native3DPreviewBuffer,
                                              WIDTH,
                                              HEIGHT);
        }
#if USE_VULKAN
        draw_abgr_buffer(renderer, native3DPreviewBuffer, WIDTH, HEIGHT);
#else
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                size_t idx =
                    ((size_t)y * (size_t)WIDTH + (size_t)x) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
                Uint8 b = native3DPreviewBuffer[idx];
                Uint8 g = native3DPreviewBuffer[idx + 1u];
                Uint8 r = native3DPreviewBuffer[idx + 2u];
                if (r > 0 || g > 0 || b > 0) {
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_RenderDrawPoint(renderer, x, y);
                }
            }
        }
#endif
        ts_stop_timer("Buffer Present");
        return;
    }

    uint64_t frameSeed = runtime_time_now_ns();
    int materialCount = BuildMaterialTable();

    bool haveCache = false;
    if (route.buildIrradianceCache) {
        haveCache = IrradianceCacheEnsure(&irradianceCache,
                                          sceneSettings.objectCount,
                                          32);
    }

    bool meshReady = false;
    if (primitivePrepPlan.enableSurfaceMeshPrep || primitivePrepPlan.enableTriangleMeshPrep) {
        meshReady = SurfaceBuildMeshes(&surfaceMesh,
                                       &triangleMesh,
                                       sceneSettings.sceneObjects,
                                       sceneSettings.objectCount,
                                       8.0);
    }

    if (primitivePrepPlan.enableUniformGrid2D && primitivePrepPlan.enableRay2DIntersections) {
        UniformGridBuild(&uniformGrid,
                         sceneSettings.sceneObjects,
                         sceneSettings.objectCount,
                         (meshReady && primitivePrepPlan.enableTriangleMeshPrep) ? &triangleMesh : NULL,
                         gridCellSize);
    } else {
        UniformGridClear(&uniformGrid);
    }

    IntegratorContext context = {
        .pixelBuffer = pixelBuffer,
        .energyBuffer = useTiles ? NULL : energyBuffer,
        .directEnergyBuffer = useTiles ? NULL : directEnergyBufferCPU,
        .width = WIDTH,
        .height = HEIGHT,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = useTiles ? &tileGrid : NULL,
        .useTiles = useTiles,
        .frameSeed = frameSeed,
        .uniformGrid = (primitivePrepPlan.enableRay2DIntersections &&
                        (uniformGrid.objectCells || uniformGrid.triangleCells))
                           ? &uniformGrid
                           : NULL,
        .integratorMode = route.integratorMode,
        .cache = (haveCache ? &irradianceCache : NULL),
        .materials = materialTable,
        .materialCount = materialCount,
        .mesh = (meshReady && primitivePrepPlan.enableSurfaceMeshPrep) ? &surfaceMesh : NULL,
        .triangleMesh = (meshReady && primitivePrepPlan.enableTriangleMeshPrep) ? &triangleMesh : NULL
    };

    LightSource activeLight = {
        .x = light.x,
        .y = light.y,
        .radius = light.r
    };

    bool cacheReady = false;
    if (route.buildIrradianceCache && haveCache) {
        ts_start_timer("Irradiance Cache");
        cacheReady = BuildReflectionCache(&context, &activeLight);
        ts_stop_timer("Irradiance Cache");
        if (!cacheReady) {
            context.cache = NULL;
        }
    } else {
        // Do not reuse stale caches when not in camera-path mode
        context.cache = NULL;
    }

    ts_start_timer("Buffer Calc");
    switch (route.integratorMode) {
        case 0: // forward
            ForwardLightIntegratorRender(&context, &activeLight);
            break;
        case 1: { // hybrid camera path (new split)
            CameraIntegratorSettings settings = {
                .directIntensityScale = animSettings.lightIntensity,
                .indirectVariance = animSettings.cacheVarianceCutoff,
                .indirectHaloRadius = animSettings.cacheHaloRadius,
                .blurEnabled = (animSettings.blurMode != 0),
                .brightnessBoost = 1.0
            };
            if (route.tilePreviewEnabled) {
                RenderHybridTilesPreview(renderer,
                                         &context,
                                         &activeLight,
                                         &settings,
                                         camera_origin_x,
                                         camera_origin_y);
            } else {
                CameraPathIntegratorRenderFromContext(&context,
                                                      &activeLight,
                                                      &settings,
                                                      camera_origin_x,
                                                      camera_origin_y);
            }
            break;
        }
        case 2: // direct-only
            DirectLightIntegratorRender(&context, &activeLight);
            break;
        default:
            ForwardLightIntegratorRender(&context, &activeLight);
            break;
    }
    ts_stop_timer("Buffer Calc");

    int blurRadius = 0;
    if (animSettings.blurMode == 1) blurRadius = 1;
    else if (animSettings.blurMode == 2) blurRadius = 2;
    else if (animSettings.blurMode == 3) blurRadius = 3;
    ts_start_timer("Buffer Present");
    if (route.tilePreviewEnabled) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect bg = {0, 0, WIDTH, HEIGHT};
        SDL_RenderFillRect(renderer, &bg);
    }
    if (blurRadius > 0) {
        RayTracingPreview_ApplySeparableBlur(pixelBuffer, WIDTH, HEIGHT, blurRadius);
    }

#if USE_VULKAN
    draw_luminance_buffer(renderer, pixelBuffer, WIDTH, HEIGHT);
#else
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Uint8 brightness = pixelBuffer[y * WIDTH + x];
            if (brightness > 0) {
                SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
#endif
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SpaceModeViewContext view_ctx = RayTracingModeBackend_BuildViewContext(&sceneSettings.camera,
                                                                           WIDTH,
                                                                           HEIGHT,
                                                                           &route);
    CameraPoint lightScreen = SpaceModeAdapter_WorldToScreen(&view_ctx, light.x, light.y);
    int lightRadius = (int)lround(light.r * sceneSettings.camera.zoom);
    RenderCircle(renderer, (int)lround(lightScreen.x), (int)lround(lightScreen.y), lightRadius, true);


    // ✅ Draw objects using the new method
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        int brightness = CalculateObjectBrightness(&sceneSettings.sceneObjects[i], light.x, light.y);
        SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
        RenderSceneObject(renderer, &sceneSettings.sceneObjects[i], true);
    }

    // Fluid overlay (density) drawn after objects for visibility.
    if (g_fluidOverlayEnabled && g_fluidManifest.count > 0) {
        int idx = g_fluidFrameIndex;
        if (idx < 0) idx = 0;
        if (idx >= (int)g_fluidManifest.count) idx = (int)g_fluidManifest.count - 1;
        if (idx != g_loadedFrameIndex) {
            fluid_frame_free(&g_fluidFrame);
            const char *path = g_fluidManifest.paths[idx];
            if (path && fluid_frame_load(path, &g_fluidFrame)) {
                g_loadedFrameIndex = idx;
                printf("[fluid] Loaded frame %d from %s\n", idx, path);
            }
        }
        if (g_fluidFrame.density) {
            fluid_overlay_draw(renderer, &g_fluidFrame, &sceneSettings.camera, WIDTH, HEIGHT);
        }
    }

    ts_stop_timer("Buffer Present");
}


void ProcessRayTracingEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN) {
        RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
        RayTracingViewCarrier viewCarrier = RayTracingModeBackend_BuildViewCarrier(&sceneSettings.camera,
                                                                                   sceneSettings.windowWidth,
                                                                                   sceneSettings.windowHeight,
                                                                                   &route);
        CameraPoint world = SpaceModeAdapter_ScreenToWorld(&viewCarrier.viewContext,
                                                           event->motion.x,
                                                           event->motion.y);
        light.x = world.x;
        light.y = world.y;
    }
    else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_b) {  // Press "B" to switch blur mode
            animSettings.blurMode = (animSettings.blurMode == 2) ? 0 : animSettings.blurMode + 1;
            printf("Blur Mode: %s\n", (animSettings.blurMode == 0) ? "None" :
                                        (animSettings.blurMode == 1) ? "Light Blur" :
                                        "Heavy Blur");
    }
}


void GetCurrentLightPosition(double* x, double* y) {
    *x = light.x;
    *y = light.y;
}

static int BuildMaterialTable(void) {
    int count = sceneSettings.objectCount;
    if (count <= 0) {
        if (materialTable) {
            free(materialTable);
            materialTable = NULL;
        }
        materialCapacity = 0;
        return 0;
    }

    if (!materialTable || materialCapacity < count) {
        MaterialBSDF* newBuffer = (MaterialBSDF*)realloc(materialTable, (size_t)count * sizeof(MaterialBSDF));
        if (!newBuffer) {
            fprintf(stderr, "ERROR: Failed to allocate material table.\n");
            free(materialTable);
            materialTable = NULL;
            materialCapacity = 0;
            return 0;
        }
        materialTable = newBuffer;
        materialCapacity = count;
    }

    for (int i = 0; i < count; i++) {
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[i], &materialTable[i]);
        ApplyMaterialOverrides(&materialTable[i]);
    }
    return count;
}
static bool BuildReflectionCache(const IntegratorContext* ctx,
                                 const LightSource* light) {
    if (!ctx || !ctx->cache) return false;
    int width = ctx->width;
    int height = ctx->height;
    size_t pixelCount = (size_t)width * (size_t)height;
    if (!reflectionForwardBuffer) {
        reflectionForwardBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!reflectionForwardBuffer) {
            return false;
        }
    }

    int savedRays = sceneSettings.rays;
    int probeRays = savedRays / 6;
    if (probeRays < 128) probeRays = 128;
    sceneSettings.rays = probeRays;

    IntegratorContext probeCtx = *ctx;
    probeCtx.pixelBuffer = NULL;
    probeCtx.energyBuffer = reflectionForwardBuffer;
    probeCtx.useTiles = false;
    probeCtx.tileGrid = NULL;
    memset(reflectionForwardBuffer, 0, pixelCount * sizeof(float));
    ForwardLightIntegratorRender(&probeCtx, light);
    sceneSettings.rays = savedRays;

    float maxEnergy = 0.0f;
    for (size_t i = 0; i < pixelCount; i++) {
        if (reflectionForwardBuffer[i] > maxEnergy) {
            maxEnergy = reflectionForwardBuffer[i];
        }
    }
    if (maxEnergy <= 0.0f) {
        maxEnergy = 1.0f;
    }

    // Two-pass cache: first direct-only to seed, then include indirect reflections.
    bool ok = IrradianceCacheFill(ctx->cache,
                                  ctx->objects,
                                  ctx->objectCount,
                                  light,
                                  ctx->uniformGrid,
                                  reflectionForwardBuffer,
                                  width,
                                  height,
                                  (double)maxEnergy,
                                  NULL,
                                  0,
                                  NULL,
                                  false);
    if (!ok) return false;
    return IrradianceCacheFill(ctx->cache,
                               ctx->objects,
                               ctx->objectCount,
                               light,
                               ctx->uniformGrid,
                               reflectionForwardBuffer,
                               width,
                               height,
                               (double)maxEnergy,
                               NULL,
                               0,
                               NULL,
                               true);
}

static void DrawTilePreview(SDL_Renderer* renderer,
                            const IntegratorContext* ctx,
                            const IntegratorTile* tile,
                            const Uint8* previewBuffer) {
    if (!renderer || !ctx || !tile || !previewBuffer) return;
#if USE_VULKAN
    return;
#endif
    for (int y = 0; y < tile->height; y++) {
        int py = tile->originY + y;
        if (py < 0 || py >= ctx->height) continue;
        for (int x = 0; x < tile->width; x++) {
            int px = tile->originX + x;
            if (px < 0 || px >= ctx->width) continue;
            size_t idx = (size_t)py * (size_t)ctx->width + (size_t)px;
            Uint8 brightness = previewBuffer[idx];
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    }
}

static void DrawPreviewBuffer(SDL_Renderer* renderer,
                              const IntegratorContext* ctx,
                              const Uint8* previewBuffer) {
    if (!renderer || !ctx || !previewBuffer) return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect bg = {0, 0, ctx->width, ctx->height};
    SDL_RenderFillRect(renderer, &bg);

#if USE_VULKAN
    draw_luminance_buffer(renderer, previewBuffer, ctx->width, ctx->height);
    return;
#endif
    int width = ctx->width;
    int height = ctx->height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Uint8 brightness = previewBuffer[(size_t)y * (size_t)width + (size_t)x];
            if (brightness == 0) continue;
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

static bool RenderNative3DTilesPreview(SDL_Renderer* renderer,
                                       Uint8* host_buffer,
                                       int host_width,
                                       int host_height,
                                       Uint8* render_buffer,
                                       int render_width,
                                       int render_height,
                                       TileGrid* grid,
                                       RayTracing3DIntegratorId integrator_id,
                                       double normalized_t,
                                       double light_x,
                                       double light_y,
                                       const RuntimeNative3DSamplingContext* sampling,
                                       bool present_progress,
                                       RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    RuntimeNative3DTemporalAccumulation tileAccumulation = {0};
    RuntimeNative3DAdaptiveSamplingMask adaptiveMask = {0};
    RuntimeNative3DFeatureBuffer tileFeatures = {0};
    IntegratorContext preview_ctx = {
        .width = host_width,
        .height = host_height
    };
    float* tileRadiance = NULL;
    float* tileResolvedRadiance = NULL;
    size_t total = (size_t)render_width * (size_t)render_height;
    int maxTilePixels = 0;
    const int temporal_frames = ResolveNative3DTemporalFrames(integrator_id);
    const bool use_adaptive_sampling =
        RuntimeNative3DAdaptiveSampling_ShouldUse(integrator_id, temporal_frames);
    const bool use_denoise =
        RuntimeNative3DDenoise_ShouldApply(integrator_id,
                                           temporal_frames,
                                           animSettings.disneyDenoiseEnabled);

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!host_buffer || !render_buffer || host_width <= 0 || host_height <= 0 ||
        render_width <= 0 || render_height <= 0 ||
        !grid || !grid->tiles || grid->count == 0) {
        return false;
    }

    RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
    RuntimeNative3DTemporalAccumulation_Init(&tileAccumulation);
    RuntimeNative3DAdaptiveSamplingMask_Init(&adaptiveMask);
    RuntimeNative3DFeatureBuffer_Init(&tileFeatures);
    if (!RuntimeNative3DPrepareFrameWithSampling(&frame,
                                                 render_width,
                                                 render_height,
                                                 normalized_t,
                                                 light_x,
                                                 light_y,
                                                 sampling)) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
        RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
        RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
        return false;
    }
    (void)RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid->tileSize);
    maxTilePixels = grid->tileSize * grid->tileSize;
    if (maxTilePixels <= 0) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
        RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
        return false;
    }
    tileRadiance = (float*)calloc((size_t)maxTilePixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                  sizeof(*tileRadiance));
    tileResolvedRadiance =
        (float*)calloc((size_t)maxTilePixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                       sizeof(*tileResolvedRadiance));
    if (!tileRadiance || !tileResolvedRadiance) {
        free(tileRadiance);
        free(tileResolvedRadiance);
        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
        RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
        return false;
    }

    if (present_progress && renderer) {
        RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                          render_width,
                                          render_height,
                                          host_buffer,
                                          host_width,
                                          host_height);
        if (!PresentNative3DTilePreviewFrameTimed(renderer,
                                                  &preview_ctx,
                                                  NULL,
                                                  host_buffer,
                                                  true)) {
            free(tileRadiance);
            free(tileResolvedRadiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
            RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
            return false;
        }
    }

    for (size_t ti = 0; ti < grid->count; ++ti) {
        const IntegratorTile* tile = &grid->tiles[ti];
        IntegratorTile host_dirty_tile = {0};
        const IntegratorTile* present_tile = NULL;
        const int tile_stride = tile->width;
        const int tile_pixels = tile->width * tile->height;
        bool tile_timer_active = false;

        if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                             tile->originX,
                                                             tile->originY,
                                                             tile->originX + tile->width,
                                                             tile->originY + tile->height)) {
            continue;
        }

        if (!RuntimeNative3DTemporalAccumulation_Ensure(&tileAccumulation, tile->width, tile->height)) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tileRadiance);
            free(tileResolvedRadiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
            RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
            return false;
        }
        RuntimeNative3DTemporalAccumulation_Clear(&tileAccumulation);
        if (use_denoise &&
            (!RuntimeNative3DFeatureBuffer_Ensure(&tileFeatures, tile->width, tile->height) ||
             !RuntimeNative3DFeatureBuffer_RenderRegion(&tileFeatures,
                                                       &frame.scene,
                                                       &frame.projector,
                                                       tile->originX,
                                                       tile->originY,
                                                       tile->originX + tile->width,
                                                       tile->originY + tile->height))) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tileRadiance);
            free(tileResolvedRadiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
            RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
            return false;
        }
        if (use_adaptive_sampling &&
            (!RuntimeNative3DAdaptiveSamplingMask_Ensure(&adaptiveMask, tile->width, tile->height) ||
             !RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(&adaptiveMask,
                                                                    &frame.scene,
                                                                    &frame.projector,
                                                                    tile->originX,
                                                                    tile->originY,
                                                                    tile->originX + tile->width,
                                                                    tile->originY + tile->height))) {
            RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
            free(tileRadiance);
            free(tileResolvedRadiance);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
            RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
            return false;
        }
        if (present_progress && renderer &&
            ResolveNative3DHostDirtyTile(tile,
                                         render_width,
                                         render_height,
                                         host_width,
                                         host_height,
                                         &host_dirty_tile)) {
            present_tile = &host_dirty_tile;
        }

        ts_start_timer("Tile Frame Calc");
        tile_timer_active = true;
        for (int subpass = 0; subpass < temporal_frames; ++subpass) {
            RuntimeNative3DPreparedFrame subpass_frame = frame;
            RuntimeNative3DRenderStats subpass_stats = {0};
            RuntimeNative3DSamplingContext subpass_sampling =
                ResolveNative3DSubpassSampling(sampling, (uint32_t)subpass);
            const uint8_t* active_mask =
                (use_adaptive_sampling && subpass > 0) ? adaptiveMask.activeSampleMask : NULL;
            const int active_mask_stride =
                (use_adaptive_sampling && subpass > 0) ? adaptiveMask.width : 0;
            if (active_mask && !RuntimeNative3DAdaptiveSampling_HasActiveSamples(&adaptiveMask)) {
                break;
            }
            subpass_frame.sampling = subpass_sampling;
            memset(tileRadiance,
                   0,
                   (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                       sizeof(*tileRadiance));
            if (!RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
                    tileRadiance,
                    tile_stride,
                    integrator_id,
                    &subpass_frame,
                    tile->originX,
                    tile->originY,
                    tile->originX + tile->width,
                    tile->originY + tile->height,
                    active_mask,
                    active_mask_stride,
                    &subpass_stats) ||
                !RuntimeNative3DTemporalAccumulation_AddRegionSamples(&tileAccumulation,
                                                                      tileRadiance,
                                                                      tile_stride,
                                                                      0,
                                                                      0,
                                                                      tile->width,
                                                                      tile->height,
                                                                      active_mask,
                                                                      active_mask_stride)) {
                ts_stop_timer("Tile Frame Calc");
                RuntimeNative3DFillPixelBufferEnvironment(render_buffer, total);
                free(tileRadiance);
                free(tileResolvedRadiance);
                RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
                RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
                RuntimeNative3DPreparedFrame_Free(&frame);
                RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
                return false;
            }
            RuntimeNative3DTemporalAccumulation_CommitSubpass(&tileAccumulation);
            RuntimeNative3DRenderStats_Accumulate(&stats, &subpass_stats);

            if (present_progress && renderer &&
                ShouldPresentNative3DTileSubpassPreview(subpass, temporal_frames)) {
                if (use_denoise) {
                    memset(tileResolvedRadiance,
                           0,
                           (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                               sizeof(*tileResolvedRadiance));
                    if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
                            &tileAccumulation,
                            tileResolvedRadiance,
                            tile_stride,
                            0,
                            0,
                            tile->width,
                            tile->height) ||
                        !RuntimeNative3DDenoise_Apply(tileResolvedRadiance,
                                                      tile_stride,
                                                      &tileFeatures)) {
                        ts_stop_timer("Tile Frame Calc");
                        free(tileRadiance);
                        free(tileResolvedRadiance);
                        RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
                        RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
                        RuntimeNative3DPreparedFrame_Free(&frame);
                        RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
                        return false;
                    }
                    RuntimeNative3DResolveRadianceRegionToPixels(render_buffer,
                                                                 render_width,
                                                                 tileResolvedRadiance,
                                                                 tile_stride,
                                                                 tile->originX,
                                                                 tile->originY,
                                                                 tile->originX + tile->width,
                                                                 tile->originY + tile->height);
                } else {
                    RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&tileAccumulation,
                                                                                     render_buffer,
                                                                                     render_width,
                                                                                     tile->originX,
                                                                                     tile->originY);
                }
                RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                                  render_width,
                                                  render_height,
                                                  host_buffer,
                                                  host_width,
                                                  host_height);
                if (!PresentNative3DTilePreviewFrameTimed(renderer,
                                                          &preview_ctx,
                                                          present_tile,
                                                          host_buffer,
                                                          false)) {
                    ts_stop_timer("Tile Frame Calc");
                    free(tileRadiance);
                    free(tileResolvedRadiance);
                    RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
                    RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
                    RuntimeNative3DPreparedFrame_Free(&frame);
                    RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
                    return false;
                }
            }
        }

        if (use_denoise) {
            memset(tileResolvedRadiance,
                   0,
                   (size_t)tile_pixels * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                       sizeof(*tileResolvedRadiance));
            if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(&tileAccumulation,
                                                                                   tileResolvedRadiance,
                                                                                   tile_stride,
                                                                                   0,
                                                                                   0,
                                                                                   tile->width,
                                                                                   tile->height) ||
                !RuntimeNative3DDenoise_Apply(tileResolvedRadiance, tile_stride, &tileFeatures)) {
                if (tile_timer_active) {
                    ts_stop_timer("Tile Frame Calc");
                }
                free(tileRadiance);
                free(tileResolvedRadiance);
                RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
                RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
                RuntimeNative3DPreparedFrame_Free(&frame);
                RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
                return false;
            }
            RuntimeNative3DResolveRadianceRegionToPixels(render_buffer,
                                                         render_width,
                                                         tileResolvedRadiance,
                                                         tile_stride,
                                                         tile->originX,
                                                         tile->originY,
                                                         tile->originX + tile->width,
                                                         tile->originY + tile->height);
        } else {
            RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&tileAccumulation,
                                                                             render_buffer,
                                                                             render_width,
                                                                             tile->originX,
                                                                             tile->originY);
        }
        if (tile_timer_active) {
            ts_stop_timer("Tile Frame Calc");
        }
    }

    RuntimeNative3DUpscaleNearestABGR(render_buffer,
                                      render_width,
                                      render_height,
                                      host_buffer,
                                      host_width,
                                      host_height);
    free(tileRadiance);
    free(tileResolvedRadiance);
    RuntimeNative3DAdaptiveSamplingMask_Free(&adaptiveMask);
    RuntimeNative3DTemporalAccumulation_Free(&tileAccumulation);
    RuntimeNative3DPreparedFrame_Free(&frame);
    RuntimeNative3DFeatureBuffer_Free(&tileFeatures);
    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static bool PresentNative3DTilePreviewFrame(SDL_Renderer* renderer,
                                            const IntegratorContext* preview_ctx,
                                            const IntegratorTile* dirty_tile,
                                            const Uint8* preview_buffer,
                                            bool reset_dirty_preview) {
    if (!renderer || !preview_ctx || !preview_buffer) return false;

    SDL_Rect dirty_rect = {0};
    SDL_Rect* dirty_rect_ptr = NULL;
    if (dirty_tile) {
        dirty_rect.x = dirty_tile->originX;
        dirty_rect.y = dirty_tile->originY;
        dirty_rect.w = dirty_tile->width;
        dirty_rect.h = dirty_tile->height;
        dirty_rect_ptr = &dirty_rect;
    }

    if (!RayTracingPreview_DrawNative3DPreviewBaseABGR(renderer,
                                                       preview_buffer,
                                                       preview_ctx->width,
                                                       preview_ctx->height,
                                                       dirty_rect_ptr,
                                                       reset_dirty_preview)) {
        return false;
    }
    ts_render();
    render_end_frame();
    if (render_device_lost()) {
        return false;
    }
    return render_begin_frame();
}

static bool PresentNative3DTilePreviewFrameTimed(SDL_Renderer* renderer,
                                                 const IntegratorContext* preview_ctx,
                                                 const IntegratorTile* dirty_tile,
                                                 const Uint8* preview_buffer,
                                                 bool reset_dirty_preview) {
    ts_stop_timer("Buffer Calc");
    ts_start_timer("Tile Preview Present");
    bool ok = PresentNative3DTilePreviewFrame(renderer,
                                              preview_ctx,
                                              dirty_tile,
                                              preview_buffer,
                                              reset_dirty_preview);
    ts_stop_timer("Tile Preview Present");
    ts_start_timer("Buffer Calc");
    return ok;
}

static bool ShouldPresentNative3DTileSubpassPreview(int subpass,
                                                    int temporal_frames) {
    int completed_subpasses = 0;
    int preview_stride = 1;

    if (subpass < 0 || temporal_frames <= 0) {
        return false;
    }

    completed_subpasses = subpass + 1;
    if (temporal_frames > 16) {
        preview_stride = 8;
    } else if (temporal_frames > 8) {
        preview_stride = 4;
    } else if (temporal_frames > 4) {
        preview_stride = 2;
    }

    return completed_subpasses == 1 ||
           completed_subpasses == temporal_frames ||
           (completed_subpasses % preview_stride) == 0;
}

static bool ResolveNative3DHostDirtyTile(const IntegratorTile* render_tile,
                                         int render_width,
                                         int render_height,
                                         int host_width,
                                         int host_height,
                                         IntegratorTile* out_host_tile) {
    if (!render_tile || !out_host_tile) {
        return false;
    }
    if (!RuntimeNative3DResolveUpscaledRect(render_tile->originX,
                                            render_tile->originY,
                                            render_tile->width,
                                            render_tile->height,
                                            render_width,
                                            render_height,
                                            host_width,
                                            host_height,
                                            &out_host_tile->originX,
                                            &out_host_tile->originY,
                                            &out_host_tile->width,
                                            &out_host_tile->height)) {
        return false;
    }
    out_host_tile->energy = NULL;
    return true;
}

static void RenderHybridTilesPreview(SDL_Renderer* renderer,
                                     IntegratorContext* ctx,
                                     const LightSource* light,
                                     const CameraIntegratorSettings* settings,
                                     double camX,
                                     double camY) {
    if (!renderer || !ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) return;
    if (!settings || !light) return;

    IntegratorDirectContext dctx = {
        .width = ctx->width,
        .height = ctx->height,
        .grid = (UniformGrid*)ctx->uniformGrid,
        .pixelBuffer = ctx->pixelBuffer,
        .energyBuffer = ctx->energyBuffer,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid
    };

    IntegratorIndirectContext ictx = {
        .width = ctx->width,
        .height = ctx->height,
        .grid = (UniformGrid*)ctx->uniformGrid,
        .cache = ctx->cache,
        .energyBuffer = ctx->energyBuffer,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid,
        .objects = ctx->objects,
        .objectCount = ctx->objectCount,
        .materials = (MaterialBSDF*)ctx->materials,
        .materialCount = ctx->materialCount
    };

    Uint8* previewBuffer = tilePreviewBuffer ? tilePreviewBuffer : ctx->pixelBuffer;
    TonemapContext tctx = {
        .width = ctx->width,
        .height = ctx->height,
        .useTiles = ctx->useTiles,
        .tiles = ctx->tileGrid,
        .energyBuffer = ctx->energyBuffer,
        .pixelBuffer = previewBuffer
    };

    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    memset(previewBuffer, 0, total * sizeof(Uint8));

    const int tilesPerPresent = 4;
    const Uint32 presentIntervalMs = 200;
    Uint32 lastPresent = SDL_GetTicks();
    int tilesSincePresent = 0;

    for (size_t ti = 0; ti < ctx->tileGrid->count; ti++) {
        IntegratorTile* tile = &ctx->tileGrid->tiles[ti];
        if (!tile->energy) continue;

        int startX = tile->originX;
        int startY = tile->originY;
        int endX = tile->originX + tile->width;
        int endY = tile->originY + tile->height;

        DirectLightingPassRegion(&dctx,
                                 light,
                                 camX,
                                 camY,
                                 settings->directIntensityScale,
                                 startX, startY, endX, endY);
        IndirectLightingPassRegion(&ictx,
                                   light,
                                   settings->indirectVariance,
                                   settings->indirectHaloRadius,
                                   settings->directIntensityScale,
                                   startX, startY, endX, endY);
        TonemapTile(&tctx, tile);
        DrawTilePreview(renderer, ctx, tile, previewBuffer);

        tilesSincePresent++;
        Uint32 now = SDL_GetTicks();
        if (tilesSincePresent >= tilesPerPresent ||
            (now - lastPresent) >= presentIntervalMs) {
            DrawPreviewBuffer(renderer, ctx, previewBuffer);
            render_end_frame();
            if (render_device_lost()) {
                return;
            }
            if (!render_begin_frame()) {
                return;
            }
            lastPresent = now;
            tilesSincePresent = 0;
        }
    }

    DrawPreviewBuffer(renderer, ctx, previewBuffer);

    if (previewBuffer != ctx->pixelBuffer) {
        tctx.pixelBuffer = ctx->pixelBuffer;
        TonemapTiles(&tctx);
    }
}
