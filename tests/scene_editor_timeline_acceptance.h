#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/camera_editor.h"
#include "app/preview_session.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/bezier_editor.h"
#include "app/ray_tracing_deep_render_desktop_host.h"

/* Exercise the actual session router and retained command boundary on each axis.
 * Restore each trial so the saved render fixture is independent of this coverage. */
static double timeline_native_axis_value(bool camera, int axis) {
    const Path* path=camera?&sceneSettings.cameraPath:&sceneSettings.bezierPath;
    if(axis==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y) return path->points[0].y;
    return camera?sceneSettings.cameraPath3D.point_z[0]:sceneSettings.bezierPath3D.point_z[0];
}
static void timeline_native_extra_axes(SceneEditor* editor, size_t track, bool camera) {
    char diagnostics[256];
    assert(SceneEditorTimelineSelectTrack(track));
    SDL_Event event={0};event.type=SDL_MOUSEMOTION;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    if(camera) CameraEditorSetSelectedPointIndex(0);else BezierEditorSetSelectedPointIndex(0);
    for(int axis=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y;axis<=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;++axis) {
        if(camera) CameraEditorSetSelectedPointIndex(0);else BezierEditorSetSelectedPointIndex(0);
        assert(SceneEditorFrameViewport(false));
        SceneEditorSessionRuntimeRender(editor);
        SceneEditorPaneLayout layout;RuntimeSceneBridge3DDigestState digest;
        SceneEditorDigestOverlayProjector projector;
        assert(SceneEditorGetPaneLayout(&layout));
        assert(SceneEditorDigestOverlayResolve(&digest));
        assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,
            SceneEditorGetViewportNavState(),&projector));
        int gx=-1,gy=-1;
        for(int y=layout.viewport_rect.y;y<layout.viewport_rect.y+layout.viewport_rect.h && gx<0;y+=2)
            for(int x=layout.viewport_rect.x;x<layout.viewport_rect.x+layout.viewport_rect.w;x+=2) {
                int picked=camera?SceneEditorDigestOverlayPickCameraGizmoAxis(&projector,&digest,x,y):
                    SceneEditorDigestOverlayPickBezierGizmoAxis(&projector,&digest,x,y);
                if(picked==axis) {gx=x;gy=y;break;}
            }
        if(gx<0) {
            double bx=0,by=0,bz=0;int px=0,py=0;
            bool resolved=SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(&projector,&digest,&bx,&by,&bz);
            SceneEditorBezier3DInteractionMetrics metrics=SceneEditorDigestOverlayResolveBezierMetrics(&digest,&projector);
            bool projected=SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(&projector,bx,by,bz,axis,metrics.gizmo_world_length,NULL,NULL,&px,&py,NULL);
            fprintf(stderr,"Missing %s axis %d mode=%d selection=%d resolved=%d projected=%d end=%d,%d viewport=%d,%d,%d,%d\n",camera?"camera":"light",axis,editor->currentMode,CameraEditorGetSelectionKind(),resolved,projected,px,py,layout.viewport_rect.x,layout.viewport_rect.y,layout.viewport_rect.w,layout.viewport_rect.h);
            capture(editor,"timeline_axis_diagnostic.ppm");
        }
        assert(gx>=0);
        for(int cancel=0;cancel<2;++cancel) {
            if(camera) CameraEditorSetSelectedPointIndex(0);else BezierEditorSetSelectedPointIndex(0);
            SceneEditorSessionRuntimeRender(editor);
            double before=timeline_native_axis_value(camera,axis);
            unsigned long long revision=SceneEditorDocumentRevision();
            event=(SDL_Event){0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;
            event.button.x=gx;event.button.y=gy;
            SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
            assert(camera?SceneEditorCameraGestureActive():SceneEditorLightGestureActive());
            event.type=SDL_MOUSEMOTION;event.motion.state=SDL_BUTTON_LMASK;event.motion.x=gx+40;event.motion.y=gy+20;
            SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
            assert(SceneEditorDocumentRevision()==revision && fabs(timeline_native_axis_value(camera,axis)-before)>1e-6);
            if(cancel) {
                event=(SDL_Event){0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_ESCAPE;
            } else {
                event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;
                event.button.x=layout.right_content_rect.x+10;event.button.y=layout.right_content_rect.y+20;
            }
            SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
            assert(!(camera?SceneEditorCameraGestureActive():SceneEditorLightGestureActive()));
            assert(SceneEditorDocumentRevision()==revision+(cancel?0:1));
            if(!cancel) assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
            assert(fabs(timeline_native_axis_value(camera,axis)-before)<1e-9);
        }
    }
}

