#include <math.h>
#include <stdio.h>
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
#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_disney_v2_transmission_internal_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_disney_v2_estimator_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_disney_v2_transport_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_specular_reflection_3d.h"
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

static bool runtime_disney_v2_test_write_text_file(const char* path, const char* text) {
    FILE* file = NULL;
    if (!path || !text) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    fputs(text, file);
    fclose(file);
    return true;
}

static bool runtime_disney_v2_test_load_transparent_preset(bool thin_walled,
                                                           double absorption_distance) {
    char dir_template[] = "/tmp/raytracing_disney_v2_materials_XXXXXX";
    char transparent_path[512];
    char json[1024];
    bool ok = false;

    if (!mkdtemp(dir_template)) {
        return false;
    }
    snprintf(transparent_path, sizeof(transparent_path), "%s/transparent.json", dir_template);
    snprintf(json,
             sizeof(json),
             "{"
             "\"diffuse\":0.05,"
             "\"specular\":0.0,"
             "\"reflectivity\":0.0,"
             "\"roughness\":1.0,"
             "\"transparency\":0.75,"
             "\"ior\":1.45,"
             "\"absorption_distance\":%.6f,"
             "\"thin_walled\":%s,"
             "\"base_color\":[1.0,1.0,1.0],"
             "\"emissive\":[0.0,0.0,0.0]"
             "}",
             absorption_distance,
             thin_walled ? "true" : "false");
    ok = runtime_disney_v2_test_write_text_file(transparent_path, json);
    if (ok) {
        MaterialManagerLoadDir(dir_template);
    }
    remove(transparent_path);
    rmdir(dir_template);
    return ok;
}

static bool runtime_disney_v2_test_load_alpha_only_default_preset(double transparency) {
    char dir_template[] = "/tmp/raytracing_disney_v2_alpha_materials_XXXXXX";
    char default_path[512];
    char json[1024];
    bool ok = false;

    if (!mkdtemp(dir_template)) {
        return false;
    }
    snprintf(default_path, sizeof(default_path), "%s/default.json", dir_template);
    snprintf(json,
             sizeof(json),
             "{"
             "\"diffuse\":0.60,"
             "\"specular\":0.05,"
             "\"reflectivity\":0.05,"
             "\"roughness\":0.70,"
             "\"transparency\":%.6f,"
             "\"ior\":1.0,"
             "\"absorption_distance\":1.0,"
             "\"thin_walled\":false,"
             "\"base_color\":[1.0,1.0,1.0],"
             "\"emissive\":[0.0,0.0,0.0]"
             "}",
             transparency);
    ok = runtime_disney_v2_test_write_text_file(default_path, json);
    if (ok) {
        MaterialManagerLoadDir(dir_template);
    }
    remove(default_path);
    rmdir(dir_template);
    return ok;
}

static bool runtime_disney_v2_test_load_preset_override(const char* filename,
                                                        const char* json) {
    char dir_template[] = "/tmp/raytracing_disney_v2_override_materials_XXXXXX";
    char path[512];
    bool ok = false;

    if (!filename || !json || !mkdtemp(dir_template)) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/%s", dir_template, filename);
    ok = runtime_disney_v2_test_write_text_file(path, json);
    if (ok) {
        MaterialManagerLoadDir(dir_template);
    }
    remove(path);
    rmdir(dir_template);
    return ok;
}

static bool runtime_disney_v2_test_load_forced_diffuse_default_preset(void) {
    return runtime_disney_v2_test_load_preset_override(
        "default.json",
        "{"
        "\"diffuse\":1.0,"
        "\"specular\":0.0,"
        "\"reflectivity\":0.0,"
        "\"roughness\":1.0,"
        "\"transparency\":0.0,"
        "\"ior\":1.0,"
        "\"absorption_distance\":1.0,"
        "\"thin_walled\":false,"
        "\"base_color\":[1.0,1.0,1.0],"
        "\"emissive\":[0.0,0.0,0.0]"
        "}");
}

static bool runtime_disney_v2_test_load_forced_transparent_preset(void) {
    return runtime_disney_v2_test_load_preset_override(
        "transparent.json",
        "{"
        "\"diffuse\":0.0,"
        "\"specular\":0.0,"
        "\"reflectivity\":0.0,"
        "\"roughness\":0.02,"
        "\"transparency\":1.0,"
        "\"ior\":1.45,"
        "\"absorption_distance\":1.0,"
        "\"thin_walled\":false,"
        "\"base_color\":[1.0,1.0,1.0],"
        "\"emissive\":[0.0,0.0,0.0]"
        "}");
}

static int test_runtime_dielectric_transport_water_ior_fresnel_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeDielectricTransport3D transport = {0};
    const double water_ior = 1.333;
    const double expected_f0 = pow((water_ior - 1.0) / (water_ior + 1.0), 2.0);
    bool ok = false;

    payload.valid = true;
    payload.opticalIor = water_ior;
    payload.bsdf.ior = water_ior;
    payload.bsdf.reflectivity = 0.0;

    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              vec3(0.0, -1.0, 0.0),
                                              &transport);
    assert_true("runtime_dielectric_transport_water_ior_resolve", ok);
    assert_true("runtime_dielectric_transport_water_ior_refracts", transport.hasRefraction);
    assert_close("runtime_dielectric_transport_water_ior_fresnel",
                 transport.fresnel,
                 expected_f0,
                 1e-6);

    payload.bsdf.reflectivity = 0.25;
    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              vec3(0.0, -1.0, 0.0),
                                              &transport);
    assert_true("runtime_dielectric_transport_water_reflectivity_resolve", ok);
    assert_close("runtime_dielectric_transport_water_reflectivity_override",
                 transport.fresnel,
                 0.25,
                 1e-9);
    return 0;
}

static int test_runtime_dielectric_transport_explicit_unit_ior_straight_through_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeDielectricTransport3D transport = {0};
    Vec3 incident = vec3_normalize(vec3(0.35, -0.82, 0.45));
    bool ok = false;

    payload.valid = true;
    payload.opticalIor = 1.0;
    payload.bsdf.ior = 1.0;
    payload.bsdf.reflectivity = 0.0;

    ok = RuntimeDielectricTransport3D_Resolve(&payload,
                                              vec3(0.0, 1.0, 0.0),
                                              incident,
                                              &transport);
    assert_true("runtime_dielectric_transport_unit_ior_resolve", ok);
    assert_true("runtime_dielectric_transport_unit_ior_has_refraction",
                transport.hasRefraction);
    assert_close("runtime_dielectric_transport_unit_ior_fresnel",
                 transport.fresnel,
                 0.0,
                 1e-9);
    assert_true("runtime_dielectric_transport_unit_ior_preserves_direction",
                fabs(transport.refractionDir.x - incident.x) < 1e-6 &&
                fabs(transport.refractionDir.y - incident.y) < 1e-6 &&
                fabs(transport.refractionDir.z - incident.z) < 1e-6);
    return 0;
}

static int test_runtime_disney_v2_transmission_sample_uses_payload_ior_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D principled = {0};
    RuntimeDisneyV2_3DTransmissionSample straight = {0};
    RuntimeDisneyV2_3DTransmissionSample water = {0};
    HitInfo3D hit = {0};
    Vec3 view_dir = vec3_normalize(vec3(-0.22, 0.74, 0.64));
    Vec3 normal = vec3_normalize(vec3(0.34, 0.0, 0.94));
    bool ok_straight = false;
    bool ok_water = false;

    payload.valid = true;
    payload.transparency = 1.0;
    payload.baseColorR = 1.0;
    payload.baseColorG = 1.0;
    payload.baseColorB = 1.0;
    payload.bsdf.baseColorR = 1.0;
    payload.bsdf.baseColorG = 1.0;
    payload.bsdf.baseColorB = 1.0;
    payload.bsdf.diffuseWeight = 0.0;
    payload.bsdf.specWeight = 0.0;
    payload.bsdf.roughness = 0.02;
    payload.bsdf.weightSum = 1.0;
    payload.thinWalled = false;
    hit.normal = normal;

    payload.opticalIor = 1.0;
    payload.bsdf.ior = 1.0;
    principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    ok_straight = RuntimeDisneyV2_3D_SampleTransmission(&payload,
                                                        &principled,
                                                        &hit,
                                                        view_dir,
                                                        1.0,
                                                        &straight);

    payload.opticalIor = 1.333;
    payload.bsdf.ior = 1.333;
    principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    ok_water = RuntimeDisneyV2_3D_SampleTransmission(&payload,
                                                     &principled,
                                                     &hit,
                                                     view_dir,
                                                     1.0,
                                                     &water);

    assert_true("runtime_disney_v2_transmission_sample_unit_ior_ok", ok_straight);
    assert_true("runtime_disney_v2_transmission_sample_water_ior_ok", ok_water);
    assert_true("runtime_disney_v2_transmission_sample_unit_ior_straight_through",
                vec3_length(vec3_sub(straight.direction,
                                     vec3_scale(vec3_normalize(view_dir), -1.0))) < 1e-6);
    assert_true("runtime_disney_v2_transmission_sample_ior_changes_direction",
                vec3_length(vec3_sub(straight.direction, water.direction)) > 1e-3);
    return 0;
}

