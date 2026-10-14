/* Actual native input -> retained commands -> real stdio MCP -> fresh process. */
#include "editor/scene_editor_agent_animation.h"
#include "editor/scene_editor_render_control.h"
static json_object* animation_mcp_read(void) {
    json_object* p=json_object_new_object(),*r=json_object_new_object();char msg[256]="";
    assert(SceneEditorAgentAnimationDispatch("animation.read",p,r,msg,sizeof(msg)));json_object_put(p);
    json_object* v=json_object_get(authoring_t1_member(r,"animation"));json_object_put(r);return v;
}
static void animation_mcp_trial_refuse(void) {
    json_object* r=json_tokener_parse("{\"method\":\"animation.activate\",\"params\":{}}");
    unsigned long long rev=SceneEditorDocumentRevision();json_object_object_add(authoring_t1_member(r,"params"),"expected_revision",json_object_new_int64(rev));
    json_object* a=SceneEditorAgentControlDispatch(r);assert(!json_object_get_boolean(authoring_t1_member(a,"ok")) && SceneEditorDocumentRevision()==rev);json_object_put(a);json_object_put(r);
}
static void animation_mcp_run(SceneEditor* editor,const char* scene,const char* mode) {
    char* args[]={"python3","-B",(char*)getenv("OPTIC_ANIMATION_MCP_TEST"),(char*)scene,(char*)mode,NULL};assert(args[2]);pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<6000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}
    if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void animation_mcp_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    char msg[512];
    if(reopen) {animation_mcp_run(editor,scene,"reopen");assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Animation fresh process timeline/frame render parity PASS\n");return;}
    unsigned char legacy[160*100*4],restored[160*100*4];camera_mcp_pixels(legacy);
    json_object* initial=animation_mcp_read();assert(!json_object_get_boolean(authoring_t1_member(initial,"present")));json_object_put(initial);assert(!SceneEditorDocumentIsDirty());
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"animation");authoring_control(editor,"setup");
    assert(SceneEditorDocumentIsDirty());json_object* ui=animation_mcp_read();assert(json_object_get_boolean(authoring_t1_member(ui,"valid")));json_object_put(ui);
    assert(SceneEditorDocumentUndo(msg,sizeof(msg)));camera_mcp_pixels(restored);assert(!memcmp(legacy,restored,sizeof(legacy)));
    initial=animation_mcp_read();assert(!json_object_get_boolean(authoring_t1_member(initial,"present")));json_object_put(initial);
    assert(SceneEditorDocumentRedo(msg,sizeof(msg)));
    static TimelineDocument d;assert(SceneEditorDocumentGetTimeline(&d)==TIMELINE_STATUS_OK);size_t lens=SIZE_MAX;
    for(size_t i=0;i<d.track_count;++i) if(!strcmp(d.tracks[i].property_id,"camera/fov_y")) lens=i;
    assert(lens!=SIZE_MAX && SceneEditorTimelineSelectTrack(lens));assert(SceneEditorTimelineSelectKey(0,false));SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"key_value");assert(SceneEditorTimelineInteractionActive());animation_mcp_trial_refuse();authoring_text(editor,"60");assert(!SceneEditorTimelineInteractionActive());
    assert(SceneEditorDocumentGetTimeline(&d)==TIMELINE_STATUS_OK && d.tracks[lens].keys[0].value.as.scalar==60);
    json_object* expected=SceneTimelineDocumentToJson(&d);assert(json_object_to_file("animation-ui-expected.json",expected)==0);json_object_put(expected);
    animation_mcp_run(editor,scene,"edit");
    assert(SceneEditorDocumentGetTimeline(&d)==TIMELINE_STATUS_OK);expected=SceneTimelineDocumentToJson(&d);json_object* saved=json_object_from_file("animation-mcp-expected.json");assert(saved && json_object_equal(expected,saved));json_object_put(expected);
    /* MCP key is immediately editable in the same UI; Undo restores MCP state. */
    assert(SceneEditorTimelineSelectTrack(lens) && SceneEditorTimelineSelectKey(0,false));SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"key_value");authoring_text(editor,"65");assert(SceneEditorDocumentUndo(msg,sizeof(msg)));
    assert(SceneEditorDocumentGetTimeline(&d)==TIMELINE_STATUS_OK);expected=SceneTimelineDocumentToJson(&d);assert(json_object_equal(expected,saved));json_object_put(expected);json_object_put(saved);
    assert(SceneEditorChromeActionsSaveAuthoring());capture(editor,"animation-mcp-ui.ppm");fprintf(stderr,"Animation UI/MCP channels/key inspector/history/legacy/render PASS\n");
}
