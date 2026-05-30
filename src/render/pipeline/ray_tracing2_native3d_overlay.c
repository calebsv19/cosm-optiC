#include "render/pipeline/ray_tracing2_native3d_overlay.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"

typedef struct {
    int centerX;
    int centerY;
    int radius;
} Native3DLightMarkerScreenInfo;

const char* RayTracing2Native3DOverlay_ResolveUpscaleModeLabel(int upscale_mode) {
    switch ((Runtime3DUpscaleMode)upscale_mode) {
        case RUNTIME_3D_UPSCALE_MODE_OFF:
            return "OFF";
        case RUNTIME_3D_UPSCALE_MODE_NEAREST:
            return "Nearest";
        case RUNTIME_3D_UPSCALE_MODE_BILINEAR:
            return "Bilinear";
        default:
            return "Unknown";
    }
}

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
    scene.camera.zoom = (scene.camera.zoom > 0.0) ? scene.camera.zoom : 1.0;
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

static void DrawFilledCircleToRenderer(SDL_Renderer* renderer,
                                       int center_x,
                                       int center_y,
                                       int radius) {
    if (!renderer || radius <= 0) return;

    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        const int dy = y - center_y;
        const double inside = (double)(radius * radius - dy * dy);
        int dx_limit = 0;
        if (inside < 0.0) continue;
        dx_limit = (int)floor(sqrt(inside));
        SDL_RenderDrawLine(renderer,
                           center_x - dx_limit,
                           y,
                           center_x + dx_limit,
                           y);
    }
}

static void DrawResolvedNative3DLightMarker(SDL_Renderer* renderer,
                                            const Native3DLightMarkerScreenInfo* marker) {
    if (!renderer || !marker) return;
    if (marker->radius <= 0) return;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    DrawFilledCircleToRenderer(renderer,
                               marker->centerX,
                               marker->centerY,
                               marker->radius);
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

bool RayTracing2Native3DOverlay_ExportFrameBMP(const char* filename,
                                               int width,
                                               int height,
                                               const Uint8* native3d_preview_buffer,
                                               const Uint8* luminance_buffer) {
    SDL_Surface* surface = NULL;
    const Uint8* source =
        native3d_preview_buffer ? native3d_preview_buffer : luminance_buffer;
    const size_t row_bytes =
        (size_t)width * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;

    if (!filename || !filename[0] || !source || width <= 0 || height <= 0) {
        return false;
    }

    if (native3d_preview_buffer) {
        if (row_bytes / (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES != (size_t)width ||
            row_bytes > (size_t)INT_MAX) {
            fprintf(stderr,
                    "native 3D BMP export rejected invalid row size for %dx%d preview.\n",
                    width,
                    height);
            return false;
        }

        surface = SDL_CreateRGBSurfaceFrom((void*)native3d_preview_buffer,
                                           width,
                                           height,
                                           32,
                                           (int)row_bytes,
                                           0x000000FFu,
                                           0x0000FF00u,
                                           0x00FF0000u,
                                           0xFF000000u);
        if (!surface) {
            fprintf(stderr, "SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
            return false;
        }
    } else {
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    }
    if (!surface) {
        fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
        return false;
    }

    if (!native3d_preview_buffer) {
        for (int y = 0; y < height; ++y) {
            uint32_t* row = (uint32_t*)((uint8_t*)surface->pixels + ((size_t)y * surface->pitch));
            size_t base = (size_t)y * (size_t)width;
            for (int x = 0; x < width; ++x) {
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
