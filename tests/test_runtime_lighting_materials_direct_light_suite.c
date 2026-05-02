#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_disney_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_volume_3d.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

static int test_runtime_direct_light_3d_shade_pixel_visible_contract(void) {
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, 0.0, 0.0);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = 0.0;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;

    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_visible_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_visible_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 5;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 5;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_visible_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &result);
    assert_true("runtime_direct_light_3d_visible_shade_ok", ok);
    assert_true("runtime_direct_light_3d_visible_hit", result.hit);
    assert_true("runtime_direct_light_3d_visible_los", result.visible);
    assert_close("runtime_direct_light_3d_visible_hit_y", result.hitInfo.position.y, -5.0, 1e-6);
    assert_true("runtime_direct_light_3d_visible_ndotl_positive", result.ndotl > 0.99);
    assert_true("runtime_direct_light_3d_visible_radiance_positive", result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_direct_light_3d_shade_pixel_shadowed_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_shadowed_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_shadowed_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 5;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 6;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "blocker");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 5;
    scene.triangleMesh.triangles[1].p0 = vec3(1.0, -4.5, -2.0);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -2.5, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(1.0, -4.5, 2.0);
    scene.triangleMesh.triangles[1].normal = vec3(1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 6;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 5;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &result);
    assert_true("runtime_direct_light_3d_shadowed_shade_ok", ok);
    assert_true("runtime_direct_light_3d_shadowed_hit", result.hit);
    assert_true("runtime_direct_light_3d_shadowed_not_visible", !result.visible);
    assert_close("runtime_direct_light_3d_shadowed_radiance_zero", result.radiance, 0.0, 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_direct_light_3d_color_tint_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_direct_light_color_tint\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_direct_light_3d_color_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.sceneObjects[0].color = 0xFF0000;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_direct_light_3d_color_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_color_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &result);
    assert_true("runtime_direct_light_3d_color_shade_ok", ok);
    assert_true("runtime_direct_light_3d_color_hit", result.hit);
    assert_true("runtime_direct_light_3d_color_visible", result.visible);
    assert_true("runtime_direct_light_3d_color_scalar_positive", result.radiance > 0.0);
    assert_true("runtime_direct_light_3d_color_red_positive", result.radianceR > 0.0);
    assert_close("runtime_direct_light_3d_color_green_zero", result.radianceG, 0.0, 1e-9);
    assert_close("runtime_direct_light_3d_color_blue_zero", result.radianceB, 0.0, 1e-9);
    assert_close("runtime_direct_light_3d_color_scalar_unchanged",
                 result.radiance,
                 result.radianceR,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_legacy_zero_color_fallback_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_direct_light_zero_color_fallback\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_direct_light_3d_zero_color_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.sceneObjects[0].color = 0x000000;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_direct_light_3d_zero_color_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_zero_color_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &result);
    assert_true("runtime_direct_light_3d_zero_color_shade_ok", ok);
    assert_true("runtime_direct_light_3d_zero_color_scalar_positive", result.radiance > 0.0);
    assert_true("runtime_direct_light_3d_zero_color_red_positive", result.radianceR > 0.0);
    assert_true("runtime_direct_light_3d_zero_color_green_positive", result.radianceG > 0.0);
    assert_true("runtime_direct_light_3d_zero_color_blue_positive", result.radianceB > 0.0);
    assert_close("runtime_direct_light_3d_zero_color_rg_equal",
                 result.radianceR,
                 result.radianceG,
                 1e-9);
    assert_close("runtime_direct_light_3d_zero_color_rb_equal",
                 result.radianceR,
                 result.radianceB,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_top_fill_lifts_upward_faces(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0x00FF00;

    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.topFillLightEnabled = true;

    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_top_fill_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_top_fill_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, -2.0, 0.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, -2.0, 0.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-2.0, 2.0, 0.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    hit.t = 5.0;
    hit.position = vec3(0.0, 0.0, 0.0);
    hit.normal = vec3(0.0, 0.0, 1.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 0;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &result);
    assert_true("runtime_direct_light_3d_top_fill_shade_ok", ok);
    assert_true("runtime_direct_light_3d_top_fill_hit", result.hit);
    assert_true("runtime_direct_light_3d_top_fill_visible", result.visible);
    assert_true("runtime_direct_light_3d_top_fill_scalar_positive", result.radiance > 0.0);
    assert_close("runtime_direct_light_3d_top_fill_red_zero", result.radianceR, 0.0, 1e-9);
    assert_true("runtime_direct_light_3d_top_fill_green_positive", result.radianceG > 0.0);
    assert_close("runtime_direct_light_3d_top_fill_blue_zero", result.radianceB, 0.0, 1e-9);
    assert_close("runtime_direct_light_3d_top_fill_scalar_matches_green",
                 result.radiance,
                 result.radianceG,
                 1e-9);

    animSettings.topFillLightEnabled = false;
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &result);
    assert_true("runtime_direct_light_3d_top_fill_disabled_shade_ok", ok);
    assert_close("runtime_direct_light_3d_top_fill_disabled_scalar_zero",
                 result.radiance,
                 0.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_transparent_blocker_partial_shadow_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[1].color = 0x0000FF;
    sceneSettings.sceneObjects[1].alpha = 1.0;

    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_partial_shadow_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_partial_shadow_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 1;
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    scene.triangleMesh.triangles[1].p0 = vec3(1.0, -4.5, -2.0);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -2.5, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(1.0, -4.5, 2.0);
    scene.triangleMesh.triangles[1].normal = vec3(1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 1;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 0;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &result);
    assert_true("runtime_direct_light_3d_partial_shadow_shade_ok", ok);
    assert_true("runtime_direct_light_3d_partial_shadow_hit", result.hit);
    assert_true("runtime_direct_light_3d_partial_shadow_visible", result.visible);
    assert_true("runtime_direct_light_3d_partial_shadow_radiance_positive", result.radiance > 0.0);
    assert_true("runtime_direct_light_3d_partial_shadow_red_below_unblocked",
                result.radianceR < scene.light.intensity * result.attenuation * result.ndotl);
    assert_true("runtime_direct_light_3d_partial_shadow_green_below_unblocked",
                result.radianceG < scene.light.intensity * result.attenuation * result.ndotl);
    assert_true("runtime_direct_light_3d_partial_shadow_blue_dominates_red",
                result.radianceB > result.radianceR + 1e-6);
    assert_true("runtime_direct_light_3d_partial_shadow_blue_dominates_green",
                result.radianceB > result.radianceG + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_direct_light_3d_authored_light_motion_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_direct_light_motion\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{\"x\":0.0,\"y\":-2.0,\"rotation\":0.0,\"handleLink\":false},"
                  "{\"x\":3.0,\"y\":-2.0,\"rotation\":0.0,\"handleLink\":false}"
                "]"
              "},"
              "\"light_path_depth\":{"
                "\"points\":["
                  "{\"z\":0.0},"
                  "{\"z\":3.0}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene_start;
    RuntimeScene3D scene_end;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult start_result = {0};
    RuntimeDirectLight3DResult end_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene_start);
    RuntimeScene3D_Init(&scene_end);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_direct_light_3d_motion_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene_start);
        RuntimeScene3D_Free(&scene_end);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.interactiveMode = true;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene_start, 0.0);
    assert_true("runtime_direct_light_3d_motion_build_start_ok", ok);
    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene_end, 1.0);
    assert_true("runtime_direct_light_3d_motion_build_end_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene_start.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_motion_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene_start);
        RuntimeScene3D_Free(&scene_end);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene_start, &projector, 50.0, 50.0, &start_result);
    assert_true("runtime_direct_light_3d_motion_shade_start_ok", ok);
    ok = RuntimeDirectLight3D_ShadePixel(&scene_end, &projector, 50.0, 50.0, &end_result);
    assert_true("runtime_direct_light_3d_motion_shade_end_ok", ok);
    assert_true("runtime_direct_light_3d_motion_start_visible", start_result.visible);
    assert_true("runtime_direct_light_3d_motion_end_visible", end_result.visible);
    assert_true("runtime_direct_light_3d_motion_radiance_changes",
                fabs(start_result.radiance - end_result.radiance) > 1e-6);
    assert_true("runtime_direct_light_3d_motion_distance_changes",
                fabs(start_result.lightDistance - end_result.lightDistance) > 1e-6);

    RuntimeScene3D_Free(&scene_start);
    RuntimeScene3D_Free(&scene_end);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_volume_hit_to_light_attenuation_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult baseline = {0};
    RuntimeDirectLight3DResult attenuated = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 2.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_volume_shadow_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_volume_shadow_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 0;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &baseline);
    assert_true("runtime_direct_light_3d_volume_shadow_baseline_ok", ok);
    assert_true("runtime_direct_light_3d_volume_shadow_baseline_visible", baseline.visible);
    assert_true("runtime_direct_light_3d_volume_shadow_baseline_positive", baseline.radiance > 0.0);

    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       6u,
                                       4u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -4.5, 0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_direct_light_3d_volume_shadow_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &scene.volume,
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK);
    assert_true("runtime_direct_light_3d_volume_shadow_alloc_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 0.6f;
        scene.volume.channels.solidMask[i] = 0u;
    }

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &attenuated);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_ok", ok);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_visible", attenuated.visible);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_positive", attenuated.radiance > 0.0);
    assert_true("runtime_direct_light_3d_volume_shadow_darker",
                attenuated.radiance < baseline.radiance);
    assert_true("runtime_direct_light_3d_volume_shadow_red_darker",
                attenuated.radianceR < baseline.radianceR);

    RuntimeScene3D_Free(&scene);
    return 0;
}

int run_test_runtime_lighting_materials_direct_light_suite(void) {
    test_runtime_direct_light_3d_shade_pixel_visible_contract();
    test_runtime_direct_light_3d_shade_pixel_shadowed_contract();
    test_runtime_direct_light_3d_color_tint_contract();
    test_runtime_direct_light_3d_legacy_zero_color_fallback_contract();
    test_runtime_direct_light_3d_top_fill_lifts_upward_faces();
    test_runtime_direct_light_3d_transparent_blocker_partial_shadow_contract();
    test_runtime_direct_light_3d_authored_light_motion_contract();
    test_runtime_direct_light_3d_volume_hit_to_light_attenuation_contract();
    return 0;
}
