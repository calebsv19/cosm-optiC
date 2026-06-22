#include "render/runtime_native_3d_render_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "import/water_surface_import.h"
#include "material/material.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_water_material_3d.h"
#include "render/runtime_native_3d_prepare_diagnostics.h"
#include "scene/object_manager.h"

static bool gRuntimeNative3DInspectionCameraPositionEnabled = false;
static bool gRuntimeNative3DInspectionCameraLookAtEnabled = false;
static Vec3 gRuntimeNative3DInspectionCameraPosition = {0};
static Vec3 gRuntimeNative3DInspectionCameraLookAt = {0};
static RuntimeScene3D gRuntimeNative3DPreparedSceneCache;
static bool gRuntimeNative3DPreparedSceneCacheInitialized = false;
static bool gRuntimeNative3DPreparedSceneCacheValid = false;
static bool gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
static double gRuntimeNative3DPreparedSceneCacheNormalizedT = 0.0;
static double gRuntimeNative3DPreparedSceneCacheLastRequestedT = 0.0;
static uint64_t gRuntimeNative3DPreparedSceneDirtyGeneration = 1u;
static uint64_t gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheHits = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheMisses = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheStores = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheInvalidations = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheTimeIndependentHits = 0u;

void runtime_native_3d_prepare_frame_set_diag(const char* message) {
    RuntimeNative3DPrepareDiagnostics_Set(message);
}

void runtime_native_3d_prepare_frame_set_diagf(const char* format, ...) {
    va_list args;
    char message[4096];
    if (!format) {
        runtime_native_3d_prepare_frame_set_diag("unknown");
        return;
    }
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    runtime_native_3d_prepare_frame_set_diag(message);
}

const char* RuntimeNative3DPrepareFrameLastDiagnostics(void) {
    return RuntimeNative3DPrepareDiagnostics_Get();
}

static void runtime_native_3d_prepared_scene_cache_ensure_initialized(void) {
    if (gRuntimeNative3DPreparedSceneCacheInitialized) return;
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
    gRuntimeNative3DPreparedSceneCacheInitialized = true;
}

void RuntimeNative3DPreparedSceneMarkDirty(const char* reason) {
    (void)reason;
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    gRuntimeNative3DPreparedSceneDirtyGeneration += 1u;
    if (gRuntimeNative3DPreparedSceneCacheValid) {
        gRuntimeNative3DPreparedSceneCacheInvalidations += 1u;
    }
    gRuntimeNative3DPreparedSceneCacheValid = false;
    gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
    RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
}

void RuntimeNative3DPreparedSceneCacheStatsSnapshot(
    RuntimeNative3DPreparedSceneCacheStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    out_stats->generation = gRuntimeNative3DPreparedSceneDirtyGeneration;
    out_stats->cachedGeneration = gRuntimeNative3DPreparedSceneCachedGeneration;
    out_stats->valid = gRuntimeNative3DPreparedSceneCacheValid;
    out_stats->hits = gRuntimeNative3DPreparedSceneCacheHits;
    out_stats->misses = gRuntimeNative3DPreparedSceneCacheMisses;
    out_stats->stores = gRuntimeNative3DPreparedSceneCacheStores;
    out_stats->invalidations = gRuntimeNative3DPreparedSceneCacheInvalidations;
    out_stats->staticGeometryReuseEnabled =
        gRuntimeNative3DPreparedSceneCacheStaticGeometry;
    out_stats->timeIndependentHits =
        gRuntimeNative3DPreparedSceneCacheTimeIndependentHits;
    out_stats->cachedNormalizedT = gRuntimeNative3DPreparedSceneCacheNormalizedT;
    out_stats->lastRequestedNormalizedT =
        gRuntimeNative3DPreparedSceneCacheLastRequestedT;
    if (gRuntimeNative3DPreparedSceneCacheValid) {
        out_stats->cachedPrimitiveCount =
            gRuntimeNative3DPreparedSceneCache.primitiveCount;
        out_stats->cachedTriangleCount =
            gRuntimeNative3DPreparedSceneCache.triangleMesh.triangleCount;
        out_stats->cachedBVHNodeCount =
            RuntimeTriangleMesh3D_BVHNodeCount(
                &gRuntimeNative3DPreparedSceneCache.triangleMesh);
        out_stats->cachedBVHLeafCount =
            RuntimeTriangleMesh3D_BVHLeafCount(
                &gRuntimeNative3DPreparedSceneCache.triangleMesh);
    }
}

