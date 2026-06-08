#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_visibility_3d.h"
#include "test_runtime_emission_transparency.h"
#include "test_support.h"

static void runtime_emission_transparency_reset_authoring_state(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
}

static bool runtime_emission_transparency_build_single_surface_scene(
    RuntimeScene3D* scene,
    HitInfo3D* out_hit,
    int material_id,
    double emissive_strength) {
    bool ok = false;

    if (!scene || !out_hit) return false;

    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = material_id;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].emissiveStrength = emissive_strength;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    scene->hasLight = true;
    scene->light.position = vec3(0.0, -2.0, 0.0);
    scene->light.radius = 0.0;
    scene->light.intensity = 10.0;
    scene->light.falloffDistance = 10.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 1;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        return false;
    }

    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 1;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene->primitives[0].source.sceneObjectIndex = 0;
    scene->triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene->triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene->triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    ok = RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
    if (!ok) return false;

    out_hit->t = 5.0;
    out_hit->position = vec3(0.0, -5.0, 0.0);
    out_hit->normal = vec3(0.0, 1.0, 0.0);
    out_hit->triangleIndex = 0;
    out_hit->primitiveIndex = 0;
    out_hit->sceneObjectIndex = 0;
    out_hit->source = scene->primitives[0].source;
    out_hit->baryU = 0.333333333333;
    out_hit->baryV = 0.333333333333;
    out_hit->baryW = 0.333333333334;
    return true;
}

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
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
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

