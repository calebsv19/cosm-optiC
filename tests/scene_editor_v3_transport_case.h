#include "editor/scene_editor_spectral_profile.h"
#include "editor/scene_editor_camera_authoring.h"
#include "render/runtime_spectral_transport.h"

static json_object *v3_dispatch(const char *method,json_object *params,bool expected) {
    json_object *request=json_object_new_object();
    json_object_object_add(request,"method",json_object_new_string(method));
    json_object_object_add(request,"params",params);
    json_object *response=SceneEditorAgentControlDispatch(request);json_object_put(request);
    if(json_object_get_boolean(authoring_t1_member(response,"ok"))!=expected)
        fprintf(stderr,"V3 unexpected response: %s\n",json_object_to_json_string(response));
    assert(json_object_get_boolean(authoring_t1_member(response,"ok"))==expected);
    return response;
}
static json_object *v3_profile_set(json_object *profile,unsigned long long revision,bool ok) {
    json_object *p=json_object_new_object();
    json_object_object_add(p,"profile",json_object_get(profile));
    json_object_object_add(p,"expected_revision",json_object_new_int64(revision));
    return v3_dispatch("render.profile_set",p,ok);
}
static json_object *v3_job(const char *id,const char *state) {
    char message[512];
    json_object *job=SceneEditorRenderJobRead(id,true,message,sizeof(message));assert(job);
    assert(!strcmp(json_object_get_string(authoring_t1_member(job,"state")),state));
    const char *path=json_object_get_string(authoring_t1_member(job,"spectral_transport_path"));
    json_object *detail=json_object_from_file(path);assert(detail);
    const char *status=json_object_get_string(authoring_t1_member(detail,"status"));
    assert(!strcmp(status,state));
    assert(json_object_object_get_ex(detail,"film",NULL));
    json_object_put(detail);return job;
}
static void v3_transport_acceptance(SceneEditor *editor,bool reopen) {
    char message[512];
    if(reopen) {
        json_object *expected=json_object_from_file("v3-expected.json");assert(expected);
        const char *ids[]={"v3-final","v3-cancel","v3-ui","v3-legacy","v3-default","v3-mcp"};
        for(int i=0;i<6;++i) {
            const char *id=i==2?json_object_get_string(authoring_t1_member(expected,"ui_job_id")):ids[i];
            json_object *job=SceneEditorRenderJobRead(id,true,message,sizeof(message));assert(job);
            assert(json_object_equal(job,authoring_t1_member(expected,ids[i])));json_object_put(job);
        }
        json_object *read=SceneEditorSpectralProfileRead();assert(read);
        assert(!strcmp(json_object_get_string(authoring_t1_member(authoring_t1_member(read,"profile"),"quality_profile")),"surface_path_v1"));
        json_object_put(read);json_object_put(expected);
        assert(!SceneEditorDocumentIsDirty());
        puts("PASS ordinary V3 saved profile and durable jobs after fresh process reopen");return;
    }
    json_object *read=v3_dispatch("render.profile_read",json_object_new_object(),true);
    json_object *original=json_object_get(authoring_t1_member(authoring_t1_member(read,"spectral_profile"),"profile"));
    assert(json_object_object_get_ex(original,"surface_path_v1",NULL));
    json_object_put(read);
    json_object *legacy=json_tokener_parse("{\"quality_profile\":\"legacy_direct_v1\"}");
    unsigned long long revision=SceneEditorDocumentRevision();
    json_object_put(v3_profile_set(legacy,revision+1,false));
    json_object_put(v3_profile_set(legacy,revision,true));
    assert(SceneEditorDocumentUndo(message,sizeof(message)));
    assert(SceneEditorDocumentRedo(message,sizeof(message)));
    json_object_put(v3_profile_set(original,SceneEditorDocumentRevision(),true));
    const double camera[3]={0,-2.7,1.15};
    assert(SceneEditorDocumentSetStaticCameraPose(camera,180,-4.6354634269,SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(SceneEditorChromeActionsSaveAuthoring());
    read=SceneEditorSpectralProfileRead();
    assert(json_object_equal(original,authoring_t1_member(read,"profile")));json_object_put(read);
    /* Standard render.start envelope exercises the same route as ordinary jobs. */
    json_object *params=json_tokener_parse("{\"request_id\":\"v3-final\",\"width\":32,\"height\":24,\"integrator\":\"disney_v3_spectral\",\"temporal_frames\":1,\"frame\":0,\"camera_samples_override\":16}");
    json_object_object_add(params,"expected_revision",json_object_new_int64(SceneEditorDocumentRevision()));
    AnimationConfig before=animSettings;
    json_object_put(v3_dispatch("render.start",params,true));
    json_object_put(v3_profile_set(legacy,SceneEditorDocumentRevision(),false));
    render_mcp_wait(editor);
    json_object *expected=json_object_new_object(),*job=v3_job("v3-final","completed");
    assert(json_object_get_boolean(authoring_t1_member(job,"artifact_integrity")));
    json_object *components=authoring_t1_member(authoring_t1_member(job,"spectral_transport"),"film_components_xyz");
    assert(json_object_get_double(json_object_array_get_idx(authoring_t1_member(components,"total"),1))>0);
    assert(animSettings.integratorMode3D==before.integratorMode3D && animSettings.deepRenderMode==before.deepRenderMode);
    json_object_object_add(expected,"v3-final",job);
    SceneEditorRenderOptions options={.width=64,.height=48,.temporal_frames=1,
        .integrator=RAY_TRACING_3D_INTEGRATOR_DISNEY_V3_SPECTRAL,.spectral_camera_samples=8192};
    assert(SceneEditorRenderStart("v3-cancel",&options,SceneEditorDocumentRevision(),message,sizeof(message)));
    /* Cancellation after at least one committed strip proves in-flight polling. */
    bool progress=false;
    for(int i=0;i<15000 && SceneEditorRenderActive();++i) {
        SceneEditorRenderPoll();
        json_object *status=SceneEditorRenderJobRead("v3-cancel",false,message,sizeof(message));
        if(status) {progress=json_object_get_int(authoring_t1_member(status,"completed_tiles"))>0;json_object_put(status);}
        if(progress)break;SDL_Delay(2);
    }
    assert(progress && SceneEditorRenderActive());
    assert(SceneEditorRenderCancel("v3-cancel",message,sizeof(message)));render_mcp_wait(editor);
    job=v3_job("v3-cancel","canceled");
    assert(!json_object_get_boolean(authoring_t1_member(job,"artifact_integrity")));
    assert(access(json_object_get_string(authoring_t1_member(job,"image_path")),F_OK)!=0);
    json_object_object_add(expected,"v3-cancel",job);
    /* Native panel controls preserve the saved profile and expose camera samples. */
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);
    SDL_Rect control;
    if(!SceneEditorRenderPanelControl("width",&control))render_mcp_control(editor,"toggle");
    render_mcp_control(editor,"width");authoring_text(editor,"32");
    render_mcp_control(editor,"height");authoring_text(editor,"24");
    render_mcp_control(editor,"temporal_frames");authoring_text(editor,"16");
    render_mcp_control(editor,"start");char ui_id[64];snprintf(ui_id,sizeof(ui_id),"%s",SceneEditorRenderLatestId());
    render_mcp_wait(editor);render_mcp_control(editor,"results");capture(editor,"v3-results.ppm");
    job=v3_job(ui_id,"completed");json_object_object_add(expected,"v3-ui",job);
    /* Alias the generated UI id in the expected file for fresh-process verification. */
    json_object_object_add(expected,"ui_job_id",json_object_new_string(ui_id));
    json_object_put(v3_profile_set(legacy,SceneEditorDocumentRevision(),true));
    options.width=32;options.height=24;options.spectral_camera_samples=0;
    assert(SceneEditorRenderStart("v3-legacy",&options,SceneEditorDocumentRevision(),message,sizeof(message)));render_mcp_wait(editor);
    job=SceneEditorRenderJobRead("v3-legacy",true,message,sizeof(message));assert(job);
    assert(!strcmp(json_object_get_string(authoring_t1_member(authoring_t1_member(job,"spectral_transport"),"route")),"legacy_direct_v1"));
    json_object_object_add(expected,"v3-legacy",job);
    json_object *defaults=json_tokener_parse("{\"quality_profile\":\"surface_path_v1\"}");
    json_object_put(v3_profile_set(defaults,SceneEditorDocumentRevision(),true));json_object_put(defaults);
    options.spectral_camera_samples=16;
    assert(SceneEditorRenderStart("v3-default",&options,SceneEditorDocumentRevision(),message,sizeof(message)));render_mcp_wait(editor);
    job=v3_job("v3-default","completed");
    json_object *sources=authoring_t1_member(authoring_t1_member(job,"spectral_transport"),"sources");
    assert(json_object_array_length(sources)>0);
    assert(!strcmp(json_object_get_string(authoring_t1_member(json_object_array_get_idx(sources,0),"id")),"compat_scene_light"));
    json_object_object_add(expected,"v3-default",job);
    json_object_put(v3_profile_set(original,SceneEditorDocumentRevision(),true));
    const char *script=getenv("OPTIC_V3_MCP_TEST");assert(script);
    char *args[]={"python3","-B",(char*)script,(char*)SceneEditorDocumentPath(),NULL};
    pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool done=false;
    for(int i=0;i<6000;++i) {
        SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
        if(waitpid(pid,&status,WNOHANG)==pid) {done=true;break;}SDL_Delay(5);
    }
    if(!done) {kill(pid,SIGTERM);waitpid(pid,&status,0);}
    assert(done && WIFEXITED(status) && WEXITSTATUS(status)==0);
    job=v3_job("v3-mcp","completed");json_object_object_add(expected,"v3-mcp",job);
    assert(SceneEditorChromeActionsSaveAuthoring());
    assert(json_object_to_file("v3-expected.json",expected)==0);
    json_object_put(expected);json_object_put(original);json_object_put(legacy);
    puts("PASS ordinary V3 native profile transactions, standard jobs, UI, in-flight cancel, owned reports and rollback");
}