void RuntimeNative3DPreparedSceneCacheResetForTests(void) {
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
    gRuntimeNative3DPreparedSceneCacheValid = false;
    gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
    gRuntimeNative3DPreparedSceneCacheNormalizedT = 0.0;
    gRuntimeNative3DPreparedSceneCacheLastRequestedT = 0.0;
    gRuntimeNative3DPreparedSceneDirtyGeneration = 1u;
    gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
    gRuntimeNative3DPreparedSceneCacheHits = 0u;
    gRuntimeNative3DPreparedSceneCacheMisses = 0u;
    gRuntimeNative3DPreparedSceneCacheStores = 0u;
    gRuntimeNative3DPreparedSceneCacheInvalidations = 0u;
    gRuntimeNative3DPreparedSceneCacheTimeIndependentHits = 0u;
}

void RuntimeNative3DRender_ResetInspectionCameraOverrides(void) {
    gRuntimeNative3DInspectionCameraPositionEnabled = false;
    gRuntimeNative3DInspectionCameraLookAtEnabled = false;
    gRuntimeNative3DInspectionCameraPosition = vec3(0.0, 0.0, 0.0);
    gRuntimeNative3DInspectionCameraLookAt = vec3(0.0, 0.0, 0.0);
}

void RuntimeNative3DRender_SetInspectionCameraPosition(Vec3 position) {
    gRuntimeNative3DInspectionCameraPositionEnabled = true;
    gRuntimeNative3DInspectionCameraPosition = position;
}

void RuntimeNative3DRender_SetInspectionCameraLookAt(Vec3 target) {
    gRuntimeNative3DInspectionCameraLookAtEnabled = true;
    gRuntimeNative3DInspectionCameraLookAt = target;
}

static double runtime_native_3d_render_resolve_default_light_radius(
    const RuntimeScene3D* scene) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;
    double span_max = 0.0;
    double radius = 0.0;

    if (!scene || scene->triangleMesh.triangleCount <= 0) {
        return 0.12;
    }

    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* tri = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {tri->p0, tri->p1, tri->p2};

        for (int p = 0; p < 3; ++p) {
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

static bool runtime_native_3d_render_scene_center(const RuntimeScene3D* scene, Vec3* out_center) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;

    if (out_center) *out_center = vec3(0.0, 0.0, 0.0);
    if (!scene || !out_center || scene->triangleMesh.triangleCount <= 0) {
        return false;
    }

    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* tri = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {tri->p0, tri->p1, tri->p2};
        for (int p = 0; p < 3; ++p) {
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
        return false;
    }

    *out_center = vec3((min_x + max_x) * 0.5,
                       (min_y + max_y) * 0.5,
                       (min_z + max_z) * 0.5);
    return true;
}

static bool runtime_native_3d_render_should_sample_authored_motion(void) {
    return !animSettings.interactiveMode || animSettings.deepRenderMode;
}

static bool runtime_native_3d_render_camera_path_has_authored_rotation(void) {
    for (int i = 0; i < sceneSettings.cameraPath.numPoints && i < MAX_BEZIER_POINTS; ++i) {
        if (sceneSettings.cameraPath.rotationSet[i]) {
            return true;
        }
    }
    return false;
}

static bool runtime_native_3d_render_camera_path_has_authored_pitch(void) {
    for (int i = 0; i < sceneSettings.cameraPath.numPoints && i < MAX_BEZIER_POINTS; ++i) {
        if (fabs(sceneSettings.cameraPath3D.point_pitch[i]) > 1e-9) {
            return true;
        }
    }
    return false;
}

static void runtime_native_3d_render_apply_focus_target(RuntimeCamera3D* io_camera, Vec3 target) {
    Vec3 to_target = vec3(0.0, 0.0, 0.0);
    double horizontal = 0.0;
    double pitch = 0.0;
    const double max_pitch = 70.0 * M_PI / 180.0;

    if (!io_camera) return;
    to_target = vec3_sub(target, io_camera->position);
    horizontal = hypot(to_target.x, to_target.y);
    if (!(horizontal > 1e-9) && !(fabs(to_target.z) > 1e-9)) {
        return;
    }

    if (horizontal > 1e-9) {
        io_camera->rotation = atan2(to_target.x, -to_target.y);
    }
    pitch = atan2(to_target.z, horizontal);
    if (pitch > max_pitch) pitch = max_pitch;
    if (pitch < -max_pitch) pitch = -max_pitch;
    io_camera->lookPitch = pitch;
}