static int test_runtime_disney_v2_reflected_transmission_sample_cap_policy(void) {
    AnimationConfig saved_anim = animSettings;
    const char* env_name = "RAY_TRACING_DISNEY_V2_REFLECTED_TRANSMISSION_SAMPLE_CAP";
    const char* saved_cap = getenv(env_name);
    char saved_cap_copy[64] = {0};

    if (saved_cap) {
        snprintf(saved_cap_copy, sizeof(saved_cap_copy), "%s", saved_cap);
    }

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.transmissionSamples3D = 16;

    unsetenv(env_name);
    assert_true("runtime_disney_v2_transmission_samples_primary_default",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_PRIMARY) == 16);
    assert_true("runtime_disney_v2_transmission_samples_reflected_default",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED) == 16);

    setenv(env_name, "4", 1);
    assert_true("runtime_disney_v2_transmission_samples_primary_ignores_cap",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_PRIMARY) == 16);
    assert_true("runtime_disney_v2_transmission_samples_reflected_uses_cap",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED) == 4);

    setenv(env_name, "0", 1);
    assert_true("runtime_disney_v2_transmission_samples_reflected_rejects_zero_cap",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED) == 16);

    setenv(env_name, "invalid", 1);
    assert_true("runtime_disney_v2_transmission_samples_reflected_rejects_invalid_cap",
                runtime_disney_v2_3d_resolve_transmission_sample_count(
                    RUNTIME_DISNEY_V2_3D_TRANSMISSION_CONTINUATION_REFLECTED) == 16);

    restore_env_or_unset(env_name, saved_cap ? saved_cap_copy : NULL);
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
    RuntimeDisneyV2_3DResult disney_v2_result = {0};
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
    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, NULL, &disney_v2_result);
    assert_true("runtime_material_mirror_reflects_chroma_disney_v2_ok", ok);

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
    assert_true("runtime_material_mirror_reflects_chroma_disney_v2_specular_ray",
                disney_v2_result.specularReflectionRayCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_disney_v2_specular_hit",
                disney_v2_result.specularReflectionHitCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_disney_v2_specular_contributes",
                disney_v2_result.specularReflectionContributingHitCount > 0);
    assert_true("runtime_material_mirror_reflects_chroma_disney_v2_red_reflection",
                disney_v2_result.specularReflectionRadianceR >
                    disney_v2_result.specularReflectionRadianceG + 1e-6 &&
                disney_v2_result.specularReflectionRadianceR >
                    disney_v2_result.specularReflectionRadianceB + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_response_3d_mirror_surface_kind_parity(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimePrimitive3DKind kinds[3] = {
        RUNTIME_PRIMITIVE_3D_KIND_PLANE,
        RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM,
        RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH
    };
    const char* labels[3] = {"plane", "prism", "mesh"};
    double reference_specular_r = -1.0;
    bool reference_set = false;

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
    sceneSettings.sceneObjects[1].reflectivity = 0.0;
    sceneSettings.sceneObjects[1].roughness = 0.6;
    animSettings.lightIntensity = 24.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = 0;
    animSettings.transmissionSamples3D = 0;
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    for (int i = 0; i < 3; ++i) {
        RuntimeScene3D scene;
        Ray3D primary_ray = {0};
        HitInfo3D hit = {0};
        RuntimeMaterialResponse3DResult material_result = {0};
        bool ok = false;
        char name[160];

        RuntimeScene3D_Init(&scene);
        scene.hasLight = true;
        scene.light.position = vec3(0.0, -1.0, 0.0);
        scene.light.radius = 0.0;
        scene.light.intensity = 24.0;
        scene.light.falloffDistance = 10.0;
        scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
        scene.primitiveCapacity = 2;
        scene.triangleMesh.triangleCapacity = 2;
        scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                       sizeof(*scene.primitives));
        scene.triangleMesh.triangles =
            (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                       sizeof(*scene.triangleMesh.triangles));
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_alloc", labels[i]);
        assert_true(name, scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
        if (!scene.primitives || !scene.triangleMesh.triangles) {
            RuntimeScene3D_Free(&scene);
            continue;
        }

        scene.primitiveCount = 2;
        scene.triangleMesh.triangleCount = 2;
        scene.primitives[0].source.kind = kinds[i];
        scene.primitives[0].source.sceneObjectIndex = 0;
        snprintf(scene.primitives[0].source.objectId,
                 sizeof(scene.primitives[0].source.objectId),
                 "mirror_%s",
                 labels[i]);
        scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        scene.primitives[1].source.sceneObjectIndex = 1;
        snprintf(scene.primitives[1].source.objectId,
                 sizeof(scene.primitives[1].source.objectId),
                 "reflected_red");

        scene.triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
        scene.triangleMesh.triangles[0].p1 = vec3(0.0, 0.0, 2.0);
        scene.triangleMesh.triangles[0].p2 = vec3(2.0, 0.0, -2.0);
        scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
        scene.triangleMesh.triangles[0].twoSided = kinds[i] == RUNTIME_PRIMITIVE_3D_KIND_PLANE;
        scene.triangleMesh.triangles[0].primitiveIndex = 0;
        scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
        scene.triangleMesh.triangles[0].localTriangleIndex = 0;

        scene.triangleMesh.triangles[1].p0 = vec3(-2.0, -2.0, -2.0);
        scene.triangleMesh.triangles[1].p1 = vec3(0.0, -2.0, 2.0);
        scene.triangleMesh.triangles[1].p2 = vec3(2.0, -2.0, -2.0);
        scene.triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
        scene.triangleMesh.triangles[1].twoSided = false;
        scene.triangleMesh.triangles[1].primitiveIndex = 1;
        scene.triangleMesh.triangles[1].sceneObjectIndex = 1;
        scene.triangleMesh.triangles[1].localTriangleIndex = 0;

        ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_bvh", labels[i]);
        assert_true(name, ok);
        if (!ok) {
            RuntimeScene3D_Free(&scene);
            continue;
        }

        primary_ray = RuntimeRay3D_Make(vec3(0.0, -0.25, 0.0), vec3(0.0, 1.0, 0.0));
        ok = RuntimeRay3D_TraceSceneFirstHit(&scene, &primary_ray, 0.001, 4.0, &hit);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_primary_hit", labels[i]);
        assert_true(name, ok);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_source_kind", labels[i]);
        assert_true(name, hit.source.kind == kinds[i]);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_scene_object", labels[i]);
        assert_true(name, hit.sceneObjectIndex == 0);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_normal_oriented", labels[i]);
        assert_close(name, hit.normal.y, -1.0, 1e-6);

        ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &material_result);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_shade", labels[i]);
        assert_true(name, ok);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_material_resolved", labels[i]);
        assert_true(name,
                    material_result.materialResolved &&
                    material_result.payload.materialId == MATERIAL_PRESET_MIRROR);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_specular_ray", labels[i]);
        assert_true(name, material_result.specularRayCount > 0);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_specular_hit", labels[i]);
        assert_true(name, material_result.specularHitCount > 0);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_specular_contributes", labels[i]);
        assert_true(name, material_result.specularContributingHitCount > 0);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_red_reflection", labels[i]);
        assert_true(name,
                    material_result.specularRadianceR >
                        material_result.specularRadianceG + 1e-6 &&
                    material_result.specularRadianceR >
                        material_result.specularRadianceB + 1e-6);
        snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_direct_separate", labels[i]);
        assert_true(name, material_result.directRadiance > 0.0);

        if (!reference_set) {
            reference_specular_r = material_result.specularRadianceR;
            reference_set = true;
        } else {
            snprintf(name, sizeof(name), "runtime_material_mirror_surface_%s_specular_parity", labels[i]);
            assert_close(name, material_result.specularRadianceR, reference_specular_r, 1e-6);
        }

        RuntimeScene3D_Free(&scene);
    }

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_response_3d_mirror_dominance_reflects_light_emitter(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimeSpecularReflection3DResult reflection = {0};
    RuntimeMaterialResponse3DResult mirror_result = {0};
    RuntimeMaterialResponse3DResult rough_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.95;
    sceneSettings.sceneObjects[0].roughness = 0.02;
    animSettings.lightIntensity = 36.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = 0;
    animSettings.transmissionSamples3D = 0;
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, 2.0, 0.0);
    scene.light.radius = 0.45;
    scene.light.intensity = 36.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_material_mirror_dominance_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
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
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].twoSided = true;
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_material_mirror_dominance_bvh", ok);
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

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload);
    assert_true("runtime_material_mirror_dominance_payload", ok && payload.valid);
    ok = RuntimeSpecularReflection3D_Trace(&scene,
                                           &hit,
                                           &payload,
                                           vec3(0.0, 1.0, 0.0),
                                           NULL,
                                           &reflection);
    assert_true("runtime_material_mirror_dominance_reflection_trace", ok);
    assert_true("runtime_material_mirror_dominance_reflects_emitter",
                reflection.traced && reflection.emitterWins && reflection.emitterHit);

    ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &mirror_result);
    assert_true("runtime_material_mirror_dominance_shade", ok);
    assert_true("runtime_material_mirror_dominance_factor",
                mirror_result.mirrorDominance > 0.90 &&
                mirror_result.mirrorBaseAttenuation < 0.10);
    assert_true("runtime_material_mirror_dominance_attenuates_base",
                mirror_result.mirrorBaseRadianceBeforeAttenuation > 0.0 &&
                mirror_result.mirrorBaseRadianceAfterAttenuation <
                    mirror_result.mirrorBaseRadianceBeforeAttenuation * 0.12);
    assert_true("runtime_material_mirror_dominance_light_visible",
                mirror_result.specularHitCount > 0 &&
                mirror_result.specularContributingHitCount > 0 &&
                mirror_result.specularRadiance >
                    mirror_result.mirrorBaseRadianceAfterAttenuation);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].reflectivity = 0.70;
    sceneSettings.sceneObjects[0].roughness = 0.60;
    ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &rough_result);
    assert_true("runtime_material_mirror_dominance_rough_shade", ok);
    assert_true("runtime_material_mirror_dominance_rough_stays_mixed",
                rough_result.mirrorDominance > 0.05 &&
                rough_result.mirrorDominance < 0.40 &&
                rough_result.mirrorBaseAttenuation > 0.60);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_specular_reflection_reaches_far_geometry(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimeSpecularReflection3DResult reflection = {0};
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

    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_specular_reflection_far_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
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
             "far_mirror");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene.primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "far_reflected_red");

    scene.triangleMesh.triangles[0].p0 = vec3(-4.0, 0.0, -4.0);
    scene.triangleMesh.triangles[0].p1 = vec3(4.0, 0.0, -4.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 4.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].twoSided = true;
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    scene.triangleMesh.triangles[0].localTriangleIndex = 0;

    scene.triangleMesh.triangles[1].p0 = vec3(-4.0, 64.0, -4.0);
    scene.triangleMesh.triangles[1].p1 = vec3(0.0, 64.0, 4.0);
    scene.triangleMesh.triangles[1].p2 = vec3(4.0, 64.0, -4.0);
    scene.triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene.triangleMesh.triangles[1].twoSided = true;
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 1;
    scene.triangleMesh.triangles[1].localTriangleIndex = 0;

    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_specular_reflection_far_bvh", ok);
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

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload);
    assert_true("runtime_specular_reflection_far_payload", ok && payload.valid);
    ok = RuntimeSpecularReflection3D_Trace(&scene,
                                           &hit,
                                           &payload,
                                           vec3(0.0, 1.0, 0.0),
                                           NULL,
                                           &reflection);
    assert_true("runtime_specular_reflection_far_trace", ok && reflection.traced);
    assert_true("runtime_specular_reflection_far_geometry_hit",
                reflection.geometryHit && !reflection.emitterWins);
    assert_true("runtime_specular_reflection_far_scene_object",
                reflection.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_specular_reflection_far_distance",
                reflection.hitInfo.t > 63.0 && reflection.hitInfo.t < 65.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_3d_illuminated_mirror_preserves_reflected_geometry(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
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
    sceneSettings.sceneObjects[1].reflectivity = 0.0;
    sceneSettings.sceneObjects[1].roughness = 0.6;
    animSettings.lightIntensity = 80.0;
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
    scene.light.intensity = 80.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_disney_illuminated_mirror_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
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
    assert_true("runtime_disney_illuminated_mirror_bvh", ok);
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

    ok = RuntimeMaterialResponse3D_ShadeHit(&scene, &hit, NULL, &material_result);
    assert_true("runtime_disney_illuminated_mirror_material_ok", ok);
    ok = RuntimeDisney3D_ShadeHit(&scene, &hit, NULL, &disney_result);
    assert_true("runtime_disney_illuminated_mirror_disney_ok", ok);

    assert_true("runtime_disney_illuminated_mirror_dominant",
                material_result.mirrorDominance > 0.90 &&
                material_result.mirrorBaseAttenuation < 0.10);
    assert_true("runtime_disney_illuminated_mirror_base_attenuated",
                material_result.mirrorBaseRadianceAfterAttenuation <
                    material_result.mirrorBaseRadianceBeforeAttenuation * 0.12);
    assert_true("runtime_disney_illuminated_mirror_material_reflection_red",
                material_result.specularRadianceR >
                    material_result.specularRadianceG + 1e-6 &&
                material_result.specularRadianceR >
                    material_result.specularRadianceB + 1e-6);
    assert_true("runtime_disney_illuminated_mirror_disney_reflection_red",
                disney_result.specularRadianceR >
                    disney_result.specularRadianceG + 1e-6 &&
                disney_result.specularRadianceR >
                    disney_result.specularRadianceB + 1e-6);
    assert_true("runtime_disney_illuminated_mirror_specular_over_base",
                disney_result.specularRadiance >
                    disney_result.baseRadiance * 2.0);
    assert_true("runtime_disney_illuminated_mirror_total_keeps_reflection",
                disney_result.radianceR >
                    disney_result.radianceG + 1e-6 &&
                disney_result.radianceR >
                    disney_result.radianceB + 1e-6);

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

static void runtime_disney_v2_test_init_reflection_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 1.5, 2.0);
    scene->light.radius = 0.45;
    scene->light.intensity = 20.0;
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

    scene->triangleMesh.triangles[1].p0 = vec3(-2.0, 3.0, -2.0);
    scene->triangleMesh.triangles[1].p1 = vec3(0.0, 3.0, 2.0);
    scene->triangleMesh.triangles[1].p2 = vec3(2.0, 3.0, -2.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static void runtime_disney_v2_test_init_reflected_transparency_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, 1.5, 2.0);
    scene->light.radius = 0.45;
    scene->light.intensity = 20.0;
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
    for (int i = 0; i < 3; ++i) {
        scene->primitives[i].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
        scene->primitives[i].source.sceneObjectIndex = i;
    }

    scene->triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene->triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;

    scene->triangleMesh.triangles[1].p0 = vec3(-3.0, 3.0, -3.0);
    scene->triangleMesh.triangles[1].p1 = vec3(0.0, 3.0, 3.0);
    scene->triangleMesh.triangles[1].p2 = vec3(3.0, 3.0, -3.0);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 1;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 1;

    scene->triangleMesh.triangles[2].p0 = vec3(-4.0, 4.5, -4.0);
    scene->triangleMesh.triangles[2].p1 = vec3(0.0, 4.5, 4.0);
    scene->triangleMesh.triangles[2].p2 = vec3(4.0, 4.5, -4.0);
    scene->triangleMesh.triangles[2].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[2].primitiveIndex = 2;
    scene->triangleMesh.triangles[2].sceneObjectIndex = 2;

    (void)RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
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

static int test_runtime_disney_v2_3d_direct_light_pdf_estimator(void) {
    const double inv_pi = 0.31830988618379067154;
    HitInfo3D hit = {0};
    RuntimePrincipledBSDF3D matte = RuntimePrincipledBSDF3D_Default();
    RuntimePrincipledBSDF3D smooth_gloss = RuntimePrincipledBSDF3D_Default();
    RuntimePrincipledBSDF3D rough_gloss = RuntimePrincipledBSDF3D_Default();
    RuntimePrincipledBSDF3D glass = RuntimePrincipledBSDF3D_Default();
    RuntimePrincipledBSDF3D rough_glass = RuntimePrincipledBSDF3D_Default();
    double matte_pdf = 0.0;
    double smooth_gloss_pdf = 0.0;
    double rough_gloss_pdf = 0.0;
    double glass_forward_pdf = 0.0;
    double glass_back_pdf = 0.0;
    double glass_oblique_back_pdf = 0.0;
    double rough_glass_back_pdf = 0.0;
    double glass_disabled_pdf = 0.0;
    double glass_tir_pdf = 0.0;
    const double no_light_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(false, 0.0, 0.0, 0, false);
    const double point_light_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(true, 0.0, 4.0, 0, false);
    const double emissive_area_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(false, 0.0, 0.0, 1, true);
    const double small_radius_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(true, 0.25, 4.0, 0, false);
    const double large_radius_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(true, 1.00, 4.0, 0, false);
    const double far_light_pdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdf(true, 0.25, 8.0, 0, false);

    assert_close("runtime_disney_v2_direct_pdf_no_light", no_light_pdf, 0.0, 1e-12);
    assert_close("runtime_disney_v2_direct_pdf_point_light",
                 point_light_pdf,
                 1.0,
                 1e-12);
    assert_close("runtime_disney_v2_direct_pdf_emissive_area",
                 emissive_area_pdf,
                 1.0,
                 1e-12);
    assert_true("runtime_disney_v2_direct_pdf_finite_positive", small_radius_pdf > 1.0);
    assert_true("runtime_disney_v2_direct_pdf_radius_scales",
                large_radius_pdf < small_radius_pdf);
    assert_true("runtime_disney_v2_direct_pdf_distance_scales",
                far_light_pdf > small_radius_pdf);

    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.position = vec3(0.0, 0.0, 0.0);

    matte.diffuseWeight = 1.0;
    matte.specularWeight = 0.0;
    matte.transmissionWeight = 0.0;
    matte = RuntimePrincipledBSDF3D_Normalize(matte);
    matte_pdf = RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&matte,
                                                         &hit,
                                                         vec3(0.0, -1.0, 0.0),
                                                         vec3(0.0, 1.0, 0.0),
                                                         false);
    assert_close("runtime_disney_v2_direct_bsdf_pdf_diffuse_lambert",
                 matte_pdf,
                 inv_pi,
                 1e-9);

    smooth_gloss.diffuseWeight = 0.0;
    smooth_gloss.specularWeight = 1.0;
    smooth_gloss.transmissionWeight = 0.0;
    smooth_gloss.roughness = 0.08;
    smooth_gloss = RuntimePrincipledBSDF3D_Normalize(smooth_gloss);
    rough_gloss = smooth_gloss;
    rough_gloss.roughness = 0.70;
    rough_gloss = RuntimePrincipledBSDF3D_Normalize(rough_gloss);
    smooth_gloss_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&smooth_gloss,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, 1.0, 0.0),
                                                 false);
    rough_gloss_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&rough_gloss,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, 1.0, 0.0),
                                                 false);
    assert_true("runtime_disney_v2_direct_bsdf_pdf_specular_positive",
                smooth_gloss_pdf > 0.0 && rough_gloss_pdf > 0.0);
    assert_true("runtime_disney_v2_direct_bsdf_pdf_specular_roughness",
                smooth_gloss_pdf > rough_gloss_pdf);

    glass.diffuseWeight = 0.0;
    glass.specularWeight = 0.0;
    glass.transmissionWeight = 1.0;
    glass.baseColorR = 1.0;
    glass.baseColorG = 1.0;
    glass.baseColorB = 1.0;
    glass.ior = 1.5;
    glass = RuntimePrincipledBSDF3D_Normalize(glass);
    rough_glass = glass;
    rough_glass.roughness = 0.85;
    rough_glass = RuntimePrincipledBSDF3D_Normalize(rough_glass);
    glass_forward_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&glass,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, 1.0, 0.0),
                                                 true);
    glass_back_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&glass,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, -1.0, 0.0),
                                                 true);
    glass_oblique_back_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&glass,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3_normalize(vec3(0.35, -1.0, 0.0)),
                                                 true);
    rough_glass_back_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&rough_glass,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, -1.0, 0.0),
                                                 true);
    glass_disabled_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&glass,
                                                 &hit,
                                                 vec3(0.0, -1.0, 0.0),
                                                 vec3(0.0, -1.0, 0.0),
                                                 false);
    glass_tir_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&glass,
                                                 &hit,
                                                 vec3_normalize(vec3(0.8, 0.6, 0.0)),
                                                 vec3(0.0, 1.0, 0.0),
                                                 true);
    assert_close("runtime_disney_v2_direct_bsdf_pdf_transmission_front",
                 glass_forward_pdf,
                 0.0,
                 1e-12);
    assert_true("runtime_disney_v2_direct_bsdf_pdf_transmission_back_positive",
                glass_back_pdf > 0.0);
    assert_true("runtime_disney_v2_direct_bsdf_pdf_transmission_refraction_peak",
                glass_back_pdf > glass_oblique_back_pdf);
    assert_true("runtime_disney_v2_direct_bsdf_pdf_transmission_roughness",
                glass_back_pdf > rough_glass_back_pdf);
    assert_close("runtime_disney_v2_direct_bsdf_pdf_transmission_disabled",
                 glass_disabled_pdf,
                 0.0,
                 1e-12);
    assert_close("runtime_disney_v2_direct_bsdf_pdf_transmission_tir",
                 glass_tir_pdf,
                 0.0,
                 1e-12);

    return 0;
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
    double light_term = 0.0;
    double bsdf_term = 0.0;
    double mis_total = 0.0;
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
    light_term = v2_result.lightSamplePdf * v2_result.lightSamplePdf;
    bsdf_term = v2_result.bsdfSamplePdf * v2_result.bsdfSamplePdf;
    mis_total = light_term + bsdf_term;
    assert_true("runtime_disney_v2_cached_mis_power_ready",
                v2_result.misHeuristicPower == 2.0 &&
                v2_result.misPowerHeuristicCount >= 1 &&
                mis_total > 0.0);
    assert_close("runtime_disney_v2_cached_mis_power_light",
                 v2_result.misWeightLight,
                 light_term / mis_total,
                 1e-9);
    assert_close("runtime_disney_v2_cached_mis_power_bsdf",
                 v2_result.misWeightBsdf,
                 bsdf_term / mis_total,
                 1e-9);
    assert_true("runtime_disney_v2_cached_finite_branch_only",
                v2_result.finiteLightMis.lightPdf > 0.0 &&
                v2_result.finiteLightMis.bsdfPdf > 0.0 &&
                v2_result.finiteLightMis.weightLight > 0.0 &&
                v2_result.finiteLightMisVertexCount >= 1 &&
                v2_result.emissiveAreaMis.lightPdf == 0.0 &&
                v2_result.emissiveAreaMis.bsdfPdf == 0.0 &&
                v2_result.emissiveAreaMisVertexCount == 0);
    assert_close("runtime_disney_v2_cached_finite_branch_light_pdf",
                 v2_result.finiteLightMis.lightPdf,
                 v2_result.lightSamplePdf,
                 1e-9);
    assert_close("runtime_disney_v2_cached_finite_branch_weight",
                 v2_result.finiteLightMis.weightLight,
                 v2_result.misWeightLight,
                 1e-9);
    assert_true("runtime_disney_v2_cached_stochastic_direct_positive",
                v2_result.stochasticDirectRadiance > 0.0);
    assert_close("runtime_disney_v2_cached_direct_uses_finite_branch_r",
                 v2_result.stochasticDirectRadianceR,
                 v2_result.directRadianceR * v2_result.finiteLightMis.weightLight,
                 1e-9);
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