static int test_runtime_emission_transparency_3d_no_emissive_capability_skips_support(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};
    RuntimeEmissionTransparency3DResult invalid_caps_result = {0};
    RuntimeEmissionTransparency3DResult valid_caps_result = {0};
    RuntimeTriangleBVH3DTraceStats invalid_caps_stats = {0};
    RuntimeTriangleBVH3DTraceStats valid_caps_stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_emission_transparency_reset_authoring_state();
    ok = runtime_emission_transparency_build_single_surface_scene(&scene,
                                                                  &hit,
                                                                  MATERIAL_PRESET_DEFAULT,
                                                                  0.0);
    assert_true("runtime_emission_transparency_no_emissive_fixture_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_emission_transparency_no_emissive_caps_valid",
                scene.capabilities.valid);
    assert_true("runtime_emission_transparency_no_emissive_caps_no_emitters",
                !scene.capabilities.hasEmissiveSurfaces);
    assert_true("runtime_emission_transparency_no_emissive_caps_skip_support",
                scene.capabilities.canSkipEmissionSupport);
    assert_true("runtime_emission_transparency_no_emissive_caps_skip_transparency",
                scene.capabilities.canSkipTransparencySupport);
    primary_hit.hit = true;
    primary_hit.primaryRay = RuntimeRay3D_Make(vec3(0.0, 0.0, 0.0),
                                               vec3(0.0, -1.0, 0.0));
    primary_hit.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    primary_hit.hitInfo = hit;

    scene.capabilities.valid = false;
    RuntimeTriangleBVH3D_ResetTraceStats();
    ok = RuntimeEmissionTransparency3D_ShadePrimaryHit(&scene,
                                                       &primary_hit,
                                                       NULL,
                                                       &invalid_caps_result);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&invalid_caps_stats);
    RuntimeTriangleBVH3D_DisableTraceStats();
    assert_true("runtime_emission_transparency_no_emissive_invalid_caps_ok", ok);
    assert_true("runtime_emission_transparency_no_emissive_invalid_caps_secondary_rays",
                invalid_caps_result.secondaryRayCount >= RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);

    RuntimeScene3D_RefreshCapabilities(&scene);
    RuntimeTriangleBVH3D_ResetTraceStats();
    ok = RuntimeEmissionTransparency3D_ShadePrimaryHit(&scene,
                                                       &primary_hit,
                                                       NULL,
                                                       &valid_caps_result);
    RuntimeTriangleBVH3D_SnapshotTraceStats(&valid_caps_stats);
    RuntimeTriangleBVH3D_DisableTraceStats();
    assert_true("runtime_emission_transparency_no_emissive_valid_caps_ok", ok);
    assert_close("runtime_emission_transparency_no_emissive_valid_caps_direct_same",
                 valid_caps_result.directRadiance,
                 invalid_caps_result.directRadiance,
                 1e-9);
    assert_close("runtime_emission_transparency_no_emissive_valid_caps_emissive_zero",
                 valid_caps_result.emissiveDirectRadiance,
                 0.0,
                 1e-12);
    assert_true("runtime_emission_transparency_no_emissive_valid_caps_secondary_not_higher",
                valid_caps_result.secondaryRayCount <= invalid_caps_result.secondaryRayCount);
    (void)invalid_caps_stats;
    (void)valid_caps_stats;

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transparent_capability_keeps_support(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};
    RuntimeEmissionTransparency3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_emission_transparency_reset_authoring_state();
    ok = runtime_emission_transparency_build_single_surface_scene(&scene,
                                                                  &hit,
                                                                  MATERIAL_PRESET_TRANSPARENT,
                                                                  0.0);
    assert_true("runtime_emission_transparency_transparent_caps_fixture_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_emission_transparency_transparent_caps_valid",
                scene.capabilities.valid);
    assert_true("runtime_emission_transparency_transparent_caps_has_transparency",
                scene.capabilities.hasTransparentSurfaces);
    assert_true("runtime_emission_transparency_transparent_caps_no_skip",
                !scene.capabilities.canSkipTransparencySupport);
    primary_hit.hit = true;
    primary_hit.primaryRay = RuntimeRay3D_Make(vec3(0.0, 0.0, 0.0),
                                               vec3(0.0, -1.0, 0.0));
    primary_hit.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    primary_hit.hitInfo = hit;

    ok = RuntimeEmissionTransparency3D_ShadePrimaryHit(&scene,
                                                       &primary_hit,
                                                       NULL,
                                                       &result);
    assert_true("runtime_emission_transparency_transparent_caps_shade_ok", ok);
    assert_true("runtime_emission_transparency_transparent_caps_payload_transparent",
                result.payload.transparency > 0.5);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_emissive_capability_keeps_support(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};
    RuntimeEmissionTransparency3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    runtime_emission_transparency_reset_authoring_state();
    ok = runtime_emission_transparency_build_single_surface_scene(&scene,
                                                                  &hit,
                                                                  MATERIAL_PRESET_EMISSIVE,
                                                                  1.0);
    assert_true("runtime_emission_transparency_emissive_caps_fixture_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_emission_transparency_emissive_caps_valid",
                scene.capabilities.valid);
    assert_true("runtime_emission_transparency_emissive_caps_has_emitters",
                scene.capabilities.hasEmissiveSurfaces);
    assert_true("runtime_emission_transparency_emissive_caps_keeps_support",
                !scene.capabilities.canSkipEmissionSupport);
    primary_hit.hit = true;
    primary_hit.primaryRay = RuntimeRay3D_Make(vec3(0.0, 0.0, 0.0),
                                               vec3(0.0, -1.0, 0.0));
    primary_hit.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    primary_hit.hitInfo = hit;

    ok = RuntimeEmissionTransparency3D_ShadePrimaryHit(&scene,
                                                       &primary_hit,
                                                       NULL,
                                                       &result);
    assert_true("runtime_emission_transparency_emissive_caps_shade_ok", ok);
    assert_true("runtime_emission_transparency_emissive_caps_payload_emissive",
                result.payload.emissive > 0.0);
    assert_true("runtime_emission_transparency_emissive_caps_direct_positive",
                result.emissiveDirectRadiance > 0.0);
    assert_true("runtime_emission_transparency_emissive_caps_secondary_rays",
                result.secondaryRayCount >= RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_dielectric_transport_3d_refraction_and_tir_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeDielectricTransport3D transport = {0};
    bool ok = false;

    payload.valid = true;
    payload.transparency = 0.95;
    payload.bsdf.reflectivity = 0.0;
    payload.bsdf.ior = 1.45;

    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              vec3(0.0, -1.0, 0.0),
                                              &transport);
    assert_true("runtime_dielectric_transport_resolve_normal_ok", ok);
    assert_true("runtime_dielectric_transport_normal_has_refraction", transport.hasRefraction);
    assert_true("runtime_dielectric_transport_normal_no_tir", !transport.totalInternalReflection);
    assert_true("runtime_dielectric_transport_normal_fresnel_small",
                transport.fresnel > 0.0 && transport.fresnel < 0.2);
    assert_true("runtime_dielectric_transport_normal_refracts_forward",
                transport.refractionDir.y < -0.9);

    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              vec3(0.0, 0.2, 0.98),
                                              &transport);
    assert_true("runtime_dielectric_transport_resolve_inside_ok", ok);
    assert_true("runtime_dielectric_transport_inside_tir", transport.totalInternalReflection);
    assert_true("runtime_dielectric_transport_inside_reflection_only",
                !transport.hasRefraction);
    return 0;
}

