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
#include "render/runtime_triangle_bvh_3d.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

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

static int test_runtime_material_response_3d_mirror_reflects_opaque_chroma(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    RuntimeDisney3DResult disney_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.95;
    sceneSettings.sceneObjects[0].roughness = 0.02;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0xFF0000;
    sceneSettings.sceneObjects[1].alpha = 1.0;
    sceneSettings.sceneObjects[1].opacity = 1.0;
    sceneSettings.sceneObjects[1].reflectivity = 0.1;
    sceneSettings.sceneObjects[1].roughness = 0.6;
    animSettings.lightIntensity = 20.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = 0;
    animSettings.transmissionSamples3D = 0;
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, 1.5, 2.0);
    scene.light.radius = 0.0;
    scene.light.intensity = 20.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_material_mirror_reflects_chroma_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_material_mirror_reflects_chroma_alloc_triangles",
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

    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene.triangleMesh.triangles[1].p0 = vec3(-2.0, 3.0, -2.0);
    scene.triangleMesh.triangles[1].p1 = vec3(0.0, 3.0, 2.0);
    scene.triangleMesh.triangles[1].p2 = vec3(2.0, 3.0, -2.0);
    scene.triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 1;

    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_material_mirror_reflects_chroma_bvh_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    hit.t = 1.0;
    hit.position = vec3(0.0, 0.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 0;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, NULL, &direct_result);
    assert_true("runtime_material_mirror_reflects_chroma_direct_ok", ok);
    ok = RuntimeDiffuseBounce3D_ShadeHit(&scene, &hit, NULL, &diffuse_result);
    assert_true("runtime_material_mirror_reflects_chroma_diffuse_ok", ok);
    ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &material_result);
    assert_true("runtime_material_mirror_reflects_chroma_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadeHit(&scene, &hit, NULL, &emission_result);
    assert_true("runtime_material_mirror_reflects_chroma_emission_ok", ok);
    ok = RuntimeDisney3D_ShadeHit(&scene, &hit, NULL, &disney_result);
    assert_true("runtime_material_mirror_reflects_chroma_disney_ok", ok);

    assert_true("runtime_material_mirror_reflects_chroma_material_specular_ray",
                material_result.specularRayCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_material_specular_hit",
                material_result.specularHitCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_material_specular_contributes",
                material_result.specularContributingHitCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_material_red_reflection",
                material_result.specularRadianceR > material_result.specularRadianceG + 1e-6 &&
                material_result.specularRadianceR > material_result.specularRadianceB + 1e-6);
    assert_true("runtime_material_mirror_reflects_chroma_lower_tiers_remain_visible",
                direct_result.visible && diffuse_result.visible);
    assert_true("runtime_material_mirror_reflects_chroma_emission_preserves_reflection",
                emission_result.radianceR >= material_result.radianceR - 1e-6);
    assert_true("runtime_material_mirror_reflects_chroma_disney_preserves_reflection",
                disney_result.specularRadianceR > disney_result.specularRadianceG + 1e-6 &&
                disney_result.specularRadianceR > disney_result.specularRadianceB + 1e-6);

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
    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &direct_result);
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
    sceneSettings.sceneObjects[1].alpha = 1.0;
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

int run_test_runtime_lighting_materials_transport_suite(void) {
    test_runtime_material_response_3d_seed_branch_contract();
    test_runtime_material_response_3d_mirror_reflects_opaque_chroma();
    test_runtime_disney_3d_lower_tier_separation_contract();
    test_runtime_disney_3d_opaque_receiver_preserves_transport_support();
    return 0;
}
