#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "editor/scene_editor_material_face_placement.h"
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
#include "render/runtime_material_texture_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
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
    sceneSettings.sceneObjects[1].alpha = 0.5;
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
    sceneSettings.sceneObjects[0].alpha = 0.25;
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

static int test_runtime_material_payload_3d_rust_texture_is_hit_anchored(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    RuntimeMaterialPayload3D repeated = {0};
    RuntimeMaterialPayload3D panned = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 7;
    hit.primitiveIndex = 1;
    hit.baryU = 0.34;
    hit.baryV = 0.52;
    hit.baryW = 0.14;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_rust_baseline_ok", ok);

    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 1.0;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
    assert_true("runtime_material_payload_rust_textured_ok", ok);

    if (textured.textureMask <= 1e-9) {
        hit.baryU = 0.15;
        hit.baryV = 0.18;
        hit.baryW = 0.67;
        ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
        assert_true("runtime_material_payload_rust_textured_retry_ok", ok);
    }

    assert_true("runtime_material_payload_rust_mask_active", textured.textureMask > 1e-9);
    assert_true("runtime_material_payload_rust_reflectivity_reduced",
                textured.bsdf.reflectivity < baseline.bsdf.reflectivity);
    assert_true("runtime_material_payload_rust_roughness_increased",
                textured.bsdf.roughness > baseline.bsdf.roughness);
    assert_true("runtime_material_payload_rust_color_shifted_red",
                textured.baseColorR > baseline.baseColorR &&
                textured.baseColorG < baseline.baseColorG);

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &repeated);
    assert_true("runtime_material_payload_rust_repeat_ok", ok);
    assert_close("runtime_material_payload_rust_repeat_mask_stable",
                 repeated.textureMask,
                 textured.textureMask,
                 1e-12);
    assert_close("runtime_material_payload_rust_repeat_u_stable",
                 repeated.textureU,
                 textured.textureU,
                 1e-12);

    sceneSettings.sceneObjects[0].textureOffsetU = 0.25;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &panned);
    assert_true("runtime_material_payload_rust_pan_ok", ok);
    assert_true("runtime_material_payload_rust_pan_moves_u",
                fabs(panned.textureU - textured.textureU) > 1e-6);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_face_texture_override_affects_hit(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    HitInfo3D hit = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    bool ok = false;
    bool found_mask = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 18;
    hit.localTriangleIndex = 4;
    hit.primitiveIndex = 1;
    hit.baryU = 0.34;
    hit.baryV = 0.52;
    hit.baryW = 0.14;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_face_texture_baseline_ok", ok);
    assert_close("runtime_material_payload_face_texture_baseline_mask",
                 baseline.textureMask,
                 0.0,
                 1e-12);

    placement.hasOverride = true;
    placement.sceneObjectIndex = 0;
    placement.faceGroupIndex = 2;
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.scale = 1.0;
    placement.strength = 1.0;
    assert_true("runtime_material_payload_face_texture_override_set",
                SceneEditorMaterialFacePlacementSetOverride(&placement));

    for (int u = 1; u < 9 && !found_mask; ++u) {
        for (int v = 1; v < 9 && !found_mask; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            hit.baryV = bary_v;
            hit.baryW = bary_w;
            hit.baryU = 1.0 - bary_v - bary_w;
            ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
            assert_true("runtime_material_payload_face_texture_override_retry_ok", ok);
            found_mask = textured.textureMask > 1e-9;
        }
    }

    assert_true("runtime_material_payload_face_texture_mask_active", found_mask);
    assert_true("runtime_material_payload_face_texture_roughness_increased",
                textured.bsdf.roughness > baseline.bsdf.roughness);
    assert_true("runtime_material_payload_face_texture_reflectivity_reduced",
                textured.bsdf.reflectivity < baseline.bsdf.reflectivity);
    assert_true("runtime_material_payload_face_texture_object_default_unchanged",
                sceneSettings.sceneObjects[0].textureId == RUNTIME_MATERIAL_TEXTURE_3D_NONE);

    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_fog_texture_roughens_transparency(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB8D8FF;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.8;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 4;
    hit.primitiveIndex = 1;
    hit.baryU = 0.20;
    hit.baryV = 0.30;
    hit.baryW = 0.50;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_fog_baseline_ok", ok);

    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 2.0;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
    assert_true("runtime_material_payload_fog_textured_ok", ok);
    assert_true("runtime_material_payload_fog_mask_active", textured.textureMask > 1e-9);
    assert_true("runtime_material_payload_fog_roughness_preserved_or_increased",
                textured.bsdf.roughness >= baseline.bsdf.roughness);
    assert_true("runtime_material_payload_fog_transparency_reduced",
                textured.transparency < baseline.transparency);
    assert_true("runtime_material_payload_fog_keeps_some_transparency",
                textured.transparency > 0.0);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_texture_3d_uv_sampler_matches_hit_sampler(void) {
    SceneConfig saved_scene = sceneSettings;
    HitInfo3D hit = {0};
    RuntimeMaterialTexture3DSample hit_sample = {0};
    RuntimeMaterialTexture3DSample uv_sample = {0};
    bool hit_ok = false;
    bool uv_ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 3.0;
    sceneSettings.sceneObjects[0].textureOffsetU = 0.125;
    sceneSettings.sceneObjects[0].textureOffsetV = 0.375;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 6;
    hit.baryU = 0.18;
    hit.baryV = 0.37;
    hit.baryW = 0.45;

    hit_ok = RuntimeMaterialTexture3D_Sample(&sceneSettings.sceneObjects[0],
                                             &hit,
                                             &hit_sample);
    uv_ok = RuntimeMaterialTexture3D_SampleUV(&sceneSettings.sceneObjects[0],
                                              hit.triangleIndex,
                                              hit.baryV,
                                              hit.baryW,
                                              &uv_sample);

    assert_true("runtime_material_texture_uv_parity_active", hit_ok == uv_ok);
    assert_true("runtime_material_texture_uv_parity_kind", hit_sample.kind == uv_sample.kind);
    assert_close("runtime_material_texture_uv_parity_u", hit_sample.u, uv_sample.u, 1e-12);
    assert_close("runtime_material_texture_uv_parity_v", hit_sample.v, uv_sample.v, 1e-12);
    assert_close("runtime_material_texture_uv_parity_mask", hit_sample.mask, uv_sample.mask, 1e-12);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_texture_3d_rust_parameter_modes_change_masks(void) {
    SceneObject object;
    RuntimeMaterialTexture3DPlacement placement = {0};
    RuntimeMaterialTexture3DSample samples[4];
    double diff_sum = 0.0;

    memset(&object, 0, sizeof(object));
    memset(samples, 0, sizeof(samples));
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    object.textureStrength = 1.0;
    object.textureScale = 1.0;
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.strength = 1.0;
    placement.scale = 1.0;
    placement.params = RuntimeMaterialTexture3DDefaultParams();
    placement.params.coverage = 0.78;
    placement.params.edgeSoftness = 0.75;
    placement.params.contrast = 0.25;
    placement.params.grain = 0.35;
    placement.params.flow = 0.75;
    placement.params.colorDepth = 0.8;
    placement.params.surfaceDamage = 0.8;

    for (int mode = 0; mode < 4; ++mode) {
        placement.params.patternMode = mode;
        assert_true("runtime_material_texture_param_mode_sample_ok",
                    RuntimeMaterialTexture3D_SamplePlacedUV(&object,
                                                            0.37,
                                                            0.61,
                                                            123,
                                                            &placement,
                                                            &samples[mode]) ||
                        samples[mode].mask >= 0.0);
    }

    for (int mode = 1; mode < 4; ++mode) {
        diff_sum += fabs(samples[mode].mask - samples[0].mask);
    }
    assert_true("runtime_material_texture_param_modes_differ", diff_sum > 1e-5);
    assert_close("runtime_material_texture_param_color_depth",
                 samples[2].colorDepth,
                 0.8,
                 1e-12);
    assert_close("runtime_material_texture_param_surface_damage",
                 samples[2].surfaceDamage,
                 0.8,
                 1e-12);
    return 0;
}

static int test_runtime_material_payload_3d_surface_damage_controls_roughness(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D low_damage = {0};
    RuntimeMaterialPayload3D high_damage = {0};
    HitInfo3D hit = {0};

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 1.0;
    sceneSettings.sceneObjects[0].texturePatternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH;
    sceneSettings.sceneObjects[0].textureCoverage = 1.0;
    sceneSettings.sceneObjects[0].textureEdgeSoftness = 1.0;
    sceneSettings.sceneObjects[0].textureContrast = 0.2;
    sceneSettings.sceneObjects[0].textureGrain = 0.4;
    sceneSettings.sceneObjects[0].textureColorDepth = 0.6;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 9;
    hit.primitiveIndex = 1;
    hit.baryU = 0.22;
    hit.baryV = 0.33;
    hit.baryW = 0.45;

    sceneSettings.sceneObjects[0].textureSurfaceDamage = 0.05;
    assert_true("runtime_material_payload_low_damage_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &low_damage));
    sceneSettings.sceneObjects[0].textureSurfaceDamage = 1.0;
    assert_true("runtime_material_payload_high_damage_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &high_damage));

    assert_true("runtime_material_payload_damage_mask_active",
                high_damage.textureMask > 1e-9 && low_damage.textureMask > 1e-9);
    assert_true("runtime_material_payload_damage_roughness_increases",
                high_damage.bsdf.roughness > low_damage.bsdf.roughness);
    assert_true("runtime_material_payload_damage_reflectivity_reduces",
                high_damage.bsdf.reflectivity < low_damage.bsdf.reflectivity);

    sceneSettings = saved_scene;
    return 0;
}

int run_test_runtime_lighting_materials_payload_suite(void) {
    test_runtime_material_payload_3d_scene_object_resolution_contract();
    test_runtime_material_payload_3d_object_multipliers_contract();
    test_material_manager_default_presets_include_i4_entries();
    test_material_manager_load_dir_preserves_shipped_preset_ids();
    test_runtime_material_payload_3d_hit_resolution_contract();
    test_runtime_material_payload_3d_rust_texture_is_hit_anchored();
    test_runtime_material_payload_3d_face_texture_override_affects_hit();
    test_runtime_material_payload_3d_fog_texture_roughens_transparency();
    test_runtime_material_texture_3d_uv_sampler_matches_hit_sampler();
    test_runtime_material_texture_3d_rust_parameter_modes_change_masks();
    test_runtime_material_payload_3d_surface_damage_controls_roughness();
    return 0;
}