static void runtime_native_3d_render_apply_auto_look_at_scene(const RuntimeScene3D* scene,
                                                               RuntimeCamera3D* io_camera) {
    Vec3 target = vec3(0.0, 0.0, 0.0);

    if (!scene || !io_camera) return;
    if (!runtime_native_3d_render_scene_center(scene, &target)) return;
    runtime_native_3d_render_apply_focus_target(io_camera, target);
}

static void runtime_native_3d_render_apply_inspection_camera_overrides(
    const RuntimeScene3D* scene,
    RuntimeCamera3D* io_camera,
    bool has_authored_rotation,
    bool has_authored_pitch) {
    if (!io_camera) return;

    if (gRuntimeNative3DInspectionCameraPositionEnabled) {
        io_camera->position = gRuntimeNative3DInspectionCameraPosition;
    }

    if (gRuntimeNative3DInspectionCameraLookAtEnabled) {
        runtime_native_3d_render_apply_focus_target(io_camera,
                                                    gRuntimeNative3DInspectionCameraLookAt);
        return;
    }

    if (gRuntimeNative3DInspectionCameraPositionEnabled &&
        !has_authored_rotation &&
        !has_authored_pitch) {
        runtime_native_3d_render_apply_auto_look_at_scene(scene, io_camera);
    }
}

static void runtime_native_3d_render_apply_live_light(RuntimeScene3D* scene,
                                                      double live_light_x,
                                                      double live_light_y,
                                                      double normalized_t) {
    RuntimeLight3D light = {0};
    RuntimeLight3D sampled_light = {0};
    bool has_authored_light = false;
    if (!scene) return;

    if (runtime_native_3d_render_should_sample_authored_motion() &&
        RuntimeScene3DSampleAuthoredLight(normalized_t, &sampled_light)) {
        light = sampled_light;
        has_authored_light = true;
    } else if (scene->hasLight) {
        light = scene->light;
        has_authored_light = true;
    }
    if (!has_authored_light) {
        light.position = vec3(live_light_x, live_light_y, animSettings.lightHeight);
    }
    light.radius = (light.radius > 0.0) ? light.radius
                                        : runtime_native_3d_render_resolve_default_light_radius(scene);
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
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    bool has_authored_rotation = false;
    bool has_authored_pitch = false;
    if (!scene) return;

    if (scene->hasCamera) {
        camera = scene->camera;
    }
    camera.position = vec3(sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.cameraZ);
    camera.rotation = sceneSettings.camera.rotation;
    camera.zoom = (sceneSettings.camera.zoom > 0.0) ? sceneSettings.camera.zoom : 1.0;
    camera.nearPlane = (camera.nearPlane > 0.0) ? camera.nearPlane : 0.1;
    camera.lookPitch = 0.0;
    if (runtime_native_3d_render_should_sample_authored_motion()) {
        if (RuntimeScene3DSampleAuthoredCamera(normalized_t, &sampled)) {
            camera = sampled;
            camera.nearPlane = (camera.nearPlane > 0.0) ? camera.nearPlane : 0.1;
        }
        runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
        has_authored_rotation = scaffold.has_camera_rotation_seed ||
                                runtime_native_3d_render_camera_path_has_authored_rotation();
        has_authored_pitch = scaffold.has_camera_pitch_seed ||
                             runtime_native_3d_render_camera_path_has_authored_pitch();
        runtime_native_3d_render_apply_inspection_camera_overrides(scene,
                                                                   &camera,
                                                                   has_authored_rotation,
                                                                   has_authored_pitch);
        if (!has_authored_rotation &&
            !has_authored_pitch &&
            !gRuntimeNative3DInspectionCameraLookAtEnabled &&
            !gRuntimeNative3DInspectionCameraPositionEnabled) {
            runtime_native_3d_render_apply_auto_look_at_scene(scene, &camera);
        }
    }

    scene->camera = camera;
    scene->hasCamera = true;
}

