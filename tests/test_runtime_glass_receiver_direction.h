#pragma once
#include "render/runtime_disney_v2_receiver_3d.h"
#include "render/runtime_glass_null_interface_3d.h"

static void test_runtime_glass_receiver_direction(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    memset(&sceneSettings,0,sizeof(sceneSettings));
    memset(&animSettings,0,sizeof(animSettings));
    sceneSettings.objectCount=1;
    sceneSettings.sceneObjects[0].material_id=MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].color=0xBBDDFF;
    sceneSettings.sceneObjects[0].alpha=1;
    sceneSettings.sceneObjects[0].roughness=.35;
    sceneSettings.sceneObjects[0].reflectivity=.35;
    RuntimeScene3D scene; RuntimeScene3D_Init(&scene);
    scene.hasLight=true;scene.light.position=vec3(-2,-2,3);
    scene.light.intensity=4;scene.light.falloffDistance=10;
    HitInfo3D hit={0};hit.sceneObjectIndex=0;hit.triangleIndex=-1;
    hit.normal=hit.geometricNormal=hit.shadingNormal=vec3(0,-1,0);
    Ray3D ray=RuntimeRay3D_Make(vec3(0,-1,0),vec3(.6,1,.2));
    RuntimeMaterialPayload3D payload={0};
    assert_true("glass_receiver_payload",RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
    RuntimeDisneyV2_3DResult parent={0},actual={0};
    parent.tracePixelX=parent.tracePixelY=-1;
    parent.payload.hasGlassTransportOverride=true;
    assert_true("glass_receiver_actual_ray",runtime_disney_v2_shade_receiver_3d(
        &scene,&hit,&ray,&payload,NULL,&parent,&actual));
    assert_close("glass_receiver_view_cosine",actual.ndotv,
        vec3_dot(hit.normal,vec3_scale(ray.direction,-1)),1e-12);
    parent.payload.hasGlassTransportOverride=false;
    assert_true("legacy_receiver_direction",runtime_disney_v2_shade_receiver_3d(
        &scene,&hit,&ray,&payload,NULL,&parent,&actual));
    assert_close("legacy_receiver_view_unchanged",actual.ndotv,1,1e-12);
    RuntimeMaterialPayload3D neutral={0};
    neutral.valid=neutral.hasGlassTransportOverride=true;
    neutral.sceneObjectIndex=-1;
    neutral.opticalIor=neutral.transparency=1;
    neutral.hasGlassInterfaceTintOverride=neutral.hasGlassAbsorptionColorOverride=true;
    neutral.glassInterfaceTintR=neutral.glassInterfaceTintG=neutral.glassInterfaceTintB=1;
    neutral.glassAbsorptionColorR=neutral.glassAbsorptionColorG=neutral.glassAbsorptionColorB=1;
    assert_true("neutral_glass_null_interface",RuntimeGlassNullInterface3D_IsNeutral(&neutral));
    RuntimeMaterialPayload3D changed=neutral;
    changed.hasGlassTransportOverride=false;
    assert_true("legacy_glass_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.opticalIor=1.52;
    assert_true("refractive_glass_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.bsdf.roughness=.3;
    assert_true("frost_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.transparency=.5;
    assert_true("partial_transmission_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.glassInterfaceTintR=.99;
    assert_true("tinted_glass_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.glassAbsorptionColorB=.99;
    assert_true("absorbing_glass_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.emissive=.1;
    assert_true("emitting_glass_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    changed=neutral;changed.hasMicrodetailNormal=true;
    assert_true("normal_response_kept",!RuntimeGlassNullInterface3D_IsNeutral(&changed));
    RuntimeScene3D_Free(&scene);sceneSettings=saved_scene;animSettings=saved_anim;
}
