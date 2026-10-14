static void optical_matrices_acceptance(SceneEditor *editor,const char *scene) {
    char message[512];SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    SceneEditorRenderOptions options={.width=96,.height=64,.temporal_frames=2,.frame=0,.integrator=RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,.sample_sequence=321};
    char ui_id[64];snprintf(ui_id,sizeof(ui_id),"ui-seeded-%d",getpid());
    assert(SceneEditorRenderStart(ui_id,&options,SceneEditorDocumentRevision(),message,sizeof(message)));
    for(int i=0;i<10000 && SceneEditorRenderActive();i++){SceneEditorRenderPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);SDL_Delay(5);}assert(!SceneEditorRenderActive());
    json_object *r=SceneEditorRenderJobRead(ui_id,true,message,sizeof(message));assert(r && json_object_get_boolean(authoring_t1_member(r,"artifact_integrity")));assert(json_object_get_int64(authoring_t1_member(r,"sample_sequence"))==321);assert(json_object_to_file("matrix-ui-render.json",r)==0);json_object_put(r);
    const char *script=getenv("OPTIC_MATRIX_MCP_TEST");assert(script);char *args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool done=false;for(int i=0;i<40000;i++){SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){done=true;break;}SDL_Delay(10);}
    if(!done){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(done&&WIFEXITED(status)&&WEXITSTATUS(status)==0);assert(!SceneEditorDocumentIsDirty() && !strcmp(SceneEditorDocumentPath(),scene));
}