static int test_runtime_dielectric_transport_3d_thin_walled_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeDielectricTransport3D transport = {0};
    bool ok = false;

    payload.valid = true;
    payload.transparency = 0.95;
    payload.opticalIor = 1.52;
    payload.thinWalled = true;

    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              vec3(0.15, -0.95, 0.25),
                                              &transport);
    assert_true("runtime_dielectric_transport_thin_walled_ok", ok);
    assert_true("runtime_dielectric_transport_thin_walled_has_refraction",
                transport.hasRefraction);
    assert_true("runtime_dielectric_transport_thin_walled_no_tir",
                !transport.totalInternalReflection);
    assert_true("runtime_dielectric_transport_thin_walled_preserves_direction",
                fabs(transport.refractionDir.x - transport.incidentDir.x) < 1e-6 &&
                fabs(transport.refractionDir.y - transport.incidentDir.y) < 1e-6 &&
                fabs(transport.refractionDir.z - transport.incidentDir.z) < 1e-6);
    return 0;
}

static int test_runtime_visibility_3d_beer_lambert_absorption_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeVisibility3DTransmittance solid_once = RuntimeVisibility3D_UnitTransmittance();
    RuntimeVisibility3DTransmittance solid_twice = RuntimeVisibility3D_UnitTransmittance();
    RuntimeVisibility3DTransmittance thin_layer = RuntimeVisibility3D_UnitTransmittance();

    payload.valid = true;
    payload.transparency = 0.8;
    payload.baseColorR = 0.5;
    payload.baseColorG = 0.25;
    payload.baseColorB = 1.0;
    payload.absorptionDistance = 2.0;
    payload.thinWalled = false;

    RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload, 2.0, &solid_once);
    RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload, 4.0, &solid_twice);

    assert_close("runtime_visibility_beer_lambert_solid_once_r",
                 solid_once.r,
                 0.8 * 0.5,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_solid_once_g",
                 solid_once.g,
                 0.8 * 0.25,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_solid_once_b",
                 solid_once.b,
                 0.8,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_solid_twice_r",
                 solid_twice.r,
                 0.8 * 0.25,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_solid_twice_g",
                 solid_twice.g,
                 0.8 * 0.0625,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_solid_twice_b",
                 solid_twice.b,
                 0.8,
                 1e-6);

    payload.thinWalled = true;
    RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(&payload, 4.0, &thin_layer);
    assert_close("runtime_visibility_beer_lambert_thin_layer_r",
                 thin_layer.r,
                 solid_once.r,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_thin_layer_g",
                 thin_layer.g,
                 solid_once.g,
                 1e-6);
    assert_close("runtime_visibility_beer_lambert_thin_layer_b",
                 thin_layer.b,
                 solid_once.b,
                 1e-6);
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
    sceneSettings.sceneObjects[0].alpha = 1.0;
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
    assert_true("runtime_emission_transparency_prism_direct_differs",
                fabs(transparent_result.directRadiance - material_result.directRadiance) > 1e-6);
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
    sceneSettings.sceneObjects[0].alpha = 1.0;

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

