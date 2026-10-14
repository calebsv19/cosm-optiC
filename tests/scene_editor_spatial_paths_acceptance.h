static void spatial_paths_run(SceneEditor* editor,const char* scene,const char* mode) {
    char* args[]={"python3","-B",(char*)getenv("OPTIC_SPATIAL_PATHS_TEST"),(char*)scene,(char*)mode,NULL};assert(args[2]);pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<6000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}
    if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void spatial_paths_oracle(bool reopen) {
    AnimationConfig original=animSettings;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    static RayEvaluatedSceneServiceResult evaluated;assert(RayEvaluatedSceneCaptureSample((TimelineSample){24,0,1},&evaluated));
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame);RuntimeNative3DSamplingContext sampling={.sampleSequence=123};
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(frame,64,40,&evaluated.snapshot,&sampling));
    unsigned char pixels[64*40*4]={0},expected[64*40*4];RuntimeNative3DRenderStats stats;
    for(int integrator=0;integrator<2;++integrator) {
        assert(RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(pixels,integrator?RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,integrator?2:1,NULL,NULL,NULL,NULL,NULL,NULL,&stats));
        const char* path=integrator?"disney-fixed-sampling.rgba":"direct-fixed-sampling.rgba";FILE* f=fopen(path,reopen?"rb":"wb");assert(f);
        if(reopen) {assert(fread(expected,1,sizeof(expected),f)==sizeof(expected));assert(!memcmp(pixels,expected,sizeof(expected)));}
        else assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
    }
    RuntimeNative3DPreparedFrame_Free(frame);free(frame);animSettings=original;
}
static void spatial_paths_acceptance(SceneEditor *editor,const char *scene,bool reopen) {
 if(reopen){spatial_paths_run(editor,scene,"reopen");spatial_paths_oracle(true);assert(!SceneEditorDocumentIsDirty());return;}
 char msg[512];MotionPaths original,after;assert(SceneEditorMotionPathsRead(&original));unsigned char before[160*100*4],restored[160*100*4];camera_mcp_pixels(before);
 choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"paths");authoring_control(editor,"new_path");animation_mcp_trial_refuse();authoring_text(editor,"UI object route");key(editor,SDLK_ESCAPE);
 assert(SceneEditorMotionPathsRead(&after)&&after.count==original.count+1);assert(SceneEditorDocumentUndo(msg,sizeof(msg))&&SceneEditorDocumentUndo(msg,sizeof(msg)));assert(SceneEditorMotionPathsRead(&after)&&!memcmp(&after,&original,sizeof(original)));camera_mcp_pixels(restored);assert(!memcmp(before,restored,sizeof(before)));assert(SceneEditorDocumentRedo(msg,sizeof(msg))&&SceneEditorDocumentRedo(msg,sizeof(msg)));
 dm4_inspector_control(editor,"path_add");assert(SceneEditorMotionPathsRead(&after)&&after.paths[after.count-1].count==3);assert(SceneEditorDocumentUndo(msg,sizeof(msg)));dm4_inspector_control(editor,"path_smooth");assert(SceneEditorMotionPathsRead(&after)&&(after.paths[after.count-1].points[0].handle_mode==MOTION_HANDLE_LINKED || after.paths[after.count-1].points[1].handle_mode==MOTION_HANDLE_LINKED));
 json_object *encoded=MotionPathsToJson(&after);assert(json_object_to_file("spatial-paths-ui-expected.json",encoded)==0);json_object_put(encoded);spatial_paths_run(editor,scene,"edit");
 SceneEditorSessionRuntimeRender(editor);char control[100];snprintf(control,sizeof(control),"path_row/%s",original.paths[0].id);dm4_inspector_control(editor,control);authoring_control(editor,"path_name");animation_mcp_trial_refuse();authoring_text(editor,"UI after MCP");assert(SceneEditorDocumentUndo(msg,sizeof(msg)));assert(SceneEditorChromeActionsSaveAuthoring());spatial_paths_oracle(false);capture(editor,"spatial-paths-ui.ppm");
 fprintf(stderr,"Spatial paths native UI/MCP history, legacy pixels and fresh reopen/render PASS\n");
}
static void spatial_paths_legacy_acceptance(SceneEditor *editor,const char *scene){spatial_paths_run(editor,scene,"legacy");assert(!SceneEditorDocumentIsDirty());}
