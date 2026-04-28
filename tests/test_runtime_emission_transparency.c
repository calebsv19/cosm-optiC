#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_emission_transparency.h"
#include "test_support.h"

static int test_runtime_emission_transparency_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_seed\","
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
            "\"material_ref\":{\"id\":\"mat_emissive\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_emissive\","
            "\"emissive\":[1.0, 1.0, 1.0]"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":4}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_seed_apply_ok", ok);
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

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[0].color = 0xFF0000;
    sceneSettings.sceneObjects[0].emissiveStrength = 1.0;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_seed_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &emission_result);
    assert_true("runtime_emission_transparency_seed_branch_ok", ok);
    assert_true("runtime_emission_transparency_seed_hit", emission_result.hit);
    assert_true("runtime_emission_transparency_seed_payload_resolved",
                emission_result.payloadResolved);
    assert_true("runtime_emission_transparency_seed_payload_valid",
                emission_result.payload.valid);
    assert_true("runtime_emission_transparency_seed_material_id_match",
                emission_result.payload.materialId == MATERIAL_PRESET_EMISSIVE);
    assert_true("runtime_emission_transparency_seed_emissive_positive",
                emission_result.payload.emissive > 0.0);
    assert_close("runtime_emission_transparency_seed_transparency_zero",
                 emission_result.payload.transparency,
                 0.0,
                 1e-9);
    assert_true("runtime_emission_transparency_seed_secondary_rays_match",
                emission_result.secondaryRayCount == RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);
    assert_true("runtime_emission_transparency_seed_direct_lifts_material",
                emission_result.directRadiance > material_result.directRadiance);
    assert_true("runtime_emission_transparency_seed_emissive_direct_positive",
                emission_result.emissiveDirectRadiance > 0.0);
    assert_true("runtime_emission_transparency_seed_emissive_red_direct_positive",
                emission_result.emissiveDirectRadianceR > 0.0);
    assert_true("runtime_emission_transparency_seed_emissive_red_direct_dominates_blue",
                emission_result.emissiveDirectRadianceR >
                    emission_result.emissiveDirectRadianceB + 1e-6);
    assert_close("runtime_emission_transparency_seed_transmitted_direct_zero",
                 emission_result.transmittedDirectRadiance,
                 0.0,
                 1e-9);
    assert_true("runtime_emission_transparency_seed_bounce_nonnegative",
                emission_result.bounceRadiance >= 0.0);
    assert_true("runtime_emission_transparency_seed_total_lifts_material",
                emission_result.radiance > material_result.radiance);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transmission_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    char dir_template[] = "/tmp/ray_tracing_materialsXXXXXX";
    char transparent_path[PATH_MAX];
    char matte_path[PATH_MAX];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_transmission\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"front_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"back_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-7.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-7.0,\"z\":0.0},"
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
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    const Material* material = NULL;
    bool ok = false;
    int transparent_id = -1;
    int matte_id = -1;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    snprintf(transparent_path, sizeof(transparent_path), "%s/00_transparent.json", dir_template);
    snprintf(matte_path, sizeof(matte_path), "%s/01_matte.json", dir_template);
    ok = write_text_file(transparent_path,
                         "{"
                         "\"diffuse\":0.15,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.75,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("runtime_emission_transparency_transparency_file_ok", ok);
    ok = write_text_file(matte_path,
                         "{"
                         "\"diffuse\":0.85,"
                         "\"specular\":0.05,"
                         "\"reflectivity\":0.1,"
                         "\"roughness\":0.6,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("runtime_emission_transparency_matte_file_ok", ok);
    MaterialManagerLoadDir(dir_template);

    for (int i = 0; i < MaterialManagerCount(); ++i) {
        material = MaterialManagerGet(i);
        if (material && material->transparency > 0.5f) {
            transparent_id = i;
        } else if (material) {
            matte_id = i;
        }
    }
    assert_true("runtime_emission_transparency_transparent_id_found", transparent_id >= 0);
    assert_true("runtime_emission_transparency_matte_id_found", matte_id >= 0);

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_transmission_apply_ok", ok);
    if (!ok) {
        remove(transparent_path);
        remove(matte_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = transparent_id;
    sceneSettings.sceneObjects[1].material_id = matte_id;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_transmission_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_transmission_projector_ok", ok);
    if (!ok) {
        remove(transparent_path);
        remove(matte_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_transmission_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_transmission_branch_ok", ok);
    assert_true("runtime_emission_transparency_transmission_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_transmission_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_transmission_transparency_positive",
                transparent_result.payload.transparency > 0.5);
    assert_true("runtime_emission_transparency_transmission_radiance_differs",
                fabs(transparent_result.radiance - material_result.radiance) > 1e-6);
    assert_true("runtime_emission_transparency_transmission_direct_differs",
                fabs(transparent_result.directRadiance - material_result.directRadiance) > 1e-6);

    remove(transparent_path);
    remove(matte_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transparent_prism_reaches_behind_surface(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_prism_through\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"front_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":2.0}"
          "},"
          "{"
            "\"object_id\":\"back_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-8.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-8.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.5,\"y\":-3.0,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_prism_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0x0000FF;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[1].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].transparency = 1.0;
    sceneSettings.sceneObjects[1].emissiveStrength = 1.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_prism_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_prism_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_prism_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_prism_branch_ok", ok);
    assert_true("runtime_emission_transparency_prism_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_prism_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_prism_transparency_positive",
                transparent_result.payload.transparency > 0.5);
    assert_true("runtime_emission_transparency_prism_reaches_emissive_surface",
                transparent_result.directRadiance > material_result.directRadiance + 0.1);
    assert_true("runtime_emission_transparency_prism_transmitted_direct_positive",
                transparent_result.transmittedDirectRadiance > 0.0);
    assert_true("runtime_emission_transparency_prism_transmitted_blue_dominates_red",
                transparent_result.transmittedDirectRadianceB >
                    transparent_result.transmittedDirectRadianceR + 1e-6);
    assert_true("runtime_emission_transparency_prism_final_blue_dominates_red",
                transparent_result.directRadianceB >
                    transparent_result.directRadianceR + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transparent_prism_reaches_emitter(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    RuntimeLightEmitterHit3DResult emitter_result = {0};
    Ray3D transmission_ray = {0};
    double legacy_front_weight = 0.0;
    double legacy_transparency = 0.0;
    double legacy_direct = 0.0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0x0000FF;
    sceneSettings.sceneObjects[0].transparency = 1.0;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, -7.0, 0.0);
    scene.light.radius = 1.5;
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
    assert_true("runtime_emission_transparency_emitter_prism_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_emission_transparency_emitter_prism_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        return 0;
    }
    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "front_wall");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -4.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(3.0, -4.0, -3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, -4.0, 3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_emitter_prism_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_emitter_prism_material_ok", ok);
    transmission_ray = RuntimeRay3D_MakeOffset(material_result.hitInfo.position,
                                               material_result.hitInfo.normal,
                                               material_result.primaryRay.direction,
                                               1e-4);
    ok = RuntimeLightEmitter3D_IntersectRay(&scene,
                                            &transmission_ray,
                                            1e-4,
                                            32.0,
                                            &emitter_result);
    assert_true("runtime_emission_transparency_emitter_prism_transmission_ray_hits_emitter", ok);
    assert_true("runtime_emission_transparency_emitter_prism_transmission_emitter_radiance_positive",
                emitter_result.radiance > 0.05);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_emitter_prism_branch_ok", ok);
    assert_true("runtime_emission_transparency_emitter_prism_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_emitter_prism_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_emitter_prism_transparency_positive",
                transparent_result.payload.transparency > 0.5);
    legacy_front_weight = 1.0 - transparent_result.payload.transparency;
    if (legacy_front_weight < 0.2) {
        legacy_front_weight = 0.2;
    }
    legacy_transparency = 1.0 - legacy_front_weight;
    legacy_direct = (material_result.directRadiance * legacy_front_weight) +
                    (emitter_result.radiance * legacy_transparency);
    assert_true("runtime_emission_transparency_emitter_prism_direct_positive",
                transparent_result.directRadiance > 0.05);
    assert_true("runtime_emission_transparency_emitter_prism_direct_lifts_material",
                transparent_result.directRadiance > material_result.directRadiance + 0.05);
    assert_true("runtime_emission_transparency_emitter_prism_direct_softens_legacy_hard_edge",
                transparent_result.directRadiance < legacy_direct - 0.01);
    assert_true("runtime_emission_transparency_emitter_prism_blue_filters_white_emitter",
                transparent_result.directRadianceB >
                    transparent_result.directRadianceR + 1e-6);
    assert_true("runtime_emission_transparency_emitter_prism_total_lifts_material",
                transparent_result.radiance > material_result.radiance + 0.05);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_emission_transparency_3d_temporal_skips_stable_emitters(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_adaptive_emitter\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"emissive_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"material_ref\":{\"id\":\"mat_emissive\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_emissive\","
            "\"emissive\":[1.0, 1.0, 1.0],"
            "\"transparency\":0.0"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":4}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats single_stats = {0};
    RuntimeNative3DRenderStats temporal_stats = {0};
    uint8_t single_pixels[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t temporal_pixels[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_adaptive_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[0].emissiveStrength = 1.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        single_pixels,
        RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY,
        101,
        101,
        0.0,
        0.0,
        -2.0,
        NULL,
        1,
        &single_stats);
    assert_true("runtime_emission_transparency_adaptive_single_ok", ok);
    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        temporal_pixels,
        RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY,
        101,
        101,
        0.0,
        0.0,
        -2.0,
        NULL,
        4,
        &temporal_stats);
    assert_true("runtime_emission_transparency_adaptive_temporal_ok", ok);
    assert_true("runtime_emission_transparency_adaptive_secondary_not_multiplied",
                temporal_stats.secondaryRayCount == single_stats.secondaryRayCount);
    assert_true("runtime_emission_transparency_adaptive_center_preserved",
                temporal_pixels[(((50 * 101) + 50) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES) + 2] ==
                    single_pixels[(((50 * 101) + 50) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES) + 2]);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_emission_transparency_tests(void) {
    test_runtime_emission_transparency_3d_seed_branch_contract();
    test_runtime_emission_transparency_3d_transmission_contract();
    test_runtime_emission_transparency_3d_transparent_prism_reaches_behind_surface();
    test_runtime_emission_transparency_3d_transparent_prism_reaches_emitter();
    test_runtime_emission_transparency_3d_temporal_skips_stable_emitters();
    return 0;
}