static int test_runtime_emission_transparency_3d_clear_surface_reduces_legacy_front_floor(void) {
    SceneConfig saved_scene = sceneSettings;
    char dir_template[] = "/tmp/ray_tracing_clear_front_floorXXXXXX";
    char clear_path[PATH_MAX];
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    RuntimeLightEmitterHit3DResult emitter_result = {0};
    Ray3D transmission_ray = {0};
    const Material* material = NULL;
    double legacy_front_weight = 0.0;
    double legacy_transparency = 0.0;
    double legacy_direct = 0.0;
    bool ok = false;
    int clear_id = -1;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        return 0;
    }

    snprintf(clear_path, sizeof(clear_path), "%s/00_clear.json", dir_template);
    ok = write_text_file(clear_path,
                         "{"
                         "\"diffuse\":0.05,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":0.08,"
                         "\"transparency\":0.95,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("runtime_emission_transparency_clear_floor_file_ok", ok);
    MaterialManagerLoadDir(dir_template);

    for (int i = 0; i < MaterialManagerCount(); ++i) {
        material = MaterialManagerGet(i);
        if (material && material->transparency > 0.9f) {
            clear_id = i;
        }
    }
    assert_true("runtime_emission_transparency_clear_floor_id_found", clear_id >= 0);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = clear_id;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;

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
    assert_true("runtime_emission_transparency_clear_floor_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_emission_transparency_clear_floor_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        remove(clear_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
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
             "clear_wall");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -4.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(3.0, -4.0, -3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, -4.0, 3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_clear_floor_projector_ok", ok);
    if (!ok) {
        remove(clear_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_clear_floor_material_ok", ok);
    transmission_ray = RuntimeRay3D_MakeOffset(material_result.hitInfo.position,
                                               material_result.hitInfo.normal,
                                               material_result.primaryRay.direction,
                                               1e-4);
    ok = RuntimeLightEmitter3D_IntersectRay(&scene,
                                            &transmission_ray,
                                            1e-4,
                                            32.0,
                                            &emitter_result);
    assert_true("runtime_emission_transparency_clear_floor_transmission_ray_hits_emitter", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_clear_floor_branch_ok", ok);
    assert_true("runtime_emission_transparency_clear_floor_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_clear_floor_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_clear_floor_is_nearly_clear",
                transparent_result.payload.transparency > 0.9);
    legacy_front_weight = 1.0 - transparent_result.payload.transparency;
    if (legacy_front_weight < 0.2) {
        legacy_front_weight = 0.2;
    }
    legacy_transparency = 1.0 - legacy_front_weight;
    legacy_direct = (material_result.directRadiance * legacy_front_weight) +
                    (emitter_result.radiance * legacy_transparency);
    assert_true("runtime_emission_transparency_clear_floor_emitter_contributes",
                transparent_result.transmittedDirectRadiance > 0.05);
    assert_true("runtime_emission_transparency_clear_floor_softens_hard_floor",
                transparent_result.directRadiance > legacy_direct + 0.01);
    assert_true("runtime_emission_transparency_clear_floor_keeps_some_front_presence",
                transparent_result.directRadiance < emitter_result.radiance - 0.01);

    remove(clear_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_emission_transparency_3d_nested_transparent_layers_do_not_consume_bounce_depth(
    void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_nested_layers\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"front_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.5,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "},"
          "{"
            "\"object_id\":\"mid_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-6.5,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "},"
          "{"
            "\"object_id\":\"back_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-9.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-9.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-3.0,\"z\":2.0}}],"
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
    assert_true("runtime_emission_transparency_nested_layers_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0x88AAFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[1].color = 0xCCE6FF;
    sceneSettings.sceneObjects[1].alpha = 1.0;
    sceneSettings.sceneObjects[2].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[2].color = 0xFFFFFF;
    sceneSettings.sceneObjects[2].emissiveStrength = 1.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_nested_layers_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_nested_layers_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &material_result);
    assert_true("runtime_emission_transparency_nested_layers_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  NULL,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_nested_layers_branch_ok", ok);
    assert_true("runtime_emission_transparency_nested_layers_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_nested_layers_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_nested_layers_front_is_transparent",
                transparent_result.payload.transparency > 0.5);
    assert_true("runtime_emission_transparency_nested_layers_reaches_behind_layers",
                transparent_result.transmittedDirectRadiance > 0.05);
    assert_true("runtime_emission_transparency_nested_layers_mixes_front_and_transmission",
                transparent_result.radiance > transparent_result.transmittedDirectRadiance &&
                    transparent_result.radiance > material_result.radiance * 0.5);
    assert_true("runtime_emission_transparency_nested_layers_counts_nested_support",
                transparent_result.secondaryRayCount > RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
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
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
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
    int before = test_support_failures();

    test_runtime_emission_transparency_3d_seed_branch_contract();
    test_runtime_dielectric_transport_3d_refraction_and_tir_contract();
    test_runtime_dielectric_transport_3d_thin_walled_contract();
    test_runtime_visibility_3d_beer_lambert_absorption_contract();
    test_runtime_emission_transparency_3d_transmission_contract();
    test_runtime_emission_transparency_3d_transparent_prism_reaches_behind_surface();
    test_runtime_emission_transparency_3d_transparent_prism_reaches_emitter();
    test_runtime_emission_transparency_3d_clear_surface_reduces_legacy_front_floor();
    test_runtime_emission_transparency_3d_nested_transparent_layers_do_not_consume_bounce_depth();
    test_runtime_emission_transparency_3d_temporal_skips_stable_emitters();
    test_runtime_emission_transparency_3d_no_emissive_capability_skips_support();
    test_runtime_emission_transparency_3d_transparent_capability_keeps_support();
    test_runtime_emission_transparency_3d_emissive_capability_keeps_support();
    return test_support_failures() - before;
}
