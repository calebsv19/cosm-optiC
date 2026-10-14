#include "editor/scene_editor_lighting.h"
#include "ui/menu_environment_settings.h"
static json_object* lighting_mcp_read(void) {
    json_object *request=json_tokener_parse("{\"method\":\"lighting.read\",\"params\":{}}"),*response=SceneEditorAgentControlDispatch(request);json_object_put(request);
    assert(json_object_get_boolean(authoring_t1_member(response,"ok")));json_object *light=json_object_get(authoring_t1_member(response,"lighting"));json_object_put(response);return light;
}
static void lighting_mcp_refuse(void) {
    json_object *request=json_tokener_parse("{\"method\":\"lighting.set_primary\",\"params\":{\"position_m\":[1,2,3],\"intensity\":5,\"radius_m\":0.2}}");
    unsigned long long revision=SceneEditorDocumentRevision();json_object_object_add(authoring_t1_member(request,"params"),"expected_revision",json_object_new_int64(revision));
    json_object *response=SceneEditorAgentControlDispatch(request);assert(!json_object_get_boolean(authoring_t1_member(response,"ok")) && SceneEditorDocumentRevision()==revision);json_object_put(response);json_object_put(request);
}
static void lighting_mcp_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    char diagnostic[512];
    if(reopen) {json_object *expected=json_object_from_file("lighting-mcp-expected.json"),*actual=lighting_mcp_read();assert(expected && json_object_equal(actual,expected));json_object_put(actual);json_object_put(expected);assert(!SceneEditorDocumentIsDirty());fprintf(stderr,"Lighting fresh-process readback PASS\n");return;}
    unsigned char legacy[160*100*4],restored[160*100*4];camera_mcp_pixels(legacy);
    double initial_xyz[3],initial_intensity,initial_radius;assert(SceneEditorLightingGetPrimary(initial_xyz,&initial_intensity,&initial_radius));
    bool initial_interactive=animSettings.interactiveMode,initial_deep=animSettings.deepRenderMode;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    RuntimeNative3DPreparedFrame* initial_frame=calloc(1,sizeof(*initial_frame));assert(initial_frame && RuntimeNative3DPrepareFrameWithSampling(initial_frame,160,100,0,0,0,NULL));
    assert(fabs(initial_frame->scene.light.radius-initial_radius)<1e-9 && fabs(initial_frame->scene.light.intensity-initial_intensity)<1e-9);RuntimeNative3DPreparedFrame_Free(initial_frame);free(initial_frame);animSettings.interactiveMode=initial_interactive;animSettings.deepRenderMode=initial_deep;
    unsigned long long revision=SceneEditorDocumentRevision();json_object *initial=lighting_mcp_read();assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
    assert(json_object_get_boolean(authoring_t1_member(authoring_t1_member(initial,"primary"),"static_editable")));json_object_put(initial);
    /* The actual light XYZ inspector shares the retained primary-light command. */
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"light");authoring_control(editor,"next");assert(BezierEditorGetSelectedPointIndex()==0);
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+64,80,28});assert(SceneEditorCameraInspectorActive());lighting_mcp_refuse();authoring_text(editor,"2.5");assert(!SceneEditorCameraInspectorActive());
    json_object* ui=lighting_mcp_read();assert(fabs(json_object_get_double(json_object_array_get_idx(authoring_t1_member(authoring_t1_member(ui,"primary"),"position_m"),2))-2.5)<1e-9);json_object_put(ui);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));camera_mcp_pixels(restored);assert(!memcmp(legacy,restored,sizeof(legacy)));assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    /* UI intensity/radius adapter and environment menu commands enter shared Undo. */
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);SceneEditorSessionRuntimeRender(editor);
    BezierEditorRenderPaneControls(editor->renderer,(SDL_Rect){20,20,320,650},20,670);SDL_Rect slider;assert(BezierEditorLightingSliderRect("intensity",&slider));
    SDL_Event event={0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=slider.x+slider.w/3;event.button.y=slider.y+slider.h/2;
    HandleBezierEditorEvents(&event,&draggingPoint,&draggingVelocity);assert(BezierEditorLightingSliderActive());lighting_mcp_refuse();
    double xyz[3],intensity,radius;assert(SceneEditorLightingGetPrimary(xyz,&intensity,&radius));assert(fabs(intensity-20.*(slider.w/3)/(slider.w-1))<1e-9);
    event.type=SDL_MOUSEBUTTONUP;HandleBezierEditorEvents(&event,&draggingPoint,&draggingVelocity);assert(!BezierEditorLightingSliderActive());
    assert(BezierEditorLightingSliderRect("radius_m",&slider));event.type=SDL_MOUSEBUTTONDOWN;event.button.x=slider.x+slider.w/10;event.button.y=slider.y+slider.h/2;HandleBezierEditorEvents(&event,&draggingPoint,&draggingVelocity);event.type=SDL_MOUSEBUTTONUP;HandleBezierEditorEvents(&event,&draggingPoint,&draggingVelocity);
    assert(SceneEditorLightingGetPrimary(xyz,&intensity,&radius) && fabs(radius-25.*(slider.w/10)/(slider.w-1))<1e-9);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_RENDER);SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"light");
    assert(menu_environment_settings_set_ambient_brightness(51));assert(menu_environment_settings_set_background_color(.7,.5,.3));
    ui=lighting_mcp_read();assert(json_object_to_file("lighting-ui-expected.json",ui)==0);json_object_put(ui);
    assert(SceneEditorLightGestureBegin());lighting_mcp_refuse();SceneEditorLightGestureCancel();
    Path saved_path=sceneSettings.bezierPath;sceneSettings.bezierPath.numPoints=2;lighting_mcp_refuse();sceneSettings.bezierPath=saved_path;
    RuntimeSceneLightTimelineDocument* animated=calloc(1,sizeof(*animated));assert(animated);
    json_object* demo=json_object_from_file(getenv("OPTIC_LIGHTING_TIMELINE_FIXTURE"));assert(demo);
    json_object* demo_author=authoring_t1_member(authoring_t1_member(authoring_t1_member(demo,"extensions"),"ray_tracing"),"authoring");
    assert(RuntimeSceneLightTimelineParseAuthoring(demo_author,SceneEditorDocumentWorldScale(),animated,diagnostic,sizeof(diagnostic))==TIMELINE_STATUS_OK);json_object_put(demo);
    for(size_t i=0;i<animated->timeline.track_count;++i) strcpy(animated->timeline.tracks[i].target_id,"light/mirror_key_light");
    assert(SceneEditorDocumentSetTimelineWithLight(&animated->timeline,animated,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));free(animated);lighting_mcp_refuse();assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    MotionPaths* routes=calloc(1,sizeof(*routes));assert(routes);routes->count=1;routes->binding_count=1;strcpy(routes->paths[0].id,"light_route_guard");strcpy(routes->paths[0].name,"light route");routes->paths[0].count=2;strcpy(routes->paths[0].points[0].id,"a");strcpy(routes->paths[0].points[1].id,"b");routes->paths[0].points[1].position[0]=1;
    strcpy(routes->bindings[0].target_id,"light/mirror_key_light");strcpy(routes->bindings[0].path_id,"light_route_guard");routes->bindings[0].enabled=true;json_object* author=json_object_new_object();json_object_object_add(author,"motion_paths",MotionPathsToJson(routes));free(routes);assert(MotionPathsRuntimeLoad(author,SceneEditorDocumentWorldScale()));json_object_put(author);lighting_mcp_refuse();MotionPathsRuntimeReset();
    /* A real stdio bridge drives both named and cached-catalog-compatible calls. */
    const char *script=getenv("OPTIC_LIGHTING_MCP_TEST");assert(script);char *args[]={"python3","-B",(char*)script,(char*)scene,NULL};pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<3000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    json_object* expected=json_object_from_file("lighting-mcp-expected.json"),*actual=lighting_mcp_read();assert(expected && json_object_equal(actual,expected));json_object_put(actual);assert(!SceneEditorDocumentIsDirty());
    /* Prepared values must match the MCP base state in ordinary Direct Light rendering. */
    double camera[3]={1.25,-2.5,3.75};assert(SceneEditorDocumentSetStaticCameraPose(camera,180,-45,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    bool interactive=animSettings.interactiveMode,deep=animSettings.deepRenderMode;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame && RuntimeNative3DPrepareFrameWithSampling(frame,160,100,0,0,0,NULL));
    assert(fabs(frame->scene.light.position.x-1.25)<1e-9 && fabs(frame->scene.light.position.y+2.5)<1e-9 && fabs(frame->scene.light.position.z-3.75)<1e-9);assert(fabs(frame->scene.light.intensity-7.5)<1e-9 && fabs(frame->scene.light.radius-.25)<1e-9);
    assert(RuntimeNative3DRenderPreparedRegion(restored,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,0,0,160,100,NULL));RuntimeNative3DPreparedFrame_Free(frame);free(frame);
    unsigned char before[160*100*4];memcpy(before,restored,sizeof(before));double light[3]={-2,-1,4};assert(SceneEditorLightingSetPrimary(light,0,.25,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));camera_mcp_pixels(restored);assert(memcmp(before,restored,sizeof(before))!=0);assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));camera_mcp_pixels(restored);assert(!memcmp(before,restored,sizeof(before)));assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));animSettings.interactiveMode=interactive;animSettings.deepRenderMode=deep;
    /* MCP -> actual UI inspector -> Undo restores complete MCP lighting source. */
    authoring_control(editor,"light");authoring_control(editor,"next");click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46,80,28});authoring_text(editor,"9.25");assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));actual=lighting_mcp_read();assert(json_object_equal(actual,expected));json_object_put(actual);json_object_put(expected);assert(SceneEditorChromeActionsSaveAuthoring());capture(editor,"lighting-mcp-ui.ppm");fprintf(stderr,"Lighting UI/MCP, history, animation guards and Direct Light PASS\n");
}
