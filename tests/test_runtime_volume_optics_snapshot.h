#include <pthread.h>
#include "render/runtime_disney_v2_atmosphere_3d.h"

typedef struct {
    const RuntimeScene3D* scene;
    Ray3D ray;
    double transmittance,scatter;
    bool passed;
} VolumeOpticsThreadProbe;
static void* volume_optics_thread(void* argument) {
    VolumeOpticsThreadProbe* p=argument;p->passed=true;
    for(int i=0;i<128;++i) {
        const RuntimeVisibility3DTransmittance t=RuntimeVolume3D_TransmittanceAlongRayRGB(&p->scene->volume,&p->ray,0,3);
        const RuntimeVolume3DScatterResult s=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(p->scene,&p->ray,0,3,NULL);
        p->passed &= t.r==p->transmittance && s.radianceR==p->scatter;
    }
    return NULL;
}
static void test_runtime_volume_optics_snapshot(void) {
    RuntimeVolume3DMaterialTuning material=RuntimeVolume3DMaterial_GetTuning();
    RuntimeVolume3DScatterTuning scatter=RuntimeVolume3DScatter_GetTuning();
    RuntimeScene3D a,b;RuntimeScene3D_Init(&a);
    a.volume.enabled=true;a.volume.affectsLighting=true;
    assert_true("snapshot_grid",RuntimeVolumeGrid3D_Configure(&a.volume.grid,1,2,2,2,0,0,.01,vec3(0,0,0),.5,vec3(0,0,1),0));
    assert_true("snapshot_density",RuntimeVolumeAttachment3D_AllocateOwnedChannels(&a.volume,RUNTIME_VOLUME_3D_CHANNEL_DENSITY));
    for(int i=0;i<8;++i)a.volume.channels.density[i]=1;
    a.environment.lightMode=ENVIRONMENT_LIGHT_MODE_AMBIENT;
    a.environment.backgroundIntensity=1;
    a.environment.backgroundTopColor=a.environment.backgroundBottomColor=vec3(1,1,1);
    RuntimeVolume3DMaterial_ResetTuning();RuntimeVolume3DScatter_ResetTuning();
    RuntimeVolume3DMaterial_SetCellCentered(true);
    assert_true("snapshot_profile",RuntimeVolume3DMaterial_SetSmokeOptics(1,.4,.6,0,true));
    RuntimeVolume3DScatter_SetStepScale(.5);RuntimeVolume3D_CaptureOptics(&a.volume);
    Ray3D ray={.origin=vec3(.5,-1,.5),.direction=vec3(0,1,0)};
    const RuntimeVisibility3DTransmittance before=RuntimeVolume3D_TransmittanceAlongRayRGB(&a.volume,&ray,0,3);
    const RuntimeVolume3DScatterResult source=RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(&a,&ray,0,3,NULL);
    assert_close("snapshot_beer_lambert",before.r,exp(-.4),1e-12);
    assert_true("snapshot_sky_source",source.radianceR>0);
    b=a;b.volume.ownsChannelBuffers=false;
    RuntimeVolume3DMaterial_SetSmokeOptics(1,0,0,0,false);
    RuntimeVolume3DScatter_SetStepScale(4);RuntimeVolume3DScatter_SetTint(1,0,0);
    RuntimeVolume3D_CaptureOptics(&b.volume);
    RuntimeVolume3DMaterial_ResetTuning();RuntimeVolume3DScatter_ResetTuning();
    RuntimeVolume3DMaterial_SetDensityScale(0);RuntimeVolume3DMaterial_SetCellCentered(false);
    assert_close("snapshot_stable_after_global_edit",RuntimeVolume3D_TransmittanceAlongRayRGB(&a.volume,&ray,0,3).r,before.r,0);
    assert_true("snapshot_layout_owned",a.volume.grid.cellCentered && a.volume.optics.material.cell_centered);
    assert_close("snapshot_other_frame",RuntimeVolume3D_TransmittanceAlongRayRGB(&b.volume,&ray,0,3).r,1,0);
    Vec3 weight=vec3(2,3,4),radiance=vec3(0,0,0);
    RuntimeDisneyV2Atmosphere3D_Advance(&a,&ray,3,NULL,&weight,&radiance);
    assert_close("disney_segment_attenuation",weight.x,2*before.r,0);
    assert_close("disney_segment_source",radiance.y,3*source.radianceG,0);
    double r=2,g=3,blue=4;
    RuntimeDisneyV2Atmosphere3D_Compose(&a,&ray,3,NULL,&r,&g,&blue);
    assert_close("disney_composition_once",r,2*before.r+source.radianceR,0);
    VolumeOpticsThreadProbe pa={&a,ray,before.r,source.radianceR,false},pb={&b,ray,1,0,false};pthread_t ta,tb;
    assert_true("snapshot_thread_a",pthread_create(&ta,NULL,volume_optics_thread,&pa)==0);
    assert_true("snapshot_thread_b",pthread_create(&tb,NULL,volume_optics_thread,&pb)==0);
    pthread_join(ta,NULL);pthread_join(tb,NULL);
    assert_true("snapshot_parallel_distinct_policies",pa.passed && pb.passed);
    a.volume.optics.material.physical_smoke=false;
    r=2;g=3;blue=4;RuntimeDisneyV2Atmosphere3D_Compose(&a,&ray,3,NULL,&r,&g,&blue);
    assert_close("disney_legacy_segment_noop",r,2,0);
    RuntimeScene3D_Free(&a);
    RuntimeVolume3DMaterial_RestoreTuning(material);RuntimeVolume3DScatter_RestoreTuning(scatter);
}
