#include "editor/scene_editor_render_control.h"
#include "editor/scene_editor_lifecycle.h"
static void render_mcp_control(SceneEditor* editor,const char* name) {SDL_Rect rect;assert(SceneEditorRenderPanelControl(name,&rect));click(editor,rect);SceneEditorSessionRuntimeRender(editor);}
static void render_mcp_wait(SceneEditor* editor) {
    for(int i=0;i<4000 && SceneEditorRenderActive();++i) {SceneEditorSessionRuntimeRender(editor);SDL_Delay(10);}assert(!SceneEditorRenderActive());
}
static void render_mcp_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    char message[512];
    if(reopen) {
        json_object* expected=json_object_from_file("render-mcp-expected.json");assert(expected);
        const char* keys[]={"direct","disney","ui","canceled","dirty","shutdown","failed"};
        for(int i=0;i<7;++i) {json_object* r=authoring_t1_member(expected,keys[i]);const char* id=json_object_get_string(authoring_t1_member(r,"job_id"));json_object* fresh=SceneEditorRenderJobRead(id,true,message,sizeof(message));assert(fresh && json_object_equal(fresh,r));json_object_put(fresh);}
        json_object_put(expected);assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Render durable result fresh-process reopen PASS\n");return;
    }
    unsigned long long revision=SceneEditorDocumentRevision();AnimationConfig original=animSettings;
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);render_mcp_control(editor,"toggle");
    render_mcp_control(editor,"width");assert(SceneEditorRenderPanelDraftActive());
    json_object* request=json_tokener_parse("{\"method\":\"render.start\",\"params\":{\"request_id\":\"draft-refuse\",\"width\":64,\"height\":40,\"integrator\":\"direct_light\",\"temporal_frames\":1,\"frame\":0}}");json_object_object_add(authoring_t1_member(request,"params"),"expected_revision",json_object_new_int64(revision));json_object* response=SceneEditorAgentControlDispatch(request);assert(!json_object_get_boolean(authoring_t1_member(response,"ok")));json_object_put(response);json_object_put(request);
    authoring_text(editor,"64");render_mcp_control(editor,"height");authoring_text(editor,"40");render_mcp_control(editor,"start");assert(*SceneEditorRenderLatestId());strcpy(message,SceneEditorRenderLatestId());render_mcp_wait(editor);render_mcp_control(editor,"results");assert(SceneEditorRenderPanelResultVerified());capture(editor,"render-results-ui.ppm");
    json_object* ui=SceneEditorRenderJobRead(message,true,message,sizeof(message));assert(ui && json_object_get_boolean(authoring_t1_member(ui,"artifact_integrity")));assert(json_object_to_file("render-ui-expected.json",ui)==0);json_object_put(ui);
    assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
    #define RENDER_CONFIG_SAME(field) assert(animSettings.field==original.field)
    RENDER_CONFIG_SAME(interactiveMode);RENDER_CONFIG_SAME(deepRenderMode);RENDER_CONFIG_SAME(useTiledRenderer);RENDER_CONFIG_SAME(renderScale3D);RENDER_CONFIG_SAME(integratorMode3D);RENDER_CONFIG_SAME(temporalFrames3D);
    #undef RENDER_CONFIG_SAME
    const char* script=getenv("OPTIC_RENDER_MCP_TEST");assert(script);char* args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<6000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);}if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    /* The actual Cancel button uses the same ID and cancellation command. */
    SceneEditorRenderOptions cancel_options={1024,768,32,0,5};assert(SceneEditorRenderStart("ui-cancel",&cancel_options,SceneEditorDocumentRevision(),message,sizeof(message)));SceneEditorSessionRuntimeRender(editor);render_mcp_control(editor,"cancel");render_mcp_wait(editor);ui=SceneEditorRenderJobRead("ui-cancel",true,message,sizeof(message));assert(ui && !strcmp(json_object_get_string(authoring_t1_member(ui,"state")),"canceled") && !json_object_get_boolean(authoring_t1_member(ui,"artifact_integrity")));json_object_put(ui);
    assert(SceneEditorDocumentIsDirty());
    assert(SceneEditorRenderStart("shutdown-cancel",&cancel_options,SceneEditorDocumentRevision(),message,sizeof(message)));
    SceneEditorInputRouterCallbacks callbacks=SceneEditorBuildInputRouterCallbacks(editor);int held_mode=editor->currentMode;
    SceneEditorChromeAction action={.kind=SCENE_EDITOR_CHROME_ACTION_MODE_SELECT,.mode_index=held_mode==EDITOR_MODE_CAMERA?EDITOR_MODE_OBJECT:EDITOR_MODE_CAMERA};callbacks.apply_chrome_action(editor,&action);assert(editor->currentMode==held_mode && SceneEditorRenderActive());
    unsigned long long held_revision=SceneEditorDocumentRevision();SDL_Event pane_event={0};pane_event.type=SDL_KEYDOWN;pane_event.key.keysym.sym=SDLK_t;
    SceneEditorPaneCommand command={.target=SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE,.kind=SCENE_EDITOR_PANE_COMMAND_KEY,.event=&pane_event};SceneEditorInputRoutingResult routed={0};callbacks.route_pane_event(editor,&command,&routed);assert(SceneEditorDocumentRevision()==held_revision && SceneEditorRenderActive());
    SDL_Event closing={0};closing.type=SDL_WINDOWEVENT;closing.window.event=SDL_WINDOWEVENT_CLOSE;closing.window.windowID=SDL_GetWindowID(editor->window);SceneEditorSessionRuntimeHandleEvent(editor,&closing);
    assert(!SceneEditorRenderActive() && SceneEditorLifecycleClosePending() && editor->running && SceneEditorDocumentIsDirty());
    SDL_Event escape={0};escape.type=SDL_KEYDOWN;escape.key.keysym.sym=SDLK_ESCAPE;SceneEditorSessionRuntimeHandleEvent(editor,&escape);assert(!SceneEditorLifecycleClosePending() && editor->running && SceneEditorDocumentIsDirty());
    ui=SceneEditorRenderJobRead("shutdown-cancel",true,message,sizeof(message));assert(ui && !strcmp(json_object_get_string(authoring_t1_member(ui,"state")),"canceled") && !json_object_get_boolean(authoring_t1_member(ui,"artifact_integrity")));
    json_object* expected=json_object_from_file("render-mcp-expected.json");assert(expected);json_object_object_add(expected,"shutdown",ui);assert(json_object_to_file("render-mcp-expected.json",expected)==0);json_object_put(expected);
    assert(SceneEditorChromeActionsSaveAuthoring());capture(editor,"render-mcp-ui.ppm");fprintf(stderr,"Render native UI/MCP start/status/cancel/results and scene preservation PASS\n");
}
