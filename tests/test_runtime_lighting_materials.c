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
#include "test_runtime_lighting_materials.h"
#include "test_support.h"

static int test_runtime_material_payload_3d_scene_object_resolution_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF expected = {0};
    SceneObject expected_object;
    int default_material_id = 0;
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 5.0, -2.0, 6.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[1].color = 0x804020;
    sceneSettings.sceneObjects[1].opacity = 0.75;
    sceneSettings.sceneObjects[1].transparency = 0.5;
    sceneSettings.sceneObjects[1].reflectivity = 0.35;
    sceneSettings.sceneObjects[1].roughness = 0.15;
    sceneSettings.sceneObjects[1].material_id = 999;

    default_material_id = MaterialManagerDefaultId();
    expected_object = sceneSettings.sceneObjects[1];
    expected_object.material_id = default_material_id;
    MaterialBSDFInitFromSceneObject(&expected_object, &expected);

    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(1, &payload);
    assert_true("runtime_material_payload_3d_scene_object_ok", ok);
    assert_true("runtime_material_payload_3d_scene_object_valid", payload.valid);
    assert_true("runtime_material_payload_3d_scene_object_index_match",
                payload.sceneObjectIndex == 1);
    assert_true("runtime_material_payload_3d_scene_object_material_clamped",
                payload.materialId == default_material_id);
    assert_close("runtime_material_payload_3d_scene_object_base_r_match",
                 payload.baseColorR,
                 expected.baseColorR,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_base_g_match",
                 payload.baseColorG,
                 expected.baseColorG,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_base_b_match",
                 payload.baseColorB,
                 expected.baseColorB,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_albedo_match",
                 payload.bsdf.albedo,
                 expected.albedo,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_opacity_match",
                 payload.bsdf.opacity,
                 expected.opacity,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_reflectivity_match",
                 payload.bsdf.reflectivity,
                 expected.reflectivity,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_roughness_match",
                 payload.bsdf.roughness,
                 expected.roughness,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_diffuse_weight_match",
                 payload.bsdf.diffuseWeight,
                 expected.diffuseWeight,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_spec_weight_match",
                 payload.bsdf.specWeight,
                 expected.specWeight,
                 1e-9);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_object_multipliers_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF bsdf = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 5.0, -2.0, 6.0, 0.0, NULL, 0);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].transparency = 0.25;
    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload);
    assert_true("runtime_material_payload_multiplier_transparency_ok", ok);
    assert_close("runtime_material_payload_multiplier_transparency_scaled",
                 payload.transparency,
                 0.25 * 0.75,
                 1e-9);

    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[1].emissiveStrength = 0.4;
    MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[1], &bsdf);
    assert_close("runtime_material_payload_multiplier_emissive_scaled",
                 bsdf.emissive,
                 0.4 * 0.5,
                 1e-9);

    sceneSettings = saved_scene;
    return 0;
}

static int test_material_manager_default_presets_include_i4_entries(void) {
    const Material* emissive = NULL;
    const Material* transparent = NULL;

    MaterialManagerResetDefaults();
    assert_true("material_manager_default_preset_count_i4",
                MaterialManagerCount() >= (MATERIAL_PRESET_TRANSPARENT + 1));

    emissive = MaterialManagerGet(MATERIAL_PRESET_EMISSIVE);
    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);

    assert_true("material_manager_emissive_preset_exists", emissive != NULL);
    assert_true("material_manager_transparent_preset_exists", transparent != NULL);
    assert_true("material_manager_emissive_preset_has_emission",
                emissive->emissive.x > 0.0f ||
                emissive->emissive.y > 0.0f ||
                emissive->emissive.z > 0.0f);
    assert_close("material_manager_emissive_preset_transparency_zero",
                 emissive->transparency,
                 0.0,
                 1e-9);
    assert_true("material_manager_transparent_preset_has_transparency",
                transparent->transparency > 0.0f);
    assert_close("material_manager_transparent_preset_emissive_zero",
                 transparent->emissive.x + transparent->emissive.y + transparent->emissive.z,
                 0.0,
                 1e-9);
    return 0;
}

