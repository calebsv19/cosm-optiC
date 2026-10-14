#include "editor/scene_editor_command_guard.h"
/* Editor/UI command owner, using the existing native desktop render lifecycle. */
#include "editor/scene_editor_render_control.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "scene_editor_document_transaction.h"
#include "app/ray_tracing_deep_render_desktop_render_internal.h"
#include "app/ray_tracing_deep_render_completion.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include "app/ray_tracing_durable_io.h"
#include "app/ray_tracing_sha256.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static SceneEditorRenderOptions options={.width=320,.height=200,.temporal_frames=1,.frame=0,.integrator=RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT};
static struct {
    RuntimeNative3DAsyncRenderJob* job;
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderDesktopRenderUnit unit;
    RayTracingDeepRenderListener listener;
    AnimationConfig saved_config;
    bool config_owned,active,cancel_requested,verified;
    unsigned long long scene_revision;
    uint64_t generation;
    uint32_t sample_sequence;
    char id[64],root[4096],image[4096],receipt[4096],scene[4096],source_hash[65],image_hash[65],diagnostic[256],state[32];
    uint64_t primitive_count,triangle_count,enabled_lights;
    bool volume_enabled,volume_attached;
    int completed_subpasses,total_subpasses;
    size_t completed_tiles,total_tiles;
} render;
static uint64_t generation;
static json_object* get(json_object* p,const char* key) {json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;}
static bool refuse(char* message,size_t size,const char* text) {if(message && size) snprintf(message,size,"%s",text);return false;}
static bool valid_id(const char* id) {
    if(!id || !*id || strlen(id)>63) return false;
    for(const char* p=id;*p;++p) if(!isalnum((unsigned char)*p) && *p!='-' && *p!='_') return false;return true;
}
static bool directory(const char* path,bool create) {
    struct stat st;if(create && mkdir(path,0700)!=0 && errno!=EEXIST) return false;
    return lstat(path,&st)==0 && S_ISDIR(st.st_mode);
}
static bool path_for(const char* id,char* root,size_t size) {
    const char* scene=SceneEditorDocumentPath();char parent[4096];
    if(!valid_id(id) || !scene || strlen(scene)>=sizeof(parent)) return false;strcpy(parent,scene);char* slash=strrchr(parent,'/');if(!slash || slash==parent) return false;*slash=0;
    return snprintf(root,size,"%s/.optic-renders/%s",parent,id)<(int)size;
}
bool SceneEditorRenderOptionsValid(const SceneEditorRenderOptions* o) {
    return o && o->width>=16 && o->width<=2048 && o->height>=16 && o->height<=2048 && o->temporal_frames>=1 && o->temporal_frames<=32 && o->frame>=0 && o->frame<=1000000000 &&
        (o->integrator==RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT || o->integrator==RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) && (o->integrator!=RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT || o->temporal_frames==1);
}
SceneEditorRenderOptions SceneEditorRenderOptionsGet(void) {return options;}
const char* SceneEditorRenderLatestId(void) {return !strcmp(render.scene,SceneEditorDocumentPath())?render.id:"";}
bool SceneEditorRenderActive(void) {return render.active;}
static json_object* options_json(void) {
    json_object* o=json_object_new_object();json_object_object_add(o,"width",json_object_new_int(options.width));json_object_object_add(o,"height",json_object_new_int(options.height));json_object_object_add(o,"temporal_frames",json_object_new_int(options.temporal_frames));json_object_object_add(o,"frame",json_object_new_int(options.frame));json_object_object_add(o,"integrator",json_object_new_string(options.integrator==0?"direct_light":"disney_v2"));if(options.sample_sequence)json_object_object_add(o,"sample_sequence",json_object_new_int64(options.sample_sequence));return o;
}
static json_object* receipt(void) {
    json_object* o=json_object_new_object();json_object_object_add(o,"schema_version",json_object_new_string("optic_editor_render_v1"));json_object_object_add(o,"job_id",json_object_new_string(render.id));json_object_object_add(o,"state",json_object_new_string(render.state));json_object_object_add(o,"editor_pid",json_object_new_int(getpid()));json_object_object_add(o,"scene_path",json_object_new_string(render.scene));json_object_object_add(o,"scene_revision",json_object_new_int64(render.scene_revision));json_object_object_add(o,"source_sha256",json_object_new_string(render.source_hash));json_object_object_add(o,"settings",options_json());json_object_object_add(o,"sample_sequence",json_object_new_int64(render.sample_sequence));json_object_object_add(o,"primitive_count",json_object_new_int64(render.primitive_count));json_object_object_add(o,"triangle_count",json_object_new_int64(render.triangle_count));json_object_object_add(o,"enabled_lights",json_object_new_int64(render.enabled_lights));json_object_object_add(o,"volume_enabled",json_object_new_boolean(render.volume_enabled));json_object_object_add(o,"volume_attached",json_object_new_boolean(render.volume_attached));json_object_object_add(o,"diagnostic",json_object_new_string(render.diagnostic));json_object_object_add(o,"cancel_requested",json_object_new_boolean(render.cancel_requested));json_object_object_add(o,"completed_subpasses",json_object_new_int(render.completed_subpasses));json_object_object_add(o,"total_subpasses",json_object_new_int(render.total_subpasses));json_object_object_add(o,"completed_tiles",json_object_new_int64(render.completed_tiles));json_object_object_add(o,"total_tiles",json_object_new_int64(render.total_tiles));json_object_object_add(o,"output_verified",json_object_new_boolean(render.verified));json_object_object_add(o,"image_path",json_object_new_string(render.image));json_object_object_add(o,"image_sha256",json_object_new_string(render.image_hash));return o;
}
static bool write_json(const char* path,json_object* o) {
    RayTracingDurableOutput out;bool ok=ray_tracing_durable_output_begin(&out,path);if(ok) {ok=fputs(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PRETTY),out.stream)>=0;if(ok) ok=ray_tracing_durable_output_commit(&out);else ray_tracing_durable_output_abort(&out);}return ok;
}
static bool persist(void) {json_object* o=receipt();bool ok=write_json(render.receipt,o);if(!ok) snprintf(render.diagnostic,sizeof(render.diagnostic),"Render receipt publication failed.");json_object_put(o);return ok;}
static void release(void) {
    RuntimeNative3DAsyncRenderJob_Destroy(render.job);render.job=NULL;
    RayTracingDeepRenderSession_Reset(&render.session);RayTracingDeepRenderDesktopRenderUnit_Destroy(&render.unit);RayTracingDeepRenderListener_Destroy(&render.listener);
    if(render.config_owned) {animSettings=render.saved_config;render.config_owned=false;}
    render.active=false;
}
bool SceneEditorRenderStart(const char* id,const SceneEditorRenderOptions* o,unsigned long long revision,char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision() || !valid_id(id) || !SceneEditorRenderOptionsValid(o)) return refuse(message,size,"Invalid render identity/settings or stale scene revision.");
    if(render.active || RayTracingDeepRenderDesktopHost_HasActiveWork()) return refuse(message,size,"A native render already owns the exclusive render context.");
    if(animSettings.spaceMode!=SPACE_MODE_3D || animSettings.causticMode3D!=0) return refuse(message,size,"Editor frame render requires native 3D with caustics off.");
    if(SceneEditorCommandInteractionActive()) return refuse(message,size,"Finish or cancel the active editor operation before rendering.");
    char root[4096],parent[4096];if(!path_for(id,root,sizeof(root))) return false;strcpy(parent,root);*strrchr(parent,'/')=0;
    if(!directory(parent,true)) return refuse(message,size,"Cannot create contained render root (symlinks are refused).");
    if(mkdir(root,0700)!=0) return refuse(message,size,"Render ID already exists or output is unavailable; inspect that ID rather than resubmitting.");
    release();memset(&render,0,sizeof(render));options=*o;render.generation=++generation;
    strcpy(render.id,id);strcpy(render.root,root);snprintf(render.image,sizeof(render.image),"%s/frame.bmp",root);snprintf(render.receipt,sizeof(render.receipt),"%s/job.json",root);snprintf(render.scene,sizeof(render.scene),"%s",SceneEditorDocumentPath());render.scene_revision=revision;strcpy(render.state,"preparing");
    char source[4096];snprintf(source,sizeof(source),"%s/source_snapshot.json",root);json_object* scene=document_authoring_root();const char* bytes=json_object_to_json_string_ext(scene,JSON_C_TO_STRING_PRETTY);
    if(!ray_tracing_sha256_bytes(bytes,strlen(bytes),render.source_hash) || !write_json(source,scene)) {strcpy(render.state,"failed");strcpy(render.diagnostic,"Source snapshot publication failed.");persist();return refuse(message,size,render.diagnostic);}
    SceneEditorTimelinePause();render.saved_config=animSettings;render.config_owned=true;
    animSettings.interactiveMode=false;animSettings.deepRenderMode=true;animSettings.useTiledRenderer=true;animSettings.tilePreviewEnabled=false;animSettings.renderScale3D=1;animSettings.integratorMode3D=o->integrator;animSettings.temporalFrames3D=o->temporal_frames;
    RayTracingDeepRenderSession_Init(&render.session);RayTracingDeepRenderListener_Init(&render.listener);RayTracingDeepRenderDesktopRenderUnit_Init(&render.unit);render.job=RuntimeNative3DAsyncRenderJob_Create();
    RayTracingDeepRenderSessionDesc session={.startFrameIndex=o->frame,.frameCount=1,.initialGeneration=render.generation};
    RayTracingDeepRenderDesktopStartDesc desc={.generation=render.generation,.fixedSampleSequence=o->sample_sequence,.absoluteFrameIndex=o->frame,.frameCount=1,.outputWidth=o->width,.outputHeight=o->height,.outputRoot=root,.frameDirectory=root,.finalFramePath=render.image};
    const char* reason="Native render allocation/session failed.";
    if(!render.job || !RayTracingDeepRenderSession_Begin(&render.session,&session) || RayTracingDeepRenderDesktopRenderUnit_Start(&render.unit,&render.session,render.job,&desc,&reason)!=RAY_TRACING_DEEP_RENDER_DESKTOP_START_READY) {
        snprintf(render.diagnostic,sizeof(render.diagnostic),"%s",reason);strcpy(render.state,"failed");release();persist();return refuse(message,size,render.diagnostic);
    }
    const RuntimeNative3DRenderRequestSnapshot* snapshot=&render.session.frameRequest.renderSnapshot;
    render.sample_sequence=snapshot->sampling.sampleSequence;
    render.primitive_count=snapshot->preparedPrimitiveCount;render.triangle_count=snapshot->preparedTriangleCount;render.enabled_lights=snapshot->enabledLightCount;render.volume_enabled=snapshot->volumeEnabled;render.volume_attached=snapshot->volumeAttached;
    render.active=true;render.total_subpasses=render.unit.temporalFrames;strcpy(render.state,"rendering");if(!persist()) {RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(render.job);release();strcpy(render.state,"failed");return refuse(message,size,render.diagnostic);}return true;
}
bool SceneEditorRenderCancel(const char* id,char* message,size_t size) {
    if(!render.active || !valid_id(id) || strcmp(id,render.id)) return refuse(message,size,"Only the current active render ID can be canceled.");
    render.cancel_requested=true;strcpy(render.state,"canceling");RuntimeNative3DAsyncRenderJob_RequestCancel(render.job);persist();return true;
}
void SceneEditorRenderPoll(void) {
    if(!render.active) return;
    RuntimeNative3DAsyncRenderTileProgressSnapshot progress;
    if(RuntimeNative3DAsyncRenderProgressBuffer_CopyLatestTileProgress(render.unit.progress,render.generation,&progress) && progress.valid) {
        render.completed_subpasses=progress.completedSubpasses;render.total_subpasses=progress.totalSubpasses;render.completed_tiles=progress.completedTilesInSubpass;render.total_tiles=progress.totalTilesInSubpass;
    }
    RayTracingDeepRenderListenerPollResult poll;
    if(!RayTracingDeepRenderListener_Poll(&render.listener,&render.session,render.unit.progress,render.job,NULL,NULL,&poll)) {
        strcpy(render.diagnostic,"Native progress listener failed.");strcpy(render.state,"failed");RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(render.job);release();persist();return;
    }
    if(!poll.terminalObserved) return;
    RuntimeNative3DAsyncRenderJob_Join(render.job);
    if(render.cancel_requested || poll.jobResult.canceled || poll.jobResult.cancelRequested) {strcpy(render.state,"canceled");strcpy(render.diagnostic,"Canceled; no final image published.");}
    else {
        RayTracingDeepRenderPresentationView view=RayTracingDeepRenderListener_GetView(&render.listener);RayTracingDeepRenderCompletionResult result;
        bool ok=RayTracingDeepRenderCompletion_Process(&render.session,render.job,&poll,&view,NULL,&result);
        render.verified=ok && result.outputVerified && result.sessionCompleted && ray_tracing_sha256_file(render.image,render.image_hash);
        render.completed_subpasses=(int)poll.jobResult.completionValue;
        strcpy(render.state,render.verified?"completed":"failed");snprintf(render.diagnostic,sizeof(render.diagnostic),"%s",RayTracingDeepRenderCompletionStatus_Name(result.status));
    }
    release();if(!persist()) {strcpy(render.state,"failed");render.verified=false;}
}
void SceneEditorRenderShutdown(void) {
    SceneEditorRenderPanelReset();
    if(render.active) {RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(render.job);render.cancel_requested=true;strcpy(render.state,"canceled");strcpy(render.diagnostic,"Editor session closed; render canceled.");release();persist();}
    else release();
}
json_object* SceneEditorRenderRead(void) {
    SceneEditorRenderPoll();json_object* o=json_object_new_object();json_object_object_add(o,"fixed_sampling_supported",json_object_new_boolean(true));json_object_object_add(o,"settings",options_json());json_object_object_add(o,"active",json_object_new_boolean(render.active));json_object_object_add(o,"latest_job_id",json_object_new_string(SceneEditorRenderLatestId()));return o;
}
json_object* SceneEditorRenderJobRead(const char* id,bool results,char* message,size_t size) {
    SceneEditorRenderPoll();char root[4096],path[4096];struct stat st;
    if(!path_for(id,root,sizeof(root))) {refuse(message,size,"Invalid render ID.");return NULL;}
    char parent[4096];strcpy(parent,root);*strrchr(parent,'/')=0;
    if(!directory(parent,false) || !directory(root,false)) {refuse(message,size,"Render ID is unavailable.");return NULL;}
    snprintf(path,sizeof(path),"%s/job.json",root);json_object* o=NULL;
    if(!strcmp(id,render.id) && !strcmp(render.scene,SceneEditorDocumentPath())) o=receipt();else if(lstat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<65536) o=json_object_from_file(path);
    if(!o || !get(o,"scene_path") || strcmp(json_object_get_string(get(o,"scene_path")),SceneEditorDocumentPath()) || !get(o,"job_id") || strcmp(json_object_get_string(get(o,"job_id")),id)) {if(o) json_object_put(o);refuse(message,size,"Render receipt identity is invalid.");return NULL;}
    if(!json_object_is_type(get(o,"state"),json_type_string) || !json_object_is_type(get(o,"image_sha256"),json_type_string) || !json_object_is_type(get(o,"settings"),json_type_object)) {json_object_put(o);refuse(message,size,"Render receipt fields are invalid.");return NULL;}
    const char* state=json_object_get_string(get(o,"state"));
    if((!strcmp(state,"rendering") || !strcmp(state,"preparing") || !strcmp(state,"canceling")) && (!render.active || strcmp(render.id,id))) {
        json_object_object_add(o,"state",json_object_new_string("interrupted"));json_object_object_add(o,"diagnostic",json_object_new_string("No render in this editor session owns this receipt; inspect before a new request."));
    }
    if(results) {
        snprintf(path,sizeof(path),"%s/frame.bmp",root);json_object* settings=get(o,"settings");char sha[65];bool verified=!strcmp(json_object_get_string(get(o,"state")),"completed") && json_object_get_boolean(get(o,"output_verified")) && RayTracingDeepRenderCompletion_VerifyFrameBMP(path,json_object_get_int(get(settings,"width")),json_object_get_int(get(settings,"height")),NULL) && ray_tracing_sha256_file(path,sha) && !strcmp(sha,json_object_get_string(get(o,"image_sha256")));
        json_object_object_add(o,"artifact_integrity",json_object_new_boolean(verified));json_object_object_add(o,"image_path",json_object_new_string(path));
    }
    return o;
}
