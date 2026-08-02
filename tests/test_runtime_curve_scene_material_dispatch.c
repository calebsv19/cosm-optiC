#include "test_runtime_curve_scene_material_dispatch.h"

#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "material/material_manager.h"
#include "render/runtime_scene_curve_3d.h"
#include "scene/object_manager.h"
#include "test_support.h"

int run_test_runtime_curve_scene_material_dispatch_tests(void) {
    const int before = test_support_failures();
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload;
    HitInfo3D hit;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0],
               OBJECT_CIRCLE,
               0.0,
               0.0,
               1.0,
               0.0,
               NULL,
               0);
    sceneSettings.sceneObjects[0].color = 0x2F7D32;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].reflectivity = 0.31;
    sceneSettings.sceneObjects[0].roughness = 0.27;
    sceneSettings.sceneObjects[0].hasHairOpticsOverride = true;
    sceneSettings.sceneObjects[0].hairAbsorptionR = 0.18;
    sceneSettings.sceneObjects[0].hairAbsorptionG = 0.62;
    sceneSettings.sceneObjects[0].hairAbsorptionB = 1.05;
    sceneSettings.sceneObjects[0].hairLongitudinalRoughness = 0.42;
    sceneSettings.sceneObjects[0].hairAzimuthalRoughness = 0.51;
    sceneSettings.sceneObjects[0].hairIor = 1.55;
    sceneSettings.sceneObjects[0].hairCuticleTiltDegrees = 2.5;

    HitInfo3D_Reset(&hit);
    hit.hasCurveTangent = true;
    hit.curveSceneInstanceIndex = 0;
    hit.sceneObjectIndex = 0;
    hit.source.kind = RUNTIME_PRIMITIVE_3D_KIND_CURVE;
    hit.source.sceneObjectIndex = 0;
    snprintf(hit.source.objectId,
             sizeof(hit.source.objectId),
             "%s",
             "psg23c_material_curve");

    assert_true("psg23c_real_material_dispatch",
                RuntimeSceneCurve3D_ResolveMaterial(&hit, &payload));
    assert_true("psg23c_real_material_valid", payload.valid);
    assert_true("psg23c_real_material_scene_identity",
                payload.sceneObjectIndex == 0);
    assert_true("psg23c_real_material_preset",
                payload.materialId == MATERIAL_PRESET_GLOSSY);
    assert_close("psg23c_real_material_roughness",
                 payload.bsdf.roughness,
                 0.27,
                 1.0e-6);
    assert_close("psg23c_real_material_reflectivity",
                 payload.bsdf.reflectivity,
                 0.31,
                 1.0e-6);
    assert_true("psg23g_curve_hair_payload_enabled", payload.hairOptics.enabled);
    assert_close("psg23g_curve_hair_payload_absorption_g",
                 payload.hairOptics.absorptionG,
                 0.62,
                 1.0e-6);
    assert_true("psg23g_curve_hair_dispatch",
                RuntimeHairScattering3D_ShouldApply(
                    hit.hasCurveTangent, &payload.hairOptics));

    hit.hasCurveTangent = false;
    assert_true("psg23g_triangle_surface_dispatch_preserved",
                !RuntimeHairScattering3D_ShouldApply(
                    hit.hasCurveTangent, &payload.hairOptics));
    assert_true("psg23c_non_curve_material_rejected",
                !RuntimeSceneCurve3D_ResolveMaterial(&hit, &payload));

    sceneSettings = saved_scene;
    MaterialManagerResetDefaults();
    return test_support_failures() - before;
}
