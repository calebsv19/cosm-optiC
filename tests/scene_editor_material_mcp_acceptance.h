/* Uses real inspector events and a real MCP subprocess on a task-owned fixture. */
static void material_mcp_pixels(unsigned char *pixels) {
    RuntimeNative3DPreparedFrame *frame=calloc(1,sizeof(*frame));assert(frame);
    assert(RuntimeNative3DPrepareFrameWithSampling(frame,160,100,0,0,0,NULL));
    assert(RuntimeNative3DRenderPreparedRegion(pixels,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,0,0,160,100,NULL));
    RuntimeNative3DPreparedFrame_Free(frame);free(frame);
}
static void material_mcp_acceptance(SceneEditor *editor,const char *scene,bool reopen) {
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    assert(SceneEditorFrameViewport(true));SceneEditorSessionRuntimeRender(editor);
    if(reopen) {
        json_object *rows=json_object_from_file("material-mcp-expected.json");assert(rows);
        for(size_t i=0;i<json_object_array_length(rows);++i) authoring_t1_same((int)i,json_object_array_get_idx(rows,i));
        json_object_put(rows);assert(!SceneEditorDocumentIsDirty());
        assert(RuntimeSurfaceGraphActive(0) && !RuntimeSurfaceGraphActive(1));
        fprintf(stderr,"Materials MCP fresh-process reopen PASS\n");return;
    }
    unsigned char legacy[160*100*4],restored[160*100*4];
    material_mcp_pixels(legacy);
    json_object *request=json_tokener_parse("{\"method\":\"material.read\",\"params\":{\"object_id\":\"rowless\"}}");
    unsigned long long revision=SceneEditorDocumentRevision();
    json_object *response=SceneEditorAgentControlDispatch(request);assert(json_object_get_boolean(authoring_t1_member(response,"ok")));
    assert(!SceneEditorDocumentIsDirty() && SceneEditorDocumentRevision()==revision);
    json_object_put(request);json_object_put(response);
    char diagnostic[512];
    assert(SceneEditorDocumentSetFlag("surface","locked",true,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    request=json_object_new_object();json_object_object_add(request,"method",json_object_new_string("material.preset"));
    json_object *params=json_tokener_parse("{\"object_id\":\"surface\",\"preset\":\"solid\",\"replace\":true}");
    json_object_object_add(params,"expected_revision",json_object_new_int64(SceneEditorDocumentRevision()));json_object_object_add(request,"params",params);
    revision=SceneEditorDocumentRevision();response=SceneEditorAgentControlDispatch(request);
    assert(!json_object_get_boolean(authoring_t1_member(response,"ok")) && SceneEditorDocumentRevision()==revision);
    json_object_put(request);json_object_put(response);assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    authoring_t1_preset(editor,"new","noise3d",true);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    material_mcp_pixels(restored);assert(!memcmp(legacy,restored,sizeof(legacy)));
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    authoring_t1_select(editor,"rough");authoring_t1_click(editor,"parameter0");
    request=json_tokener_parse("{\"method\":\"scene.undo\",\"params\":{\"expected_revision\":0}}");
    response=SceneEditorAgentControlDispatch(request);assert(!json_object_get_boolean(authoring_t1_member(response,"ok")));
    assert(strstr(json_object_get_string(authoring_t1_member(response,"message")),"active editor operation"));
    json_object_put(request);json_object_put(response);
    authoring_t1_text(editor,"0.43");key(editor,SDLK_RETURN);
    json_object *row=authoring_t1_row(0);assert(json_object_to_file("material-ui-expected.json",row)==0);json_object_put(row);
    const char *script=getenv("OPTIC_MATERIAL_MCP_TEST");assert(script);
    char *args[]={"python3",(char*)script,(char*)scene,NULL};pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool finished=false;
    for(int i=0;i<3000;++i) {
        SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
        if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);
    }
    if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}
    assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    json_object *rows=json_object_from_file("material-mcp-expected.json");assert(rows);
    authoring_t1_same(0,json_object_array_get_idx(rows,0));json_object_put(rows);
    assert(!SceneEditorDocumentIsDirty());
    /* MCP parameter -> UI parameter commit, then Undo restores the exact MCP row. */
    row=authoring_t1_row(0);authoring_t1_select(editor,"rough");authoring_t1_click(editor,"parameter0");
    authoring_t1_text(editor,"0.61");key(editor,SDLK_RETURN);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));authoring_t1_same(0,row);json_object_put(row);
    assert(SceneEditorChromeActionsSaveAuthoring());
    capture(editor,"material-mcp-ui.ppm");
    fprintf(stderr,"Materials native UI/MCP PASS: legacy exact Direct Light restore, active draft refusal and bidirectional shared commands\n");
}
