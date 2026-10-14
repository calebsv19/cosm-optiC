#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"

static void disney_atmosphere_test_volume(RuntimeScene3D* scene,double y) {
    scene->volume.enabled=true;scene->volume.affectsLighting=true;
    assert_true("disney_atmosphere_grid",RuntimeVolumeGrid3D_Configure(&scene->volume.grid,1,2,2,2,0,0,.01,vec3(-.5,y,-.5),.5,vec3(0,0,1),0));
    assert_true("disney_atmosphere_density",RuntimeVolumeAttachment3D_AllocateOwnedChannels(&scene->volume,RUNTIME_VOLUME_3D_CHANNEL_DENSITY));
    for(int i=0;i<8;++i)scene->volume.channels.density[i]=1;
    RuntimeVolume3DMaterial_SetCellCentered(true);
    RuntimeVolume3DMaterial_SetSmokeOptics(1,0,0,0,false);
    RuntimeVolume3D_CaptureOptics(&scene->volume);
}
static void test_runtime_disney_atmosphere_paths(void) {
    const SceneConfig saved_scene=sceneSettings;const AnimationConfig saved_anim=animSettings;
    const RuntimeVolume3DMaterialTuning saved_material=RuntimeVolume3DMaterial_GetTuning();
    const RuntimeVolume3DScatterTuning saved_scatter=RuntimeVolume3DScatter_GetTuning();
    RuntimeNative3DSamplingContext sampling={.sampleSequence=31,.temporalSubpassCount=1};
    memset(&sceneSettings,0,sizeof(sceneSettings));memset(&animSettings,0,sizeof(animSettings));
    MaterialManagerResetDefaults();animSettings.specularDepth3D=1;animSettings.transmissionDepth3D=1;animSettings.transmissionSamples3D=1;
    runtime_disney_v2_test_configure_scene_material(1,MATERIAL_PRESET_DEFAULT,0xFFFFFF,0,.5);
    sceneSettings.sceneObjects[1].emissiveStrength=1;
    RuntimeScene3D scene;runtime_disney_v2_test_init_reflection_scene(&scene);
    scene.light.position=vec3(0,2.5,0);scene.light.radius=0;
    disney_atmosphere_test_volume(&scene,1);
    RuntimePrimaryHit3DResult primary=runtime_disney_v2_test_primary_hit(&scene);
    RuntimeMaterialPayload3D mirror=runtime_disney_v2_test_payload(1,1,1,.95,.02,0,1,0,0);
    RuntimeDisneyV2_3DResult clear={0},fog={0};
    assert_true("disney_atmosphere_mirror_clear",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,&primary,&mirror,&sampling,&clear));
    assert_true("disney_atmosphere_mirror_has_receiver",clear.specularReflectionGeometryHitCount>0 && clear.specularReflectionRadiance>0);
    RuntimeVolume3DMaterial_SetSmokeOptics(1,1,0,0,false);RuntimeVolume3D_CaptureOptics(&scene.volume);
    assert_true("disney_atmosphere_mirror_fog",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,&primary,&mirror,&sampling,&fog));
    assert_close("disney_atmosphere_mirror_one_segment",fog.specularReflectionRadiance,clear.specularReflectionRadiance*exp(-1),1e-8);
    RuntimeScene3D_Free(&scene);

    runtime_disney_v2_test_init_primary_transparency_scene(&scene);
    scene.light.position=vec3(0,-1.75,0);scene.light.radius=0;
    disney_atmosphere_test_volume(&scene,-1.5);
    primary=runtime_disney_v2_test_primary_hit(&scene);
    RuntimeMaterialPayload3D glass=runtime_disney_v2_test_payload(1,1,1,0,0,0,.01,1,0);
    glass.thinWalled=true;glass.opticalIor=glass.bsdf.ior=1;
    memset(&clear,0,sizeof(clear));memset(&fog,0,sizeof(fog));
    assert_true("disney_atmosphere_glass_clear",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,&primary,&glass,&sampling,&clear));
    assert_true("disney_atmosphere_glass_has_receiver",clear.primaryTransmissionContinued && clear.primaryTransmissionRadiance>0);
    RuntimeVolume3DMaterial_SetSmokeOptics(1,1,0,0,false);RuntimeVolume3D_CaptureOptics(&scene.volume);
    assert_true("disney_atmosphere_glass_fog",RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(&scene,&primary,&glass,&sampling,&fog));
    assert_close("disney_atmosphere_glass_one_segment",fog.primaryTransmissionPathState.throughputR,
        clear.primaryTransmissionPathState.throughputR*exp(-1),1e-12);
    assert_close("disney_atmosphere_glass_receiver_composition",fog.primaryTransmissionRadianceR,
        fog.primaryTransmissionReceiverRadianceR*fog.primaryTransmissionPathState.throughputR,1e-8);
    RuntimeScene3D_Free(&scene);
    sceneSettings=saved_scene;animSettings=saved_anim;MaterialManagerResetDefaults();
    RuntimeVolume3DMaterial_RestoreTuning(saved_material);RuntimeVolume3DScatter_RestoreTuning(saved_scatter);
}
