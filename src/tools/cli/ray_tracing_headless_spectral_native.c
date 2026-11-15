#include "tools/ray_tracing_headless_spectral.h"
#include "tools/ray_tracing_headless_spectral_map.h"
#include "tools/ray_tracing_render_headless_internal.h"
#include "render/runtime_spectral_transport.h"
#include <time.h>

bool RayTracingHeadlessSpectral_IsOrdinary(const RayTracingAgentRenderRequest *request) {
    const RuntimeSpectralPencil *p=&request->spectral;
    RuntimeSpectralTransportRoute route=RuntimeSpectralTransport_Route(p);
    return route==RUNTIME_SPECTRAL_UNRESOLVED || route==RUNTIME_SPECTRAL_SURFACE_PATH || route==RUNTIME_SPECTRAL_LEGACY_DIRECT;
}
static const RuntimeSpectralPencil *explicit_profile(const RayTracingAgentRenderRequest *r) {
    return r->spectral.surface_path.enabled || r->spectral.legacy_direct?&r->spectral:NULL;
}
bool RayTracingHeadlessSpectral_PrepareNative(const RayTracingAgentRenderRequest *r,
    const RayEvaluatedSceneSnapshot *scene,RuntimeNative3DPreparedFrame *frame) {
    return RuntimeNative3DPrepareEvaluatedFrameWithProfile(frame,r->width,r->height,scene,NULL,NULL,NULL,explicit_profile(r));
}
typedef struct {const RayTracingAgentRenderRequest *request;struct timespec start;} NativeProgress;
static bool progress(const RuntimeNative3DTileSchedulerProgress *p,void *user) {
    NativeProgress *context=user;
    ray_tracing_render_headless_write_progress_and_job_status(context->request->progress_path,context->request,
        "spectral_surface_path",context->request->start_frame,0,0,p->completedSubpasses,1,
        p->completedTilesInSubpass,p->totalTilesInSubpass,ray_tracing_elapsed_seconds_since(&context->start),-1,
        "running","ordinary V3 camera path sampling",NULL,NULL,NULL,-1);
    return true;
}
bool RayTracingHeadlessSpectral_RenderNative(const RayTracingAgentRenderRequest *request,const char *image_path,
    const RayEvaluatedSceneSnapshot *scene,uint8_t *pixels,RuntimeSpectralReadout *out,RuntimeSpectralTransportSnapshot *snapshot,char *diag,size_t size) {
    RuntimeNative3DPreparedFrame frame={0};
    if(!RayTracingHeadlessSpectral_PrepareNative(request,scene,&frame)) {
        snprintf(diag,size,"%s",RuntimeNative3DPrepareFrameLastDiagnostics());return false;
    }
    if(!RuntimeSpectralTransport_ConfigureOutput(&frame,image_path,request->overwrite)) {
        snprintf(diag,size,"V3: linear component output already exists or has an invalid path");
        RuntimeNative3DPreparedFrame_Free(&frame);return false;
    }
    NativeProgress context={.request=request};clock_gettime(CLOCK_MONOTONIC,&context.start);
    bool ok;
    if(RuntimeSpectralTransport_IsSurfacePath(&frame)) {
        ok=RuntimeSpectralTransport_Render(&frame,pixels,NULL,progress,&context,NULL);
        *out=frame.spectralFrame->readout;
        snprintf(diag,size,"%s",frame.spectralFrame->diagnostic);
    } else if(frame.spectralFrame && frame.spectralFrame->profile.map.enabled) {
        /* Saved calibration scenes retain their declared estimator, never an added opaque GI term. */
        RayTracingAgentRenderRequest effective=*request;effective.spectral=frame.spectralFrame->profile;
        ok=RayTracingHeadlessSpectralMap_Run(&effective,&frame,pixels,out,diag,size);
    } else ok=RuntimeNative3DRenderPreparedRegion(pixels,RAY_TRACING_3D_INTEGRATOR_DISNEY_V3_SPECTRAL,
        &frame,0,0,frame.width,frame.height,NULL);
    if(frame.spectralFrame) {
        if(!frame.spectralFrame->surface_path) {frame.spectralFrame->readout=*out;snprintf(frame.spectralFrame->diagnostic,sizeof(frame.spectralFrame->diagnostic),"%s",ok?"ok":diag);}
        RuntimeSpectralTransport_Finish(&frame,ok,NULL);
        if(snapshot)(void)RuntimeSpectralTransport_Capture(&frame,true,snapshot);
    }
    RuntimeNative3DPreparedFrame_Free(&frame);return ok;
}
