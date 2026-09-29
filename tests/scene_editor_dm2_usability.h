static void dm2_navigation_drag(SceneEditor *editor, int x, int y, Uint8 button, SDL_Keymod mods) {
    SceneEditorDigestOverlayNavState before=*SceneEditorGetViewportNavState();
    unsigned long long revision=SceneEditorDocumentRevision();
    SDL_SetModState(mods);
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=button;e.button.x=x;e.button.y=y;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
    e=(SDL_Event){0};e.type=SDL_MOUSEMOTION;e.motion.state=SDL_BUTTON(button);e.motion.x=x+24;e.motion.y=y+14;e.motion.xrel=24;e.motion.yrel=14;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
    e=(SDL_Event){0};e.type=SDL_MOUSEBUTTONUP;e.button.button=button;e.button.x=x+24;e.button.y=y+14;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);SDL_SetModState(KMOD_NONE);
    const SceneEditorDigestOverlayNavState *after=SceneEditorGetViewportNavState();
    assert(SceneEditorDocumentRevision()==revision);
    if(button==SDL_BUTTON_LEFT) assert(fabs(after->orbit_yaw_deg-before.orbit_yaw_deg)>1e-6);
    else assert(fabs(after->target_x-before.target_x)+fabs(after->target_y-before.target_y)+fabs(after->target_z-before.target_z)>1e-6);
    assert(!after->pan_active&&!after->orbit_active);
    SceneEditorRestoreViewportNav(&before);SceneEditorSessionRuntimeRender(editor);
}
/* Exercise the user-facing order: shape with no follower, then attach/time. */
static void dm2_usability(SceneEditor* editor,const char* scene){
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
    authoring_control(editor,"camera");authoring_control(editor,"shape");CameraEditorSetSelectedPointIndex(0);
    Path camera=sceneSettings.cameraPath,light=sceneSettings.bezierPath;
    CameraPath3D camera_depth=sceneSettings.cameraPath3D,light_depth=sceneSettings.bezierPath3D;
    authoring_control(editor,"paths");
    assert(SceneEditorMotionPathPanelActive() && CameraEditorGetSelectedPointIndex()==-1 && BezierEditorGetSelectedPointIndex()==-1);
    authoring_control(editor,"new_path");authoring_text(editor,"Viewport route");
    MotionPaths paths;assert(SceneEditorMotionPathsRead(&paths)&&paths.count==1&&paths.binding_count==0);
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    SceneEditorDigestOverlayProjector projector=dm1_projector();double scale=SceneEditorDocumentWorldScale();int x0,y0,x1,y1;
    assert(SceneEditorDigestOverlayProjectPoint(&projector,paths.paths[0].points[0].position[0]*scale,paths.paths[0].points[0].position[1]*scale,paths.paths[0].points[0].position[2]*scale,&x0,&y0));
    assert(SceneEditorDigestOverlayProjectPoint(&projector,paths.paths[0].points[1].position[0]*scale,paths.paths[0].points[1].position[1]*scale,paths.paths[0].points[1].position[2]*scale,&x1,&y1));
    assert(hypot(x1-x0,y1-y0)>60); /* no near-invisible two-unit seed */
    int px=layout.viewport_rect.x+layout.viewport_rect.w*3/4,py=layout.viewport_rect.y+layout.viewport_rect.h*3/4;
    unsigned long long rev=SceneEditorDocumentRevision();
    click(editor,(SDL_Rect){px,py,1,1});assert(SceneEditorDocumentRevision()==rev);
    assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==2);
    dm2_navigation_drag(editor,x0,y0,SDL_BUTTON_LEFT,KMOD_ALT);
    dm2_navigation_drag(editor,x0,y0,SDL_BUTTON_LEFT,(SDL_Keymod)(KMOD_ALT|KMOD_SHIFT));
    dm2_navigation_drag(editor,x0,y0,SDL_BUTTON_RIGHT,KMOD_NONE);
    dm2_navigation_drag(editor,x0,y0,SDL_BUTTON_MIDDLE,KMOD_NONE);
    SDL_SetModState(KMOD_SHIFT);click(editor,(SDL_Rect){px,py,1,1});SDL_SetModState(KMOD_NONE);assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==3&&paths.binding_count==0);assert(SceneEditorDocumentRevision()==rev+1);
    key(editor,SDLK_ESCAPE);SceneEditorSessionRuntimeRender(editor);
    rev=SceneEditorDocumentRevision();click(editor,(SDL_Rect){px-25,py-20,1,1});assert(SceneEditorDocumentRevision()==rev);
    SDL_SetModState(KMOD_SHIFT);click(editor,(SDL_Rect){px-70,py-30,1,1});SDL_SetModState(KMOD_NONE);
    assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==4&&paths.binding_count==0);
    choose_menu(editor,1,0);assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==3);
    choose_menu(editor,1,1);assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==4);
    authoring_control(editor,"path_plane_z");authoring_text(editor,"3");
    authoring_control(editor,"path_place_tool");SDL_SetModState(KMOD_SHIFT);click(editor,(SDL_Rect){px-120,py+10,1,1});SDL_SetModState(KMOD_NONE);
    assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==5&&fabs(paths.paths[0].points[4].position[2]-3)<1e-8);
    key(editor,SDLK_ESCAPE);SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"path_frame_selected");
    /* Legacy editing shortcuts and clicks must not mutate camera/light. */
    key(editor,SDLK_l);key(editor,SDLK_t);SceneEditorSessionRuntimeRender(editor);
    assert(!memcmp(&camera,&sceneSettings.cameraPath,sizeof(camera))&&!memcmp(&light,&sceneSettings.bezierPath,sizeof(light)));
    assert(!memcmp(&camera_depth,&sceneSettings.cameraPath3D,sizeof(camera_depth))&&!memcmp(&light_depth,&sceneSettings.bezierPath3D,sizeof(light_depth)));
    capture(editor,"dm2_usability_shape.ppm");
    authoring_control(editor,"path_followers");authoring_control(editor,"follower_next");
    authoring_control(editor,"path_attach");assert(SceneEditorMotionPathsRead(&paths)&&paths.binding_count==1);
    authoring_control(editor,"path_timing");assert(!SceneEditorMotionPathPanelActive());
    authoring_control(editor,"paths");assert(SceneEditorMotionPathPanelActive());
    char message[256];assert(SceneEditorDocumentSave(message,sizeof(message)));assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
    assert(SceneEditorMotionPathsRead(&paths)&&paths.paths[0].count==5&&paths.binding_count==1);
    authoring_control(editor,"camera");assert(!SceneEditorMotionPathPanelActive());authoring_control(editor,"paths");assert(SceneEditorMotionPathPanelActive());
    SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"path_select_tool");
    SDL_Rect add,select,frame;assert(SceneEditorRenderAuthoringControl("path_place_tool",&add));assert(SceneEditorRenderAuthoringControl("path_select_tool",&select));assert(SceneEditorRenderAuthoringControl("path_frame_selected",&frame));
    assert(SceneEditorGetPaneLayout(&layout));
    assert(select.x+select.w<add.x && frame.y>add.y);
    assert(add.x>=layout.left_content_rect.x && add.x+add.w<=layout.left_content_rect.x+layout.left_content_rect.w);
    assert(!SDL_HasIntersection(&add,&layout.viewport_rect));capture(editor,"dm2_usability_compact.ppm");
    fprintf(stderr,"D-M2 usability PASS: visible framed seed, guarded Shift append, plain click nonmutation, Option orbit and right/middle pan over points, plane height, no follower required, undo/redo, camera/light isolation, attach/timing, persistence and compact toolbar.\n");
}
