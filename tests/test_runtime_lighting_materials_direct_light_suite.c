#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
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
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_volume_3d.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

static void runtime_lighting_materials_direct_reset_authoring_state(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    MaterialManagerResetDefaults();
}

static int test_runtime_direct_light_3d_shade_pixel_visible_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
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
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_visible_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &result);
    assert_true("runtime_direct_light_3d_visible_shade_ok", ok);
    assert_true("runtime_direct_light_3d_visible_hit", result.hit);
    assert_true("runtime_direct_light_3d_visible_los", result.visible);
    assert_close("runtime_direct_light_3d_visible_hit_y", result.hitInfo.position.y, -5.0, 1e-6);
    assert_true("runtime_direct_light_3d_visible_ndotl_positive", result.ndotl > 0.99);
    assert_true("runtime_direct_light_3d_visible_radiance_positive", result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_shade_pixel_shadowed_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0xFFFFFF;
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
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "blocker");
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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
    assert_true("runtime_direct_light_3d_shadowed_shade_ok", ok);
    assert_true("runtime_direct_light_3d_shadowed_hit", result.hit);
    assert_true("runtime_direct_light_3d_shadowed_not_visible", !result.visible);
    assert_close("runtime_direct_light_3d_shadowed_radiance_zero", result.radiance, 0.0, 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
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

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &result);
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

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &result);
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
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0x00FF00;

    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 0.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.environment.lightMode = ENVIRONMENT_LIGHT_MODE_TOP_FILL;
    scene.environment.topFillIntensity = 3.0;

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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
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

    scene.environment.lightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
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
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
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
    RuntimeScene3D_RefreshMaterialFlags(&scene);
    assert_true("runtime_direct_light_3d_partial_shadow_flags_valid",
                scene.materialFlags.valid);
    assert_true("runtime_direct_light_3d_partial_shadow_flags_transparent",
                scene.materialFlags.hasTransparentSurfaces);
    assert_true("runtime_direct_light_3d_partial_shadow_fast_path_disabled",
                !RuntimeVisibility3D_CanUseOpaqueNoVolumeFastPath(&scene));

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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
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
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_opaque_no_volume_fast_path_gate(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeVisibility3DTransmittance transmittance = {0};
    RuntimeTriangleBVH3DTraceStats stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0xFFFFFF;

    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_opaque_fast_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_opaque_fast_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
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
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_direct_light_3d_opaque_fast_bvh_ok", ok);
    RuntimeScene3D_RefreshMaterialFlags(&scene);
    assert_true("runtime_direct_light_3d_opaque_fast_flags_valid",
                scene.materialFlags.valid);
    assert_true("runtime_direct_light_3d_opaque_fast_caps_valid",
                scene.capabilities.valid);
    assert_true("runtime_direct_light_3d_opaque_fast_flags_opaque",
                !scene.materialFlags.hasTransparentSurfaces);
    assert_true("runtime_direct_light_3d_opaque_fast_caps_opaque",
                !scene.capabilities.hasTransparentSurfaces);
    assert_true("runtime_direct_light_3d_opaque_fast_caps_skip_scatter",
                scene.capabilities.canSkipVolumeScatter);
    assert_true("runtime_direct_light_3d_opaque_fast_enabled",
                RuntimeVisibility3D_CanUseOpaqueNoVolumeFastPath(&scene));

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

    RuntimeTriangleBVH3D_ResetTraceStats();
    transmittance = RuntimeVisibility3D_TransmittanceFromHitToPointRGB(&scene,
                                                                       &hit,
                                                                       vec3(2.0, -2.0, 0.0),
                                                                       -1,
                                                                       -1);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&stats);
    RuntimeTriangleBVH3D_DisableTraceStats();
    assert_close("runtime_direct_light_3d_opaque_fast_blocked_luma",
                 transmittance.luma,
                 0.0,
                 1e-12);
    assert_true("runtime_direct_light_3d_opaque_fast_single_trace",
                stats.traceCalls == 1u);

    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       1u,
                                       1u,
                                       1u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(0.0, -4.0, 0.0),
                                       1.0,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_direct_light_3d_opaque_fast_volume_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &scene.volume,
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY);
    assert_true("runtime_direct_light_3d_opaque_fast_volume_alloc_ok", ok);
    if (ok && scene.volume.channels.density) {
        scene.volume.channels.density[0] = 0.5f;
    }
    assert_true("runtime_direct_light_3d_opaque_fast_volume_disabled",
                !RuntimeVisibility3D_CanUseOpaqueNoVolumeFastPath(&scene));
    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_direct_light_3d_opaque_fast_volume_caps_extinction",
                scene.capabilities.hasLightingExtinctionVolume);
    assert_true("runtime_direct_light_3d_opaque_fast_volume_caps_no_scatter_skip",
                !scene.capabilities.canSkipVolumeScatter);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_area_light_softens_edge_shadow_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult center_result = {0};
    RuntimeDirectLight3DResult area_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0x808080;

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
    assert_true("runtime_direct_light_3d_area_shadow_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_area_shadow_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
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
    scene.triangleMesh.triangles[1].p0 = vec3(1.0, -3.7, -0.04);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -3.3, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(1.0, -3.7, 0.04);
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

    scene.light.radius = 0.0;
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &center_result);
    assert_true("runtime_direct_light_3d_area_shadow_center_ok", ok);
    assert_true("runtime_direct_light_3d_area_shadow_center_blocked", !center_result.visible);
    assert_close("runtime_direct_light_3d_area_shadow_center_zero",
                 center_result.radiance,
                 0.0,
                 1e-9);

    scene.light.radius = 0.9;
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &area_result);
    assert_true("runtime_direct_light_3d_area_shadow_area_ok", ok);
    assert_true("runtime_direct_light_3d_area_shadow_area_visible", area_result.visible);
    assert_true("runtime_direct_light_3d_area_shadow_area_positive", area_result.radiance > 0.0);
    assert_true("runtime_direct_light_3d_area_shadow_softens_scalar",
                area_result.radiance > center_result.radiance + 1e-6);
    assert_true("runtime_direct_light_3d_area_shadow_softens_red",
                area_result.radianceR > center_result.radianceR + 1e-6);
    assert_true("runtime_direct_light_3d_area_shadow_softens_green",
                area_result.radianceG > center_result.radianceG + 1e-6);
    assert_true("runtime_direct_light_3d_area_shadow_softens_blue",
                area_result.radianceB > center_result.radianceB + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_area_light_sampling_sequence_changes_shadow(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult sequence_a = {0};
    RuntimeDirectLight3DResult sequence_a_repeat = {0};
    RuntimeDirectLight3DResult sequence_b = {0};
    RuntimeNative3DSamplingContext sampling_a = {
        .sampleSequence = 101u,
        .temporalSubpassIndex = 0u,
        .temporalSubpassCount = 1u
    };
    RuntimeNative3DSamplingContext sampling_b = {
        .sampleSequence = 102u,
        .temporalSubpassIndex = 0u,
        .temporalSubpassCount = 1u
    };
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0x808080;

    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.radius = 0.9;
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
    assert_true("runtime_direct_light_3d_area_sampling_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_area_sampling_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
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
    scene.triangleMesh.triangles[1].p0 = vec3(1.0, -3.7, -0.04);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -3.3, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(1.0, -3.7, 0.04);
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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &sampling_a, &sequence_a);
    assert_true("runtime_direct_light_3d_area_sampling_a_ok", ok);
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &sampling_a, &sequence_a_repeat);
    assert_true("runtime_direct_light_3d_area_sampling_repeat_ok", ok);
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &sampling_b, &sequence_b);
    assert_true("runtime_direct_light_3d_area_sampling_b_ok", ok);

    assert_close("runtime_direct_light_3d_area_sampling_repeat_r",
                 sequence_a.radianceR,
                 sequence_a_repeat.radianceR,
                 1e-12);
    assert_close("runtime_direct_light_3d_area_sampling_repeat_g",
                 sequence_a.radianceG,
                 sequence_a_repeat.radianceG,
                 1e-12);
    assert_close("runtime_direct_light_3d_area_sampling_repeat_b",
                 sequence_a.radianceB,
                 sequence_a_repeat.radianceB,
                 1e-12);
    assert_true("runtime_direct_light_3d_area_sampling_sequence_diverges",
                fabs(sequence_a.radianceR - sequence_b.radianceR) > 1e-9 ||
                    fabs(sequence_a.radianceG - sequence_b.radianceG) > 1e-9 ||
                    fabs(sequence_a.radianceB - sequence_b.radianceB) > 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_area_light_clear_visibility_stops_after_four(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    RuntimeTriangleBVH3DTraceStats stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 0.0);
    scene.light.radius = 0.8;
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
    assert_true("runtime_direct_light_3d_area_adaptive_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_area_adaptive_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
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
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_direct_light_3d_area_adaptive_bvh_ok", ok);

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

    RuntimeTriangleBVH3D_ResetTraceStats();
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&stats);
    RuntimeTriangleBVH3D_DisableTraceStats();
    assert_true("runtime_direct_light_3d_area_adaptive_ok", ok);
    assert_true("runtime_direct_light_3d_area_adaptive_visible", result.visible);
    assert_true("runtime_direct_light_3d_area_adaptive_radiance_positive",
                result.radiance > 0.0);
    assert_true("runtime_direct_light_3d_area_adaptive_four_shadow_traces",
                stats.traceCalls == 4u);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_low_contribution_skips_shadow_trace(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    RuntimeTriangleBVH3DTraceStats stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 0.0);
    scene.light.radius = 0.0;
    scene.light.intensity = 1e-10;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_low_contrib_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_low_contrib_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
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
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_direct_light_3d_low_contrib_bvh_ok", ok);

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

    RuntimeTriangleBVH3D_ResetTraceStats();
    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&stats);
    RuntimeTriangleBVH3D_DisableTraceStats();
    assert_true("runtime_direct_light_3d_low_contrib_ok", ok);
    assert_true("runtime_direct_light_3d_low_contrib_hit", result.hit);
    assert_true("runtime_direct_light_3d_low_contrib_not_visible", !result.visible);
    assert_close("runtime_direct_light_3d_low_contrib_radiance_zero",
                 result.radiance,
                 0.0,
                 1e-12);
    assert_true("runtime_direct_light_3d_low_contrib_no_shadow_trace",
                stats.traceCalls == 0u);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_direct_light_3d_oil_overlay_opaque_blocker_stays_solid(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeDirectLight3DResult result = {0};
    RuntimeMaterialTextureStack blocker_stack = RuntimeMaterialTextureStackEmpty();
    HitInfo3D hit = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0x808080;

    blocker_stack.layerCount = 2;
    blocker_stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL);
    blocker_stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    blocker_stack.layers[1].placement.scale = 3.0;
    blocker_stack.layers[1].placement.strength = 1.0;
    blocker_stack.layers[1].params.coverage = 1.0;
    blocker_stack.layers[1].params.grain = 0.55;
    blocker_stack.layers[1].params.edgeSoftness = 1.0;
    blocker_stack.layers[1].params.contrast = 0.45;
    blocker_stack.layers[1].params.flow = 0.65;
    blocker_stack.layers[1].params.colorDepth = 1.0;
    blocker_stack.layers[1].params.surfaceDamage = 1.0;
    assert_true("runtime_direct_light_3d_oil_blocker_stack_set",
                SceneEditorMaterialStackSetObjectStack(1, &blocker_stack));

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
    assert_true("runtime_direct_light_3d_oil_blocker_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_oil_blocker_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &result);
    assert_true("runtime_direct_light_3d_oil_blocker_shade_ok", ok);
    assert_true("runtime_direct_light_3d_oil_blocker_hit", result.hit);
    assert_true("runtime_direct_light_3d_oil_blocker_not_visible", !result.visible);
    assert_close("runtime_direct_light_3d_oil_blocker_radiance_zero", result.radiance, 0.0, 1e-9);

    RuntimeScene3D_Free(&scene);
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
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

    ok = RuntimeDirectLight3D_ShadePixel(&scene_start,
                                         &projector,
                                         50.0,
                                         50.0,
                                         NULL,
                                         &start_result);
    assert_true("runtime_direct_light_3d_motion_shade_start_ok", ok);
    ok = RuntimeDirectLight3D_ShadePixel(&scene_end,
                                         &projector,
                                         50.0,
                                         50.0,
                                         NULL,
                                         &end_result);
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
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult baseline = {0};
    RuntimeDirectLight3DResult attenuated = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_lighting_materials_direct_reset_authoring_state();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
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
        sceneSettings = saved_scene;
        animSettings = saved_anim;
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

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &baseline);
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
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 0.6f;
        scene.volume.channels.solidMask[i] = 0u;
    }

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &attenuated);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_ok", ok);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_visible", attenuated.visible);
    assert_true("runtime_direct_light_3d_volume_shadow_attenuated_positive", attenuated.radiance > 0.0);
    assert_true("runtime_direct_light_3d_volume_shadow_darker",
                attenuated.radiance < baseline.radiance);
    assert_true("runtime_direct_light_3d_volume_shadow_red_darker",
                attenuated.radianceR < baseline.radianceR);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_lighting_materials_direct_light_suite(void) {
    test_runtime_direct_light_3d_shade_pixel_visible_contract();
    test_runtime_direct_light_3d_shade_pixel_shadowed_contract();
    test_runtime_direct_light_3d_color_tint_contract();
    test_runtime_direct_light_3d_legacy_zero_color_fallback_contract();
    test_runtime_direct_light_3d_top_fill_lifts_upward_faces();
    test_runtime_direct_light_3d_transparent_blocker_partial_shadow_contract();
    test_runtime_direct_light_3d_opaque_no_volume_fast_path_gate();
    test_runtime_direct_light_3d_area_light_softens_edge_shadow_contract();
    test_runtime_direct_light_3d_area_light_sampling_sequence_changes_shadow();
    test_runtime_direct_light_3d_area_light_clear_visibility_stops_after_four();
    test_runtime_direct_light_3d_low_contribution_skips_shadow_trace();
    test_runtime_direct_light_3d_oil_overlay_opaque_blocker_stays_solid();
    test_runtime_direct_light_3d_authored_light_motion_contract();
    test_runtime_direct_light_3d_volume_hit_to_light_attenuation_contract();
    return 0;
}