static bool runtime_native_3d_render_build_live_scene(RuntimeScene3D* scene,
                                                      int width,
                                                      int height,
                                                      double normalized_t,
                                                      double live_light_x,
                                                      double live_light_y) {
    if (!scene) return false;
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    gRuntimeNative3DPreparedSceneCacheLastRequestedT = normalized_t;
    if (gRuntimeNative3DPreparedSceneCacheValid &&
        gRuntimeNative3DPreparedSceneCachedGeneration ==
            gRuntimeNative3DPreparedSceneDirtyGeneration &&
        (gRuntimeNative3DPreparedSceneCacheStaticGeometry ||
         fabs(gRuntimeNative3DPreparedSceneCacheNormalizedT - normalized_t) <= 1e-12)) {
        gRuntimeNative3DPreparedSceneCacheHits += 1u;
        if (fabs(gRuntimeNative3DPreparedSceneCacheNormalizedT - normalized_t) > 1e-12) {
            gRuntimeNative3DPreparedSceneCacheTimeIndependentHits += 1u;
        }
        if (!RuntimeScene3D_CopyGeometryFrom(scene,
                                             &gRuntimeNative3DPreparedSceneCache)) {
            return false;
        }
        RuntimeScene3D_RefreshMaterialFlags(scene);
    } else {
        RuntimeScene3D built_scene = {0};
        RuntimeScene3D_Init(&built_scene);
        gRuntimeNative3DPreparedSceneCacheMisses += 1u;
        if (!RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&built_scene, normalized_t)) {
            RuntimeScene3D_Free(&built_scene);
            return false;
        }
        RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
        gRuntimeNative3DPreparedSceneCache = built_scene;
        memset(&built_scene, 0, sizeof(built_scene));
        gRuntimeNative3DPreparedSceneCacheValid = true;
        gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
        gRuntimeNative3DPreparedSceneCacheNormalizedT = normalized_t;
        gRuntimeNative3DPreparedSceneCachedGeneration =
            gRuntimeNative3DPreparedSceneDirtyGeneration;
        gRuntimeNative3DPreparedSceneCacheStores += 1u;
        if (!RuntimeScene3D_CopyGeometryFrom(scene,
                                             &gRuntimeNative3DPreparedSceneCache)) {
            return false;
        }
        RuntimeScene3D_RefreshMaterialFlags(scene);
    }

    runtime_native_3d_render_apply_live_light(scene,
                                             live_light_x,
                                             live_light_y,
                                             normalized_t);
    (void)width;
    (void)height;
    runtime_native_3d_render_apply_live_camera(scene, normalized_t);
    return scene->primitiveCount > 0 &&
           scene->triangleMesh.triangleCount > 0 &&
           scene->hasLight &&
           scene->hasCamera;
}

static RuntimeVolume3DSourceKind runtime_native_3d_render_map_volume_source_kind(
    int config_kind) {
    switch (config_kind) {
        case VOLUME_SOURCE_MANIFEST:
            return RUNTIME_VOLUME_3D_SOURCE_MANIFEST;
        case VOLUME_SOURCE_RAW_VF3D:
            return RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
        case VOLUME_SOURCE_PACK:
            return RUNTIME_VOLUME_3D_SOURCE_PACK;
        case VOLUME_SOURCE_NONE:
        default:
            return RUNTIME_VOLUME_3D_SOURCE_NONE;
    }
}

static bool runtime_native_3d_render_attach_configured_volume(RuntimeScene3D* scene,
                                                              int frame_index) {
    RuntimeVolume3DSourceKind source_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[1024] = {0};

    if (!scene) return false;

    scene->volume.affectsLighting = animSettings.volumeAffectsLighting;
    scene->volume.debugOverlayEnabled = animSettings.volumeDebugOverlayEnabled;
    if (!animSettings.volumeInteractionEnabled) {
        return true;
    }

    source_kind = runtime_native_3d_render_map_volume_source_kind(animSettings.volumeSourceKind);
    if (source_kind == RUNTIME_VOLUME_3D_SOURCE_NONE ||
        animSettings.volumeSourcePath[0] == '\0') {
        return false;
    }

    if (!fluid_volume_import_3d_load_source_at_frame(animSettings.volumeSourcePath,
                                                     source_kind,
                                                     frame_index,
                                                     &scene->volume,
                                                     diagnostics,
                                                     sizeof(diagnostics))) {
        runtime_native_3d_prepare_frame_set_diag(diagnostics[0] ? diagnostics
                                                                : "volume import failed");
        RuntimeVolumeAttachment3D_Reset(&scene->volume);
        return false;
    }

    scene->volume.affectsLighting = animSettings.volumeAffectsLighting;
    scene->volume.debugOverlayEnabled = animSettings.volumeDebugOverlayEnabled;
    return true;
}