static int test_material_manager_load_dir_preserves_shipped_preset_ids(void) {
    char dir_template[] = "/tmp/rt_material_ids_XXXXXX";
    char mirror_path[512];
    char emissive_path[512];
    char transparent_path[512];
    const Material* mirror = NULL;
    const Material* emissive = NULL;
    const Material* transparent = NULL;
    bool ok = false;

    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        return 0;
    }

    snprintf(mirror_path, sizeof(mirror_path), "%s/mirror.json", dir_template);
    snprintf(emissive_path, sizeof(emissive_path), "%s/emissive.json", dir_template);
    snprintf(transparent_path, sizeof(transparent_path), "%s/transparent.json", dir_template);

    ok = write_text_file(mirror_path,
                         "{"
                         "\"diffuse\":0.0,"
                         "\"specular\":0.1,"
                         "\"reflectivity\":0.77,"
                         "\"roughness\":0.0,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("material_manager_load_dir_mirror_file_ok", ok);
    ok = write_text_file(emissive_path,
                         "{"
                         "\"diffuse\":0.0,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.25,0.25,0.25]"
                         "}");
    assert_true("material_manager_load_dir_emissive_file_ok", ok);
    ok = write_text_file(transparent_path,
                         "{"
                         "\"diffuse\":0.05,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.66,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("material_manager_load_dir_transparent_file_ok", ok);

    MaterialManagerLoadDir(dir_template);

    mirror = MaterialManagerGet(MATERIAL_PRESET_MIRROR);
    emissive = MaterialManagerGet(MATERIAL_PRESET_EMISSIVE);
    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);

    assert_true("material_manager_load_dir_mirror_preset_exists", mirror != NULL);
    assert_true("material_manager_load_dir_emissive_preset_exists", emissive != NULL);
    assert_true("material_manager_load_dir_transparent_preset_exists", transparent != NULL);
    assert_close("material_manager_load_dir_mirror_keeps_canonical_id",
                 mirror->reflectivity,
                 0.77,
                 1e-6);
    assert_close("material_manager_load_dir_emissive_keeps_canonical_id",
                 emissive->emissive.x,
                 0.25,
                 1e-6);
    assert_close("material_manager_load_dir_transparent_keeps_canonical_id",
                 transparent->transparency,
                 0.66,
                 1e-6);

    remove(mirror_path);
    remove(emissive_path);
    remove(transparent_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_material_payload_3d_hit_resolution_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF expected = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xC0C0FF;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].reflectivity = 0.5;
    sceneSettings.sceneObjects[0].roughness = 0.05;
    MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[0], &expected);

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 3;
    hit.primitiveIndex = 1;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload);
    assert_true("runtime_material_payload_3d_hit_ok", ok);
    assert_true("runtime_material_payload_3d_hit_valid", payload.valid);
    assert_true("runtime_material_payload_3d_hit_index_match",
                payload.sceneObjectIndex == 0);
    assert_true("runtime_material_payload_3d_hit_material_match",
                payload.materialId == MATERIAL_PRESET_GLOSSY);
    assert_close("runtime_material_payload_3d_hit_base_r_match",
                 payload.baseColorR,
                 expected.baseColorR,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_base_g_match",
                 payload.baseColorG,
                 expected.baseColorG,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_base_b_match",
                 payload.baseColorB,
                 expected.baseColorB,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_albedo_match",
                 payload.bsdf.albedo,
                 expected.albedo,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_reflectivity_match",
                 payload.bsdf.reflectivity,
                 expected.reflectivity,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_roughness_match",
                 payload.bsdf.roughness,
                 expected.roughness,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_emissive_match",
                 payload.emissive,
                 expected.emissive,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_transparency_match",
                 payload.transparency,
                 0.0,
                 1e-9);
    assert_true("runtime_material_payload_3d_hit_invalid_index_rejected",
                !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(4, &payload));
    assert_true("runtime_material_payload_3d_hit_invalid_hit_rejected",
                !RuntimeMaterialPayload3D_ResolveFromHit(NULL, &payload));

    sceneSettings = saved_scene;
    return 0;
}

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
    sceneSettings.sceneObjects[1].transparency = 1.0;

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

