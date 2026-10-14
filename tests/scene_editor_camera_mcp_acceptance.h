/* Native camera inspector and actual stdio bridge over a retained scene. */
static void camera_mcp_pixels(unsigned char* pixels) {
    bool interactive=animSettings.interactiveMode,deep=animSettings.deepRenderMode;
    animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    material_mcp_pixels(pixels);animSettings.interactiveMode=interactive;animSettings.deepRenderMode=deep;
}
static json_object* camera_mcp_read(void) {
    json_object *request=json_tokener_parse("{\"method\":\"camera.read\",\"params\":{}}"),*response=SceneEditorAgentControlDispatch(request);json_object_put(request);
    assert(json_object_get_boolean(authoring_t1_member(response,"ok")));json_object *camera=json_object_get(authoring_t1_member(response,"camera"));json_object_put(response);return camera;
}
static void camera_mcp_refuse(void) {
    json_object *request=json_tokener_parse("{\"method\":\"camera.set_pose\",\"params\":{\"position_m\":[1,2,3],\"yaw_degrees\":20,\"pitch_degrees\":10}}");
    unsigned long long revision=SceneEditorDocumentRevision();json_object_object_add(authoring_t1_member(request,"params"),"expected_revision",json_object_new_int64(revision));
    json_object *response=SceneEditorAgentControlDispatch(request);assert(!json_object_get_boolean(authoring_t1_member(response,"ok")) && SceneEditorDocumentRevision()==revision);json_object_put(response);json_object_put(request);
}
static void camera_mcp_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    char diagnostic[512];
    if(reopen) {
        json_object *expected=json_object_from_file("camera-mcp-expected.json"),*camera=camera_mcp_read();assert(expected && json_object_equal(camera,expected));json_object_put(camera);json_object_put(expected);assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Camera fresh-process MCP pose/retained source PASS\n");return;
    }
    unsigned char legacy[160*100*4],restored[160*100*4];camera_mcp_pixels(legacy);
    unsigned long long revision=SceneEditorDocumentRevision();json_object *initial=camera_mcp_read();assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
    assert(json_object_get_boolean(authoring_t1_member(initial,"static_editable")));
    json_object *retained=authoring_t1_member(initial,"retained");assert(!authoring_t1_member(retained,"camera_path"));
    json_object_put(initial);
    /* UI inspector XYZ -> MCP readback, with exact legacy restoration via Undo. */
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"camera");authoring_control(editor,"next");assert(CameraEditorGetSelectedPointIndex()==0);
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+64,80,28});assert(SceneEditorCameraInspectorActive());camera_mcp_refuse();
    json_object *during=camera_mcp_read();json_object_put(during);
    capture(editor,"camera-shared-draft.ppm");authoring_text(editor,"2.5");assert(!SceneEditorCameraInspectorActive());
    double xyz[3],yaw,pitch;assert(SceneEditorDocumentGetStaticCameraPose(xyz,&yaw,&pitch));assert(fabs(xyz[2]-2.5)<1e-9);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));camera_mcp_pixels(restored);assert(!memcmp(legacy,restored,sizeof(legacy)));
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));json_object *ui=camera_mcp_read();assert(json_object_to_file("camera-ui-expected.json",ui)==0);json_object_put(ui);
    /* An animated path and an active camera gesture must not be flattened. */
    Path path=sceneSettings.cameraPath;CameraPath3D depth=sceneSettings.cameraPath3D;path.numPoints=2;path.points[1]=path.points[0];path.points[1].x+=1;depth.point_z[1]=depth.point_z[0];
    assert(SceneEditorDocumentSetCameraPath(&path,&depth,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));camera_mcp_refuse();assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorCameraGestureBegin());camera_mcp_refuse();SceneEditorCameraGestureCancel();
    TimelineDocument* doc=calloc(1,sizeof(*doc));TimelineTrack* track=calloc(1,sizeof(*track));assert(doc && track);
    assert(TimelineDocumentInit(doc,(TimelineRate){30,1},(TimelineRange){0,21})==TIMELINE_STATUS_OK);
    assert(TimelineTrackInit(track,"camera_yaw_guard","camera/main","camera/yaw",TIMELINE_VALUE_SCALAR)==TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(track,TIMELINE_UNIT_RADIANS)==TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(track,0,TimelineValueScalar(.2),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(doc,track)==TIMELINE_STATUS_OK);
    assert(SceneEditorDocumentSetTimeline(doc,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));free(track);free(doc);camera_mcp_refuse();assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    MotionPaths* routes=calloc(1,sizeof(*routes));assert(routes);routes->count=1;routes->binding_count=1;
    strcpy(routes->paths[0].id,"camera_route_guard");strcpy(routes->paths[0].name,"camera route");routes->paths[0].count=2;
    strcpy(routes->paths[0].points[0].id,"a");strcpy(routes->paths[0].points[1].id,"b");routes->paths[0].points[1].position[0]=1;
    strcpy(routes->bindings[0].target_id,"camera/main");strcpy(routes->bindings[0].path_id,"camera_route_guard");routes->bindings[0].enabled=true;
    json_object* author=json_object_new_object();json_object* route=MotionPathsToJson(routes);assert(route);json_object_object_add(author,"motion_paths",route);free(routes);
    assert(MotionPathsRuntimeLoad(author,SceneEditorDocumentWorldScale()));json_object_put(author);camera_mcp_refuse();MotionPathsRuntimeReset();
    const char *script=getenv("OPTIC_CAMERA_MCP_TEST");assert(script);char *args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool finished=false;
    for(int i=0;i<3000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}
    if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    json_object *expected=json_object_from_file("camera-mcp-expected.json"),*actual=camera_mcp_read();assert(expected && json_object_equal(actual,expected));json_object_put(actual);assert(!SceneEditorDocumentIsDirty());
    PreviewCameraSample sample;assert(PreviewCameraSampleEvaluate(&sceneSettings.camera,sceneSettings.cameraZ,&sceneSettings.cameraPath,&sceneSettings.cameraPath3D,.5,160,100,&sample));
    assert(fabs(sample.position_x-1.25)<1e-9 && fabs(sample.position_y+2.5)<1e-9 && fabs(sample.position_z-3.75)<1e-9);
    assert(fabs(sample.yaw_radians-180/57.29577951308232)<1e-9 && fabs(sample.pitch_radians+45/57.29577951308232)<1e-9);
    bool interactive=animSettings.interactiveMode,deep=animSettings.deepRenderMode;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame);
    assert(RuntimeNative3DPrepareFrameWithSampling(frame,160,100,0,0,0,NULL));
    assert(fabs(frame->scene.camera.position.x-1.25)<1e-9 && fabs(frame->scene.camera.position.y+2.5)<1e-9 && fabs(frame->scene.camera.position.z-3.75)<1e-9);
    assert(fabs(frame->scene.camera.rotation-180/57.29577951308232)<1e-9 && fabs(frame->scene.camera.lookPitch+45/57.29577951308232)<1e-9);
    assert(RuntimeNative3DRenderPreparedRegion(restored,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,0,0,160,100,NULL));RuntimeNative3DPreparedFrame_Free(frame);free(frame);animSettings.interactiveMode=interactive;animSettings.deepRenderMode=deep;
    assert(memcmp(legacy,restored,sizeof(legacy))!=0);
    /* MCP pose -> inspector field commit -> shared Undo restores exact MCP pose. */
    authoring_control(editor,"camera");authoring_control(editor,"next");click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46,80,28});authoring_text(editor,"9.25");assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    actual=camera_mcp_read();assert(json_object_equal(actual,expected));json_object_put(actual);json_object_put(expected);assert(SceneEditorChromeActionsSaveAuthoring());capture(editor,"camera-mcp-ui.ppm");fprintf(stderr,"Camera UI/MCP static pose, shared history, draft/path refusal and legacy Direct Light PASS\n");
}