static void runtime_native_3d_render_configure_water_surface_object(
    SceneObject* object,
    const RuntimeWaterSurfaceFrame* water) {
    unsigned char r = 88u;
    unsigned char g = 178u;
    unsigned char b = 235u;
    double tint_r = 88.0 / 255.0;
    double tint_g = 178.0 / 255.0;
    double tint_b = 235.0 / 255.0;
    if (!object || !water) return;

    RuntimeWaterMaterial3D_ComputeTransmittanceTint(
        water->material.valid ? water->material.absorption_distance_m : 4.0,
        water->material.valid ? water->material.absorption_rgb[0] : 0.10,
        water->material.valid ? water->material.absorption_rgb[1] : 0.035,
        water->material.valid ? water->material.absorption_rgb[2] : 0.015,
        &tint_r,
        &tint_g,
        &tint_b);
    r = (unsigned char)(runtime_native_3d_render_clamp01(tint_r) * 255.0);
    g = (unsigned char)(runtime_native_3d_render_clamp01(tint_g) * 255.0);
    b = (unsigned char)(runtime_native_3d_render_clamp01(tint_b) * 255.0);

    snprintf(object->type, sizeof(object->type), "%s", "water_surface");
    object->x = water->sample_origin_x +
                (((double)water->grid_w - 1.0) * water->sample_spacing_x * 0.5);
    object->y = water->sample_origin_z +
                (((double)water->grid_d - 1.0) * water->sample_spacing_z * 0.5);
    object->z = water->surface_avg_y;
    object->scale = 1.0;
    object->color = SceneObjectPackRGBBytes(r, g, b);
    object->opacity = 1.0;
    object->alpha = 1.0;
    object->reflectivity = 0.0;
    object->roughness = 0.02;
    object->material_id = MATERIAL_PRESET_TRANSPARENT;
    object->dirty = false;
    object->guideOnly = false;
}

static int runtime_native_3d_render_ensure_water_surface_object(
    const RuntimeWaterSurfaceFrame* water) {
    SceneObject* object = NULL;
    if (!water) return -1;
    for (int i = 0; i < sceneSettings.objectCount && i < MAX_OBJECTS; ++i) {
        if (strcmp(sceneSettings.sceneObjects[i].type, "water_surface") == 0) {
            runtime_native_3d_render_configure_water_surface_object(&sceneSettings.sceneObjects[i],
                                                                    water);
            return i;
        }
    }
    if (sceneSettings.objectCount < 0 || sceneSettings.objectCount >= MAX_OBJECTS) {
        return -1;
    }

    object = &sceneSettings.sceneObjects[sceneSettings.objectCount];
    memset(object, 0, sizeof(*object));
    runtime_native_3d_render_configure_water_surface_object(object, water);
    sceneSettings.objectCount += 1;
    return sceneSettings.objectCount - 1;
}

static bool runtime_native_3d_render_apply_water_surface_material(
    int scene_object_index,
    const RuntimeWaterSurfaceFrame* water) {
    RuntimeWaterMaterial3DOverride override = {0};
    if (scene_object_index < 0 || !water) {
        return false;
    }

    override.valid = true;
    override.ior = water->material.valid ? water->material.ior : 1.333;
    override.absorptionDistance =
        water->material.valid ? water->material.absorption_distance_m : 4.0;
    override.absorptionR = water->material.valid ? water->material.absorption_rgb[0]
                                                 : 0.10;
    override.absorptionG = water->material.valid ? water->material.absorption_rgb[1]
                                                 : 0.035;
    override.absorptionB = water->material.valid ? water->material.absorption_rgb[2]
                                                 : 0.015;
    override.transparency = 0.92;
    override.reflectivity = 0.0;
    override.roughness = 0.02;
    return RuntimeWaterMaterial3D_Set(scene_object_index, &override);
}

