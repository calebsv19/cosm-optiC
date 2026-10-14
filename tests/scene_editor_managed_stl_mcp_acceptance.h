#include "editor/scene_editor_managed_import.h"
#include "editor/scene_editor_agent_mesh.h"
static json_object *mesh_test_read(const char *id) {
    json_object *p=json_object_new_object(),*r=json_object_new_object();char msg[512];
    json_object_object_add(p,"object_id",json_object_new_string(id));
    assert(SceneEditorAgentMeshDispatch("mesh.read",p,r,msg,sizeof(msg)));json_object_put(p);return r;
}
static void mesh_mcp_acceptance(SceneEditor *editor,const char *scene,bool reopen) {
    char diagnostic[512];SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    if(reopen) {
        json_object *expected=json_object_from_file("mesh-expected.json");assert(expected);
        const char *id=json_object_get_string(authoring_t1_member(authoring_t1_member(expected,"mesh"),"object_id"));
        json_object *actual=mesh_test_read(id);assert(json_object_equal(expected,actual));json_object_put(actual);json_object_put(expected);
        json_object *pixels=json_object_from_file("mesh-reopen-render.json");assert(pixels);
        unsigned char rgba[160*100*4];material_mcp_pixels(rgba);
        FILE *f=fopen("mesh-saved.rgba","rb");assert(f);unsigned char saved[sizeof(rgba)];assert(fread(saved,1,sizeof(saved),f)==sizeof(saved));fclose(f);assert(!memcmp(saved,rgba,sizeof(saved)));
        json_object_put(pixels);assert(!SceneEditorDocumentIsDirty());return;
    }
    unsigned char before[160*100*4],after[sizeof(before)];material_mcp_pixels(before);
    char source[4096],cwd[4096];assert(getcwd(cwd,sizeof(cwd)));snprintf(source,sizeof(source),"%s/mesh.stl",cwd);
    assert(SceneEditorTransformPanelStageImportSTL(source));SceneEditorSessionRuntimeRender(editor);
    json_object *blocked=json_tokener_parse("{\"method\":\"mesh.import\",\"params\":{}}");
    json_object_object_add(authoring_t1_member(blocked,"params"),"expected_revision",json_object_new_int64(SceneEditorDocumentRevision()));
    json_object *refusal=SceneEditorAgentControlDispatch(blocked);assert(!json_object_get_boolean(authoring_t1_member(refusal,"ok")));
    assert(strstr(json_object_get_string(authoring_t1_member(refusal,"message")),"active editor operation"));json_object_put(blocked);json_object_put(refusal);
    SDL_Rect rect;assert(SceneEditorTransformPanelImportControl("millimeters",&rect));
    SDL_Event event={0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=rect.x+rect.w/2;event.button.y=rect.y+rect.h/2;
    assert(SceneEditorTransformPanelImportHandleEvent(&event));assert(SceneEditorTransformPanelImportControl("apply",&rect));event.button.x=rect.x+rect.w/2;event.button.y=rect.y+rect.h/2;assert(SceneEditorTransformPanelImportHandleEvent(&event));
    char id[65];snprintf(id,sizeof(id),"%s",SceneEditorManagedImportLatestId());assert(id[0]);
    bool done=false;for(int i=0;i<6000;i++) {
        SceneEditorTransformPanelPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
        json_object *r=json_object_new_object();assert(SceneEditorManagedImportRead(id,r,diagnostic,sizeof(diagnostic)));
        const char *state=json_object_get_string(authoring_t1_member(authoring_t1_member(r,"import"),"state"));
        done=!strcmp(state,"completed");assert(strcmp(state,"failed"));json_object_put(r);if(done)break;SDL_Delay(10);
    }assert(done);
    json_object *ui=mesh_test_read(id);assert(json_object_to_file("mesh-ui-expected.json",ui)==0);json_object_put(ui);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));material_mcp_pixels(after);assert(!memcmp(before,after,sizeof(before)));
    assert(SceneEditorChromeActionsSaveAuthoring());
    const char *script=getenv("OPTIC_MESH_MCP_TEST");assert(script);char *args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool finished=false;for(int i=0;i<12000;i++) {
        SceneEditorAgentControlPoll();SceneEditorTransformPanelPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
        if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);
    }if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    material_mcp_pixels(after);FILE *f=fopen("mesh-saved.rgba","wb");assert(f);assert(fwrite(after,1,sizeof(after),f)==sizeof(after));fclose(f);
    capture(editor,"mesh-mcp-ui.ppm");
}
