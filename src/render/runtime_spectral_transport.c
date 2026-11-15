#include "render/runtime_spectral_transport.h"
#include "render/runtime_spectral_scene_profile.h"
#include "runtime_spectral_json.h"
#include <stdlib.h>
#include <string.h>

RuntimeSpectralTransportRoute RuntimeSpectralTransport_Route(const RuntimeSpectralPencil *p) {
    if(p->surface_path.enabled)return RUNTIME_SPECTRAL_SURFACE_PATH;
    if(p->map.enabled)return RUNTIME_SPECTRAL_PHOTON_MAP;
    if(p->diffuse.enabled)return RUNTIME_SPECTRAL_TWO_SURFACE_CALIBRATION;
    if(p->count)return RUNTIME_SPECTRAL_PENCIL_CALIBRATION;
    if(p->legacy_direct)return RUNTIME_SPECTRAL_LEGACY_DIRECT;
    return RUNTIME_SPECTRAL_UNRESOLVED;
}
const char *RuntimeSpectralTransport_RouteLabel(RuntimeSpectralTransportRoute route) {
    const char *labels[]={"unresolved","surface_path_v1","spectral_photon_map_v1","two_surface_calibration_v1","spectral_pencil_calibration_v1","legacy_direct_v1"};
    return route>=RUNTIME_SPECTRAL_UNRESOLVED && route<=RUNTIME_SPECTRAL_LEGACY_DIRECT?labels[route]:"invalid";
}

const char *RuntimeSpectralTransport_StatusLabel(RuntimeSpectralExecutionStatus status) {
    const char *labels[]={"prepared","running","completed","canceled","failed"};
    return status>=RUNTIME_SPECTRAL_EXEC_PREPARED && status<=RUNTIME_SPECTRAL_EXEC_FAILED?labels[status]:"invalid";
}
void RuntimeSpectralTransport_Finish(RuntimeNative3DPreparedFrame *frame,bool ok,const RuntimeNative3DTileSchedulerCancelToken *cancel) {
    if(!frame || !frame->spectralFrame)return;
    RuntimeSpectralFrame *s=frame->spectralFrame;
    bool canceled=RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel);
    s->execution_status=canceled?RUNTIME_SPECTRAL_EXEC_CANCELED:ok?RUNTIME_SPECTRAL_EXEC_COMPLETED:RUNTIME_SPECTRAL_EXEC_FAILED;
    if(canceled)snprintf(s->diagnostic,sizeof(s->diagnostic),"canceled before job publication");
    else if(ok)snprintf(s->diagnostic,sizeof(s->diagnostic),"ok");
    else if(!s->diagnostic[0] || !strcmp(s->diagnostic,"prepared"))snprintf(s->diagnostic,sizeof(s->diagnostic),"native V3 execution failed");
}

bool RuntimeSpectralTransport_ConfigureOutput(RuntimeNative3DPreparedFrame *frame,const char *image,bool overwrite) {
    if(!RuntimeSpectralTransport_IsSurfacePath(frame) || frame->spectralFrame->preview ||
       !frame->spectralFrame->profile.surface_path.linear_components)return true;
    return RuntimeSpectralSurfacePath_ConfigureOutput(frame->spectralFrame->surface_path,image,overwrite);
}

bool RuntimeSpectralTransport_IsSurfacePath(const RuntimeNative3DPreparedFrame *frame) {
    return frame && frame->spectralFrame && frame->spectralFrame->surface_path;
}

