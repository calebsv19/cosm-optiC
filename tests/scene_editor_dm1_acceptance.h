/* D-M1 closes existing path authoring; it does not introduce new path bindings. */
static Path* dm1_path(bool camera) {return camera?&sceneSettings.cameraPath:&sceneSettings.bezierPath;}
static CameraPath3D* dm1_depth(bool camera) {return camera?&sceneSettings.cameraPath3D:&sceneSettings.bezierPath3D;}
static SceneEditorDigestOverlayProjector dm1_projector(void) {
    SceneEditorPaneLayout layout;RuntimeSceneBridge3DDigestState digest;
    SceneEditorDigestOverlayProjector p;
    assert(SceneEditorGetPaneLayout(&layout) && SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&p));return p;
}
static void dm1_point_click(SceneEditor* editor,bool camera,int index) {
    SceneEditorDigestOverlayProjector p=dm1_projector();int x,y;
    assert(SceneEditorDigestOverlayProjectPoint(&p,dm1_path(camera)->points[index].x,
        dm1_path(camera)->points[index].y,dm1_depth(camera)->point_z[index],&x,&y));
    click(editor,(SDL_Rect){x,y,1,1});
}
static void dm1_copy_scene(const char* source,const char* destination) {
    json_object* root=json_object_from_file(source);assert(root);
    assert(json_object_to_file_ext(destination,root,JSON_C_TO_STRING_PRETTY)==0);json_object_put(root);
}
static void dm1_samples(const char* filename,bool compare) {
    json_object* expected=compare?json_object_from_file(filename):json_object_new_array();assert(expected);
    const int frames[]={0,30,60};
    for(size_t i=0;i<3;++i) {
        RayEvaluatedSceneSnapshot s;assert(SceneEditorTimelineSeek(frames[i]));
        assert(SceneEditorTimelineCopyEvaluated(&s) && s.camera.valid && s.light.valid);
        double values[]={s.camera.position.x,s.camera.position.y,s.camera.position.z,s.camera.yaw_radians,
            s.camera.pitch_radians,s.camera.fov_y_degrees,s.light.position.x,s.light.position.y,s.light.position.z,s.light.progress,s.light.intensity};
        json_object* row=compare?json_object_array_get_idx(expected,i):json_object_new_array();assert(row);
        for(size_t j=0;j<11;++j) {
            if(compare) assert(fabs(values[j]-json_object_get_double(json_object_array_get_idx(row,j)))<1e-8);
            else json_object_array_add(row,json_object_new_double(values[j]));
        }
        if(!compare) json_object_array_add(expected,row);
    }
    if(!compare) assert(json_object_to_file_ext(filename,expected,JSON_C_TO_STRING_PRETTY)==0);
    json_object_put(expected);
}
static void dm1_gestures(SceneEditor* editor,bool camera) {
    authoring_control(editor,camera?"camera":"light");authoring_control(editor,"shape");
    authoring_control(editor,"frame_paths");
    Path before=*dm1_path(camera);CameraPath3D depth=*dm1_depth(camera);
    assert(before.numPoints>=3);unsigned long long revision=SceneEditorDocumentRevision();
    /* Delete an interior point: XYZ, orientation and segment storage must stay aligned. */
    authoring_control(editor,"delete");dm1_point_click(editor,camera,1);
    assert(dm1_path(camera)->numPoints==before.numPoints-1);
    assert(SceneEditorDocumentRevision()==revision+1);
    assert(fabs(dm1_path(camera)->points[1].x-before.points[2].x)<1e-8);
    assert(fabs(dm1_depth(camera)->point_z[1]-depth.point_z[2])<1e-8);
    assert(fabs(dm1_path(camera)->rotations[1]-before.rotations[2])<1e-8);
    assert(fabs(dm1_depth(camera)->point_pitch[1]-depth.point_pitch[2])<1e-8);
    choose_menu(editor,1,0);assert(dm1_path(camera)->numPoints==before.numPoints);
    assert(fabs(dm1_depth(camera)->point_z[1]-depth.point_z[1])<1e-8);
    choose_menu(editor,1,1);assert(dm1_path(camera)->numPoints==before.numPoints-1);
    choose_menu(editor,1,0);
    /* Existing Add appends at the viewport edit plane. */
    authoring_control(editor,"add");SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    click(editor,(SDL_Rect){layout.viewport_rect.x+layout.viewport_rect.w/5,layout.viewport_rect.y+layout.viewport_rect.h/4,1,1});
    assert(dm1_path(camera)->numPoints==before.numPoints+1);
    choose_menu(editor,1,0);assert(dm1_path(camera)->numPoints==before.numPoints);
    choose_menu(editor,1,1);assert(dm1_path(camera)->numPoints==before.numPoints+1);
    choose_menu(editor,1,0);
    authoring_control(editor,"select");
    if(camera) CameraEditorClearSelection();else BezierEditorClearSelection();
    SceneEditorSessionRuntimeRender(editor);
    /* Select the actual displayed handle through the normal input router. */
    double hx,hy,hz;int sx,sy;
    assert(CameraPath3D_GetHandleWorldPosition(dm1_path(camera),dm1_depth(camera),0,0,&hx,&hy,&hz,NULL,NULL,NULL));
    SceneEditorDigestOverlayProjector p=dm1_projector();assert(SceneEditorDigestOverlayProjectPoint(&p,hx,hy,hz,&sx,&sy));
    click(editor,(SDL_Rect){sx,sy,1,1});
    assert(camera?CameraEditorGetSelectionKind()==CAMERA_EDITOR_SELECTION_BEZIER_HANDLE:BezierEditorGetSelectionKind()==BEZIER_EDITOR_SELECTION_HANDLE);
    RuntimeSceneBridge3DDigestState digest;assert(SceneEditorDigestOverlayResolve(&digest));
    p=dm1_projector();int gx=-1,gy=-1;
    for(int y=layout.viewport_rect.y;y<layout.viewport_rect.y+layout.viewport_rect.h && gx<0;y+=2)
        for(int x=layout.viewport_rect.x;x<layout.viewport_rect.x+layout.viewport_rect.w;x+=2) {
            int axis=camera?SceneEditorDigestOverlayPickCameraGizmoAxis(&p,&digest,x,y):SceneEditorDigestOverlayPickBezierGizmoAxis(&p,&digest,x,y);
            if(axis==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X) {gx=x;gy=y;break;}
        }
    assert(gx>=0);
    for(int cancel=1;cancel>=0;--cancel) {
        revision=SceneEditorDocumentRevision();double old=dm1_path(camera)->handles[0][0].vx;
        SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=gx;e.button.y=gy;
        SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
        fprintf(stderr,"D-M1 drag start camera=%d cancel=%d x=%d y=%d selected=%d\n",camera,cancel,gx,gy,camera?(int)CameraEditorGetSelectionKind():(int)BezierEditorGetSelectionKind());
        assert(camera?SceneEditorCameraGestureActive():SceneEditorLightGestureActive());
        e.type=SDL_MOUSEMOTION;e.motion.state=SDL_BUTTON_LMASK;e.motion.x=gx+30;e.motion.y=gy+10;
        SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
        assert(fabs(dm1_path(camera)->handles[0][0].vx-old)>1e-6 && SceneEditorDocumentRevision()==revision);
        if(cancel) {key(editor,SDLK_ESCAPE);e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=gx+30;e.button.y=gy+10;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);}
        else {e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=gx+30;e.button.y=gy+10;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);}
        SceneEditorSessionRuntimeRender(editor);
        assert(!(camera?SceneEditorCameraGestureActive():SceneEditorLightGestureActive()));
        assert(SceneEditorDocumentRevision()==revision+(cancel?0:1));
        if(cancel) assert(fabs(dm1_path(camera)->handles[0][0].vx-old)<1e-8);
    }
    double changed=dm1_path(camera)->handles[0][0].vx;
    choose_menu(editor,1,0);assert(fabs(dm1_path(camera)->handles[0][0].vx-before.handles[0][0].vx)<1e-8);
    choose_menu(editor,1,1);assert(fabs(dm1_path(camera)->handles[0][0].vx-changed)<1e-8);
    /* Numeric handle depth edits also use one retained transaction. */
    assert(SceneEditorGetPaneLayout(&layout));
    double wx,wy,wz;assert(CameraPath3D_GetHandleWorldPosition(dm1_path(camera),dm1_depth(camera),0,0,&wx,&wy,&wz,NULL,NULL,NULL));
    char value[40];snprintf(value,sizeof(value),"%.12g",wz+.4);
    click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+64,80,28});authoring_text(editor,value);
    assert(fabs(dm1_depth(camera)->handles_vz[0][0]-depth.handles_vz[0][0]-.4)<1e-7);
    capture(editor,camera?"dm1_camera_handles.ppm":"dm1_light_handles.ppm");
    fprintf(stderr,"D-M1 %s topology/handle gestures PASS\n",camera?"camera":"light");
}
static void dm1_removal_boundaries(void) {
    const int counts[]={1,2,5,MAX_BEZIER_POINTS};
    for(size_t n=0;n<sizeof(counts)/sizeof(counts[0]);++n) for(int removed=0;removed<counts[n];++removed) {
        Path p={0};CameraPath3D d={0};p.numPoints=counts[n];
        for(int j=0;j<p.numPoints;++j) {
            p.points[j]=(Point){j+1,j+2};p.rotations[j]=j+.25;p.rotationSet[j]=true;p.handleLink[j]=(j%2)!=0;
            d.point_z[j]=j+3;d.point_pitch[j]=j+.5;
            for(int k=0;k<2;++k) {p.handles[j][k]=(Velocity){10*j+k+1,10*j+k+2};d.handles_vz[j][k]=10*j+k+3;}
        }
        CameraPath3D_RemovePoint(&d,removed,p.numPoints);RemoveBezierPoint(&p,removed);
        assert(p.numPoints==counts[n]-1);
        for(int j=0;j<p.numPoints;++j) {
            int old=j<removed?j:j+1;
            assert(p.points[j].x==old+1 && p.rotations[j]==old+.25 && p.rotationSet[j]);
            assert(d.point_z[j]==old+3 && d.point_pitch[j]==old+.5 && p.handleLink[j]==((old%2)!=0));
        }
        for(int j=0;j<p.numPoints-1;++j) {
            int out=j<removed?j:j+1,in=j<removed-1?j:j+1;
            assert(p.handles[j][0].vx==10*out+1 && p.handles[j][1].vx==10*in+2);
            assert(d.handles_vz[j][0]==10*out+3 && d.handles_vz[j][1]==10*in+4);
        }
    }
    fprintf(stderr,"D-M1 removal boundaries PASS: all indices, empty/small/full capacity, XYZ/orientation/handles\n");
}
static void dm1_acceptance(SceneEditor* editor,const char* scene,bool reopen) {
    dm1_removal_boundaries();
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
    if(reopen) {
        dm1_samples("dm1_expected.json",true);
        capture(editor,"dm1_fresh_reopen.ppm");
        fprintf(stderr,"D-M1 fresh-process exact evaluated samples PASS\n");return;
    }
    authoring_control(editor,"setup");
    char diagnostics[256];assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    dm1_copy_scene(scene,"dm1_before.json");dm1_samples("dm1_before_samples.json",false);
    dm1_gestures(editor,true);dm1_gestures(editor,false);
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    dm1_samples("dm1_expected.json",false);
    SDL_SetWindowSize(editor->window,1000,700);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"camera");authoring_control(editor,"shape");
    SDL_Rect r;SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    assert(SceneEditorRenderAuthoringControl("delete",&r));assert(r.y+r.h<=layout.left_content_rect.y+layout.left_content_rect.h);
    authoring_control(editor,"delete");authoring_control(editor,"timing");
    assert(SceneEditorToolStateGetActive()==SCENE_EDITOR_TOOL_SELECT);
    TimelineSample before,after;assert(SceneEditorTimelineCurrentSample(&before));
    unsigned long long revision=SceneEditorDocumentRevision();click(editor,layout.viewport_rect);
    assert(SceneEditorDocumentRevision()==revision && SceneEditorTimelineCurrentSample(&after) && before.absolute_frame==after.absolute_frame);
    authoring_control(editor,"shape");capture(editor,"dm1_compact.ppm");
    fprintf(stderr,"D-M1 compact layout, timing isolation, save PASS\n");
}
