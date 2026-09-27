/* Real input routing on a disposable scene: no coordinate duplication for controls. */
static void dock_control(SceneEditor* e,const char* name) {SDL_Rect r;assert(SceneEditorTimelineControl(name,&r));click(e,r);}
static void dock_drag(SceneEditor* editor,int x,int y,int dx,int dy,bool cancel) {
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
    e.type=SDL_MOUSEMOTION;e.motion.x=dx;e.motion.y=dy;e.motion.state=SDL_BUTTON_LMASK;SceneEditorSessionRuntimeHandleEvent(editor,&e);
    if(cancel) key(editor,SDLK_ESCAPE);
    e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=dx;e.button.y=dy;SceneEditorSessionRuntimeHandleEvent(editor,&e);
    SceneEditorSessionRuntimeRender(editor);
}
static void timeline_dock_acceptance(SceneEditor* editor,const char* path) {
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
    assert(SceneEditorTimelineActivate());SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"frame_paths");
    static TimelineDocument doc;assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    size_t camera=SIZE_MAX,light=SIZE_MAX;
    for(size_t i=0;i<doc.track_count;++i) {
        if(!strcmp(doc.tracks[i].property_id,"camera/path_progress")) camera=i;
        if(!strcmp(doc.tracks[i].property_id,"light/path_progress")) light=i;
    }
    assert(camera!=SIZE_MAX && light!=SIZE_MAX);
    SDL_Rect row,ruler,graph;SceneEditorPaneLayout l;assert(SceneEditorGetPaneLayout(&l));
    assert(l.timeline_rect.x==10 && l.timeline_rect.w==1260);
    assert(l.left_pane_rect.y+l.left_pane_rect.h<l.timeline_rect.y);
    assert(l.right_pane_rect.y+l.right_pane_rect.h<l.timeline_rect.y);
    assert(SceneEditorTimelineTrackRect(camera,&row));click(editor,row);
    assert(editor->currentMode==EDITOR_MODE_CAMERA && SceneEditorRenderAuthoringTiming());
    unsigned long long revision=SceneEditorDocumentRevision();
    dock_control(editor,"frame");authoring_text(editor,"40");
    assert(SceneEditorDocumentRevision()==revision);
    dock_control(editor,"add_key");assert(SceneEditorDocumentRevision()==revision+1);
    dock_control(editor,"value");authoring_text(editor,"0.3");
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    revision=SceneEditorDocumentRevision();
    dock_control(editor,"next");key(editor,SDLK_RIGHT);SceneEditorSessionRuntimeRender(editor);
    TimelineSample sample;assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==42);
    assert(SceneEditorDocumentRevision()==revision);
    int initial_x=SceneEditorTimelineFrameX(40);dock_control(editor,"zoom_in");
    assert(SceneEditorTimelineFrameX(40)!=initial_x && SceneEditorDocumentRevision()==revision);
    dock_control(editor,"fit");assert(SceneEditorTimelineFrameX(40)==initial_x);
    assert(SceneEditorTimelineControl("ruler",&ruler));
    dock_drag(editor,SceneEditorTimelineFrameX(30),ruler.y+10,SceneEditorTimelineFrameX(60),ruler.y+10,false);
    assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==60 && SceneEditorDocumentRevision()==revision);
    assert(SceneEditorTimelineTrackRect(camera,&row));
    dock_drag(editor,SceneEditorTimelineFrameX(40),row.y+row.h/2,SceneEditorTimelineFrameX(50),row.y+row.h/2,true);
    assert(SceneEditorDocumentRevision()==revision);
    dock_drag(editor,SceneEditorTimelineFrameX(40),row.y+row.h/2,SceneEditorTimelineFrameX(50),row.y+row.h/2,false);
    assert(SceneEditorDocumentRevision()==revision+1);
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    assert(doc.tracks[camera].keys[1].frame==50);
    choose_menu(editor,1,0);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].frame==40);
    choose_menu(editor,1,1);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].frame==50);
    dock_control(editor,"frame");authoring_text(editor,"50");dock_control(editor,"interpolation");
    SDL_Rect menu;assert(SceneEditorTimelineControl("interpolation",&menu));
    click(editor,(SDL_Rect){menu.x,menu.y-24,menu.w,24});
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER);
    dock_control(editor,"curves");assert(SceneEditorTimelineControl("graph",&graph));
    assert(graph.h>l.timeline_rect.h/2);
    TimelineKeyframe k=doc.tracks[camera].keys[1];
    int hx=SceneEditorTimelineFrameX((int64_t)llround(k.frame+k.outgoing_frame_offset));
    int hy=graph.y+graph.h-(int)llround((k.value.as.scalar+.15)/1.3*graph.h);
    revision=SceneEditorDocumentRevision();dock_drag(editor,hx,hy,hx+12,hy-12,true);assert(SceneEditorDocumentRevision()==revision);
    dock_drag(editor,hx,hy,hx+12,hy-12,false);assert(SceneEditorDocumentRevision()==revision+1);
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].outgoing_value_offset>0);
    capture(editor,"timeline_dock_curves.ppm");
    /* Drag the curve key in both dimensions; one release is one history command. */
    k=doc.tracks[camera].keys[1];revision=SceneEditorDocumentRevision();
    int ky=graph.y+graph.h-(int)llround((k.value.as.scalar+.15)/1.3*graph.h);
    dock_drag(editor,SceneEditorTimelineFrameX(50),ky,SceneEditorTimelineFrameX(55),ky-5,false);
    assert(SceneEditorDocumentRevision()==revision+1);
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].frame==55 && doc.tracks[camera].keys[1].value.as.scalar>.3);
    dock_control(editor,"keys");assert(SceneEditorTimelineTrackRect(light,&row));click(editor,row);
    assert(editor->currentMode==EDITOR_MODE_PATH);
    revision=SceneEditorDocumentRevision();dock_control(editor,"start");dock_control(editor,"end");assert(SceneEditorDocumentRevision()==revision);
    dock_control(editor,"play");SDL_Delay(100);SceneEditorSessionRuntimeRender(editor);dock_control(editor,"play");
    assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame>0 && sample.absolute_frame<217 && SceneEditorDocumentRevision()==revision);
    /* View zoom, pan, grouping, and resize never create document commands. */
    assert(SceneEditorTimelineControl("ruler",&ruler));
    int x_before=SceneEditorTimelineFrameX(100);
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_MIDDLE;e.button.x=ruler.x+300;e.button.y=ruler.y+10;SceneEditorSessionRuntimeHandleEvent(editor,&e);
    e.type=SDL_MOUSEMOTION;e.motion.x=ruler.x+350;e.motion.y=ruler.y+10;SceneEditorSessionRuntimeHandleEvent(editor,&e);
    e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_MIDDLE;SceneEditorSessionRuntimeHandleEvent(editor,&e);
    assert(SceneEditorTimelineFrameX(100)>x_before && SceneEditorDocumentRevision()==revision);dock_control(editor,"fit");
    assert(SceneEditorTimelineTrackRect(camera,&row));click(editor,row);dock_control(editor,"frame");authoring_text(editor,"55");
    for(int cancel=0;cancel<2;++cancel) {
        revision=SceneEditorDocumentRevision();
        e=(SDL_Event){0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;
        e.button.x=SceneEditorTimelineFrameX(55);e.button.y=row.y+row.h/2;SceneEditorSessionRuntimeHandleEvent(editor,&e);
        e.type=SDL_MOUSEMOTION;e.motion.x=SceneEditorTimelineFrameX(60);e.motion.y=row.y+row.h/2;SceneEditorSessionRuntimeHandleEvent(editor,&e);
        e.type=SDL_WINDOWEVENT;e.window.event=cancel?SDL_WINDOWEVENT_SIZE_CHANGED:SDL_WINDOWEVENT_FOCUS_LOST;
        e.window.data1=1280;e.window.data2=800;SceneEditorSessionRuntimeHandleEvent(editor,&e);
        e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;e.button.x=SceneEditorTimelineFrameX(60);e.button.y=row.y+row.h/2;SceneEditorSessionRuntimeHandleEvent(editor,&e);
        assert(SceneEditorDocumentRevision()==revision);
    }
    SceneEditorSessionRuntimeRender(editor);
    capture(editor,"timeline_dock_keys.ppm");
    choose_menu(editor,0,0);char diagnostics[256];assert(SceneEditorDocumentOpen(path,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[camera].keys[1].frame==55);
    SDL_SetWindowSize(editor->window,1000,700);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorGetPaneLayout(&l) && l.timeline_rect.w==980);
    capture(editor,"timeline_dock_compact.ppm");
    fprintf(stderr,"Timeline dock UI PASS: full width, selection, frame entry/step/scrub, zoom/pan, retime/cancel, undo/redo, interpolation, curve handles/key drag, save/reopen, compact layout\n");
}