static int test_runtime_disney_v2_3d_recursive_lobe_resamples_secondary_material(void) {
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
    RuntimeDisneyV2_3DResult red = {0};
    RuntimeDisneyV2_3DResult blue = {0};
    RuntimeDisneyV2_3DResult metal = {0};
    RuntimeDisneyV2_3DResult glass = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 3;
    animSettings.specularDepth3D = 3;
    animSettings.transmissionDepth3D = 3;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_recursive_scene(&scene);
    assert_true("runtime_disney_v2_recursive_resample_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_GLOSSY,
                                                    0xFFFFFF,
                                                    0.9,
                                                    0.02);

    ok = runtime_disney_v2_test_load_forced_diffuse_default_preset();
    assert_true("runtime_disney_v2_recursive_resample_load_diffuse", ok);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF0000,
                                                    0.0,
                                                    1.0);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &red);
    assert_true("runtime_disney_v2_recursive_resample_red_ok", ok);

    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x0000FF,
                                                    0.0,
                                                    1.0);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &blue);
    assert_true("runtime_disney_v2_recursive_resample_blue_ok", ok);

    MaterialManagerResetDefaults();
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_MIRROR,
                                                    0xE5E5E5,
                                                    0.95,
                                                    0.02);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &metal);
    assert_true("runtime_disney_v2_recursive_resample_metal_ok", ok);

    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_forced_transparent_preset();
    assert_true("runtime_disney_v2_recursive_resample_load_glass", ok);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xB8E6FF,
                                                    0.0,
                                                    0.02);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &glass);
    assert_true("runtime_disney_v2_recursive_resample_glass_ok", ok);

    assert_true("runtime_disney_v2_recursive_resample_primary_stays_specular",
                red.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR &&
                blue.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR &&
                metal.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR &&
                glass.sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR);
    assert_true("runtime_disney_v2_recursive_resample_all_record_vertex",
                red.recursiveLoopVertexCount >= 1 &&
                blue.recursiveLoopVertexCount >= 1 &&
                metal.recursiveLoopVertexCount >= 1 &&
                glass.recursiveLoopVertexCount >= 1);
    assert_true("runtime_disney_v2_recursive_resample_lobes_recomputed",
                red.recursiveLoopStates[0].sampledLobe ==
                    RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE &&
                blue.recursiveLoopStates[0].sampledLobe ==
                    RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE &&
                metal.recursiveLoopStates[0].sampledLobe ==
                    RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR &&
                glass.recursiveLoopStates[0].sampledLobe ==
                    RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION);
    assert_true("runtime_disney_v2_recursive_resample_material_color_local",
                red.recursiveLoopPrincipled[0].valid &&
                blue.recursiveLoopPrincipled[0].valid &&
                red.recursiveLoopPrincipled[0].baseColorR >
                    red.recursiveLoopPrincipled[0].baseColorB + 0.5 &&
                blue.recursiveLoopPrincipled[0].baseColorB >
                    blue.recursiveLoopPrincipled[0].baseColorR + 0.5);
    assert_true("runtime_disney_v2_recursive_resample_metal_glass_materials",
                metal.recursiveLoopPrincipled[0].valid &&
                metal.recursiveLoopPrincipled[0].specularWeight > 0.95 &&
                glass.recursiveLoopPrincipled[0].valid &&
                glass.recursiveLoopPrincipled[0].transmissionWeight > 0.95);
    assert_true("runtime_disney_v2_recursive_resample_pdf_and_throughput",
                red.recursiveLoopStates[0].pdf > 0.0 &&
                blue.recursiveLoopStates[0].pdf > 0.0 &&
                metal.recursiveLoopStates[0].pdf > 0.0 &&
                glass.recursiveLoopStates[0].pdf > 0.0 &&
                runtime_disney_v2_3d_test_peak(red.recursiveLoopStates[0].throughputR,
                                               red.recursiveLoopStates[0].throughputG,
                                               red.recursiveLoopStates[0].throughputB) > 0.0 &&
                runtime_disney_v2_3d_test_peak(glass.recursiveLoopStates[0].throughputR,
                                               glass.recursiveLoopStates[0].throughputG,
                                               glass.recursiveLoopStates[0].throughputB) > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_reflection_recurses_reflected_geometry(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 19U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_forced_diffuse_default_preset();
    assert_true("runtime_disney_v2_reflection_recursive_load_diffuse", ok);
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_MIRROR,
                                                    0xFFFFFF,
                                                    0.95,
                                                    0.02);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF0000,
                                                    0.0,
                                                    1.0);
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_reflection_recursive_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    hit = runtime_disney_v2_test_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &result);
    assert_true("runtime_disney_v2_reflection_recursive_ok", ok);
    assert_true("runtime_disney_v2_reflection_recursive_primary_ray",
                result.specularReflectionRayCount == 1);
    assert_true("runtime_disney_v2_reflection_recursive_geometry_outcome",
                result.specularReflectionGeometryHitCount == 1 &&
                result.specularReflectionEmitterHitCount == 0 &&
                result.specularReflectionNoHitCount == 0);
    assert_true("runtime_disney_v2_reflection_recursive_vertex",
                result.specularReflectionRecursiveVertexCount >= 1 &&
                result.specularReflectionRecursiveStates[0].valid &&
                result.specularReflectionRecursiveStates[0].depth == 2 &&
                result.specularReflectionRecursivePrincipled[0].valid);
    assert_true("runtime_disney_v2_reflection_recursive_local_material",
                result.specularReflectionRecursivePrincipled[0].baseColorR >
                    result.specularReflectionRecursivePrincipled[0].baseColorB + 0.5 &&
                result.specularReflectionRecursiveStates[0].sampledLobe ==
                    RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE);
    assert_true("runtime_disney_v2_reflection_recursive_emitter_contributes",
                result.specularReflectionRecursiveRayCount >= 1 &&
                result.specularReflectionRecursiveEmitterHitCount >= 1 &&
                result.specularReflectionRecursiveContributingHitCount >= 1 &&
                result.specularReflectionRecursiveRadianceR > 0.0);
    assert_true("runtime_disney_v2_reflection_recursive_final_includes_delta",
                result.recursiveBsdfRadianceR >=
                    result.specularReflectionRecursiveRadianceR - 1e-9 &&
                result.radianceR >= result.specularReflectionRecursiveRadianceR);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_reflection_continues_transparent_geometry(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 23U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult continued = {0};
    RuntimeDisneyV2_3DResult blocked = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_forced_transparent_preset();
    assert_true("runtime_disney_v2_reflection_transparent_load", ok);
    animSettings.bounceDepth3D = 3;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 1;
    animSettings.transmissionSamples3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_MIRROR,
                                                    0xFFFFFF,
                                                    0.95,
                                                    0.02);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xC8F0FF,
                                                    0.0,
                                                    0.02);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x20FF40,
                                                    0.05,
                                                    0.55);
    runtime_disney_v2_test_init_reflected_transparency_scene(&scene);
    assert_true("runtime_disney_v2_reflection_transparent_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    hit = runtime_disney_v2_test_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &continued);
    assert_true("runtime_disney_v2_reflection_transparent_ok", ok);

    animSettings.transmissionDepth3D = 0;
    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &blocked);
    assert_true("runtime_disney_v2_reflection_transparent_blocked_ok", ok);

    assert_true("runtime_disney_v2_reflection_transparent_first_hit",
                continued.specularReflectionGeometryHitCount == 1 &&
                continued.specularReflectionRecursivePolicyTerminationCount >= 1);
    assert_true("runtime_disney_v2_reflection_transparent_continues",
                continued.specularReflectionRecursiveRadianceG > 0.0 &&
                continued.specularReflectionRecursiveRadianceG >
                    continued.specularReflectionRecursiveRadianceR + 1e-6 &&
                continued.specularReflectionRecursiveRadianceG >
                    blocked.specularReflectionRecursiveRadianceG + 1e-6);
    assert_true("runtime_disney_v2_reflection_transparent_final_radiance",
                continued.radianceG >= continued.specularReflectionRecursiveRadianceG &&
                continued.radianceG > blocked.radianceG + 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_mirror_dominance_reflects_light_emitter(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 23U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_MIRROR,
                                                    0xFFFFFF,
                                                    0.95,
                                                    0.02);
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, 2.0, 0.0);
    scene.light.radius = 0.45;
    scene.light.intensity = 36.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_disney_v2_mirror_dominance_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].twoSided = true;
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_disney_v2_mirror_dominance_bvh", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }

    hit = runtime_disney_v2_test_hit(&scene);
    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &result);
    assert_true("runtime_disney_v2_mirror_dominance_ok", ok);
    assert_true("runtime_disney_v2_mirror_dominance_factor",
                result.mirrorDominance > 0.90 &&
                result.mirrorBaseAttenuation < 0.10);
    assert_true("runtime_disney_v2_mirror_dominance_attenuates_base",
                result.mirrorBaseRadianceBeforeAttenuation > 0.0 &&
                result.mirrorBaseRadianceAfterAttenuation <
                    result.mirrorBaseRadianceBeforeAttenuation * 0.12);
    assert_true("runtime_disney_v2_mirror_dominance_reflects_emitter",
                result.specularReflectionRayCount == 1 &&
                result.specularReflectionEmitterHitCount == 1 &&
                result.specularReflectionGeometryHitCount == 0 &&
                result.specularReflectionContributingHitCount == 1 &&
                result.specularReflectionRadiance >
                    result.mirrorBaseRadianceAfterAttenuation);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_rough_reflection_records_stochastic_sample(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D rough_glossy =
        runtime_disney_v2_test_payload(0.90, 0.90, 0.90, 0.85, 0.45, 0.0, 1.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 23U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_forced_diffuse_default_preset();
    assert_true("runtime_disney_v2_rough_reflection_load_diffuse", ok);
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF0000,
                                                    0.0,
                                                    1.0);
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_rough_reflection_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &rough_glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_rough_reflection_ok", ok);
    assert_true("runtime_disney_v2_rough_reflection_primary_geometry",
                result.specularReflectionRayCount == 1 &&
                result.specularReflectionGeometryHitCount == 1);
    assert_true("runtime_disney_v2_rough_reflection_sample_diagnostics",
                result.specularReflectionRoughSampleCount == 4 &&
                result.specularReflectionRoughness > 0.40 &&
                result.specularReflectionRoughHitCount > 0 &&
                result.specularReflectionRoughHitCount +
                    result.specularReflectionRoughNoHitCount ==
                    result.specularReflectionRoughSampleCount);
    assert_true("runtime_disney_v2_rough_reflection_recursive_contributes",
                result.specularReflectionRecursiveVertexCount >= 1 &&
                result.specularReflectionRecursiveRayCount >= 1 &&
                result.specularReflectionRoughContributingSampleCount > 0 &&
                result.specularReflectionRoughContribution > 0.0 &&
                result.specularReflectionRecursiveRadianceR > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_reflection_recursion_respects_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 29U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult limited = {0};
    RuntimeDisneyV2_3DResult roulette = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_forced_diffuse_default_preset();
    assert_true("runtime_disney_v2_reflection_policy_load_diffuse", ok);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_MIRROR,
                                                    0xFFFFFF,
                                                    0.95,
                                                    0.02);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF0000,
                                                    0.0,
                                                    1.0);
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_reflection_policy_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    hit = runtime_disney_v2_test_hit(&scene);

    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &limited);
    assert_true("runtime_disney_v2_reflection_policy_limited_ok", ok);
    assert_true("runtime_disney_v2_reflection_policy_limited_primary",
                limited.specularReflectionGeometryHitCount == 1);
    assert_true("runtime_disney_v2_reflection_policy_limited_stops",
                limited.specularReflectionRecursivePolicyTerminationCount == 1 &&
                limited.specularReflectionRecursiveRayCount == 0 &&
                limited.specularReflectionRecursiveVertexCount == 0 &&
                limited.specularReflectionRecursiveRadiance == 0.0);

    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x000001,
                                                    0.0,
                                                    1.0);
    animSettings.bounceDepth3D = 2;
    animSettings.specularDepth3D = 2;
    animSettings.transmissionDepth3D = 2;
    animSettings.rouletteThreshold3D = 0.1;
    ok = RuntimeDisneyV2_3D_ShadeHit(&scene, &hit, &sampling, &roulette);
    assert_true("runtime_disney_v2_reflection_policy_roulette_ok", ok);
    assert_true("runtime_disney_v2_reflection_policy_roulette_primary",
                roulette.specularReflectionGeometryHitCount == 1);
    assert_true("runtime_disney_v2_reflection_policy_roulette_stops",
                roulette.specularReflectionRecursiveRouletteTerminationCount == 1 &&
                roulette.specularReflectionRecursiveRayCount == 0 &&
                roulette.specularReflectionRecursiveVertexCount == 0 &&
                roulette.specularReflectionRecursiveRadiance == 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
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
        .sampleSequence = 31U,
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
                blocked.primaryTransmissionRadiance == 0.0);
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

