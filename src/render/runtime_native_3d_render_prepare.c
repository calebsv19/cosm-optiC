#include "render/runtime_native_3d_render_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "import/water_surface_import.h"
#include "material/material.h"
#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_map_store_3d.h"
#include "render/runtime_caustic_photon_scene_descriptor_3d.h"
#include "render/runtime_volume_3d_sampling.h"
#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_water_material_3d.h"
#include "render/runtime_native_3d_prepare_diagnostics.h"
#include "render/runtime_native_3d_prepared_scene_cache_internal.h"
#include "render/runtime_scene_accel_3d.h"
#include "scene/object_manager.h"

static const double kRuntimeNative3DWaterSurfaceReflectivity = 0.12;
static const double kRuntimeNative3DWaterSurfaceRoughness = 0.02;

static double runtime_native_3d_water_material_or_default(double value, double fallback) {
    return isfinite(value) && value >= 0.0 ? value : fallback;
}

static bool gRuntimeNative3DInspectionCameraPositionEnabled = false;
static bool gRuntimeNative3DInspectionCameraLookAtEnabled = false;
static bool gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled = false;
static Vec3 gRuntimeNative3DInspectionCameraPosition = {0};
static Vec3 gRuntimeNative3DInspectionCameraLookAt = {0};
static RuntimeCausticPhotonIntegrationSettings3D
    gRuntimeNative3DCausticPhotonRenderPrepSettings;
static RuntimeCausticPhotonMapStore3D gRuntimeNative3DCausticPhotonMapStore;
static const char* kRuntimeNative3DFrameBVHSkipOnTLASEnv =
    "RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS";
static const char* kRuntimeNative3DFrameBVHSkipDefaultDisableEnv =
    "RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP";

static bool runtime_native_3d_env_truthy(const char* name) {
    const char* value = name ? getenv(name) : NULL;
    if (!value || value[0] == '\0') return false;
    if (strcmp(value, "0") == 0 ||
        strcmp(value, "false") == 0 ||
        strcmp(value, "FALSE") == 0 ||
        strcmp(value, "off") == 0 ||
        strcmp(value, "OFF") == 0 ||
        strcmp(value, "no") == 0 ||
        strcmp(value, "NO") == 0) {
        return false;
    }
    return true;
}

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

void RuntimeNative3DRender_SetCausticPhotonRenderPrepPopulation(
    bool enabled,
    const RuntimeCausticPhotonIntegrationSettings3D* settings) {
    gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled = enabled;
    if (settings) {
        gRuntimeNative3DCausticPhotonRenderPrepSettings = *settings;
    } else {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(
            &gRuntimeNative3DCausticPhotonRenderPrepSettings);
    }
    if (!enabled) RuntimeNative3DRender_ResetCausticPhotonMapStore();
}