static bool runtime_native_3d_render_attach_configured_water_surface(RuntimeScene3D* scene,
                                                                     int frame_index) {
    RuntimeWaterSurfaceFrame water = {0};
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    bool found = false;
    char diagnostics[1024] = {0};
    int scene_object_index = -1;
    int appended_triangle_count = 0;

    if (!scene) return false;
    if (!animSettings.volumeInteractionEnabled ||
        animSettings.volumeSourceKind != VOLUME_SOURCE_MANIFEST ||
        animSettings.volumeSourcePath[0] == '\0') {
        return true;
    }

    RuntimeWaterSurfaceFrame_Init(&water);
    if (!RuntimeWaterSurfaceImport_LoadSourceAtFrame(animSettings.volumeSourcePath,
                                                     frame_index,
                                                     &water,
                                                     &found,
                                                     diagnostics,
                                                     sizeof(diagnostics))) {
        runtime_native_3d_prepare_frame_set_diag(diagnostics[0] ? diagnostics
                                                                : "water surface import failed");
        RuntimeWaterSurfaceFrame_Free(&water);
        return false;
    }
    if (!found) {
        RuntimeWaterSurfaceFrame_Free(&water);
        return true;
    }

    scene_object_index = runtime_native_3d_render_ensure_water_surface_object(&water);
    if (scene_object_index < 0) {
        runtime_native_3d_prepare_frame_set_diag("water surface scene object unavailable");
        RuntimeWaterSurfaceFrame_Free(&water);
        return false;
    }
    if (!runtime_native_3d_render_apply_water_surface_material(scene_object_index, &water)) {
        runtime_native_3d_prepare_frame_set_diag("water surface material payload unavailable");
        RuntimeWaterSurfaceFrame_Free(&water);
        return false;
    }

    desc.object_id = "water_surface";
    desc.scene_object_index = scene_object_index;
    desc.grid_w = water.grid_w;
    desc.grid_d = water.grid_d;
    desc.heights_y = water.heights_y;
    desc.sample_origin_x = water.sample_origin_x;
    desc.sample_origin_z = water.sample_origin_z;
    desc.sample_spacing_x = water.sample_spacing_x;
    desc.sample_spacing_z = water.sample_spacing_z;
    desc.dry_height = water.surface_min_y;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = true;
    desc.two_sided = true;
    desc.map_y_height_to_scene_z = true;

    if (!RuntimeScene3DBuilder_AppendHeightfieldSurface(scene,
                                                        &desc,
                                                        &appended_triangle_count)) {
        runtime_native_3d_prepare_frame_set_diag("water surface mesh append failed");
        RuntimeWaterSurfaceFrame_Free(&water);
        return false;
    }

    RuntimeWaterSurfaceFrame_Free(&water);
    return true;
}

static bool runtime_native_3d_render_ensure_ready_bvh(RuntimeScene3D* scene) {
    RuntimeTriangleBVH3DBuildStats stats = {0};
    if (!scene || scene->triangleMesh.triangleCount <= 0) return false;
    if (!RuntimeTriangleMesh3D_HasReadyBVH(&scene->triangleMesh)) {
        if (!RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh)) {
            runtime_native_3d_prepare_frame_set_diagf(
                "triangle BVH build failed: triangle_count=%d lower=%s",
                scene->triangleMesh.triangleCount,
                RuntimeTriangleMesh3D_BVHLastDiagnostics());
            return false;
        }
    }
    if (!RuntimeTriangleMesh3D_BVHBuildStats(&scene->triangleMesh, &stats) ||
        !stats.ready ||
        stats.nodeCount <= 0) {
        runtime_native_3d_prepare_frame_set_diagf(
            "triangle BVH unavailable after build: triangle_count=%d node_count=%d",
            stats.triangleCount,
            stats.nodeCount);
        return false;
    }
    return true;
}

bool RuntimeNative3DPrepareFrame(RuntimeNative3DPreparedFrame* out_frame,
                                 int width,
                                 int height,
                                 double normalized_t,
                                 double live_light_x,
                                 double live_light_y) {
    return RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(out_frame,
                                                               width,
                                                               height,
                                                               normalized_t,
                                                               0,
                                                               live_light_x,
                                                               live_light_y,
                                                               NULL);
}