static int test_runtime_disney_v2_3d_scene_object_glass_override_continues_camera_ray(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D principled = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 41U,
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
    animSettings.transmissionDepth3D = 1;
    animSettings.transmissionSamples3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_primary_transparency_scene(&scene);
    assert_true("runtime_disney_v2_object_glass_scene_alloc",
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
                                                    0xBEEBFF,
                                                    0.10,
                                                    0.004);
    sceneSettings.sceneObjects[0].alpha = 0.24;
    sceneSettings.sceneObjects[0].hasGlassTransportOverride = true;
    sceneSettings.sceneObjects[0].glassTransmission = 0.96;
    sceneSettings.sceneObjects[0].glassIor = 1.45;
    sceneSettings.sceneObjects[0].glassAbsorptionDistance = 6.0;
    sceneSettings.sceneObjects[0].glassThinWalled = true;
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);

    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload);
    assert_true("runtime_disney_v2_object_glass_payload_resolved", ok && payload.valid);
    assert_true("runtime_disney_v2_object_glass_payload_override",
                payload.hasGlassTransportOverride &&
                payload.transparency > 0.95 &&
                payload.thinWalled &&
                payload.absorptionDistance > 5.5);
    principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    assert_true("runtime_disney_v2_object_glass_principled_transmits",
                principled.valid &&
                principled.transmissionWeight > 0.95 &&
                principled.opacity < 0.05);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &payload,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_object_glass_shade_ok", ok);
    assert_true("runtime_disney_v2_object_glass_continues",
                result.primaryTransmissionContinued &&
                result.primaryTransmissionPathState.valid &&
                result.primaryTransmissionPathState.hit &&
                result.primaryTransmissionPathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_object_glass_receiver_contributes",
                result.primaryTransmissionReceiverSampleCount > 0 &&
                result.primaryTransmissionReceiverRadiance > 0.0 &&
                result.primaryTransmissionRadianceR >
                    result.primaryTransmissionRadianceB + 1e-6);

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
                depth_limited.primaryTransmissionContinued &&
                depth_limited.primaryTransmissionDepthLimitCount > 0 &&
                depth_limited.primaryTransmissionReceiverSampleCount == 0 &&
                depth_limited.primaryTransmissionTransparentLayerRadiance > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_primary_transparency_accumulates_transparent_layers(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.70, 0.92, 1.0, 0.0, 0.08, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 29U,
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
    animSettings.transmissionDepth3D = 1;
    animSettings.transmissionSamples3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_nested_transparency_scene(&scene);
    assert_true("runtime_disney_v2_transparent_layers_scene_alloc",
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
                                                    0xC8F0FF,
                                                    0.0,
                                                    0.08);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0x30FF40,
                                                    0.05,
                                                    0.55);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_transparent_layers_ok", ok);
    assert_true("runtime_disney_v2_transparent_layers_continue_without_opaque_receiver",
                result.primaryTransmissionContinued &&
                result.primaryTransmissionPathState.valid &&
                result.primaryTransmissionPathState.hit &&
                result.primaryTransmissionPathState.hitInfo.sceneObjectIndex == 1 &&
                result.primaryTransmissionDepthLimitCount > 0);
    assert_true("runtime_disney_v2_transparent_layers_shaded",
                result.primaryTransmissionTransparentSurfaceCount > 0 &&
                result.primaryTransmissionTransparentLayerShadeCount > 0 &&
                result.primaryTransmissionReceiverSampleCount == 0 &&
                result.primaryTransmissionReceiverShadeCount == 0);
    assert_true("runtime_disney_v2_transparent_layers_green_contributes",
                result.primaryTransmissionTransparentLayerRadiance > 0.0 &&
                result.primaryTransmissionTransparentLayerRadianceG >
                    result.primaryTransmissionTransparentLayerRadianceR + 1e-6 &&
                result.primaryTransmissionRadianceG >
                    result.primaryTransmissionRadianceR + 1e-6 &&
                result.primaryTransmissionContributingSampleCount > 0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_disney_v2_3d_transparency_policy_splits_thin_walled_and_solid(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.92, 0.92, 0.92, 0.0, 0.08, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 31U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult solid = {0};
    RuntimeDisneyV2_3DResult thin = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 3;
    animSettings.specularDepth3D = 3;
    animSettings.transmissionDepth3D = 1;
    animSettings.transmissionSamples3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_nested_transparency_scene(&scene);
    assert_true("runtime_disney_v2_transparency_policy_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = runtime_disney_v2_test_load_transparent_preset(false, 0.25);
    assert_true("runtime_disney_v2_transparency_policy_load_solid", ok);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xEAEAEA,
                                                    0.0,
                                                    0.08);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0x808080,
                                                    0.05,
                                                    0.55);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &solid);
    assert_true("runtime_disney_v2_transparency_policy_solid_ok", ok);

    MaterialManagerResetDefaults();
    ok = runtime_disney_v2_test_load_transparent_preset(true, 0.25);
    assert_true("runtime_disney_v2_transparency_policy_load_thin", ok);
    glass.thinWalled = true;
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xEAEAEA,
                                                    0.0,
                                                    0.08);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0x808080,
                                                    0.05,
                                                    0.55);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &thin);
    assert_true("runtime_disney_v2_transparency_policy_thin_ok", ok);

    assert_true("runtime_disney_v2_transparency_policy_classifies_surfaces",
                solid.primaryTransmissionSolidSurfaceCount > 0 &&
                solid.primaryTransmissionThinWalledSurfaceCount == 0 &&
                thin.primaryTransmissionAlphaOnlySurfaceCount > 0 &&
                thin.primaryTransmissionThinWalledSurfaceCount == 0 &&
                thin.primaryTransmissionSolidSurfaceCount == 0);
    assert_true("runtime_disney_v2_transparency_policy_preserves_thin_throughput",
                thin.primaryTransmissionCameraThroughputR >
                    solid.primaryTransmissionCameraThroughputR + 1e-6 &&
                thin.primaryTransmissionCameraThroughputG >
                    solid.primaryTransmissionCameraThroughputG + 1e-6 &&
                thin.primaryTransmissionCameraThroughputB >
                    solid.primaryTransmissionCameraThroughputB + 1e-6);
    assert_true("runtime_disney_v2_transparency_policy_both_layers_visible",
                thin.primaryTransmissionTransparentLayerShadeCount > 0 &&
                solid.primaryTransmissionPhysicalSurfaceCount > 0 &&
                solid.primaryTransmissionReceiverSampleCount == 0 &&
                thin.primaryTransmissionReceiverSampleCount == 0);
    assert_true("runtime_disney_v2_transparency_policy_solid_interior_return",
                solid.primaryTransmissionInteriorReturnSampleCount == 0 &&
                solid.primaryTransmissionInteriorReturnSurfaceCount == 0 &&
                solid.primaryTransmissionPhysicalSurfaceCount > 0 &&
                solid.primaryTransmissionMaxMediumStackDepth > 0);
    assert_true("runtime_disney_v2_transparency_policy_thin_has_no_interior_return",
                thin.primaryTransmissionInteriorReturnSampleCount == 0 &&
                thin.primaryTransmissionInteriorReturnSurfaceCount == 0 &&
                thin.primaryTransmissionInteriorReturnRadiance == 0.0 &&
                thin.primaryTransmissionMaxMediumStackDepth == 0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_transparency_policy_classifies_alpha_only(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D glass =
        runtime_disney_v2_test_payload(0.92, 0.92, 0.92, 0.0, 0.08, 0.0, 0.01, 1.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 37U,
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
    animSettings.transmissionDepth3D = 1;
    animSettings.transmissionSamples3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_nested_transparency_scene(&scene);
    assert_true("runtime_disney_v2_alpha_policy_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = runtime_disney_v2_test_load_alpha_only_default_preset(0.65);
    assert_true("runtime_disney_v2_alpha_policy_load_default", ok);
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_TRANSPARENT,
                                                    0xEAEAEA,
                                                    0.0,
                                                    0.08);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0x40F0FF,
                                                    0.05,
                                                    0.70);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xFF3020,
                                                    0.05,
                                                    0.55);
    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glass,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_alpha_policy_ok", ok);
    assert_true("runtime_disney_v2_alpha_policy_classified",
                result.primaryTransmissionAlphaOnlySurfaceCount > 0 &&
                result.primaryTransmissionPhysicalSurfaceCount <= 1 &&
                result.primaryTransmissionSolidSurfaceCount <= 1 &&
                result.primaryTransmissionThinWalledSurfaceCount == 0);
    assert_true("runtime_disney_v2_alpha_policy_no_solid_return",
                result.primaryTransmissionTransparentLayerShadeCount > 0 &&
                result.primaryTransmissionInteriorReturnSampleCount == 0 &&
                result.primaryTransmissionInteriorReturnRadiance == 0.0);
    assert_true("runtime_disney_v2_alpha_policy_preserves_primary_medium_only",
                result.primaryTransmissionMediumEntryCount == 1 &&
                result.primaryTransmissionMediumExitCount == 0 &&
                result.primaryTransmissionMaxMediumStackDepth == 1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
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
                result.lightSampleContributionCount >= 1 &&
                result.bsdfSampleContributionCount >= 1);
    assert_true("runtime_disney_v2_mis_accounting_primary_and_secondary_light",
                result.lightSampleContributionR[1] > 0.0 &&
                result.stochasticDirectRadianceR == result.lightSampleContributionR[0]);
    assert_true("runtime_disney_v2_mis_accounting_finite_light_emitter",
                result.bsdfSampleContributionR[1] > 0.0 &&
                result.finiteLightEmitterHitCount >= 1 &&
                result.emissiveMaterialHitCount == 0 &&
                result.misVertexEmitterKind[1] ==
                    RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT);
    assert_true("runtime_disney_v2_mis_accounting_vertex_count",
                result.misVertexCount >= 2);
    assert_true("runtime_disney_v2_mis_accounting_finite_light_pdf",
                result.misVertexLightPdf[0] > 1.0);
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

static int test_runtime_disney_v2_3d_emissive_material_surface_hit_contributes(void) {
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
    runtime_disney_v2_test_init_one_bounce_scene(&scene);
    assert_true("runtime_disney_v2_emissive_surface_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    scene.light.radius = 0.0;
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_EMISSIVE,
                                                    0xFFAA44,
                                                    0.0,
                                                    1.0);
    sceneSettings.sceneObjects[1].emissiveStrength = 1.0;
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_emissive_surface_ok", ok);
    assert_true("runtime_disney_v2_emissive_surface_hit",
                result.pathState.valid &&
                result.pathState.hit &&
                result.pathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_emissive_surface_endpoint",
                result.pathState.emitterHit &&
                result.pathState.emitterWins &&
                result.pathState.emitterHitInfo.radiance > 0.0);
    assert_true("runtime_disney_v2_emissive_surface_accounting",
                result.emissiveMaterialHitCount == 1 &&
                result.finiteLightEmitterHitCount == 0 &&
                result.misVertexEmitterKind[1] ==
                    RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL);
    assert_true("runtime_disney_v2_emissive_surface_contribution",
                result.bsdfSampleContributionR[1] > 0.0 &&
                result.stochasticBsdfRadianceR > 0.0 &&
                result.bsdfSampleContributionR[1] > result.bsdfSampleContributionB[1]);
    assert_true("runtime_disney_v2_emissive_surface_terminates_before_recursion",
                result.recursiveLoopVertexCount == 0 &&
                result.secondaryContributingHitCount == 1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_emissive_area_sample_lights_primary_vertex(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D matte =
        runtime_disney_v2_test_payload(0.64, 0.62, 0.58, 0.02, 0.70, 1.0, 0.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 41U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeEmissiveDirect3DResult area_pdf_probe = {0};
    RuntimeDisneyV2_3DResult result = {0};
    double expected_area_light_pdf = 0.0;
    bool ok = false;
    bool area_ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_emissive_area_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    scene.hasLight = false;
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_EMISSIVE,
                                                    0x3366FF,
                                                    0.0,
                                                    1.0);
    sceneSettings.sceneObjects[1].emissiveStrength = 1.0;
    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_disney_v2_emissive_area_light_set_cached",
                scene.emissiveLightSet.valid &&
                scene.emissiveLightSet.candidateCount == 1 &&
                scene.capabilities.emissiveLightCandidateCount == 1 &&
                !scene.capabilities.canSkipEmissionSupport);
    primary = runtime_disney_v2_test_primary_hit(&scene);
    area_ok = RuntimeEmissiveDirect3D_ShadeHit(&scene,
                                               &primary.hitInfo,
                                               &sampling,
                                               &area_pdf_probe);
    expected_area_light_pdf = area_pdf_probe.candidateSelectionPdf *
                              area_pdf_probe.areaPdf *
                              area_pdf_probe.sampleDistance *
                              area_pdf_probe.sampleDistance /
                              fmax(area_pdf_probe.sampleEmitterCos, 1e-12);
    assert_true("runtime_disney_v2_emissive_area_pdf_probe_ok", area_ok);
    assert_true("runtime_disney_v2_emissive_area_pdf_probe_diagnostics",
                area_pdf_probe.candidateSelectionPdf > 0.0 &&
                area_pdf_probe.areaPdf > 0.0 &&
                area_pdf_probe.lightPdf > 1.0 &&
                area_pdf_probe.sampleDistance > 0.0 &&
                area_pdf_probe.sampleEmitterCos > 0.0 &&
                vec3_length(area_pdf_probe.sampleDirection) > 0.99);
    assert_close("runtime_disney_v2_emissive_area_pdf_solid_angle",
                 area_pdf_probe.lightPdf,
                 expected_area_light_pdf,
                 1e-9);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &matte,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_emissive_area_ok", ok);
    assert_true("runtime_disney_v2_emissive_area_no_finite_light",
                result.directRadiance == 0.0 &&
                result.finiteLightEmitterHitCount == 0);
    assert_true("runtime_disney_v2_emissive_area_sampled",
                result.emissiveAreaLightSampleCount == 1 &&
                result.emissiveAreaSampledTriangleCount == 1 &&
                result.emissiveAreaContributingTriangleCount == 1);
    assert_true("runtime_disney_v2_emissive_area_bounded_diagnostics",
                result.emissiveAreaCandidateCount == 1 &&
                result.emissiveAreaSelectedCandidateCount == 1 &&
                result.emissiveAreaVisibilityRayCount == 1 &&
                result.emissiveAreaPrimarySampleCount == 1 &&
                result.emissiveAreaRecursiveSampleCount == 0 &&
                result.emissiveAreaRecursivePolicySkipCount == 0 &&
                result.emissiveAreaFullScanFallbackCount == 0);
    assert_true("runtime_disney_v2_emissive_area_mis_light_branch",
                result.lightSamplePdf > 0.0 &&
                result.misWeightLight > 0.0 &&
                result.lightSampleContributionCount >= 1);
    assert_true("runtime_disney_v2_emissive_area_branch_only",
                result.finiteLightMis.lightPdf == 0.0 &&
                result.finiteLightMis.bsdfPdf == 0.0 &&
                result.finiteLightMisVertexCount == 0 &&
                result.emissiveAreaMis.lightPdf > 0.0 &&
                result.emissiveAreaMis.bsdfPdf > 0.0 &&
                result.emissiveAreaMis.weightLight > 0.0 &&
                result.emissiveAreaMisVertexCount >= 1);
    assert_close("runtime_disney_v2_emissive_area_mis_uses_area_pdf",
                 result.lightSamplePdf,
                 area_pdf_probe.lightPdf,
                 1e-9);
    assert_close("runtime_disney_v2_emissive_area_branch_uses_area_pdf",
                 result.emissiveAreaMis.lightPdf,
                 area_pdf_probe.lightPdf,
                 1e-9);
    assert_close("runtime_disney_v2_emissive_area_vertex_branch_pdf",
                 result.misVertexEmissiveArea[0].lightPdf,
                 area_pdf_probe.lightPdf,
                 1e-9);
    assert_true("runtime_disney_v2_emissive_area_bsdf_pdf_from_area_direction",
                result.bsdfSamplePdf > 0.0 &&
                result.misVertexBsdfPdf[0] > 0.0);
    assert_true("runtime_disney_v2_emissive_area_blue_contribution",
                result.emissiveAreaRadianceB > 0.0 &&
                result.emissiveAreaRadianceB >
                    result.emissiveAreaRadianceR + 1e-6 &&
                result.stochasticDirectRadianceB >=
                    result.emissiveAreaRadianceB - 1e-9);
    assert_close("runtime_disney_v2_emissive_area_light_sample_total_b",
                 result.lightSampleContributionB[0],
                 result.emissiveAreaRadianceB,
                 1e-9);
    assert_true("runtime_disney_v2_emissive_area_not_endpoint_hit",
                result.emissiveMaterialHitCount == 0 &&
                result.misVertexEmitterKind[0] !=
                    RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL);
    assert_true("runtime_disney_v2_emissive_area_affects_final",
                result.radianceB >= result.emissiveAreaRadianceB &&
                result.radianceWithoutLightSamplesB <
                    result.radianceB - 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_mixed_finite_and_emissive_area_branches(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimePrimaryHit3DResult primary = {0};
    RuntimeMaterialPayload3D matte =
        runtime_disney_v2_test_payload(0.64, 0.62, 0.58, 0.02, 0.70, 1.0, 0.0, 0.0, 0.0);
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 41U,
        .temporalSubpassIndex = 0U,
        .temporalSubpassCount = 1U,
    };
    RuntimeDisneyV2_3DResult result = {0};
    double finite_light_term = 0.0;
    double finite_bsdf_term = 0.0;
    double finite_total = 0.0;
    double area_light_term = 0.0;
    double area_bsdf_term = 0.0;
    double area_total = 0.0;
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_mixed_area_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_EMISSIVE,
                                                    0x3366FF,
                                                    0.0,
                                                    1.0);
    sceneSettings.sceneObjects[1].emissiveStrength = 1.0;
    RuntimeScene3D_RefreshCapabilities(&scene);
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &matte,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_mixed_area_ok", ok);
    assert_true("runtime_disney_v2_mixed_area_both_branches",
                result.directRadiance > 0.0 &&
                result.emissiveAreaRadianceB > 0.0 &&
                result.finiteLightMis.lightPdf > 0.0 &&
                result.finiteLightMis.bsdfPdf > 0.0 &&
                result.emissiveAreaMis.lightPdf > 0.0 &&
                result.emissiveAreaMis.bsdfPdf > 0.0 &&
                result.finiteLightMisVertexCount >= 1 &&
                result.emissiveAreaMisVertexCount >= 1);

    finite_light_term = result.finiteLightMis.lightPdf * result.finiteLightMis.lightPdf;
    finite_bsdf_term = result.finiteLightMis.bsdfPdf * result.finiteLightMis.bsdfPdf;
    finite_total = finite_light_term + finite_bsdf_term;
    area_light_term = result.emissiveAreaMis.lightPdf * result.emissiveAreaMis.lightPdf;
    area_bsdf_term = result.emissiveAreaMis.bsdfPdf * result.emissiveAreaMis.bsdfPdf;
    area_total = area_light_term + area_bsdf_term;
    assert_true("runtime_disney_v2_mixed_area_branch_totals",
                finite_total > 0.0 && area_total > 0.0);
    assert_close("runtime_disney_v2_mixed_finite_branch_light_weight",
                 result.finiteLightMis.weightLight,
                 finite_light_term / finite_total,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_finite_branch_bsdf_weight",
                 result.finiteLightMis.weightBsdf,
                 finite_bsdf_term / finite_total,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_area_branch_light_weight",
                 result.emissiveAreaMis.weightLight,
                 area_light_term / area_total,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_area_branch_bsdf_weight",
                 result.emissiveAreaMis.weightBsdf,
                 area_bsdf_term / area_total,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_vertex_finite_pdf",
                 result.misVertexFiniteLight[0].lightPdf,
                 result.finiteLightMis.lightPdf,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_vertex_area_pdf",
                 result.misVertexEmissiveArea[0].lightPdf,
                 result.emissiveAreaMis.lightPdf,
                 1e-9);
    assert_close("runtime_disney_v2_mixed_light_contribution_sum_b",
                 result.lightSampleContributionB[0],
                 result.directRadianceB * result.finiteLightMis.weightLight +
                     result.emissiveAreaRadianceB,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_recursive_emissive_area_sample_lights_vertex(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D start_hit = {0};
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 41U,
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
    runtime_disney_v2_test_init_reflection_scene(&scene);
    assert_true("runtime_disney_v2_recursive_emissive_area_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    scene.hasLight = false;
    runtime_disney_v2_test_configure_scene_material(0,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xD8D8D8,
                                                    0.0,
                                                    1.0);
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_EMISSIVE,
                                                    0x3366FF,
                                                    0.0,
                                                    1.0);
    sceneSettings.sceneObjects[1].emissiveStrength = 1.0;
    RuntimeScene3D_RefreshCapabilities(&scene);
    assert_true("runtime_disney_v2_recursive_emissive_area_light_set_cached",
                scene.emissiveLightSet.valid &&
                scene.emissiveLightSet.candidateCount == 1 &&
                scene.capabilities.emissiveLightCandidateCount == 1 &&
                !scene.capabilities.canSkipEmissionSupport);
    start_hit = runtime_disney_v2_test_hit(&scene);
    result.pathPolicy = RuntimePathDepthPolicy3D_Resolve();
    result.pathPolicyResolved = true;

    ok = RuntimeDisneyV2_3D_ApplyRecursivePathLoopFromDirection(&scene,
                                                                &start_hit,
                                                                &sampling,
                                                                vec3(0.0, -1.0, 0.0),
                                                                2,
                                                                0.55,
                                                                0.50,
                                                                0.45,
                                                                &result);
    assert_true("runtime_disney_v2_recursive_emissive_area_ok", ok);
    assert_true("runtime_disney_v2_recursive_emissive_area_no_finite_light",
                result.directRadiance == 0.0 &&
                result.stochasticDirectRadianceR == 0.0 &&
                result.finiteLightEmitterHitCount == 0);
    assert_true("runtime_disney_v2_recursive_emissive_area_sampled",
                result.emissiveAreaLightSampleCount == 1 &&
                result.emissiveAreaSampledTriangleCount == 1 &&
                result.emissiveAreaContributingTriangleCount == 1);
    assert_true("runtime_disney_v2_recursive_emissive_area_bounded_diagnostics",
                result.emissiveAreaCandidateCount == 1 &&
                result.emissiveAreaSelectedCandidateCount == 1 &&
                result.emissiveAreaVisibilityRayCount == 1 &&
                result.emissiveAreaPrimarySampleCount == 0 &&
                result.emissiveAreaRecursiveSampleCount == 1 &&
                result.emissiveAreaRecursivePolicySkipCount == 0 &&
                result.emissiveAreaFullScanFallbackCount == 0);
    assert_true("runtime_disney_v2_recursive_emissive_area_branch",
                result.lightSamplePdf > 0.0 &&
                result.bsdfSamplePdf > 0.0 &&
                result.misWeightLight > 0.0 &&
                result.lightSampleContributionCount == 1 &&
                result.lightSampleContributionB[1] > 0.0);
    assert_true("runtime_disney_v2_recursive_emissive_area_branch_split",
                result.finiteLightMis.lightPdf == 0.0 &&
                result.finiteLightMis.bsdfPdf == 0.0 &&
                result.finiteLightMisVertexCount == 0 &&
                result.emissiveAreaMis.lightPdf > 0.0 &&
                result.emissiveAreaMis.bsdfPdf > 0.0 &&
                result.emissiveAreaMis.weightLight > 0.0 &&
                result.emissiveAreaMisVertexCount >= 2 &&
                result.misVertexEmissiveArea[1].lightPdf > 0.0 &&
                result.misVertexFiniteLight[1].lightPdf == 0.0);
    assert_true("runtime_disney_v2_recursive_emissive_area_blue",
                result.emissiveAreaRadianceB > 0.0 &&
                result.emissiveAreaRadianceB >
                    result.emissiveAreaRadianceR + 1e-6 &&
                result.recursiveBsdfRadianceB >=
                    result.emissiveAreaRadianceB - 1e-9);
    assert_close("runtime_disney_v2_recursive_emissive_area_light_total_b",
                 result.lightSampleContributionB[1],
                 result.emissiveAreaRadianceB,
                 1e-9);
    assert_true("runtime_disney_v2_recursive_emissive_area_not_endpoint",
                result.emissiveMaterialHitCount == 0 &&
                result.misVertexEmitterKind[1] !=
                    RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL);
    assert_true("runtime_disney_v2_recursive_emissive_area_mis_vertex",
                result.misVertexCount >= 2 &&
                result.misVertexLightPdf[1] > 0.0 &&
                result.misVertexBsdfPdf[1] > 0.0 &&
                result.misVertexWeightLight[1] > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_disney_v2_3d_recursive_emissive_area_policy_caps(void) {
    RuntimeScene3D scene;
    RuntimeDisneyV2_3DResult result = {0};
    bool allow_primary = false;
    bool allow_recursive = false;

    RuntimeScene3D_Init(&scene);
    scene.emissiveLightSet.valid = true;
    scene.emissiveLightSet.candidateCount = 17;
    scene.triangleMesh.triangleCount = 128;

    allow_primary = RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(&scene,
                                                                             false,
                                                                             &result);
    allow_recursive = RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(&scene,
                                                                               true,
                                                                               &result);
    assert_true("runtime_disney_v2_emissive_area_policy_primary_allowed",
                allow_primary);
    assert_true("runtime_disney_v2_emissive_area_policy_candidate_cap",
                !allow_recursive &&
                result.emissiveAreaCandidateCount == 17 &&
                result.emissiveAreaRecursiveCandidateCap == 16 &&
                result.emissiveAreaRecursiveTriangleCap == 8192 &&
                result.emissiveAreaRecursivePolicySkipCount == 1 &&
                result.emissiveAreaRecursiveCandidateCapSkipCount == 1 &&
                result.emissiveAreaRecursiveTriangleCapSkipCount == 0);

    memset(&result, 0, sizeof(result));
    scene.emissiveLightSet.candidateCount = 1;
    scene.triangleMesh.triangleCount = 8193;
    allow_recursive = RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(&scene,
                                                                               true,
                                                                               &result);
    assert_true("runtime_disney_v2_emissive_area_policy_triangle_cap",
                !allow_recursive &&
                result.emissiveAreaCandidateCount == 1 &&
                result.emissiveAreaRecursivePolicySkipCount == 1 &&
                result.emissiveAreaRecursiveCandidateCapSkipCount == 0 &&
                result.emissiveAreaRecursiveTriangleCapSkipCount == 1);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_disney_v2_3d_recursive_emissive_material_surface_terminates(void) {
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
    assert_true("runtime_disney_v2_recursive_emissive_scene_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }
    scene.light.radius = 0.0;
    runtime_disney_v2_test_configure_scene_material(1,
                                                    MATERIAL_PRESET_DEFAULT,
                                                    0xD8D8D8,
                                                    0.05,
                                                    0.60);
    runtime_disney_v2_test_configure_scene_material(2,
                                                    MATERIAL_PRESET_EMISSIVE,
                                                    0x44AAFF,
                                                    0.0,
                                                    1.0);
    sceneSettings.sceneObjects[2].emissiveStrength = 1.0;
    primary = runtime_disney_v2_test_primary_hit(&scene);

    ok = RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,
                                                       &primary,
                                                       &glossy,
                                                       &sampling,
                                                       &result);
    assert_true("runtime_disney_v2_recursive_emissive_ok", ok);
    assert_true("runtime_disney_v2_recursive_emissive_first_hit",
                result.pathState.valid &&
                result.pathState.hit &&
                result.pathState.hitInfo.sceneObjectIndex == 1);
    assert_true("runtime_disney_v2_recursive_emissive_endpoint",
                result.recursiveLoopVertexCount == 1 &&
                result.recursiveLoopStates[0].hit &&
                result.recursiveLoopStates[0].hitInfo.sceneObjectIndex == 2 &&
                result.recursiveLoopStates[0].emitterHit &&
                result.recursiveLoopStates[0].emitterWins &&
                result.recursiveLoopStates[0].emitterHitInfo.radiance > 0.0);
    assert_true("runtime_disney_v2_recursive_emissive_accounting",
                result.emissiveMaterialHitCount == 1 &&
                result.finiteLightEmitterHitCount == 0 &&
                result.recursiveLoopEmitterHitCount == 1 &&
                result.misVertexEmitterKind[1] ==
                    RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL);
    assert_true("runtime_disney_v2_recursive_emissive_contribution",
                result.recursiveLoopContributionB[0] > 0.0 &&
                result.recursiveBsdfRadianceB > 0.0 &&
                result.recursiveLoopContributionB[0] > result.recursiveLoopContributionR[0]);
    assert_true("runtime_disney_v2_recursive_emissive_terminates",
                result.recursiveLoopContributingHitCount == 1 &&
                result.recursiveLoopTerminationReason ==
                    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
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
    test_runtime_dielectric_transport_water_ior_fresnel_contract();
    test_runtime_dielectric_transport_explicit_unit_ior_straight_through_contract();
    test_runtime_disney_v2_transmission_sample_uses_payload_ior_contract();
    test_runtime_disney_v2_reflected_transmission_sample_cap_policy();
    test_runtime_material_response_3d_seed_branch_contract();
    test_runtime_material_response_3d_mirror_reflects_opaque_chroma();
    test_runtime_material_response_3d_mirror_surface_kind_parity();
    test_runtime_material_response_3d_mirror_dominance_reflects_light_emitter();
    test_runtime_specular_reflection_reaches_far_geometry();
    test_runtime_disney_3d_illuminated_mirror_preserves_reflected_geometry();
    test_runtime_disney_3d_lower_tier_separation_contract();
    test_runtime_disney_3d_opaque_receiver_preserves_transport_support();
    test_runtime_disney_v2_3d_direct_light_pdf_estimator();
    test_runtime_disney_v2_3d_consumes_cached_principled_payload();
    test_runtime_disney_v2_3d_material_diagnostics_order_lobes();
    test_runtime_disney_v2_3d_sampling_context_moves_bsdf_path_state();
    test_runtime_disney_v2_3d_one_bounce_geometry_contributes();
    test_runtime_disney_v2_3d_secondary_material_vertex_modulates_contribution();
    test_runtime_disney_v2_3d_recursive_lobe_resamples_secondary_material();
    test_runtime_disney_v2_3d_reflection_recurses_reflected_geometry();
    test_runtime_disney_v2_3d_reflection_continues_transparent_geometry();
    test_runtime_disney_v2_3d_mirror_dominance_reflects_light_emitter();
    test_runtime_disney_v2_3d_rough_reflection_records_stochastic_sample();
    test_runtime_disney_v2_3d_reflection_recursion_respects_policy();
    test_runtime_disney_v2_3d_one_bounce_light_emitter_contributes();
    test_runtime_disney_v2_3d_transmission_glass_participates();
    test_runtime_disney_v2_3d_primary_transparency_continues_camera_ray();
    test_runtime_disney_v2_3d_scene_object_glass_override_continues_camera_ray();
    test_runtime_disney_v2_3d_nested_rough_primary_transparency_reaches_receiver();
    test_runtime_disney_v2_3d_primary_transparency_accumulates_transparent_layers();
    test_runtime_disney_v2_3d_transparency_policy_splits_thin_walled_and_solid();
    test_runtime_disney_v2_3d_transparency_policy_classifies_alpha_only();
    test_runtime_disney_v2_3d_bounded_recursive_participates();
    test_runtime_disney_v2_3d_mis_and_emitter_accounting_separates_branches();
    test_runtime_disney_v2_3d_emissive_material_surface_hit_contributes();
    test_runtime_disney_v2_3d_emissive_area_sample_lights_primary_vertex();
    test_runtime_disney_v2_3d_mixed_finite_and_emissive_area_branches();
    test_runtime_disney_v2_3d_recursive_emissive_area_sample_lights_vertex();
    test_runtime_disney_v2_3d_recursive_emissive_area_policy_caps();
    test_runtime_disney_v2_3d_recursive_emissive_material_surface_terminates();
    test_runtime_disney_v2_3d_bounded_recursive_loop_depth_three_participates();
    test_runtime_disney_v2_3d_bounded_recursive_loop_depth_limit_stops_before_emitter();
    test_runtime_disney_v2_3d_path_depth_policy_blocks_recursive_depth();
    test_runtime_disney_v2_3d_path_policy_roulette_can_terminate_recursive_depth();
    return 0;
}
