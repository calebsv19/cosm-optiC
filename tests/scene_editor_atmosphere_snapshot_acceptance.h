#include <pthread.h>
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"

typedef struct {
    const RuntimeNative3DPreparedFrame* frame;
    RayTracing3DIntegratorId integrator;
    unsigned char pixels[96*64*4];
    bool ok;
} AtmospherePreparedThread;
static void* atmosphere_prepared_thread(void* context) {
    AtmospherePreparedThread* p=context;
    p->ok=RuntimeNative3DRenderPreparedRegion(p->pixels,p->integrator,p->frame,0,0,96,64,NULL);
    return NULL;
}
static void atmosphere_snapshot_native(SceneEditor* editor) {
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_ENVIRONMENT);
    SceneAtmosphereAttachment value;assert(SceneEditorAtmosphereRead(&value) && value.optics.physical);
    RayEvaluatedSceneSnapshot evaluated;
    bool evaluated_ready=false;
    for(int i=0;i<100 && !evaluated_ready;++i){SceneEditorSessionRuntimeRender(editor);evaluated_ready=SceneEditorTimelineCopyEvaluated(&evaluated);}
    assert(evaluated_ready);
    RuntimeNative3DPreparedFrame *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a && b);
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(a,96,64,&evaluated,NULL));
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(b,96,64,&evaluated,NULL));
    assert(a->scene.volume.opticsCaptured && a->scene.volume.optics.material.physical_smoke);
    unsigned char direct[96*64*4],disney[96*64*4],result[96*64*4];
    assert(RuntimeNative3DRenderPreparedRegion(direct,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,a,0,0,96,64,NULL));
    assert(RuntimeNative3DRenderPreparedRegion(disney,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,a,0,0,96,64,NULL));
    RuntimeVolume3DMaterialTuning material=RuntimeVolume3DMaterial_GetTuning();
    RuntimeVolume3DScatterTuning scatter=RuntimeVolume3DScatter_GetTuning();
    RuntimeVolume3DMaterial_SetSmokeOptics(.003,500,.2,-.4,true);RuntimeVolume3DMaterial_SetCellCentered(false);
    RuntimeVolume3DMaterial_SetSkyErrorBudget(0);RuntimeVolume3DMaterial_SetDensityScale(9);RuntimeVolume3DMaterial_SetOpacity(.1);
    RuntimeVolume3DScatter_SetStepScale(2);RuntimeVolume3DScatter_SetStrengthGain(10);RuntimeVolume3DScatter_SetTint(1,0,0);
    RuntimeVolume3D_CaptureOptics(&b->scene.volume);
    assert(RuntimeNative3DRenderPreparedRegion(result,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,a,0,0,96,64,NULL));assert(!memcmp(result,direct,sizeof(result)));
    assert(RuntimeNative3DRenderPreparedRegion(result,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,a,0,0,96,64,NULL));assert(!memcmp(result,disney,sizeof(result)));
    assert(RuntimeNative3DRenderPreparedRegion(result,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,b,0,0,96,64,NULL));assert(memcmp(result,direct,sizeof(result)));
    AtmospherePreparedThread pa={.frame=a,.integrator=RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT},pb={.frame=b,.integrator=RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT};pthread_t ta,tb;
    assert(pthread_create(&ta,NULL,atmosphere_prepared_thread,&pa)==0);assert(pthread_create(&tb,NULL,atmosphere_prepared_thread,&pb)==0);
    pthread_join(ta,NULL);pthread_join(tb,NULL);assert(pa.ok && pb.ok);
    assert(!memcmp(pa.pixels,direct,sizeof(direct)) && !memcmp(pb.pixels,result,sizeof(result)));
    RuntimeVolume3DMaterial_RestoreTuning(material);RuntimeVolume3DScatter_RestoreTuning(scatter);
    /* Reconstruct the legacy pre-snapshot lookup path with identical values. */
    material.physical_smoke=false;material.cell_centered=false;material.sky_error_budget=0;
    RuntimeVolume3DMaterial_RestoreTuning(material);RuntimeVolume3DScatter_RestoreTuning(scatter);
    RuntimeVolume3D_CaptureOptics(&a->scene.volume);RuntimeVolume3D_CaptureOptics(&b->scene.volume);
    a->physicalAtmosphere=b->physicalAtmosphere=false;b->scene.volume.opticsCaptured=false;
    assert(RuntimeNative3DRenderPreparedRegion(direct,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,a,0,0,96,64,NULL));
    assert(RuntimeNative3DRenderPreparedRegion(result,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,b,0,0,96,64,NULL));assert(!memcmp(direct,result,sizeof(result)));
    assert(RuntimeNative3DRenderPreparedRegion(disney,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,a,0,0,96,64,NULL));
    assert(RuntimeNative3DRenderPreparedRegion(result,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,b,0,0,96,64,NULL));assert(!memcmp(disney,result,sizeof(result)));
    RuntimeNative3DPreparedFrame_Free(a);RuntimeNative3DPreparedFrame_Free(b);free(a);free(b);
    fprintf(stderr,"Prepared optical snapshots PASS: complete value copies, global-edit isolation for Direct Light/Disney V2, distinct-frame concurrent pixels, exact legacy lookup parity for both integrators, no document mutation\n");
}