bool RuntimeSpectralTransport_Prepare(RuntimeNative3DPreparedFrame *frame,const RuntimeSpectralPencil *requested,
    const char *scene_path,RuntimeSpectralTransportPurpose purpose,char *diag,size_t size) {
    RuntimeSpectralPencil saved={0};
    if(!requested && scene_path && *scene_path) {
        json_object *document=json_object_from_file(scene_path);
        bool ok=document && RuntimeSpectralProfile_FromScene(document,scene_path,false,&saved);
        if(document)json_object_put(document);
        if(!ok) {snprintf(diag,size,"V3: invalid saved spectral_transport_v1 profile");return false;}
        requested=&saved;
    }
    if(requested)saved=*requested;
    if(RuntimeSpectralTransport_Route(&saved)==RUNTIME_SPECTRAL_UNRESOLVED)
        RuntimeSpectralSurfacePath_Defaults(&saved);
    RuntimeSpectralTransportRoute selected=RuntimeSpectralTransport_Route(&saved);
    if(selected==RUNTIME_SPECTRAL_TWO_SURFACE_CALIBRATION || selected==RUNTIME_SPECTRAL_PENCIL_CALIBRATION) {
        snprintf(diag,size,"V3: calibration profile requires the explicit calibration instrument");return false;
    }
    if(frame->scene.volume.enabled || (frame->physicalAtmosphere && saved.surface_path.enabled)) {
        snprintf(diag,size,"V3: ordinary volume/atmosphere needs an explicitly qualified spectral medium route");return false;
    }
    int requested_samples=saved.surface_path.samples;
    if(purpose==RUNTIME_SPECTRAL_PREVIEW && saved.surface_path.enabled) {
        /* Purpose changes sampling cost, never the transport model. Final-only
         * pixel diagnostics are preserved in the saved profile, not remapped. */
        saved.surface_path.samples=1;saved.surface_path.probe_count=0;saved.surface_path.trace_enabled=false;
    }
    frame->spectralFrame=RuntimeSpectralFrame_CreateForFilm(&saved,&frame->scene,frame->width,frame->height,diag,size);
    if(!frame->spectralFrame)return false;
    frame->spectralFrame->preview=purpose==RUNTIME_SPECTRAL_PREVIEW;
    frame->spectralFrame->requested_samples=requested_samples;
    if(frame->spectralFrame->surface_path)RuntimeSpectralSurfacePath_GetMetadata(frame->spectralFrame->surface_path,&frame->spectralFrame->readout);
    frame->tileOccupancyConservativeAllTiles=true;
    snprintf(frame->spectralFrame->diagnostic,sizeof(frame->spectralFrame->diagnostic),"prepared");
    return true;
}

bool RuntimeSpectralTransport_Render(RuntimeNative3DPreparedFrame *frame,uint8_t *pixels,
    const RuntimeNative3DTileSchedulerControl *control,RuntimeNative3DTileSchedulerProgressCallback callback,
    void *user,RuntimeNative3DRenderStats *stats) {
    if(!RuntimeSpectralTransport_IsSurfacePath(frame))return false;
    RuntimeSpectralFrame *state=frame->spectralFrame;
    if(control && control->checkpoint) {
        snprintf(state->diagnostic,sizeof(state->diagnostic),"surface_path_v1: checkpoint/resume is not qualified");RuntimeSpectralTransport_Finish(frame,false,control->cancelToken);return false;
    }
    state->execution_status=RUNTIME_SPECTRAL_EXEC_RUNNING;
    bool ok=RuntimeSpectralSurfacePath_Render(state->surface_path,frame,pixels,&state->readout,
        control?control->cancelToken:NULL,callback,user,state->diagnostic,sizeof(state->diagnostic));
    RuntimeSpectralTransport_Finish(frame,ok,control?control->cancelToken:NULL);
    if(stats) {
        *stats=(RuntimeNative3DRenderStats){0};
        stats->hitPixelCount=(int)state->readout.surface_path.pixels;
        stats->temporalCommittedSubpasses=ok?1:0;
    }
    return ok;
}

bool RuntimeSpectralTransport_Capture(const RuntimeNative3DPreparedFrame *frame,bool terminal,RuntimeSpectralTransportSnapshot *out) {
    if(!frame || !frame->spectralFrame || !out)return false;
    const RuntimeSpectralFrame *s=frame->spectralFrame;
    *out=(RuntimeSpectralTransportSnapshot){.valid=true,.terminal=terminal,.preview=s->preview,.requested_samples=s->requested_samples,.profile=s->profile};
    out->execution_status=terminal?s->execution_status:RUNTIME_SPECTRAL_EXEC_PREPARED;
    out->width=frame->width;out->height=frame->height;out->projector=frame->projector;out->camera_zoom=frame->scene.camera.zoom;
    out->frame_index=frame->evaluatedSceneBound?(int)frame->evaluatedScene.frame.sample.absolute_frame:0;
    out->profile.active_volume=NULL;
    for(int i=0;i<out->profile.count;++i)out->profile.lines[i].camera_cache=NULL;
    if(terminal)out->readout=s->readout;
    else if(s->profile.surface_path.enabled) {
        const RuntimeScene3D *scene=frame->traceScene?frame->traceScene:&frame->scene;
        RuntimeSpectralSurfacePathReadout *r=&out->readout.surface_path;
        r->source_count=s->profile.surface_path.native_sources?RuntimeLightSet3D_EnabledCount(&scene->lightSet):1;
        for(int i=0;i<r->source_count;++i) {
            const RuntimeLightSource3D *source=s->profile.surface_path.native_sources?RuntimeLightSet3D_GetEnabled(&scene->lightSet,i):NULL;
            snprintf(r->sources[i].id,sizeof(r->sources[i].id),"%s",source?source->id:s->profile.surface_path.sky?"sky":"area");
            r->sources[i].kind=source?source->kind:-1;r->sources[i].radiometry=source?source->radiometryMode:-1;
        }
    }
    snprintf(out->diagnostic,sizeof(out->diagnostic),"%s",terminal?s->diagnostic:"prepared");
    return true;
}

