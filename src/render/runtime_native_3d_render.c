#include "render/runtime_native_3d_render.h"

#include <stddef.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"

static void runtime_native_3d_render_apply_live_light(RuntimeScene3D* scene,
                                                      double live_light_x,
                                                      double live_light_y) {
    RuntimeLight3D light = {0};
    if (!scene) return;

    if (scene->hasLight) {
        light = scene->light;
    }
    light.position = vec3(live_light_x, live_light_y, animSettings.lightHeight);
    light.radius = (light.radius > 0.0) ? light.radius : 10.0;
    light.intensity = animSettings.lightIntensity;
    light.falloffDistance = animSettings.forwardDecay;
    light.falloffMode = animSettings.forwardFalloffMode;
    scene->light = light;
    scene->hasLight = true;
}

static void runtime_native_3d_render_apply_live_camera(RuntimeScene3D* scene,
                                                       double normalized_t) {
    RuntimeCamera3D camera = {0};
    RuntimeCamera3D sampled = {0};
    if (!scene) return;

    if (scene->hasCamera) {
        camera = scene->camera;
    }
    camera.position = vec3(sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.cameraZ);
    camera.rotation = sceneSettings.camera.rotation;
    camera.zoom = (sceneSettings.camera.zoom > 0.0) ? sceneSettings.camera.zoom : 1.0;
    camera.nearPlane = (camera.nearPlane > 0.0) ? camera.nearPlane : 0.1;
    camera.lookPitch = 0.0;
    if (!animSettings.interactiveMode &&
        RuntimeScene3DSampleAuthoredCamera(normalized_t, &sampled)) {
        camera.lookPitch = sampled.lookPitch;
    }

    scene->camera = camera;
    scene->hasCamera = true;
}

static bool runtime_native_3d_render_build_live_scene(RuntimeScene3D* scene,
                                                      double normalized_t,
                                                      double live_light_x,
                                                      double live_light_y) {
    if (!scene) return false;
    if (!RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, normalized_t)) {
        return false;
    }

    runtime_native_3d_render_apply_live_light(scene, live_light_x, live_light_y);
    runtime_native_3d_render_apply_live_camera(scene, normalized_t);
    return scene->primitiveCount > 0 &&
           scene->triangleMesh.triangleCount > 0 &&
           scene->hasLight &&
           scene->hasCamera;
}

bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats) {
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeNative3DRenderStats stats = {0};
    bool ok = false;

    if (!pixel_buffer || width <= 0 || height <= 0) return false;

    memset(pixel_buffer, 0, (size_t)width * (size_t)height);
    RuntimeScene3D_Init(&scene);

    ok = runtime_native_3d_render_build_live_scene(&scene,
                                                   normalized_t,
                                                   live_light_x,
                                                   live_light_y);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    ok = RuntimeCameraProjector3D_Build(&scene.camera, width, height, &projector);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            RuntimeDirectLight3DResult result = {0};
            size_t idx = (size_t)y * (size_t)width + (size_t)x;
            if (!RuntimeDirectLight3D_ShadePixel(&scene,
                                                 &projector,
                                                 (double)x,
                                                 (double)y,
                                                 &result)) {
                continue;
            }
            stats.hitPixelCount += 1;
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            pixel_buffer[idx] = (uint8_t)(TonemapCurve((float)result.radiance) * 255.0f);
        }
    }

    RuntimeScene3D_Free(&scene);
    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}