static int test_runtime_material_response_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_response_seed\","
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
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"material_ref\":{\"id\":\"mat_glossy\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_glossy\","
            "\"albedo\":[0.8, 0.8, 0.8]"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":3}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    RuntimeMaterialResponse3DResult matte_result = {0};
    RuntimeMaterialResponse3DResult mirror_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_material_response_seed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_material_response_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_material_response_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].color = 0x0000FF;

    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &diffuse_result);
    assert_true("runtime_material_response_seed_diffuse_ok", ok);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &matte_result);
    assert_true("runtime_material_response_seed_matte_ok", ok);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &mirror_result);
    assert_true("runtime_material_response_seed_mirror_ok", ok);
    assert_true("runtime_material_response_seed_matte_hit", matte_result.hit);
    assert_true("runtime_material_response_seed_mirror_hit", mirror_result.hit);
    assert_true("runtime_material_response_seed_matte_payload_resolved",
                matte_result.materialResolved);
    assert_true("runtime_material_response_seed_mirror_payload_resolved",
                mirror_result.materialResolved);
    assert_true("runtime_material_response_seed_matte_id_match",
                matte_result.payload.materialId == MATERIAL_PRESET_DEFAULT);
    assert_true("runtime_material_response_seed_mirror_id_match",
                mirror_result.payload.materialId == MATERIAL_PRESET_MIRROR);
    assert_true("runtime_material_response_seed_matte_secondary_rays_match",
                matte_result.secondaryRayCount == diffuse_result.secondaryRayCount);
    assert_true("runtime_material_response_seed_mirror_secondary_rays_match",
                mirror_result.secondaryRayCount == diffuse_result.secondaryRayCount);
    assert_true("runtime_material_response_seed_matte_differs_from_diffuse",
                fabs(matte_result.radiance - diffuse_result.radiance) > 1e-6);
    assert_true("runtime_material_response_seed_mirror_differs_from_diffuse",
                fabs(mirror_result.radiance - diffuse_result.radiance) > 1e-6);
    assert_true("runtime_material_response_seed_matte_blue_direct_dominates_red",
                matte_result.directRadianceB > matte_result.directRadianceR + 1e-6);
    assert_true("runtime_material_response_seed_matte_blue_direct_dominates_green",
                matte_result.directRadianceB > matte_result.directRadianceG + 1e-6);
    assert_true("runtime_material_response_seed_mirror_blue_total_dominates_red",
                mirror_result.radianceB > mirror_result.radianceR + 1e-6);
    assert_true("runtime_material_response_seed_mirror_blue_total_dominates_green",
                mirror_result.radianceB > mirror_result.radianceG + 1e-6);
    assert_true("runtime_material_response_seed_mirror_direct_vs_matte",
                fabs(mirror_result.directRadiance - matte_result.directRadiance) > 1e-6);
    assert_true("runtime_material_response_seed_bounce_zero_preserved",
                matte_result.bounceRadiance == 0.0 &&
                mirror_result.bounceRadiance == 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_3d_lower_tier_separation_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_disney_tier_compare\","
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
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"material_ref\":{\"id\":\"mat_glossy\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_glossy\","
            "\"albedo\":[0.8, 0.8, 0.8]"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":3}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    RuntimeDisney3DResult glossy_disney = {0};
    RuntimeDisney3DResult matte_disney = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_disney_tier_compare_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_disney_tier_compare_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_disney_tier_compare_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &direct_result);
    assert_true("runtime_disney_tier_compare_direct_ok", ok);
    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &diffuse_result);
    assert_true("runtime_disney_tier_compare_diffuse_ok", ok);
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_disney_tier_compare_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &emission_result);
    assert_true("runtime_disney_tier_compare_emission_ok", ok);
    ok = RuntimeDisney3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &glossy_disney);
    assert_true("runtime_disney_tier_compare_disney_glossy_ok", ok);

    assert_true("runtime_disney_tier_compare_direct_hit", direct_result.hit);
    assert_true("runtime_disney_tier_compare_diffuse_hit", diffuse_result.hit);
    assert_true("runtime_disney_tier_compare_material_radiance_positive",
                material_result.radiance > 0.0);
    assert_true("runtime_disney_tier_compare_emission_hit", emission_result.hit);
    assert_true("runtime_disney_tier_compare_disney_hit", glossy_disney.hit);
    assert_true("runtime_disney_tier_compare_disney_payload_resolved",
                glossy_disney.payloadResolved);
    assert_true("runtime_disney_tier_compare_disney_secondary_rays_match_diffuse",
                glossy_disney.secondaryRayCount == diffuse_result.secondaryRayCount);
    assert_true("runtime_disney_tier_compare_disney_secondary_rays_match_material",
                glossy_disney.secondaryRayCount == material_result.secondaryRayCount);
    assert_true("runtime_disney_tier_compare_disney_secondary_rays_bounded_by_emission",
                glossy_disney.secondaryRayCount <= emission_result.secondaryRayCount);
    assert_true("runtime_disney_tier_compare_disney_specular_positive",
                glossy_disney.specularRadiance > 0.01);
    assert_true("runtime_disney_tier_compare_disney_base_positive",
                glossy_disney.baseRadiance > 0.01);
    assert_true("runtime_disney_tier_compare_disney_direct_differs_from_direct_light",
                fabs(glossy_disney.radiance - direct_result.radiance) > 1e-6);
    assert_true("runtime_disney_tier_compare_disney_differs_from_diffuse",
                fabs(glossy_disney.radiance - diffuse_result.radiance) > 1e-6);
    assert_true("runtime_disney_tier_compare_disney_differs_from_material",
                fabs(glossy_disney.radiance - material_result.radiance) > 1e-6);
    assert_true("runtime_disney_tier_compare_disney_differs_from_emission",
                fabs(glossy_disney.radiance - emission_result.radiance) > 1e-6);
    assert_true("runtime_disney_tier_compare_disney_direct_differs_from_material",
                fabs(glossy_disney.directRadiance - material_result.directRadiance) > 1e-6);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    ok = RuntimeDisney3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &matte_disney);
    assert_true("runtime_disney_tier_compare_disney_matte_ok", ok);
    assert_true("runtime_disney_tier_compare_disney_matte_hit", matte_disney.hit);
    assert_true("runtime_disney_tier_compare_glossy_specular_exceeds_matte",
                glossy_disney.specularRadiance > matte_disney.specularRadiance + 1e-6);
    assert_true("runtime_disney_tier_compare_glossy_roughness_weight_exceeds_matte",
                glossy_disney.roughnessWeight > matte_disney.roughnessWeight + 1e-6);
    assert_true("runtime_disney_tier_compare_glossy_total_differs_from_matte",
                fabs(glossy_disney.radiance - matte_disney.radiance) > 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_3d_opaque_receiver_preserves_transport_support(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    RuntimeDisney3DResult disney_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 3;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[2].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[1].transparency = 1.0;
    sceneSettings.sceneObjects[2].emissiveStrength = 1.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;

    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 3;
    scene.triangleMesh.triangleCapacity = 3;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_disney_transport_support_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_disney_transport_support_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 3;
    scene.triangleMesh.triangleCount = 3;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 1;
    scene.primitives[2].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[2].source.sceneObjectIndex = 2;

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

    scene.triangleMesh.triangles[2].p0 = vec3(0.5, -4.0, -1.0);
    scene.triangleMesh.triangles[2].p1 = vec3(0.5, -3.0, 1.0);
    scene.triangleMesh.triangles[2].p2 = vec3(0.5, -5.0, 1.0);
    scene.triangleMesh.triangles[2].normal = vec3(-1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[2].primitiveIndex = 2;
    scene.triangleMesh.triangles[2].sceneObjectIndex = 2;

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

    ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &material_result);
    assert_true("runtime_disney_transport_support_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadeHit(&scene, &hit, NULL, &emission_result);
    assert_true("runtime_disney_transport_support_emission_ok", ok);
    ok = RuntimeDisney3D_ShadeHit(&scene, &hit, NULL, &disney_result);
    assert_true("runtime_disney_transport_support_disney_ok", ok);

    assert_true("runtime_disney_transport_support_opaque_receiver",
                emission_result.payload.transparency <= 1e-9 &&
                emission_result.payload.emissive <= 1e-9);
    assert_true("runtime_disney_transport_support_emissive_direct_positive",
                emission_result.emissiveDirectRadiance > 0.0);
    assert_true("runtime_disney_transport_support_disney_emission_positive",
                disney_result.emissionRadiance > 0.0);
    assert_close("runtime_disney_transport_support_disney_transmission_zero_on_opaque_receiver",
                 disney_result.transmissionRadiance,
                 0.0,
                 1e-9);
    assert_true("runtime_disney_transport_support_disney_lifts_material",
                disney_result.radiance > material_result.radiance);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_lighting_materials_tests(void) {
    int before = test_support_failures();

    test_runtime_material_payload_3d_scene_object_resolution_contract();
    test_runtime_material_payload_3d_object_multipliers_contract();
    test_material_manager_default_presets_include_i4_entries();
    test_material_manager_load_dir_preserves_shipped_preset_ids();
    test_runtime_material_payload_3d_hit_resolution_contract();
    test_runtime_direct_light_3d_shade_pixel_visible_contract();
    test_runtime_direct_light_3d_shade_pixel_shadowed_contract();
    test_runtime_direct_light_3d_color_tint_contract();
    test_runtime_direct_light_3d_legacy_zero_color_fallback_contract();
    test_runtime_direct_light_3d_top_fill_lifts_upward_faces();
    test_runtime_direct_light_3d_transparent_blocker_partial_shadow_contract();
    test_runtime_direct_light_3d_authored_light_motion_contract();
    test_runtime_material_response_3d_seed_branch_contract();
    test_runtime_disney_3d_lower_tier_separation_contract();
    test_runtime_disney_3d_opaque_receiver_preserves_transport_support();

    return test_support_failures() - before;
}