json_object *RuntimeSpectralTransportSnapshot_JSON(const RuntimeSpectralTransportSnapshot *snapshot,bool detailed) {
    if(!snapshot || !snapshot->valid)return NULL;
    const RuntimeSpectralPencil *p=&snapshot->profile;
    if(p->surface_path.enabled) {
        json_object *o=RuntimeSpectralSurfacePath_JSON(p,&snapshot->readout,NULL,detailed,
            RuntimeSpectralTransport_StatusLabel(snapshot->execution_status),snapshot->diagnostic);
        if(!o)return NULL;
        if(!spectral_json_add(o,"purpose",json_object_new_string(snapshot->preview?"interactive_preview":"final")) ||
           !spectral_json_add(o,"requested_samples_per_pixel",json_object_new_int(snapshot->requested_samples))) {
            json_object_put(o);return NULL;
        }
        json_object *film=json_object_new_object();
        if(!film || !spectral_json_add(film,"width",json_object_new_int(snapshot->width)) ||
           !spectral_json_add(film,"height",json_object_new_int(snapshot->height)) ||
           !spectral_json_add(film,"frame_index",json_object_new_int(snapshot->frame_index)) ||
           !spectral_json_add(film,"camera_origin_m",spectral_json_vec(snapshot->projector.origin)) ||
           !spectral_json_add(film,"camera_forward",spectral_json_vec(snapshot->projector.forward)) ||
           !spectral_json_add(film,"camera_up",spectral_json_vec(snapshot->projector.up)) ||
           !spectral_json_add(film,"camera_right",spectral_json_vec(snapshot->projector.right)) ||
           !spectral_json_add(film,"near_plane_m",json_object_new_double(snapshot->projector.nearPlane)) ||
           !spectral_json_add(film,"tan_half_fov_x",json_object_new_double(snapshot->projector.tanHalfFovX)) ||
           !spectral_json_add(film,"tan_half_fov_y",json_object_new_double(snapshot->projector.tanHalfFovY)) ||
           !spectral_json_add(film,"camera_zoom",json_object_new_double(snapshot->camera_zoom))) {
            if(film)json_object_put(film);json_object_put(o);return NULL;
        }
        if(!spectral_json_add(o,"film",film)) {json_object_put(o);return NULL;}
        return o;
    }
    json_object *o=json_object_new_object();if(!o)return NULL;
    RuntimeSpectralTransportRoute route=RuntimeSpectralTransport_Route(p);
    const char *label=RuntimeSpectralTransport_RouteLabel(route);
    const char *partition=route==RUNTIME_SPECTRAL_PHOTON_MAP?"specialized_glass_receiver_and_volume; no_ordinary_GI_added":
        route==RUNTIME_SPECTRAL_TWO_SURFACE_CALIBRATION?"two_surface_lambertian_calibration":
        route==RUNTIME_SPECTRAL_PENCIL_CALIBRATION?"deterministic_line_transport":
        route==RUNTIME_SPECTRAL_LEGACY_DIRECT?"direct_preview_only":"unresolved_no_execution";
    bool ok=spectral_json_add(o,"status",json_object_new_string(RuntimeSpectralTransport_StatusLabel(snapshot->execution_status))) &&
        spectral_json_add(o,"schema",json_object_new_string("optic_v3_transport_v1")) &&
        spectral_json_add(o,"route",json_object_new_string(label)) &&
        spectral_json_add(o,"ordinary_indirect",json_object_new_boolean(false)) &&
        spectral_json_add(o,"explicit_legacy_optout",json_object_new_boolean(p->legacy_direct)) &&
        spectral_json_add(o,"photon_maps_used",json_object_new_boolean(p->map.enabled)) &&
        spectral_json_add(o,"estimator_partition",json_object_new_string(partition));
    if(!ok) {json_object_put(o);return NULL;}
    return o;
}
json_object *RuntimeSpectralTransport_JSON(const RuntimeNative3DPreparedFrame *frame,bool terminal) {
    RuntimeSpectralTransportSnapshot snapshot;
    return RuntimeSpectralTransport_Capture(frame,terminal,&snapshot)?RuntimeSpectralTransportSnapshot_JSON(&snapshot,true):NULL;
}