static void timeline_native_acceptance(SceneEditor* editor,const char* scene_path) {
    char diagnostics[256];
    Path path={0};CameraPath3D depth={0};
    path.mode=BEZIER_CUBIC;path.numPoints=2;
    path.points[0]=(Point){-3,1};path.points[1]=(Point){3,1};
    path.handles[0][0]=(Velocity){2,0};path.handles[0][1]=(Velocity){-2,0};
    path.rotationSet[0]=path.rotationSet[1]=true;
    depth.point_z[0]=depth.point_z[1]=2;
    assert(SceneEditorDocumentSetCameraPath(&path,&depth,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics)));
    /* Camera-mode entry must expose Render's timeline without reselecting the workspace. */
    SetSceneMode(editor,EDITOR_MODE_CAMERA);
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_RENDER);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorFrameViewport(false));
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout layout;
    assert(SceneEditorGetPaneLayout(&layout) && layout.timeline_visible);
    click(editor,(SDL_Rect){layout.timeline_rect.x+10,layout.timeline_rect.y+10,20,15});

    static TimelineDocument timeline;
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    assert(timeline.track_count>=2);
    TimelineSample selection_before,selection_after;
    assert(SceneEditorTimelineCurrentSample(&selection_before));
    SDL_Rect channel_rect;assert(SceneEditorTimelineTrackRect(0,&channel_rect));click(editor,channel_rect);
    assert(editor->currentMode==EDITOR_MODE_PATH);
    for(size_t i=0;i<timeline.track_count;++i) if(!strcmp(timeline.tracks[i].property_id,"camera/path_progress"))
        {assert(SceneEditorTimelineTrackRect(i,&channel_rect));click(editor,channel_rect);}
    assert(editor->currentMode==EDITOR_MODE_CAMERA && SceneEditorTimelineCurrentSample(&selection_after) &&
        selection_before.absolute_frame==selection_after.absolute_frame);
    capture(editor,"timeline_render_initial.ppm");
    SceneEditorRenderAuthoringSetTiming(false);SceneEditorSessionRuntimeRender(editor);
    CameraEditorSetSelectedPointIndex(0);
    click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+2*32+5,20,10});
    SDL_Event event={0};event.type=SDL_TEXTINPUT;
    snprintf(event.text.text,sizeof(event.text.text),"2.75");
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    key(editor,SDLK_RETURN);SceneEditorSessionRuntimeRender(editor);
    assert(fabs(sceneSettings.cameraPath3D.point_z[0]-2.75)<1e-9);
    size_t camera_track=SIZE_MAX;
    for(size_t i=0;i<timeline.track_count;++i)
        if(!strcmp(timeline.tracks[i].property_id,"camera/path_progress")) camera_track=i;
    assert(camera_track!=SIZE_MAX);
    assert(SceneEditorTimelineSelectTrack(camera_track));
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+10));
    assert(SceneEditorTimelineSetKey(.25));
    assert(SceneEditorTimelineSetInterpolation(TIMELINE_INTERPOLATION_CUBIC_BEZIER));
    assert(SceneEditorGetPaneLayout(&layout));
    SDL_Rect graph,curve_button;assert(SceneEditorTimelineControl("curves",&curve_button));click(editor,curve_button);
    assert(SceneEditorTimelineControl("graph",&graph));
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    TimelineKeyframe curve_key=timeline.tracks[camera_track].keys[1];
    double span=(double)(timeline.range.frame_count-1);
    int hx=graph.x+(int)llround((curve_key.frame+curve_key.outgoing_frame_offset-timeline.range.start_frame)/span*graph.w);
    int hy=graph.y+graph.h-(int)llround((curve_key.value.as.scalar+curve_key.outgoing_value_offset+.15)/1.3*graph.h);
    unsigned long long curve_revision=SceneEditorDocumentRevision();
    event=(SDL_Event){0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=hx;event.button.y=hy;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    event.type=SDL_MOUSEMOTION;event.motion.x=hx+10;event.motion.y=hy-10;event.motion.state=SDL_BUTTON_LMASK;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    assert(SceneEditorDocumentRevision()==curve_revision);
    event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;event.button.x=hx+10;event.button.y=hy-10;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    assert(SceneEditorDocumentRevision()==curve_revision+1);
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    assert(timeline.tracks[camera_track].keys[1].outgoing_value_offset>0);
    TimelineKeyframe edited_curve_key=timeline.tracks[camera_track].keys[1];
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_frame_offset-curve_key.outgoing_frame_offset)<1e-9);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_value_offset-curve_key.outgoing_value_offset)<1e-9);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_frame_offset-edited_curve_key.outgoing_frame_offset)<1e-9);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_value_offset-edited_curve_key.outgoing_value_offset)<1e-9);
    capture(editor,"timeline_render_authored.ppm");
    RayEvaluatedSceneSnapshot moved_a,moved_b;
    assert(SceneEditorTimelineCopyEvaluated(&moved_a));
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+40));
    assert(SceneEditorTimelineCopyEvaluated(&moved_b));
    assert(fabs(moved_a.camera.position.x-moved_b.camera.position.x)>1e-4);
    capture(editor,"timeline_render_motion.ppm");
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+10));
    RayEvaluatedSceneSnapshot before,after;
    assert(SceneEditorTimelineCopyEvaluated(&before));
    size_t light_track=SIZE_MAX;
    for(size_t i=0;i<timeline.track_count;++i)
        if(!strcmp(timeline.tracks[i].property_id,"light/path_progress")) light_track=i;
    assert(light_track!=SIZE_MAX && SceneEditorTimelineSelectTrack(light_track));
    SceneEditorRenderAuthoringSelect(editor,false);SceneEditorSessionRuntimeRender(editor);
    SDL_Rect add_channel_control;
    assert(SceneEditorRenderAuthoringControl("animation",&add_channel_control));click(editor,add_channel_control);
    assert(SceneEditorRenderAuthoringControl("intensity",&add_channel_control));click(editor,add_channel_control);
    TimelineTrack intensity_track;TimelineRate intensity_rate;TimelineRange intensity_range;TimelineSample intensity_sample;
    assert(SceneEditorTimelineSelectedTrack(&intensity_track,&intensity_rate,&intensity_range,&intensity_sample) &&
        !strcmp(intensity_track.property_id,"light/intensity"));
    assert(SceneEditorTimelineSetKey(24.0));
    assert(SceneEditorTimelineCopyEvaluated(&after) && after.light.intensity==24.0 &&
        after.frame.sample.absolute_frame==before.frame.sample.absolute_frame);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineCopyEvaluated(&after) && after.light.intensity==before.light.intensity);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineCopyEvaluated(&after) && after.light.intensity==24.0);
    assert(SceneEditorRenderAuthoringControl("path",&add_channel_control));click(editor,add_channel_control);
    assert(SceneEditorTimelineSelectTrack(camera_track));
    unsigned long long light_revision=SceneEditorDocumentRevision();
    assert(SceneEditorLightGestureBegin());
    double prior_light_x=sceneSettings.bezierPath.points[0].x;
    sceneSettings.bezierPath.points[0].x+=.5;
    assert(SceneEditorDocumentRevision()==light_revision);
    assert(SceneEditorLightGestureCommit() && SceneEditorDocumentRevision()==light_revision+1);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)) &&
        fabs(sceneSettings.bezierPath.points[0].x-prior_light_x)<1e-9);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)) &&
        fabs(sceneSettings.bezierPath.points[0].x-prior_light_x-.5)<1e-9);
    assert(SceneEditorLightGestureBegin());sceneSettings.bezierPath.points[0].x+=1;
    SceneEditorLightGestureCancel();
    assert(fabs(sceneSettings.bezierPath.points[0].x-prior_light_x-.5)<1e-9);
    assert(SceneEditorTimelineSelectTrack(light_track));
    event=(SDL_Event){0};event.type=SDL_MOUSEMOTION;
    event.motion.x=layout.viewport_rect.x+20;event.motion.y=layout.viewport_rect.y+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    BezierEditorSetSelectedPointIndex(0);
    SceneEditorSessionRuntimeRender(editor);
    BezierMode mode_before=sceneSettings.bezierPath.mode;
    light_revision=SceneEditorDocumentRevision();
    SDL_Event light_control={0};light_control.type=SDL_KEYDOWN;light_control.key.keysym.sym=SDLK_t;
    HandleBezierEditorEvents(&light_control,&draggingPoint,&draggingVelocity);
    assert(SceneEditorDocumentRevision()==light_revision+1 && sceneSettings.bezierPath.mode!=mode_before);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)) && sceneSettings.bezierPath.mode==mode_before);
    bool linked_before=sceneSettings.bezierPath.handleLink[0];
    light_control.key.keysym.sym=SDLK_l;
    HandleBezierEditorEvents(&light_control,&draggingPoint,&draggingVelocity);
    assert(sceneSettings.bezierPath.handleLink[0]!=linked_before);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)) && sceneSettings.bezierPath.handleLink[0]==linked_before);
    RuntimeSceneBridge3DDigestState light_digest;
    SceneEditorDigestOverlayProjector light_projector;
    assert(SceneEditorDigestOverlayResolve(&light_digest));
    assert(SceneEditorDigestOverlayBuildProjector(&light_digest,&layout.viewport_rect,
        SceneEditorGetViewportNavState(),&light_projector));
    int gx=-1,gy=-1;
    for(int y=layout.viewport_rect.y;y<layout.viewport_rect.y+layout.viewport_rect.h && gx<0;y+=2)
        for(int x=layout.viewport_rect.x;x<layout.viewport_rect.x+layout.viewport_rect.w;x+=2)
            if(SceneEditorDigestOverlayPickBezierGizmoAxis(&light_projector,&light_digest,x,y)==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X) {
                gx=x;gy=y;break;
            }
    assert(gx>=0);
    double gizmo_before=sceneSettings.bezierPath.points[0].x;
    light_revision=SceneEditorDocumentRevision();
    event=(SDL_Event){0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=gx;event.button.y=gy;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorLightGestureActive());
    event.type=SDL_MOUSEMOTION;event.motion.state=SDL_BUTTON_LMASK;event.motion.x=gx+40;event.motion.y=gy+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorDocumentRevision()==light_revision && fabs(sceneSettings.bezierPath.points[0].x-gizmo_before)>1e-6);
    event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;event.button.x=layout.right_content_rect.x+10;event.button.y=layout.right_content_rect.y+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);
    assert(!SceneEditorLightGestureActive() && SceneEditorDocumentRevision()==light_revision+1);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)) && fabs(sceneSettings.bezierPath.points[0].x-gizmo_before)<1e-9);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineSelectTrack(camera_track));
    event=(SDL_Event){0};event.type=SDL_MOUSEMOTION;
    event.motion.x=layout.viewport_rect.x+20;event.motion.y=layout.viewport_rect.y+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    CameraEditorSetSelectedPointIndex(0);
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorDigestOverlayResolve(&light_digest));
    assert(SceneEditorDigestOverlayBuildProjector(&light_digest,&layout.viewport_rect,
        SceneEditorGetViewportNavState(),&light_projector));
    gx=-1;gy=-1;
    for(int y=layout.viewport_rect.y;y<layout.viewport_rect.y+layout.viewport_rect.h && gx<0;y+=2)
        for(int x=layout.viewport_rect.x;x<layout.viewport_rect.x+layout.viewport_rect.w;x+=2)
            if(SceneEditorDigestOverlayPickCameraGizmoAxis(&light_projector,&light_digest,x,y)==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X) {
                gx=x;gy=y;break;
            }
    assert(gx>=0);
    gizmo_before=sceneSettings.cameraPath.points[0].x;light_revision=SceneEditorDocumentRevision();
    event=(SDL_Event){0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=gx;event.button.y=gy;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorCameraGestureActive());
    event.type=SDL_MOUSEMOTION;event.motion.state=SDL_BUTTON_LMASK;event.motion.x=gx+40;event.motion.y=gy+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorDocumentRevision()==light_revision && fabs(sceneSettings.cameraPath.points[0].x-gizmo_before)>1e-6);
    event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;event.button.x=layout.right_content_rect.x+10;event.button.y=layout.right_content_rect.y+20;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);SceneEditorSessionRuntimeRender(editor);
    assert(!SceneEditorCameraGestureActive() && SceneEditorDocumentRevision()==light_revision+1);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)) && fabs(sceneSettings.cameraPath.points[0].x-gizmo_before)<1e-9);
    assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
    timeline_native_extra_axes(editor,light_track,false);
    timeline_native_extra_axes(editor,camera_track,true);
    assert(SceneEditorTimelineCopyEvaluated(&before));
    choose_menu(editor,0,0); /* File > Save, through the visible workspace menu. */
    assert(SceneEditorDocumentOpen(scene_path,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_frame_offset-edited_curve_key.outgoing_frame_offset)<1e-9);
    assert(fabs(timeline.tracks[camera_track].keys[1].outgoing_value_offset-edited_curve_key.outgoing_value_offset)<1e-9);
    assert(SceneEditorTimelineSelectTrack(camera_track));
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+10));
    assert(SceneEditorTimelineCopyEvaluated(&after));
    assert(fabs(before.camera.position.x-after.camera.position.x)<1e-9);
    assert(fabs(before.camera.position.z-after.camera.position.z)<1e-9);
    FILE* expected=fopen("timeline_expected_sample.json","w");assert(expected);
    fprintf(expected,"{\"frame\":%lld,\"camera\":[%.17g,%.17g,%.17g,%.17g,%.17g,%.17g],\"light\":[%.17g,%.17g,%.17g,%.17g,%.17g]}\n",
        (long long)after.frame.sample.absolute_frame,after.camera.position.x,after.camera.position.y,
        after.camera.position.z,after.camera.yaw_radians,after.camera.pitch_radians,after.camera.fov_y_degrees,
        after.light.position.x,after.light.position.y,after.light.position.z,after.light.progress,after.light.intensity);
    assert(fclose(expected)==0);
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+40));
    assert(SceneEditorTimelineCopyEvaluated(&after));
    expected=fopen("timeline_expected_interpolated_sample.json","w");assert(expected);
    fprintf(expected,"{\"frame\":%lld,\"camera\":[%.17g,%.17g,%.17g,%.17g,%.17g,%.17g],\"light\":[%.17g,%.17g,%.17g,%.17g,%.17g]}\n",
        (long long)after.frame.sample.absolute_frame,after.camera.position.x,after.camera.position.y,
        after.camera.position.z,after.camera.yaw_radians,after.camera.pitch_radians,after.camera.fov_y_degrees,
        after.light.position.x,after.light.position.y,after.light.position.z,after.light.progress,after.light.intensity);
    assert(fclose(expected)==0);
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+10));
    capture(editor,"timeline_render_reopened.ppm");
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+40));
    SDL_Event preview_key={0};preview_key.type=SDL_KEYDOWN;
    preview_key.key.windowID=SDL_GetWindowID(editor->window);preview_key.key.keysym.sym=SDLK_RIGHT;
    assert(SDL_PushEvent(&preview_key)==1);
    preview_key.key.keysym.sym=SDLK_ESCAPE;assert(SDL_PushEvent(&preview_key)==1);
    assert(vk_renderer_request_capture((VkRenderer*)editor->renderer,"timeline_preview_handoff.ppm")==VK_SUCCESS);
    click(editor,previewButton);
    TimelineSample returned;
    assert(SceneEditorTimelineCurrentSample(&returned) && returned.absolute_frame==timeline.range.start_frame+41);
    assert(SceneEditorTimelineSeek(timeline.range.start_frame+10));
    AnimationConfig render_config=animSettings;
    int render_width=sceneSettings.windowWidth,render_height=sceneSettings.windowHeight;
    char scratch_root[PATH_MAX];assert(getcwd(scratch_root,sizeof(scratch_root)));
    assert(snprintf(animSettings.outputRoot,sizeof(animSettings.outputRoot),"%s/desktop_timeline",scratch_root)<(int)sizeof(animSettings.outputRoot));
    assert(snprintf(animSettings.frameDir,sizeof(animSettings.frameDir),"%s/frames",animSettings.outputRoot)<(int)sizeof(animSettings.frameDir));
    animSettings.deepRenderMode=true;animSettings.asyncDeepRender=true;animSettings.useTiledRenderer=true;
    animSettings.volumeInteractionEnabled=false;animSettings.temporalFrames3D=1;animSettings.pathSamplesPerPixel=1;
    animSettings.renderScale3D=1;
    sceneSettings.windowWidth=64;sceneSettings.windowHeight=40;
    assert(RayTracingDeepRenderDesktopHost_BeginRun((int)timeline.range.start_frame+40,1));
    bool render_running=true;int rendered_frames=0;
    Uint64 render_deadline=SDL_GetTicks64()+30000;
    while(render_running && !RayTracingDeepRenderDesktopHost_CompletedSuccessfully() && SDL_GetTicks64()<render_deadline) {
        assert(RayTracingDeepRenderDesktopHost_SubmitFrame(editor->window,editor->renderer,0,0,&rendered_frames,&render_running));
        SDL_Delay(2);
    }
    assert(RayTracingDeepRenderDesktopHost_CompletedSuccessfully());
    char final_frame[PATH_MAX];snprintf(final_frame,sizeof(final_frame),"%s/frame_%04d.bmp",animSettings.frameDir,(int)timeline.range.start_frame+40);
    SDL_Surface* desktop_image=SDL_LoadBMP(final_frame);
    assert(desktop_image && desktop_image->w==64 && desktop_image->h==40);
    SDL_FreeSurface(desktop_image);
    RayTracingDeepRenderDesktopHost_Shutdown();
    animSettings=render_config;sceneSettings.windowWidth=render_width;sceneSettings.windowHeight=render_height;
    puts("timeline native acceptance: PASS");
}