bool RuntimeNative3DPrepareFrameAtFrameIndex(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             int frame_index,
                                             double live_light_x,
                                             double live_light_y) {
    return RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(out_frame,
                                                               width,
                                                               height,
                                                               normalized_t,
                                                               frame_index,
                                                               live_light_x,
                                                               live_light_y,
                                                               NULL);
}

bool RuntimeNative3DPrepareFrameWithSampling(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             double live_light_x,
                                             double live_light_y,
                                             const RuntimeNative3DSamplingContext* sampling) {
    return RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(out_frame,
                                                               width,
                                                               height,
                                                               normalized_t,
                                                               0,
                                                               live_light_x,
                                                               live_light_y,
                                                               sampling);
}

bool RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(
    RuntimeNative3DPreparedFrame* out_frame,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling) {
    RuntimeNative3DPreparedFrame frame = {0};

    runtime_native_3d_prepare_frame_set_diag("ok");
    if (!out_frame || width <= 0 || height <= 0) return false;
    RuntimeWaterMaterial3D_ClearAll();

    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeScene3D_Init(&frame.scene);
    if (!runtime_native_3d_render_build_live_scene(&frame.scene,
                                                   width,
                                                   height,
                                                   normalized_t,
                                                   live_light_x,
                                                   live_light_y)) {
        runtime_native_3d_prepare_frame_set_diagf(
            "build_live_scene failed: primitive_count=%d triangle_count=%d has_light=%s has_camera=%s builder=%s",
            frame.scene.primitiveCount,
            frame.scene.triangleMesh.triangleCount,
            frame.scene.hasLight ? "true" : "false",
            frame.scene.hasCamera ? "true" : "false",
            RuntimeScene3DBuilder_LastDiagnostics());
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }
    if (!runtime_native_3d_render_attach_configured_volume(&frame.scene, frame_index)) {
        char previous_diagnostics[4096];
        snprintf(previous_diagnostics,
                 sizeof(previous_diagnostics),
                 "%s",
                 RuntimeNative3DPrepareFrameLastDiagnostics());
        runtime_native_3d_prepare_frame_set_diagf(
            "attach_configured_volume failed: %s | volume_enabled=%s source_kind=%d source_path=%s frame_index=%d",
            previous_diagnostics,
            animSettings.volumeInteractionEnabled ? "true" : "false",
            animSettings.volumeSourceKind,
            animSettings.volumeSourcePath,
            frame_index);
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }
    if (!runtime_native_3d_render_attach_configured_water_surface(&frame.scene, frame_index)) {
        char previous_diagnostics[4096];
        snprintf(previous_diagnostics,
                 sizeof(previous_diagnostics),
                 "%s",
                 RuntimeNative3DPrepareFrameLastDiagnostics());
        runtime_native_3d_prepare_frame_set_diagf(
            "attach_configured_water_surface failed: %s | volume_enabled=%s source_kind=%d source_path=%s frame_index=%d",
            previous_diagnostics,
            animSettings.volumeInteractionEnabled ? "true" : "false",
            animSettings.volumeSourceKind,
            animSettings.volumeSourcePath,
            frame_index);
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }
    if (!runtime_native_3d_render_ensure_ready_bvh(&frame.scene)) {
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(&frame.scene);

    if (!RuntimeCameraProjector3D_Build(&frame.scene.camera, width, height, &frame.projector)) {
        runtime_native_3d_prepare_frame_set_diagf(
            "camera_projector_build failed: camera_pos=(%.6f,%.6f,%.6f) rotation=%.6f lookPitch=%.6f zoom=%.6f near=%.6f viewport=%dx%d",
            frame.scene.camera.position.x,
            frame.scene.camera.position.y,
            frame.scene.camera.position.z,
            frame.scene.camera.rotation,
            frame.scene.camera.lookPitch,
            frame.scene.camera.zoom,
            frame.scene.camera.nearPlane,
            width,
            height);
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }

    frame.width = width;
    frame.height = height;
    if (sampling) {
        frame.sampling = *sampling;
    }
    frame.valid = true;
    *out_frame = frame;
    return true;
}
