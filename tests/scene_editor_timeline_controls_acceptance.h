#include "editor/scene_editor_timeline_manage.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "editor/scene_editor_document_transaction.h"
static json_object* timeline_controls_raw(void) {
    json_object* a=authoring_t1_member(authoring_t1_member(authoring_t1_member(document_authoring_root(),"extensions"),"ray_tracing"),"authoring");
    json_object* copy=NULL;assert(json_object_deep_copy(authoring_t1_member(a,"scene_timeline"),&copy,NULL)==0);return copy;
}
static void timeline_controls_run(SceneEditor* editor,const char* scene,const char* mode) {
    char* args[]={"python3","-B",(char*)getenv("OPTIC_TIMELINE_CONTROLS_TEST"),(char*)scene,(char*)mode,NULL};assert(args[2]);pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<6000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}
    if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void timeline_controls_oracle(bool reopen) {
    AnimationConfig original=animSettings;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    static RayEvaluatedSceneServiceResult evaluated;assert(RayEvaluatedSceneCaptureSample((TimelineSample){24,0,1},&evaluated));
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame);RuntimeNative3DSamplingContext sampling={.sampleSequence=123};
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(frame,64,40,&evaluated.snapshot,&sampling));
    assert(evaluated.snapshot.frame.rate.frames_per_second_numerator==30000 && evaluated.snapshot.frame.rate.frames_per_second_denominator==1001);
    assert(evaluated.snapshot.frame.range.start_frame==-5 && evaluated.snapshot.frame.range.frame_count==106);
    unsigned char pixels[64*40*4]={0},expected[64*40*4];RuntimeNative3DRenderStats stats;
    for(int integrator=0;integrator<2;++integrator) {
        assert(RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(pixels,integrator?RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,integrator?2:1,NULL,NULL,NULL,NULL,NULL,NULL,&stats));
        const char* path=integrator?"disney-fixed-sampling.rgba":"direct-fixed-sampling.rgba";FILE* f=fopen(path,reopen?"rb":"wb");assert(f);
        if(reopen) {assert(fread(expected,1,sizeof(expected),f)==sizeof(expected));assert(!memcmp(pixels,expected,sizeof(expected)));}
        else assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
    }
    RuntimeNative3DPreparedFrame_Free(frame);free(frame);animSettings=original;
}
static void timeline_controls_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    char msg[512];if(reopen) {timeline_controls_run(editor,scene,"reopen");timeline_controls_oracle(true);assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Timeline controls fresh process metadata, clock, channels and render parity PASS\n");return;}
    json_object* legacy=timeline_controls_raw();unsigned char before[160*100*4],after[160*100*4];camera_mcp_pixels(before);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"animation");authoring_control(editor,"timeline_settings");
    authoring_control(editor,"clock_numerator");animation_mcp_trial_refuse();authoring_text(editor,"30000");
    authoring_control(editor,"clock_denominator");authoring_text(editor,"1001");
    static TimelineDocument doc;assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.rate.frames_per_second_numerator==30000 && doc.rate.frames_per_second_denominator==1001);
    assert(SceneEditorDocumentUndo(msg,sizeof(msg)) && SceneEditorDocumentUndo(msg,sizeof(msg)));json_object* raw=timeline_controls_raw();assert(json_object_equal(legacy,raw));json_object_put(raw);camera_mcp_pixels(after);assert(!memcmp(before,after,sizeof(before)));
    assert(SceneEditorDocumentRedo(msg,sizeof(msg)) && SceneEditorDocumentRedo(msg,sizeof(msg)));
    /* An external command between draw and click cannot invert a stale label. */
    SDL_Rect stale;assert(SceneEditorRenderAuthoringControl("channel_enabled",&stale));
    assert(SceneEditorTimelineSetClock((TimelineRate){60000,1001},doc.range,SceneEditorDocumentRevision(),msg,sizeof(msg)));
    unsigned long long changed=SceneEditorDocumentRevision();click(editor,stale);assert(SceneEditorDocumentRevision()==changed);
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[0].enabled);assert(SceneEditorDocumentUndo(msg,sizeof(msg)));SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"channel_next");authoring_control(editor,"channel_enabled");assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && !doc.tracks[1].enabled);
    raw=timeline_controls_raw();authoring_control(editor,"channel_remove");assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.track_count==7);
    assert(SceneEditorDocumentUndo(msg,sizeof(msg)));json_object* restored=timeline_controls_raw();assert(json_object_equal(raw,restored));json_object_put(restored);json_object_put(raw);
    raw=timeline_controls_raw();assert(json_object_to_file("timeline-controls-ui-expected.json",raw)==0);json_object_put(raw);json_object_put(legacy);
    timeline_controls_run(editor,scene,"edit");
    /* MCP rate is immediately editable through UI, and shared Undo is exact. */
    raw=timeline_controls_raw();authoring_control(editor,"clock_numerator");authoring_text(editor,"48");assert(SceneEditorDocumentUndo(msg,sizeof(msg)));restored=timeline_controls_raw();assert(json_object_equal(raw,restored));json_object_put(raw);json_object_put(restored);
    assert(SceneEditorChromeActionsSaveAuthoring());timeline_controls_oracle(false);SDL_Rect close;if(SceneEditorRenderPanelControl("close",&close)) click(editor,close);capture(editor,"timeline-controls-ui.ppm");fprintf(stderr,"Timeline controls UI/MCP rational clock, grouped channel commands, retained metadata/history PASS\n");
}
