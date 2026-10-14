/* Actual retained native document, real SDL UI events and stdio MCP. */
static json_object *object_mcp_row(const char *id) {
    size_t n=SceneEditorDocumentObjectJSONSize(id);assert(n);char *text=malloc(n);assert(text);
    assert(SceneEditorDocumentGetObjectJSON(id,text,n));json_object *o=json_tokener_parse(text);free(text);assert(o);return o;
}
static json_object *object_mcp_all(void) {
    json_object *rows=json_object_new_array();
    for(int i=0;i<SceneEditorDocumentObjectCount();++i) {
        SceneEditorDocumentObjectInfo info;assert(SceneEditorDocumentObjectAt(i,&info));json_object_array_add(rows,object_mcp_row(info.id));
    }
    return rows;
}
static void object_mcp_acceptance(SceneEditor *editor,const char *scene,bool reopen) {
    char diagnostic[512];SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    ObjectEditorSetSelectedObjectIndex(0);assert(SceneEditorFrameViewport(false));SceneEditorSessionRuntimeRender(editor);
    if(reopen) {
        json_object *expected=json_object_from_file("object-mcp-expected.json"),*actual=object_mcp_all();assert(expected);
        assert(json_object_equal(expected,actual));json_object_put(expected);json_object_put(actual);
        assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Object MCP fresh-process retained readback PASS\n");return;
    }
    unsigned char legacy[160*100*4],restored[160*100*4];material_mcp_pixels(legacy);
    /* UI rename draft blocks agent mutation but allows inspection. */
    rename_key(editor,SDLK_F2,KMOD_NONE);assert(SceneEditorRenameActive());
    json_object *request=json_tokener_parse("{\"method\":\"object.rename\",\"params\":{\"object_id\":\"surface\",\"name\":\"blocked\"}}");
    json_object_object_add(authoring_t1_member(request,"params"),"expected_revision",json_object_new_int64(SceneEditorDocumentRevision()));
    unsigned long long revision=SceneEditorDocumentRevision();json_object *response=SceneEditorAgentControlDispatch(request);
    assert(!json_object_get_boolean(authoring_t1_member(response,"ok")) && SceneEditorDocumentRevision()==revision);
    assert(strstr(json_object_get_string(authoring_t1_member(response,"message")),"active editor operation"));json_object_put(response);json_object_put(request);
    request=json_tokener_parse("{\"method\":\"object.read\",\"params\":{\"object_id\":\"surface\"}}");
    response=SceneEditorAgentControlDispatch(request);assert(json_object_get_boolean(authoring_t1_member(response,"ok")));json_object_put(request);json_object_put(response);
    rename_key(editor,SDLK_a,KMOD_GUI);rename_text(editor,"UI object café");rename_key(editor,SDLK_RETURN,KMOD_NONE);assert(!SceneEditorRenameActive());
    /* UI gizmo -> MCP retained transform, with exact legacy pixels after Undo. */
    transform_mode_click(editor,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE);
    SceneEditorObjectTransformHandle h;int ex,ey;transform_handle(editor,0,1,&h,&ex,&ey);
    move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
    request=json_tokener_parse("{\"method\":\"object.rename\",\"params\":{\"object_id\":\"surface\",\"name\":\"blocked drag\"}}");
    json_object_object_add(authoring_t1_member(request,"params"),"expected_revision",json_object_new_int64(SceneEditorDocumentRevision()));
    response=SceneEditorAgentControlDispatch(request);assert(!json_object_get_boolean(authoring_t1_member(response,"ok")));json_object_put(response);json_object_put(request);
    move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));material_mcp_pixels(restored);assert(!memcmp(legacy,restored,sizeof(legacy))); /* Rename has no optical effect. */
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    json_object *ui=object_mcp_row("surface");assert(json_object_to_file("object-ui-expected.json",ui)==0);json_object_put(ui);
    const char *script=getenv("OPTIC_OBJECT_MCP_TEST");assert(script);char *args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool finished=false;
    for(int i=0;i<3000;++i) {
        SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
        if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);
    }
    if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}
    assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    json_object *expected=json_object_from_file("object-mcp-expected.json"),*actual=object_mcp_all();assert(expected && json_object_equal(expected,actual));json_object_put(expected);json_object_put(actual);
    assert(!SceneEditorDocumentIsDirty());
    /* MCP -> UI rename and shared Undo restore exact MCP-retained object. */
    ui=object_mcp_row("surface");rename_key(editor,SDLK_F2,KMOD_NONE);assert(SceneEditorRenameActive());rename_key(editor,SDLK_a,KMOD_GUI);rename_text(editor,"UI after MCP");rename_key(editor,SDLK_RETURN,KMOD_NONE);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));actual=object_mcp_row("surface");assert(json_object_equal(ui,actual));json_object_put(ui);json_object_put(actual);
    assert(SceneEditorChromeActionsSaveAuthoring());capture(editor,"object-mcp-ui.ppm");fprintf(stderr,"Object native UI/MCP shared history and legacy Direct Light PASS\n");
}
