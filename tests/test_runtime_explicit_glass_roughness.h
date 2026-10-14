/* Explicit native optical response does not change the common legacy BSDF. */
static void test_runtime_explicit_glass_optical_roughness(void) {
    SceneConfig saved=sceneSettings;
    MaterialManagerResetDefaults();memset(&sceneSettings,0,sizeof(sceneSettings));
    sceneSettings.objectCount=1;
    SceneObject* object=&sceneSettings.sceneObjects[0];
    InitObject(object,OBJECT_CIRCLE,0,0,10,0,NULL,0);
    object->material_id=MATERIAL_PRESET_TRANSPARENT;object->roughness=0;
    RuntimeMaterialPayload3D payload={0};MaterialBSDF common={0};
    assert_true("legacy_zero_glass_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("legacy_glass_roughness_floor",payload.bsdf.roughness,.02,1e-12);
    SceneObjectSeedGlassTransportOverrideFromMaterial(object);
    assert_true("explicit_zero_glass_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("explicit_optical_polished_zero",payload.bsdf.roughness,0,1e-12);
    MaterialBSDFInitFromSceneObject(object,&common);
    assert_close("common_ggx_floor_preserved",common.roughness,.02,1e-12);
    RuntimePrincipledBSDF3D principled=RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    assert_close("principled_ggx_floor_preserved",principled.roughness,.02,1e-12);
    object->glassIor=1;
    assert_true("index_matched_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("index_matched_no_inherited_reflectivity",payload.bsdf.reflectivity,0,1e-12);
    principled=RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    assert_close("index_matched_fresnel_zero",principled.dielectricF0,0,1e-12);
    object->glassIor=1.5;
    assert_true("clear_glass_fresnel_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("clear_glass_fresnel_four_percent",payload.bsdf.reflectivity,.04,1e-12);
    principled=RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    assert_close("clear_glass_no_legacy_f0_floor",principled.dielectricF0,.04,1e-12);
    object->roughness=.08;
    assert_true("explicit_frost_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("explicit_frost_preserved",payload.bsdf.roughness,.08,1e-12);
    object->material_id=MATERIAL_PRESET_DEFAULT;object->roughness=0;
    assert_true("opaque_zero_payload",RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&payload));
    assert_close("opaque_floor_preserved",payload.bsdf.roughness,.02,1e-12);
    sceneSettings=saved;
}
