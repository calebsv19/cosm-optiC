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
#include "render/runtime_disney_v2_3d.h"
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

static double runtime_disney_v2_3d_test_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
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

static void runtime_disney_v2_test_init_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.0;
    scene->light.intensity = 12.0;
    scene->light.falloffDistance = 10.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->environment.topFillIntensity = 0.0;
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 1;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }
    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 1;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[0].source.sceneObjectIndex = 0;
    scene->triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;
    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static void runtime_disney_v2_test_init_one_bounce_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.0;
    scene->light.intensity = 16.0;
    scene->light.falloffDistance = 10.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->environment.topFillIntensity = 0.0;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 2;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[0].source.sceneObjectIndex = 0;
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[1].source.sceneObjectIndex = 1;

    scene->triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene->triangleMesh.triangles[1].p0 = vec3(-24.0, 2.0, -24.0);
    scene->triangleMesh.triangles[1].p1 = vec3(24.0, 2.0, -24.0);
    scene->triangleMesh.triangles[1].p2 = vec3(0.0, 2.0, 24.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static void runtime_disney_v2_test_init_light_hit_scene(RuntimeScene3D* scene) {
    runtime_disney_v2_test_init_scene(scene);
    if (!scene || !scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }
    scene->light.position = vec3(0.0, 2.0, 0.0);
    scene->light.radius = 0.45;
    scene->light.intensity = 18.0;
    scene->light.falloffDistance = 8.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
}

static void runtime_disney_v2_test_init_transmission_scene(RuntimeScene3D* scene) {
    runtime_disney_v2_test_init_scene(scene);
    if (!scene || !scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }
    scene->light.position = vec3(0.0, -2.0, 0.0);
    scene->light.radius = 0.45;
    scene->light.intensity = 18.0;
    scene->light.falloffDistance = 8.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
}

static void runtime_disney_v2_test_init_primary_transparency_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.0;
    scene->light.intensity = 16.0;
    scene->light.falloffDistance = 12.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->environment.topFillIntensity = 0.0;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 2;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[0].source.sceneObjectIndex = 0;
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[1].source.sceneObjectIndex = 1;

    scene->triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene->triangleMesh.triangles[1].p0 = vec3(-6.0, -2.0, -6.0);
    scene->triangleMesh.triangles[1].p1 = vec3(6.0, -2.0, -6.0);
    scene->triangleMesh.triangles[1].p2 = vec3(0.0, -2.0, 6.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static void runtime_disney_v2_test_init_nested_transparency_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.0;
    scene->light.intensity = 18.0;
    scene->light.falloffDistance = 12.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->environment.topFillIntensity = 0.0;
    scene->primitiveCapacity = 3;
    scene->triangleMesh.triangleCapacity = 3;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }

    scene->primitiveCount = 3;
    scene->triangleMesh.triangleCount = 3;
    for (int i = 0; i < 3; ++i) {
        scene->primitives[i].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
        scene->primitives[i].source.sceneObjectIndex = i;
    }

    scene->triangleMesh.triangles[0].p0 = vec3(-3.0, 0.0, -3.0);
    scene->triangleMesh.triangles[0].p1 = vec3(3.0, 0.0, -3.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 3.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene->triangleMesh.triangles[1].p0 = vec3(-4.0, -1.0, -4.0);
    scene->triangleMesh.triangles[1].p1 = vec3(4.0, -1.0, -4.0);
    scene->triangleMesh.triangles[1].p2 = vec3(0.0, -1.0, 4.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    scene->triangleMesh.triangles[2].p0 = vec3(-8.0, -2.5, -8.0);
    scene->triangleMesh.triangles[2].p1 = vec3(8.0, -2.5, -8.0);
    scene->triangleMesh.triangles[2].p2 = vec3(0.0, -2.5, 8.0);
    scene->triangleMesh.triangles[2].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[2].primitiveIndex = 2;
    scene->triangleMesh.triangles[2].sceneObjectIndex = 2;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static void runtime_disney_v2_test_init_recursive_scene(RuntimeScene3D* scene) {
    runtime_disney_v2_test_init_one_bounce_scene(scene);
    if (!scene || !scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.50;
    scene->light.intensity = 18.0;
    scene->light.falloffDistance = 10.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
}

static void runtime_disney_v2_test_init_recursive_depth_three_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 5.0, 0.0);
    scene->light.radius = 0.50;
    scene->light.intensity = 18.0;
    scene->light.falloffDistance = 10.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->environment.topFillIntensity = 0.0;
    scene->primitiveCapacity = 3;
    scene->triangleMesh.triangleCapacity = 3;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return;
    }

    scene->primitiveCount = 3;
    scene->triangleMesh.triangleCount = 3;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[0].source.sceneObjectIndex = 0;
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[1].source.sceneObjectIndex = 1;
    scene->primitives[2].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[2].source.sceneObjectIndex = 2;

    scene->triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene->triangleMesh.triangles[1].p0 = vec3(-24.0, 2.0, -24.0);
    scene->triangleMesh.triangles[1].p1 = vec3(24.0, 2.0, -24.0);
    scene->triangleMesh.triangles[1].p2 = vec3(0.0, 2.0, 24.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    scene->triangleMesh.triangles[2].p0 = vec3(-24.0, 3.0, -24.0);
    scene->triangleMesh.triangles[2].p1 = vec3(24.0, 3.0, -24.0);
    scene->triangleMesh.triangles[2].p2 = vec3(0.0, 3.0, 24.0);
    scene->triangleMesh.triangles[2].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[2].primitiveIndex = 2;
    scene->triangleMesh.triangles[2].sceneObjectIndex = 2;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static HitInfo3D runtime_disney_v2_test_hit(const RuntimeScene3D* scene) {
    HitInfo3D hit = {0};
    hit.t = 4.0;
    hit.position = vec3(0.0, 0.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 0;
    if (scene && scene->primitiveCount > 0) {
        hit.source = scene->primitives[0].source;
    }
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;
    return hit;
}

static RuntimePrimaryHit3DResult runtime_disney_v2_test_primary_hit(
    const RuntimeScene3D* scene) {
    RuntimePrimaryHit3DResult primary = {0};
    primary.hit = true;
    primary.primaryRay.origin = vec3(0.0, 4.0, 0.0);
    primary.primaryRay.direction = vec3(0.0, -1.0, 0.0);
    primary.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    primary.hitInfo = runtime_disney_v2_test_hit(scene);
    return primary;
}

static RuntimeMaterialPayload3D runtime_disney_v2_test_payload(double r,
                                                               double g,
                                                               double b,
                                                               double reflectivity,
                                                               double roughness,
                                                               double diffuse_weight,
                                                               double specular_weight,
                                                               double transparency,
                                                               double emissive) {
    RuntimeMaterialPayload3D payload = {0};
    payload.valid = true;
    payload.sceneObjectIndex = 0;
    payload.materialId = MATERIAL_PRESET_DEFAULT;
    payload.baseColorR = r;
    payload.baseColorG = g;
    payload.baseColorB = b;
    payload.transparency = transparency;
    payload.opticalIor = 1.5;
    payload.emissive = emissive;
    payload.bsdf.albedo = (r + g + b) / 3.0;
    payload.bsdf.baseColorR = r;
    payload.bsdf.baseColorG = g;
    payload.bsdf.baseColorB = b;
    payload.bsdf.opacity = 1.0 - transparency;
    payload.bsdf.reflectivity = reflectivity;
    payload.bsdf.roughness = roughness;
    payload.bsdf.ior = 1.5;
    payload.bsdf.model = reflectivity > 0.05 ? MATERIAL_BSDF_GGX : MATERIAL_BSDF_LAMBERT;
    payload.bsdf.diffuseWeight = diffuse_weight;
    payload.bsdf.specWeight = specular_weight;
    payload.bsdf.weightSum = diffuse_weight + specular_weight;
    payload.bsdf.emissive = emissive;
    if (!(payload.bsdf.weightSum > 1e-9)) {
        payload.bsdf.diffuseWeight = 1.0;
        payload.bsdf.weightSum = 1.0;
    }
    return payload;
}

static void runtime_disney_v2_test_configure_scene_material(int scene_object_index,
                                                            int material_id,
                                                            int color,
                                                            double reflectivity,
                                                            double roughness) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return;
    }
    if (sceneSettings.objectCount <= scene_object_index) {
        sceneSettings.objectCount = scene_object_index + 1;
    }
    sceneSettings.sceneObjects[scene_object_index].material_id = material_id;
    sceneSettings.sceneObjects[scene_object_index].color = color;
    sceneSettings.sceneObjects[scene_object_index].alpha = 1.0;
    sceneSettings.sceneObjects[scene_object_index].opacity = 1.0;
    sceneSettings.sceneObjects[scene_object_index].reflectivity = reflectivity;
    sceneSettings.sceneObjects[scene_object_index].roughness = roughness;
}

static int test_runtime_disney_v2_3d_consumes_cached_principled_payload(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D payload =
        runtime_disney_v2_test_payload(0.20, 0.45, 0.85, 0.18, 0.42, 0.70, 0.30, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 17U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult v2_result = {0};
    RuntimeDisney3DResult current_disney = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    runtime_disney_v2_test_init_scene(&scene);
    assert_true("runtime_disney_v2_cached_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &payload,
                                                       &sampling,
                                                       &v2_result);
    assert_true("runtime_disney_v2_cached_shade_ok", ok);
    assert_true("runtime_disney_v2_cached_payload_resolved", v2_result.payloadResolved);
    assert_true("runtime_disney_v2_cached_principled_valid", v2_result.principled.valid);
    assert_close("runtime_disney_v2_cached_base_color_b",
                 v2_result.principled.baseColorB,
                 payload.baseColorB,
                 1e-12);
    assert_true("runtime_disney_v2_cached_lobe_probs",
                v2_result.diffuseProbability > 0.0 && v2_result.specularProbability > 0.0);
    assert_true("runtime_disney_v2_cached_direct_positive", v2_result.directRadiance > 0.0);
    assert_true("runtime_disney_v2_cached_diffuse_positive", v2_result.diffuseRadiance > 0.0);
    assert_true("runtime_disney_v2_cached_specular_nonnegative",
                v2_result.specularRadiance >= 0.0);
    assert_true("runtime_disney_v2_cached_pdf_nonnegative",
                v2_result.specularHalfPdf >= 0.0);
    assert_true("runtime_disney_v2_cached_path_state_valid",
                v2_result.pathState.valid && v2_result.pathState.depth == 1);
    assert_true("runtime_disney_v2_cached_path_ray_direction",
                vec3_length(v2_result.pathState.ray.direction) > 0.99);
    assert_true("runtime_disney_v2_cached_sample_counts",
                v2_result.bsdfSampleCount == 1 && v2_result.secondaryRayCount == 1);
    assert_true("runtime_disney_v2_cached_pdf_positive",
                v2_result.bsdfSamplePdf > 0.0 && v2_result.lightSamplePdf > 0.0);
    assert_close("runtime_disney_v2_cached_mis_balance",
                 v2_result.misWeightLight + v2_result.misWeightBsdf,
                 1.0,
                 1e-9);
    assert_true("runtime_disney_v2_cached_stochastic_direct_positive",
                v2_result.stochasticDirectRadiance > 0.0);
    assert_true("runtime_disney_v2_cached_path_throughput_positive",
                runtime_disney_v2_3d_test_peak(v2_result.pathState.throughputR,
                                               v2_result.pathState.throughputG,
                                               v2_result.pathState.throughputB) > 0.0);

    ok = RuntimeDisney3D_ShadePrimaryHitWithPayload(&scene, &primary, &payload, NULL, &current_disney);
    assert_true("runtime_disney_v2_current_disney_still_callable", ok);
    assert_true("runtime_disney_v2_current_disney_payload_resolved",
                current_disney.payloadResolved);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_material_diagnostics_order_lobes(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D matte =
        runtime_disney_v2_test_payload(0.75, 0.75, 0.75, 0.02, 0.75, 1.0, 0.0, 0.0, 0.0);
    RuntimeMaterialPayload3D metal =
        runtime_disney_v2_test_payload(0.95, 0.70, 0.25, 0.85, 0.16, 0.05, 0.95, 0.0, 0.0);
    RuntimeMaterialPayload3D emissive =
        runtime_disney_v2_test_payload(0.20, 0.50, 1.0, 0.05, 0.45, 0.7, 0.3, 0.0, 0.8);
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.70, 0.90, 1.0, 0.08, 0.08, 0.25, 0.75, 0.65, 0.0);
    RuntimeDisneyV2_3DResult matte_result = {0};
    RuntimeDisneyV2_3DResult metal_result = {0};
    RuntimeDisneyV2_3DResult emissive_result = {0};
    RuntimeDisneyV2_3DResult glass_result = {0};
    RuntimeDisneyV2_3DDiagnostics matte_diag = {0};
    RuntimeDisneyV2_3DDiagnostics metal_diag = {0};
    RuntimeDisneyV2_3DDiagnostics emissive_diag = {0};
    RuntimeDisneyV2_3DDiagnostics glass_diag = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    runtime_disney_v2_test_init_scene(&scene);
    assert_true("runtime_disney_v2_order_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene, &primary, &matte, NULL, &matte_result);
    assert_true("runtime_disney_v2_order_matte_ok", ok);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene, &primary, &metal, NULL, &metal_result);
    assert_true("runtime_disney_v2_order_metal_ok", ok);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene, &primary, &emissive, NULL, &emissive_result);
    assert_true("runtime_disney_v2_order_emissive_ok", ok);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene, &primary, &glass, NULL, &glass_result);
    assert_true("runtime_disney_v2_order_glass_ok", ok);

    assert_true("runtime_disney_v2_order_matte_diffuse_probability",
                matte_result.diffuseProbability > metal_result.diffuseProbability);
    assert_true("runtime_disney_v2_order_metal_specular_probability",
                metal_result.specularProbability > matte_result.specularProbability);
    assert_true("runtime_disney_v2_order_metal_specular_radiance",
                metal_result.specularRadiance > matte_result.specularRadiance);
    assert_true("runtime_disney_v2_order_emissive_channel",
                emissive_result.emissionRadiance > matte_result.emissionRadiance);
    assert_true("runtime_disney_v2_order_transmission_channel",
                glass_result.transmissionRadiance > matte_result.transmissionRadiance);
    assert_true("runtime_disney_v2_order_secondary_transport_active",
                matte_result.pathState.valid && metal_result.pathState.valid &&
                matte_result.secondaryRayCount == 1 && metal_result.secondaryRayCount == 1);

    ok = RuntimeDisneyV2_3D_BuildDiagnostics(&matte_result, &matte_diag);
    assert_true("runtime_disney_v2_diag_matte_ok", ok);
    ok = RuntimeDisneyV2_3D_BuildDiagnostics(&metal_result, &metal_diag);
    assert_true("runtime_disney_v2_diag_metal_ok", ok);
    ok = RuntimeDisneyV2_3D_BuildDiagnostics(&emissive_result, &emissive_diag);
    assert_true("runtime_disney_v2_diag_emissive_ok", ok);
    ok = RuntimeDisneyV2_3D_BuildDiagnostics(&glass_result, &glass_diag);
    assert_true("runtime_disney_v2_diag_glass_ok", ok);
    assert_true("runtime_disney_v2_diag_route_ready",
                matte_diag.routeProofReady && metal_diag.routeProofReady &&
                emissive_diag.routeProofReady && glass_diag.routeProofReady);
    assert_true("runtime_disney_v2_diag_matte_dominant_diffuse",
                matte_diag.dominantLobe == RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE);
    assert_true("runtime_disney_v2_diag_metal_dominant_specular",
                metal_diag.dominantLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR);
    assert_true("runtime_disney_v2_diag_emissive_signal",
                emissive_diag.hasEmissionSignal &&
                emissive_diag.emissionStrength > matte_diag.emissionStrength);
    assert_true("runtime_disney_v2_diag_transmission_signal",
                glass_diag.hasTransmissionSignal &&
                glass_diag.transmissionWeight > matte_diag.transmissionWeight);
    assert_true("runtime_disney_v2_diag_ratio_orders_specular",
                metal_diag.specularToDiffuseRatio > matte_diag.specularToDiffuseRatio);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_sampling_context_moves_bsdf_path_state(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D payload =
        runtime_disney_v2_test_payload(0.55, 0.40, 0.25, 0.22, 0.65, 0.85, 0.20, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling_a = {
        .sampleSequence = 101U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 2U,
    };
    RuntimeNative3DSamplingContext sampling_b = {
        .sampleSequence = 102U,
        .temporalSubpassIndex = 1U,
        .temporalSubpassCount = 2U,
    };
    RuntimeDisneyV2_3DResult result_a = {0};
    RuntimeDisneyV2_3DResult result_b = {0};
    double direction_delta = 0.0;
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    runtime_disney_v2_test_init_scene(&scene);
    assert_true("runtime_disney_v2_sampling_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &payload,
                                                       &sampling_a,
                                                       &result_a);
    assert_true("runtime_disney_v2_sampling_a_ok", ok);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &payload,
                                                       &sampling_b,
                                                       &result_b);
    assert_true("runtime_disney_v2_sampling_b_ok", ok);

    assert_true("runtime_disney_v2_sampling_a_path_ready",
                result_a.pathState.valid && result_a.bsdfSamplePdf > 0.0);
    assert_true("runtime_disney_v2_sampling_b_path_ready",
                result_b.pathState.valid && result_b.bsdfSamplePdf > 0.0);
    assert_true("runtime_disney_v2_sampling_a_light_sample",
                result_a.stochasticDirectRadiance > 0.0 && result_a.misWeightLight > 0.0);
    assert_true("runtime_disney_v2_sampling_b_light_sample",
                result_b.stochasticDirectRadiance > 0.0 && result_b.misWeightLight > 0.0);
    assert_close("runtime_disney_v2_sampling_a_mis_sum",
                 result_a.misWeightLight + result_a.misWeightBsdf,
                 1.0,
                 1e-9);
    assert_close("runtime_disney_v2_sampling_b_mis_sum",
                 result_b.misWeightLight + result_b.misWeightBsdf,
                 1.0,
                 1e-9);
    direction_delta =
        fabs(result_a.pathState.ray.direction.x - result_b.pathState.ray.direction.x) +
        fabs(result_a.pathState.ray.direction.y - result_b.pathState.ray.direction.y) +
        fabs(result_a.pathState.ray.direction.z - result_b.pathState.ray.direction.z);
    assert_true("runtime_disney_v2_sampling_context_changes_bsdf_ray",
                direction_delta > 1e-5);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_one_bounce_geometry_contributes(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    runtime_disney_v2_test_init_one_bounce_scene(&scene);
    assert_true("runtime_disney_v2_one_bounce_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_one_bounce_ok", ok);
    assert_true("runtime_disney_v2_one_bounce_path_valid", result.pathState.valid);
    assert_true("runtime_disney_v2_one_bounce_specular_lobe",
                result.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR);
    assert_true("runtime_disney_v2_one_bounce_geometry_hit",
                result.pathState.hit && result.secondaryHitCount == 1);
    assert_true("runtime_disney_v2_one_bounce_hits_ceiling",
                result.pathState.hitInfo.triangleIndex == 1 &&
                result.pathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_one_bounce_contributes",
                result.secondaryContributingHitCount == 1 &&
                result.stochasticBsdfRadiance > 0.0);
    assert_true("runtime_disney_v2_one_bounce_final_includes_bsdf",
                result.radiance >= result.stochasticBsdfRadiance &&
                result.radiance > result.directRadiance);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_secondary_material_vertex_modulates_contribution(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult red_result = {0};
    RuntimeDisneyV2_3DResult blue_result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_one_bounce_scene(&scene);
    assert_true("runtime_disney_v2_secondary_vertex_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_GLOSSY,
                                                    0xFFFFFF,
                                                    0.9,
                                                    0.02);

    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF0000,
                                                    0.1,
                                                    0.6);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &red_result);
    assert_true("runtime_disney_v2_secondary_vertex_red_ok", ok);

    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x0000FF,
                                                    0.1,
                                                    0.6);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &blue_result);
    assert_true("runtime_disney_v2_secondary_vertex_blue_ok", ok);

    assert_true("runtime_disney_v2_secondary_vertex_first_hit_stable",
                red_result.payloadResolved &&
                blue_result.payloadResolved &&
                fabs(red_result.principled.baseColorR - glossy.baseColorR) < 1e-12 &&
                fabs(blue_result.principled.baseColorB - glossy.baseColorB) < 1e-12);
    assert_true("runtime_disney_v2_secondary_vertex_payloads",
                red_result.secondaryPayloadResolved &&
                blue_result.secondaryPayloadResolved &&
                red_result.secondaryPayload.sceneObjectIndex == 1 &&
                blue_result.secondaryPayload.sceneObjectIndex == 1 &&
                red_result.secondaryPrincipled.valid &&
                blue_result.secondaryPrincipled.valid);
    assert_true("runtime_disney_v2_secondary_vertex_material_identity",
                red_result.secondaryPayload.materialId == MATERIAL_PRESET_DEFAULT &&
                blue_result.secondaryPayload.materialId == MATERIAL_PRESET_DEFAULT &&
                red_result.pathState.hitInfo.sceneObjectIndex == 1 &&
                blue_result.pathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_secondary_vertex_red_response",
                red_result.secondaryPrincipled.baseColorR >
                    red_result.secondaryPrincipled.baseColorB + 0.5 &&
                red_result.secondaryMaterialResponseR >
                    red_result.secondaryMaterialResponseB + 0.5 &&
                red_result.secondaryVertexThroughputR >
                    red_result.secondaryVertexThroughputB + 0.1);
    assert_true("runtime_disney_v2_secondary_vertex_blue_response",
                blue_result.secondaryPrincipled.baseColorB >
                    blue_result.secondaryPrincipled.baseColorR + 0.5 &&
                blue_result.secondaryMaterialResponseB >
                    blue_result.secondaryMaterialResponseR + 0.5 &&
                blue_result.secondaryVertexThroughputB >
                    blue_result.secondaryVertexThroughputR + 0.1);
    assert_true("runtime_disney_v2_secondary_vertex_contribution_changes",
                red_result.stochasticBsdfRadianceR >
                    red_result.stochasticBsdfRadianceB + 1e-6 &&
                blue_result.stochasticBsdfRadianceB >
                    blue_result.stochasticBsdfRadianceR + 1e-6 &&
                fabs(red_result.stochasticBsdfRadianceR -
                     blue_result.stochasticBsdfRadianceR) > 1e-6 &&
                fabs(red_result.stochasticBsdfRadianceB -
                     blue_result.stochasticBsdfRadianceB) > 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_one_bounce_light_emitter_contributes(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.85, 0.82, 0.72, 0.95, 0.0, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 3U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    runtime_disney_v2_test_init_light_hit_scene(&scene);
    assert_true("runtime_disney_v2_light_hit_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_light_hit_ok", ok);
    assert_true("runtime_disney_v2_light_hit_path_valid", result.pathState.valid);
    assert_true("runtime_disney_v2_light_hit_specular_lobe",
                result.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR);
    assert_true("runtime_disney_v2_light_hit_emitter_wins",
                result.pathState.emitterHit && result.pathState.emitterWins);
    assert_true("runtime_disney_v2_light_hit_no_secondary_geometry",
                !result.pathState.hit && result.secondaryHitCount == 0);
    assert_true("runtime_disney_v2_light_hit_emitter_radiance",
                result.pathState.emitterHitInfo.radiance > 0.0 &&
                result.stochasticBsdfRadiance > 0.0);
    assert_true("runtime_disney_v2_light_hit_contributes",
                result.secondaryContributingHitCount == 1 &&
                result.radiance >= result.stochasticBsdfRadiance);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_transmission_glass_participates(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.66, 0.88, 1.0, 0.0, 0.02, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 3U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_transmission_scene(&scene);
    assert_true("runtime_disney_v2_transmission_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_transmission_ok", ok);
    assert_true("runtime_disney_v2_transmission_lobe",
                result.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION);
    assert_true("runtime_disney_v2_transmission_policy",
                result.pathPolicyResolved &&
                result.sampledLobeMaxDepth == 1 &&
                !result.pathDepthLimitReached);
    assert_true("runtime_disney_v2_transmission_path_state",
                result.pathState.valid &&
                result.pathState.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION &&
                result.pathState.ray.direction.y < -0.5 &&
                result.bsdfSamplePdf > 0.0);
    assert_true("runtime_disney_v2_transmission_emitter",
                result.pathState.emitterHit &&
                result.pathState.emitterWins &&
                result.pathState.emitterHitInfo.radiance > 0.0);
    assert_true("runtime_disney_v2_transmission_throughput_tinted",
                result.pathState.throughputB >= result.pathState.throughputG &&
                result.pathState.throughputG >= result.pathState.throughputR &&
                runtime_disney_v2_3d_test_peak(result.pathState.throughputR,
                                               result.pathState.throughputG,
                                               result.pathState.throughputB) > 0.0);
    assert_true("runtime_disney_v2_transmission_contributes",
                result.stochasticBsdfRadiance > 0.0 &&
                result.radiance >= result.stochasticBsdfRadiance &&
                result.secondaryContributingHitCount == 1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_primary_transparency_continues_camera_ray(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.72, 0.90, 1.0, 0.0, 0.02, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 3U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult continued = {0};
    RuntimeDisneyV2_3DResult blocked = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_primary_transparency_scene(&scene);
    assert_true("runtime_disney_v2_primary_transparency_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xB8E6FF,
                                                    0.0,
                                                    0.02);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &continued);
    assert_true("runtime_disney_v2_primary_transparency_ok", ok);

    animSettings.transmissionDepth3D = 0;
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &blocked);
    assert_true("runtime_disney_v2_primary_transparency_blocked_ok", ok);

    assert_true("runtime_disney_v2_primary_transparency_continued",
                continued.primaryTransmissionContinued &&
                continued.primaryTransmissionPathState.valid &&
                continued.primaryTransmissionPathState.hit &&
                continued.primaryTransmissionPathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_primary_transparency_contributes",
                continued.primaryTransmissionRadiance > 0.0 &&
                continued.primaryTransmissionRadianceR >
                    continued.primaryTransmissionRadianceB + 1e-6 &&
                continued.radianceR > blocked.radianceR + 1e-6);
    assert_true("runtime_disney_v2_primary_transparency_policy_blocks",
                !blocked.primaryTransmissionContinued &&
                blocked.primaryTransmissionRadiance == 0.0);
    assert_true("runtime_disney_v2_primary_transparency_surface_weight",
                continued.primaryTransmissionSurfaceWeight > 0.0 &&
                continued.primaryTransmissionSurfaceWeight < 1.0 &&
                continued.primaryTransmissionBlendWeight > 0.0);
    assert_true("runtime_disney_v2_primary_transparency_camera_throughput",
                continued.primaryTransmissionReceiverSampleCount > 0 &&
                continued.primaryTransmissionReceiverShadeCount ==
                    continued.primaryTransmissionReceiverSampleCount &&
                continued.primaryTransmissionReceiverRadiance > 0.0 &&
                runtime_disney_v2_3d_test_peak(
                    continued.primaryTransmissionCameraThroughputR,
                    continued.primaryTransmissionCameraThroughputG,
                    continued.primaryTransmissionCameraThroughputB) > 0.0);
    assert_true("runtime_disney_v2_primary_transparency_front_surface_reduced",
                continued.primaryTransmissionFrontDiffuseWeight < 0.20 &&
                continued.primaryTransmissionFrontSpecularWeight >
                    continued.primaryTransmissionFrontDiffuseWeight &&
                continued.primaryTransmissionRadianceR >
                    continued.diffuseRadianceR + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_nested_rough_primary_transparency_reaches_receiver(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D rough_glass =
        runtime_disney_v2_test_payload(0.70, 0.92, 1.0, 0.0, 0.42, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 19U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult nested = {0};
    RuntimeDisneyV2_3DResult depth_limited = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 3;
    animSettings.specularDepth3D = 3;
    animSettings.transmissionDepth3D = 2;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_MIN;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_nested_transparency_scene(&scene);
    assert_true("runtime_disney_v2_nested_transparency_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xB8E6FF,
                                                    0.0,
                                                    0.42);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xC8F0FF,
                                                    0.0,
                                                    0.35);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x20FF40,
                                                    0.05,
                                                    0.55);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &rough_glass,
                                                       &sampling,
                                                       &nested);
    assert_true("runtime_disney_v2_nested_transparency_ok", ok);

    animSettings.transmissionDepth3D = 1;
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &rough_glass,
                                                       &sampling,
                                                       &depth_limited);
    assert_true("runtime_disney_v2_nested_transparency_depth_limit_ok", ok);

    assert_true("runtime_disney_v2_nested_transparency_reaches_receiver",
                nested.primaryTransmissionContinued &&
                nested.primaryTransmissionPathState.valid &&
                nested.primaryTransmissionPathState.hit &&
                nested.primaryTransmissionPathState.depth == 2 &&
                nested.primaryTransmissionPathState.hitInfo.sceneObjectIndex == 2);
    assert_true("runtime_disney_v2_nested_transparency_samples_rough_glass",
                nested.primaryTransmissionSampleCount == RUNTIME_3D_TRANSMISSION_SAMPLES_MIN &&
                nested.primaryTransmissionRayCount >= RUNTIME_3D_TRANSMISSION_SAMPLES_MIN &&
                nested.primaryTransmissionTransparentSurfaceCount >=
                    RUNTIME_3D_TRANSMISSION_SAMPLES_MIN);
    assert_true("runtime_disney_v2_nested_transparency_green_receiver_contributes",
                nested.primaryTransmissionRadiance > 0.0 &&
                nested.primaryTransmissionRadianceG >
                    nested.primaryTransmissionRadianceR + 1e-6 &&
                nested.radianceG > depth_limited.radianceG + 1e-6);
    assert_true("runtime_disney_v2_nested_transparency_camera_composite",
                nested.primaryTransmissionReceiverSampleCount > 0 &&
                nested.primaryTransmissionReceiverShadeCount ==
                    nested.primaryTransmissionReceiverSampleCount &&
                nested.primaryTransmissionReceiverRadiance > 0.0 &&
                nested.primaryTransmissionFrontDiffuseWeight < 0.25 &&
                nested.primaryTransmissionCameraThroughputG > 0.0 &&
                nested.primaryTransmissionRadianceG >
                    nested.diffuseRadianceG + 1e-6);
    assert_true("runtime_disney_v2_nested_transparency_depth_blocks_nested_pane",
                !depth_limited.primaryTransmissionContinued &&
                depth_limited.primaryTransmissionDepthLimitCount > 0 &&
                depth_limited.primaryTransmissionRadiance == 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_bounded_recursive_participates(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_scene(&scene);
    assert_true("runtime_disney_v2_recursive_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_recursive_ok", ok);
    assert_true("runtime_disney_v2_recursive_first_path_valid",
                result.pathState.valid && result.pathState.hit);
    assert_true("runtime_disney_v2_recursive_first_hits_geometry",
                result.pathState.hitInfo.triangleIndex == 1 &&
                result.secondaryHitCount == 1);
    assert_true("runtime_disney_v2_recursive_depth_two",
                result.pathDepth == 2 && result.secondaryRayCount == 2);
    assert_true("runtime_disney_v2_recursive_policy_resolved",
                result.pathPolicyResolved &&
                result.sampledLobeMaxDepth == 2 &&
                !result.pathDepthLimitReached);
    assert_true("runtime_disney_v2_recursive_roulette_disabled",
                !result.rouletteEvaluated &&
                !result.rouletteTerminated &&
                result.rouletteSurvivalProbability == 1.0);
    assert_true("runtime_disney_v2_recursive_state_valid",
                result.recursivePathState.valid && result.recursivePathState.depth == 2);
    assert_true("runtime_disney_v2_recursive_hits_light",
                result.recursivePathState.emitterHit &&
                result.recursivePathState.emitterWins &&
                result.recursivePathState.emitterHitInfo.radiance > 0.0);
    assert_true("runtime_disney_v2_recursive_loop_diagnostics",
                result.recursiveLoopVertexCount == 1 &&
                result.recursiveLoopRayCount == 1 &&
                result.recursiveLoopEmitterHitCount == 1 &&
                result.recursiveLoopContributingHitCount == 1 &&
                result.recursiveLoopContributionR[0] > 0.0 &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER);
    assert_true("runtime_disney_v2_recursive_throughput_positive",
                runtime_disney_v2_3d_test_peak(result.recursivePathState.throughputR,
                                               result.recursivePathState.throughputG,
                                               result.recursivePathState.throughputB) > 0.0);
    assert_true("runtime_disney_v2_recursive_radiance_positive",
                result.recursiveBsdfRadiance > 0.0 &&
                result.radiance >= result.recursiveBsdfRadiance);
    assert_true("runtime_disney_v2_recursive_contribution_count",
                result.secondaryContributingHitCount == 2);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_mis_and_emitter_accounting_separates_branches(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;
    int i = 0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_scene(&scene);
    assert_true("runtime_disney_v2_mis_accounting_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_mis_accounting_ok", ok);
    assert_true("runtime_disney_v2_mis_accounting_both_branches",
                result.lightSampleContribution > 0.0 &&
                result.bsdfSampleContribution > 0.0 &&
                result.lightSampleContributionCount >= 2 &&
                result.bsdfSampleContributionCount >= 1);
    assert_true("runtime_disney_v2_mis_accounting_primary_and_secondary_light",
                result.lightSampleContributionR[0] > 0.0 &&
                result.lightSampleContributionR[1] > 0.0 &&
                result.stochasticDirectRadianceR == result.lightSampleContributionR[0]);
    assert_true("runtime_disney_v2_mis_accounting_finite_light_emitter",
                result.bsdfSampleContributionR[1] > 0.0 &&
                result.finiteLightEmitterHitCount == 1 &&
                result.emissiveMaterialHitCount == 0 &&
                result.misVertexEmitterKind[1] ==
                    RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT);
    assert_true("runtime_disney_v2_mis_accounting_vertex_count",
                result.misVertexCount >= 2);
    for (i = 0; i < result.misVertexCount; ++i) {
        if (result.misVertexLightPdf[i] + result.misVertexBsdfPdf[i] > 1e-9) {
            assert_close("runtime_disney_v2_mis_accounting_weight_sum",
                         result.misVertexWeightLight[i] + result.misVertexWeightBsdf[i],
                         1.0,
                         1e-9);
        }
    }
    assert_close("runtime_disney_v2_mis_accounting_light_branch_remove_r",
                 result.radianceWithoutLightSamplesR,
                 result.radianceR - result.lightSampleContributionTotalR,
                 1e-9);
    assert_close("runtime_disney_v2_mis_accounting_bsdf_branch_remove_r",
                 result.radianceWithoutBsdfSamplesR,
                 result.radianceR - result.bsdfSampleContributionTotalR,
                 1e-9);
    assert_true("runtime_disney_v2_mis_accounting_branch_removal_isolated",
                fabs(result.radianceWithoutLightSamplesR -
                     result.radianceWithoutBsdfSamplesR) > 1e-6 &&
                result.radianceWithoutLightSamplesR < result.radianceR &&
                result.radianceWithoutBsdfSamplesR < result.radianceR);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_bounded_recursive_loop_depth_three_participates(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 3;
    animSettings.specularDepth3D = 3;
    animSettings.transmissionDepth3D = 3;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_depth_three_scene(&scene);
    assert_true("runtime_disney_v2_recursive_d3_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFFFFFF,
                                                    0.1,
                                                    0.6);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xCCEEFF,
                                                    0.1,
                                                    0.6);
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_recursive_d3_ok", ok);
    assert_true("runtime_disney_v2_recursive_d3_first_hit",
                result.pathState.valid &&
                result.pathState.hit &&
                result.pathState.hitInfo.triangleIndex == 1);
    assert_true("runtime_disney_v2_recursive_d3_loop_hits_second_surface",
                result.recursiveLoopVertexCount == 2 &&
                result.recursiveLoopGeometryHitCount == 1 &&
                result.recursiveLoopStates[0].valid &&
                result.recursiveLoopStates[0].hit &&
                result.recursiveLoopStates[0].hitInfo.triangleIndex == 2 &&
                result.recursiveLoopPrincipled[0].valid &&
                result.recursiveLoopTerminationReasons[0] ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NONE);
    assert_true("runtime_disney_v2_recursive_d3_loop_hits_emitter",
                result.pathDepth == 3 &&
                result.secondaryRayCount == 3 &&
                result.secondaryHitCount == 2 &&
                result.recursiveLoopRayCount == 2 &&
                result.recursiveLoopEmitterHitCount == 1 &&
                result.recursiveLoopStates[1].valid &&
                result.recursiveLoopStates[1].depth == 3 &&
                result.recursiveLoopStates[1].emitterWins &&
                result.recursiveLoopTerminationReasons[1] ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER);
    assert_true("runtime_disney_v2_recursive_d3_contributes",
                result.recursiveBsdfRadiance > 0.0 &&
                result.recursiveLoopContributionR[1] > 0.0 &&
                result.recursiveLoopContributingHitCount == 1 &&
                result.secondaryContributingHitCount >= 1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_bounded_recursive_loop_depth_limit_stops_before_emitter(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_depth_three_scene(&scene);
    assert_true("runtime_disney_v2_recursive_dlimit_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFFFFFF,
                                                    0.1,
                                                    0.6);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xCCEEFF,
                                                    0.1,
                                                    0.6);
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_recursive_dlimit_ok", ok);
    assert_true("runtime_disney_v2_recursive_dlimit_first_loop_surface",
                result.pathDepth == 2 &&
                result.recursiveLoopVertexCount == 1 &&
                result.recursiveLoopStates[0].depth == 2 &&
                result.recursiveLoopStates[0].hit &&
                result.recursiveLoopStates[0].hitInfo.triangleIndex == 2 &&
                result.recursiveLoopPrincipled[0].valid);
    assert_true("runtime_disney_v2_recursive_dlimit_policy_stop",
                result.sampledLobeMaxDepth == 2 &&
                result.pathDepthLimitReached &&
                result.recursiveLoopPolicyTerminationCount == 1 &&
                result.recursiveLoopTerminationReasons[0] ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH);
    assert_true("runtime_disney_v2_recursive_dlimit_no_emitter_contribution",
                result.secondaryRayCount == 2 &&
                result.secondaryHitCount == 2 &&
                result.recursiveLoopEmitterHitCount == 0 &&
                result.recursiveBsdfRadiance == 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_path_depth_policy_blocks_recursive_depth(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glossy =
        runtime_disney_v2_test_payload(0.90, 0.78, 0.52, 0.90, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_scene(&scene);
    assert_true("runtime_disney_v2_depth_policy_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_depth_policy_ok", ok);
    assert_true("runtime_disney_v2_depth_policy_first_path_hit",
                result.pathState.valid && result.pathState.hit);
    assert_true("runtime_disney_v2_depth_policy_resolved",
                result.pathPolicyResolved &&
                result.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR &&
                result.sampledLobeMaxDepth == 1);
    assert_true("runtime_disney_v2_depth_policy_blocks_depth_two",
                result.pathDepth == 1 &&
                result.secondaryRayCount == 1 &&
                result.pathDepthLimitReached &&
                !result.recursivePathState.valid &&
                result.recursiveLoopPolicyTerminationCount == 1 &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH);
    assert_true("runtime_disney_v2_depth_policy_keeps_first_contribution",
                result.stochasticBsdfRadiance > 0.0 &&
                result.recursiveBsdfRadiance == 0.0 &&
                result.secondaryContributingHitCount == 1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_path_policy_roulette_can_terminate_recursive_depth(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D dim_glossy =
        runtime_disney_v2_test_payload(0.02, 0.02, 0.02, 0.01, 0.02, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 7U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    RuntimePathDepthPolicy3D policy = {0};
    double survival = 1.0;
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.1;
    runtime_disney_v2_test_init_recursive_scene(&scene);
    assert_true("runtime_disney_v2_roulette_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    policy = RuntimePathDepthPolicy3D_Resolve();
    survival = RuntimePathDepthPolicy3D_SurvivalProbability(&policy, 2, 0.001);
    assert_true("runtime_disney_v2_roulette_policy_survival_below_one",
                survival > 0.0 && survival < 1.0);
    assert_true("runtime_disney_v2_roulette_policy_can_terminate",
                RuntimePathDepthPolicy3D_ShouldTerminate(&policy, 2, 0.001, 1.0, &survival));

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &dim_glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_roulette_ok", ok);
    assert_true("runtime_disney_v2_roulette_first_path_hit",
                result.pathState.valid && result.pathState.hit);
    assert_true("runtime_disney_v2_roulette_evaluated",
                result.rouletteEvaluated &&
                result.rouletteThroughputLuma > 0.0 &&
                result.rouletteSurvivalProbability > 0.0 &&
                result.rouletteSurvivalProbability < 1.0);
    assert_true("runtime_disney_v2_roulette_terminates_depth_two",
                result.rouletteTerminated &&
                !result.recursivePathState.valid &&
                result.pathDepth == 1 &&
                result.secondaryRayCount == 1 &&
                result.recursiveLoopRouletteTerminationCount == 1 &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_ROULETTE);

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
    test_runtime_disney_v2_3d_consumes_cached_principled_payload();
    test_runtime_disney_v2_3d_material_diagnostics_order_lobes();
    test_runtime_disney_v2_3d_sampling_context_moves_bsdf_path_state();
    test_runtime_disney_v2_3d_one_bounce_geometry_contributes();
    test_runtime_disney_v2_3d_secondary_material_vertex_modulates_contribution();
    test_runtime_disney_v2_3d_one_bounce_light_emitter_contributes();
    test_runtime_disney_v2_3d_transmission_glass_participates();
    test_runtime_disney_v2_3d_primary_transparency_continues_camera_ray();
    test_runtime_disney_v2_3d_nested_rough_primary_transparency_reaches_receiver();
    test_runtime_disney_v2_3d_bounded_recursive_participates();
    test_runtime_disney_v2_3d_mis_and_emitter_accounting_separates_branches();
    test_runtime_disney_v2_3d_bounded_recursive_loop_depth_three_participates();
    test_runtime_disney_v2_3d_bounded_recursive_loop_depth_limit_stops_before_emitter();
    test_runtime_disney_v2_3d_path_depth_policy_blocks_recursive_depth();
    test_runtime_disney_v2_3d_path_policy_roulette_can_terminate_recursive_depth();
    return 0;
}