void RuntimeNative3DRender_ResetCausticPhotonMapStore(void) {
    RuntimeCausticPhotonMapStore3D_Free(&gRuntimeNative3DCausticPhotonMapStore);
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

    if (scene->lightSet.lightCount > 0) {
        RuntimeSceneBridge3DScaffoldState scaffold = {0};
        runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
        if (runtime_native_3d_render_should_sample_authored_motion() &&
            scaffold.has_light_path &&
            RuntimeScene3DSampleAuthoredLight(normalized_t, &sampled_light)) {
            sampled_light.radius =
                (sampled_light.radius > 0.0)
                    ? sampled_light.radius
                    : runtime_native_3d_render_resolve_default_light_radius(scene);
            sampled_light.intensity = animSettings.lightIntensity;
            sampled_light.falloffDistance = animSettings.forwardDecay;
            sampled_light.falloffMode = animSettings.forwardFalloffMode;
            scene->light = sampled_light;
            scene->hasLight = true;
            (void)RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(&scene->lightSet,
                                                                             &scene->light);
            return;
        }
        const RuntimeLightSource3D* first_enabled =
            RuntimeLightSet3D_GetEnabled(&scene->lightSet, 0);
        if (!first_enabled) first_enabled = &scene->lightSet.lights[0];
        scene->light.position = first_enabled->position;
        scene->light.radius = (first_enabled->radius > 0.0)
                                  ? first_enabled->radius
                                  : runtime_native_3d_render_resolve_default_light_radius(scene);
        scene->light.intensity = first_enabled->intensity;
        scene->light.falloffDistance = first_enabled->falloffDistance;
        scene->light.falloffMode = first_enabled->falloffMode;
        scene->hasLight = first_enabled->enabled;
        if (scene->hasLight) {
            (void)RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(&scene->lightSet,
                                                                             &scene->light);
        }
        return;
    }

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
    (void)RuntimeLightSet3D_BuildFromCompatibilityLight(&scene->lightSet,
                                                        &scene->light,
                                                        scene->hasLight);
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
    RuntimeNative3DPreparedSceneCacheStats* stats =
        runtime_native_3d_prepared_scene_dataflow_stats();
    if (!scene) return false;
    if (!runtime_native_3d_prepared_scene_build_or_copy_for_frame(scene, normalized_t)) {
        return false;
    }

    runtime_native_3d_render_apply_live_light(scene,
                                             live_light_x,
                                             live_light_y,
                                             normalized_t);
    (void)width;
    (void)height;
    runtime_native_3d_render_apply_live_camera(scene, normalized_t);
    if (scene->primitiveCount > 0 &&
        scene->triangleMesh.triangleCount > 0 &&
        scene->hasLight &&
        scene->hasCamera) {
        if (stats->dataflowStatsEnabled) {
            stats->lastPrepareValid = true;
        }
        return true;
    }
    return false;
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
    double water_transparency = 0.92;
    double water_ior = 1.333;
    double water_absorption_distance = 4.0;
    if (!object || !water) return;

    water_transparency = runtime_native_3d_render_clamp01(water_transparency);
    if (water->material.valid) {
        water_ior = fmax(1.0, fmin(4.0, water->material.ior));
        water_absorption_distance = fmax(water->material.absorption_distance_m, 1e-6);
    }

    RuntimeWaterMaterial3D_ComputeTransmittanceTint(
        water_absorption_distance,
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
    object->alpha = water_transparency;
    object->reflectivity = runtime_native_3d_water_material_or_default(
        water->material.reflectivity,
        kRuntimeNative3DWaterSurfaceReflectivity);
    object->roughness = runtime_native_3d_water_material_or_default(
        water->material.roughness,
        kRuntimeNative3DWaterSurfaceRoughness);
    object->material_id = MATERIAL_PRESET_TRANSPARENT;
    SceneObjectSeedGlassTransportOverrideFromMaterial(object);
    object->glassTransmission = water_transparency;
    object->glassIor = water_ior;
    object->glassAbsorptionDistance = water_absorption_distance;
    object->glassThinWalled = false;
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
    override.reflectivity = runtime_native_3d_water_material_or_default(
        water->material.reflectivity,
        kRuntimeNative3DWaterSurfaceReflectivity);
    override.roughness = runtime_native_3d_water_material_or_default(
        water->material.roughness,
        kRuntimeNative3DWaterSurfaceRoughness);
    return RuntimeWaterMaterial3D_Set(scene_object_index, &override);
}

static void runtime_native_3d_record_water_surface_accel_lifecycle(
    const RuntimeWaterSurfaceFrame* water,
    const RuntimeScene3D* scene,
    int first_triangle_index,
    int appended_triangle_count) {
    RuntimeDynamicGeometryAcceleration3DInput input = {0};
    RuntimeDynamicGeometryAcceleration3DClassification classification = {0};

    if (!water) return;
    input.water_surface_source_found = true;
    input.water_surface_loaded = water->valid;
    input.water_surface_frame_selection_built = true;
    input.water_surface_frame_selection_dynamic = false;
    input.water_surface_mesh_attached = appended_triangle_count > 0;
    input.water_surface_first_frame_index = water->frame_index;
    input.water_surface_last_frame_index = water->frame_index;
    input.water_surface_first_grid_w = water->grid_w;
    input.water_surface_first_grid_d = water->grid_d;
    input.water_surface_first_sample_count = water->sample_count;
    input.water_surface_last_grid_w = water->grid_w;
    input.water_surface_last_grid_d = water->grid_d;
    input.water_surface_last_sample_count = water->sample_count;
    input.water_surface_triangle_count = appended_triangle_count;

    RuntimeDynamicGeometryAcceleration3D_Classify(&input, &classification);
    (void)RuntimeDynamicGeometryAcceleration3D_RecordWaterSurfaceFrame(
        &classification,
        water->frame_index,
        appended_triangle_count);
    (void)RuntimeDynamicGeometryAcceleration3D_StoreWaterSurfaceMeshFromScene(
        scene,
        first_triangle_index,
        appended_triangle_count);
}

static bool runtime_native_3d_render_attach_configured_water_surface(RuntimeScene3D* scene,
                                                                     int frame_index) {
    RuntimeWaterSurfaceFrame water = {0};
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    bool found = false;
    char diagnostics[1024] = {0};
    int scene_object_index = -1;
    int first_water_triangle_index = 0;
    int appended_triangle_count = 0;

    if (!scene) return false;
    if (animSettings.volumeSourceKind != VOLUME_SOURCE_MANIFEST ||
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

    first_water_triangle_index = scene->triangleMesh.triangleCount;
    if (!RuntimeScene3DBuilder_AppendHeightfieldSurface(scene,
                                                        &desc,
                                                        &appended_triangle_count)) {
        runtime_native_3d_prepare_frame_set_diag("water surface mesh append failed");
        RuntimeWaterSurfaceFrame_Free(&water);
        return false;
    }
    runtime_native_3d_record_water_surface_accel_lifecycle(&water,
                                                           scene,
                                                           first_water_triangle_index,
                                                           appended_triangle_count);

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

static void runtime_native_3d_prepare_record_frame_bvh_stats(
    const RuntimeScene3D* scene,
    bool ready) {
    RuntimeNative3DPreparedSceneCacheStats* stats =
        runtime_native_3d_prepared_scene_dataflow_stats();
    RuntimeTriangleBVH3DBuildStats bvh_stats = {0};
    if (!stats || !stats->dataflowStatsEnabled) return;
    stats->lastFrameBVHReady = ready;
    stats->lastFrameBVHTriangleCount =
        scene ? scene->triangleMesh.triangleCount : 0;
    stats->lastFrameBVHNodeCount = 0;
    stats->lastFrameBVHTotalBytes = 0u;
    if (scene &&
        RuntimeTriangleMesh3D_BVHBuildStats(&scene->triangleMesh, &bvh_stats)) {
        stats->lastFrameBVHTriangleCount = bvh_stats.triangleCount;
        stats->lastFrameBVHNodeCount = bvh_stats.nodeCount;
        stats->lastFrameBVHTotalBytes = bvh_stats.totalBytes;
        stats->lastFrameBVHReady = bvh_stats.ready;
    }
}

static bool runtime_native_3d_prepare_try_skip_frame_bvh_for_tlas(
    const RuntimeScene3D* scene) {
    struct timespec bind_started_at = {0};
    RuntimeNative3DPreparedSceneCacheStats* stats =
        runtime_native_3d_prepared_scene_dataflow_stats();
    RuntimeRay3DTraceRoute route = RuntimeRay3D_CurrentTraceRoute();
    bool force_enabled = runtime_native_3d_env_truthy(
        kRuntimeNative3DFrameBVHSkipOnTLASEnv);
    bool default_disabled = runtime_native_3d_env_truthy(
        kRuntimeNative3DFrameBVHSkipDefaultDisableEnv);
    bool default_enabled =
        route == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS && !default_disabled;
    bool skip_enabled = force_enabled || default_enabled;
    bool tlas_ready = false;

    if (stats && stats->dataflowStatsEnabled) {
        stats->flattenedBVHSkipOnTLASEnabled = skip_enabled;
        stats->flattenedBVHSkipOnTLASDefaultEnabled = default_enabled;
        stats->flattenedBVHSkipOnTLASForceEnabled = force_enabled;
        stats->flattenedBVHSkipOnTLASDefaultDisabled = default_disabled;
        stats->lastFrameBVHRequired = true;
        stats->lastFrameBVHSkippedForTLAS = false;
        stats->lastTLASReadyForFrameBVHSkip = false;
        stats->lastFrameBVHSkipDecision =
            RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_NOT_REQUESTED;
    }
    if (route != RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS) {
        if (stats && stats->dataflowStatsEnabled) {
            stats->lastFrameBVHSkipDecision =
                RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_ROUTE_REQUIRES_FLATTENED_BVH;
        }
        return false;
    }
    if (default_disabled && !force_enabled) {
        if (stats && stats->dataflowStatsEnabled) {
            stats->lastFrameBVHSkipDecision =
                RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_DISABLED_BY_ENV;
        }
        return false;
    }
    if (!skip_enabled) {
        return false;
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &bind_started_at);
    tlas_ready = RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(scene);
    if (stats && stats->dataflowStatsEnabled) {
        const double bind_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&bind_started_at);
        stats->frameBVHTLASReadinessChecks += 1u;
        stats->frameBVHTLASReadinessBindMsTotal += bind_ms;
        stats->lastFrameBVHTLASReadinessBindMs = bind_ms;
        stats->lastTLASReadyForFrameBVHSkip = tlas_ready;
    }
    if (!tlas_ready) {
        if (stats && stats->dataflowStatsEnabled) {
            stats->lastFrameBVHSkipDecision =
                RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_TLAS_BIND_NOT_READY;
        }
        return false;
    }

    if (stats && stats->dataflowStatsEnabled) {
        stats->frameBVHSkipForTLASCalls += 1u;
        stats->lastFrameBVHRequired = false;
        stats->lastFrameBVHSkippedForTLAS = true;
        stats->lastFrameBVHSkipDecision =
            RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_SKIPPED_TLAS_READY;
        runtime_native_3d_prepare_record_frame_bvh_stats(scene, false);
    }
    return true;
}

static bool runtime_native_3d_prepare_ensure_frame_bvh(RuntimeScene3D* scene) {
    struct timespec ensure_started_at = {0};
    RuntimeNative3DPreparedSceneCacheStats* stats =
        runtime_native_3d_prepared_scene_dataflow_stats();
    bool had_ready_bvh = scene && RuntimeTriangleMesh3D_HasReadyBVH(&scene->triangleMesh);
    bool ok = false;

    (void)clock_gettime(CLOCK_MONOTONIC, &ensure_started_at);
    ok = runtime_native_3d_render_ensure_ready_bvh(scene);
    if (stats && stats->dataflowStatsEnabled) {
        const double ensure_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&ensure_started_at);
        stats->frameBVHEnsureCalls += 1u;
        stats->frameBVHEnsureMsTotal += ensure_ms;
        stats->lastFrameBVHEnsureMs = ensure_ms;
        stats->lastFrameBVHRequired = true;
        if (had_ready_bvh) {
            stats->frameBVHAlreadyReadyCalls += 1u;
        } else if (ok) {
            stats->frameBVHBuildCalls += 1u;
        }
        runtime_native_3d_prepare_record_frame_bvh_stats(scene, ok);
    }
    return ok;
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

static bool runtime_native_3d_prepare_harvest_photon_mesh_dielectric(
    const RuntimeScene3D* scene,
    RuntimeCausticLensShape3D* out_shape,
    RuntimeTriangle3D* out_entry_triangle,
    RuntimeCausticPhotonMapPopulationReadback3D* io_population) {
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    const RuntimeCausticPhotonMeshDielectricDescriptor3D* selected = NULL;

    if (io_population) {
        io_population->preparedSceneMeshDielectricAttempted = true;
        io_population->preparedSceneMeshDielectricSceneObjectIndex = -1;
        io_population->preparedSceneMeshDielectricPrimitiveIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleIndex = -1;
        io_population->preparedSceneMeshDielectricTriangleCount = 0;
    }
    if (!scene || !out_shape || !out_entry_triangle) return false;
    if (!RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(scene,
                                                                          &batch)) {
        if (io_population) {
            io_population->preparedSceneMeshDielectricCandidateCount =
                batch.meshDielectricCandidateCount;
        }
        return false;
    }

    selected = RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(&batch);
    if (!selected) return false;
    *out_shape = selected->shape;
    *out_entry_triangle = selected->entryTriangle;
    if (io_population) {
        io_population->preparedSceneMeshDielectricSucceeded = true;
        io_population->preparedSceneMeshDielectricCandidateCount =
            batch.meshDielectricCandidateCount;
        io_population->preparedSceneMeshDielectricSceneObjectIndex =
            selected->sceneObjectIndex;
        io_population->preparedSceneMeshDielectricPrimitiveIndex =
            selected->primitiveIndex;
        io_population->preparedSceneMeshDielectricTriangleIndex =
            selected->triangleIndex;
        io_population->preparedSceneMeshDielectricTriangleCount =
            selected->triangleCount;
    }
    return true;
}

static void runtime_native_3d_prepare_populate_photon_render_prep(
    RuntimeNative3DPreparedFrame* frame) {
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonMap3D* surface_map;
    RuntimeCausticBeamMap3D* beam_map;
    RuntimeCausticPhotonMapPopulationReadback3D population;
    RuntimeCausticPhotonMapPopulationReadback3D volume_population;
    RuntimeCausticPhotonMapPopulationReadback3D harvest;
    RuntimeCausticPhotonReceiverSelection3D receiver;
    RuntimeCausticPhotonIntegrationQuery3D volume_query;
    RuntimeCausticPhotonMapLifecycleInput3D lifecycle_input;
    RuntimeCausticPhotonMapLifecycleReadback3D lifecycle_readback;
    RuntimeCausticLensShape3D shape;
    RuntimeTriangle3D triangle;
    struct timespec fingerprint_started_at = {0};
    struct timespec map_build_started_at = {0};
    struct timespec query_started_at = {0};
    double map_build_cpu_ms = 0.0;
    uint64_t cache_capacity = 1u;

    if (!frame || !gRuntimeNative3DCausticPhotonRenderPrepPopulationEnabled) return;

    settings = gRuntimeNative3DCausticPhotonRenderPrepSettings;
    if (RuntimeCausticPhotonIntegration3D_RouteForSettings(&settings) !=
        RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY) {
        return;
    }
    if (!settings.renderContributionEnabled || !settings.surfaceQueryEnabled) return;
    if (settings.sampleBudget <= 0) settings.sampleBudget = 8;
    if (!(settings.surfaceQueryRadius > 0.0)) settings.surfaceQueryRadius = 0.20;
    if (!(settings.volumeQueryRadius > 0.0)) settings.volumeQueryRadius = 0.20;
    cache_capacity = (uint64_t)settings.sampleBudget;

    memset(&harvest, 0, sizeof(harvest));
    if (!runtime_native_3d_prepare_harvest_photon_mesh_dielectric(&frame->scene,
                                                                  &shape,
                                                                  &triangle,
                                                                  &harvest)) {
        return;
    }

    if (!RuntimeCausticSurfaceCache3D_IsAllocated(&frame->causticSurfaceCache)) {
        (void)RuntimeCausticSurfaceCache3D_Allocate(&frame->causticSurfaceCache,
                                                    cache_capacity);
    }
    if (settings.volumeQueryEnabled &&
        !RuntimeCausticVolumeCache3D_IsAllocated(&frame->causticVolumeCache)) {
        (void)RuntimeCausticVolumeCache3D_AllocateFromVolume(&frame->causticVolumeCache,
                                                             &frame->scene.volume);
    }

    clock_gettime(CLOCK_MONOTONIC, &fingerprint_started_at);
    RuntimeCausticPhotonMapLifecycle3D_BuildInputFromScene(
        &frame->scene,
        settings.sampleBudget,
        settings.maxPathDepth,
        settings.surfaceQueryRadius,
        settings.volumeQueryRadius,
        settings.volumeQueryEnabled,
        true,
        &lifecycle_input);
    if (!RuntimeCausticPhotonMapStore3D_Begin(
        &gRuntimeNative3DCausticPhotonMapStore,
        &lifecycle_input,
        &lifecycle_readback)) {
        return;
    }
    lifecycle_readback.fingerprintCpuMs =
        runtime_native_3d_prepare_elapsed_ms_since(&fingerprint_started_at);
    lifecycle_readback.budgetTier = RuntimeCausticPhotonBudgetTier3D_FromBudget(
        settings.sampleBudget,
        settings.maxPathDepth);
    surface_map = &gRuntimeNative3DCausticPhotonMapStore.surfaceMap;
    beam_map = &gRuntimeNative3DCausticPhotonMapStore.beamMap;

    if (lifecycle_readback.rebuilt) {
        clock_gettime(CLOCK_MONOTONIC, &map_build_started_at);
        memset(&population, 0, sizeof(population));
        memset(&receiver, 0, sizeof(receiver));
        if (!RuntimeCausticPhotonIntegration3D_PopulateReceiverSurfaceMapFromMeshDielectricScene(
                surface_map,
                &frame->scene,
                &shape,
                &triangle,
                &settings,
                &population,
                &receiver)) {
            population.preparedSceneMeshDielectricAttempted =
                harvest.preparedSceneMeshDielectricAttempted;
            population.preparedSceneMeshDielectricSucceeded =
                harvest.preparedSceneMeshDielectricSucceeded;
            population.preparedSceneMeshDielectricCandidateCount =
                harvest.preparedSceneMeshDielectricCandidateCount;
            RuntimeCausticPhotonMapStore3D_CommitPopulation(
                &gRuntimeNative3DCausticPhotonMapStore,
                &population);
            frame->causticPhotonRenderPrepReadback.mapLifecycle = lifecycle_readback;
            frame->causticPhotonRenderPrepReadback.mapPopulation = population;
            frame->causticPhotonRenderPrepReadbackBuilt = true;
            return;
        }
        population.preparedSceneMeshDielectricAttempted =
            harvest.preparedSceneMeshDielectricAttempted;
        population.preparedSceneMeshDielectricSucceeded =
            harvest.preparedSceneMeshDielectricSucceeded;
        population.fixtureMeshDielectricFallbackUsed =
            harvest.fixtureMeshDielectricFallbackUsed;
        population.preparedSceneMeshDielectricCandidateCount =
            harvest.preparedSceneMeshDielectricCandidateCount;
        population.preparedSceneMeshDielectricSceneObjectIndex =
            harvest.preparedSceneMeshDielectricSceneObjectIndex;
        population.preparedSceneMeshDielectricPrimitiveIndex =
            harvest.preparedSceneMeshDielectricPrimitiveIndex;
        population.preparedSceneMeshDielectricTriangleIndex =
            harvest.preparedSceneMeshDielectricTriangleIndex;
        population.preparedSceneMeshDielectricTriangleCount =
            harvest.preparedSceneMeshDielectricTriangleCount;

        if (settings.volumeQueryEnabled) {
            RuntimeCausticPhotonIntegration3D_DefaultQuery(&volume_query);
            volume_query.querySurface = false;
            volume_query.queryVolume = true;
            volume_query.volume.mediumId = 1;
            volume_query.volume.requireMediumId = true;
            volume_query.volume.radius = settings.volumeQueryRadius;
            memset(&volume_population, 0, sizeof(volume_population));
            if (RuntimeCausticPhotonIntegration3D_PopulateMapsFromMeshDielectricFixture(
                    NULL,
                    beam_map,
                    &frame->scene.lightSet,
                    &shape,
                    &triangle,
                    &settings,
                    &volume_query,
                    &volume_population)) {
                population.volumeBeamMapAllocated = volume_population.volumeBeamMapAllocated;
                population.volumeBeamPopulationAttempted =
                    volume_population.volumeBeamPopulationAttempted;
                population.volumeBeamPopulated = volume_population.volumeBeamPopulated;
                population.volumeBeamStoreAttemptCount =
                    volume_population.volumeBeamStoreAttemptCount;
                population.volumeBeamStoreAcceptedCount =
                    volume_population.volumeBeamStoreAcceptedCount;
                population.volumeBeamStoreRejectedCount =
                    volume_population.volumeBeamStoreRejectedCount;
                population.volumeBeamSegmentCount = volume_population.volumeBeamSegmentCount;
                population.volumeBeamAccelerationInsertedCount =
                    volume_population.volumeBeamAccelerationInsertedCount;
                population.totalStoredVolumeFlux = volume_population.totalStoredVolumeFlux;
            }
        }
        map_build_cpu_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&map_build_started_at);
        RuntimeCausticPhotonMapStore3D_CommitPopulation(
            &gRuntimeNative3DCausticPhotonMapStore,
            &population);
    } else {
        population = gRuntimeNative3DCausticPhotonMapStore.population;
    }

    memset(&frame->causticPhotonRenderPrepReadback,
           0,
           sizeof(frame->causticPhotonRenderPrepReadback));
    frame->causticPhotonRenderPrepReadback.mapLifecycle = lifecycle_readback;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.mapBuildCpuMs =
        map_build_cpu_ms;
    frame->causticPhotonRenderPrepReadback.productMode = settings.productMode;
    frame->causticPhotonRenderPrepReadback.route =
        RuntimeCausticPhotonIntegration3D_RouteForSettings(&settings);
    frame->causticPhotonRenderPrepReadback.renderContributionSuppressed =
        !settings.renderContributionEnabled;
    frame->causticPhotonRenderPrepReadback.queryAttempted = true;
    frame->causticPhotonRenderPrepReadback.contributionAttempted = true;
    frame->causticPhotonRenderPrepReadback.cacheDepositAttempted = true;
    clock_gettime(CLOCK_MONOTONIC, &query_started_at);
    (void)RuntimeCausticPhotonIntegration3D_DepositSurfaceContributionsForReceiverBuckets(
        surface_map,
        &frame->causticSurfaceCache,
        &settings,
        &frame->causticPhotonRenderPrepReadback.receiverContribution);
    frame->causticPhotonRenderPrepReadback.queryHit =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverQueryHitCount > 0u;
    frame->causticPhotonRenderPrepReadback.contributionEligible =
        frame->causticPhotonRenderPrepReadback.receiverContribution.eligible;
    frame->causticPhotonRenderPrepReadback.surfaceDeposited =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceDepositAcceptedCount > 0u;
    frame->causticPhotonRenderPrepReadback.surfaceCandidateCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceCandidateCount;
    frame->causticPhotonRenderPrepReadback.surfaceContributingCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceContributingCount;
    frame->causticPhotonRenderPrepReadback.estimatedCost =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverQueryAttemptCount;
    frame->causticPhotonRenderPrepReadback.radiance =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceRadiance;
    if (settings.volumeQueryEnabled &&
        RuntimeCausticVolumeCache3D_IsAllocated(&frame->causticVolumeCache)) {
        RuntimeCausticPhotonIntegration3D_DefaultQuery(&volume_query);
        volume_query.querySurface = false;
        volume_query.queryVolume = true;
        volume_query.volume.mediumId = 1;
        volume_query.volume.requireMediumId = true;
        volume_query.volume.radius = settings.volumeQueryRadius;
        if (RuntimeCausticPhotonIntegration3D_SelectVolumeBeamQueryForVolume(
                beam_map,
                &frame->scene.volume,
                volume_query.volume.mediumId,
                settings.volumeQueryRadius,
                &volume_query.volume)) {
            (void)RuntimeCausticPhotonIntegration3D_DepositVolumeContributionFromBeamMap(
                beam_map,
                &frame->causticVolumeCache,
                &frame->scene.volume,
                &settings,
                &volume_query.volume,
                &frame->causticPhotonRenderPrepReadback.beamContribution);
        }
    }
    frame->causticPhotonRenderPrepReadback.mapLifecycle.queryAndDepositCpuMs =
        runtime_native_3d_prepare_elapsed_ms_since(&query_started_at);
    frame->causticPhotonRenderPrepReadback.volumeDeposited =
        frame->causticPhotonRenderPrepReadback.beamContribution.volumeDeposited;
    frame->causticPhotonRenderPrepReadback.volumeCandidateCount =
        frame->causticPhotonRenderPrepReadback.beamContribution.candidateCount;
    frame->causticPhotonRenderPrepReadback.volumeContributingCount =
        frame->causticPhotonRenderPrepReadback.beamContribution.contributingCount;
    frame->causticPhotonRenderPrepReadback.estimatedCost +=
        frame->causticPhotonRenderPrepReadback.beamContribution.queryAttemptCount;
    frame->causticPhotonRenderPrepReadback.radiance = vec3_add(
        frame->causticPhotonRenderPrepReadback.radiance,
        frame->causticPhotonRenderPrepReadback.beamContribution.radiance);
    frame->causticPhotonRenderPrepReadback.mapLifecycle.emissionCount =
        lifecycle_readback.rebuilt ? population.emittedPhotonCount : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.tracedCount =
        lifecycle_readback.rebuilt ? population.traceRecordCount : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.storedSurfaceRecordCount =
        population.surfaceMapRecordCount;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.storedBeamSegmentCount =
        population.volumeBeamSegmentCount;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.accelerationBuildCount =
        lifecycle_readback.rebuilt
            ? population.surfaceMapAccelerationInsertedCount +
                  population.volumeBeamAccelerationInsertedCount
            : 0u;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.queryCount =
        frame->causticPhotonRenderPrepReadback.estimatedCost;
    frame->causticPhotonRenderPrepReadback.mapLifecycle.cacheDepositCount =
        frame->causticPhotonRenderPrepReadback.receiverContribution
            .receiverSurfaceDepositAcceptedCount +
        frame->causticPhotonRenderPrepReadback.beamContribution
            .volumeDepositAcceptedCount;
    frame->causticPhotonRenderPrepReadback.mapPopulation = population;
    frame->causticPhotonRenderPrepReadbackBuilt = true;
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
    struct timespec caustic_prep_started_at = {0};

    runtime_native_3d_prepare_frame_set_diag("ok");
    if (!out_frame || width <= 0 || height <= 0) return false;
    RuntimeWaterMaterial3D_ClearAll();

    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeScene3D_Init(&frame.scene);
    RuntimeCausticVolumeCache3D_Init(&frame.causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Init(&frame.causticSurfaceCache);
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
    if (!runtime_native_3d_prepare_try_skip_frame_bvh_for_tlas(&frame.scene) &&
        !runtime_native_3d_prepare_ensure_frame_bvh(&frame.scene)) {
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(&frame.scene);
    (void)clock_gettime(CLOCK_MONOTONIC, &caustic_prep_started_at);
    frame.causticSidecarProbeValid =
        RuntimeDisneyV2_3D_BuildCausticSidecarProbe(&frame.scene,
                                                    &frame.causticSidecarProbe);
    if (!RuntimeCausticTransport3D_PopulateCaches(&frame.scene,
                                                  &frame.causticVolumeCache,
                                                  &frame.causticSurfaceCache,
                                                  &frame.causticTransportDiagnostics)) {
        (void)RuntimeCausticBootstrap3D_PopulateAnalyticVolumeCache(
            &frame.scene,
            &frame.causticVolumeCache,
            &frame.causticBootstrapDiagnostics);
    }
    runtime_native_3d_prepare_populate_photon_render_prep(&frame);
    frame.causticCachePrepMs =
        runtime_native_3d_prepare_elapsed_ms_since(&caustic_prep_started_at);

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
        RuntimeCausticVolumeCache3D_Free(&frame.causticVolumeCache);
        RuntimeCausticSurfaceCache3D_Free(&frame.causticSurfaceCache);
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
    out_frame->traceScene = &out_frame->scene;
    runtime_native_3d_prepare_bind_scene_for_frame(&out_frame->scene, true);
    return true;
}
