#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_document_timeline.h"

static void authoring_control(SceneEditor* editor,const char* name) {
    SDL_Rect rect;
    if(!SceneEditorRenderAuthoringControl(name,&rect) && SceneEditorMotionPathPanelActive()) {
        SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
        SDL_Rect panes[]={layout.left_content_rect,layout.right_content_rect};
        for(int pane=0;pane<2 && !SceneEditorRenderAuthoringControl(name,&rect);++pane) {
            SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.mouseX=panes[pane].x+10;e.wheel.mouseY=panes[pane].y+65;
            e.wheel.y=100;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
            for(int i=0;i<30 && !SceneEditorRenderAuthoringControl(name,&rect);++i) {
                e.wheel.y=-1;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
            }
        }
    }
    if(!SceneEditorRenderAuthoringControl(name,&rect)) fprintf(stderr,"Missing authoring control: %s\n",name);
    assert(SceneEditorRenderAuthoringControl(name,&rect));click(editor,rect);
}
static void authoring_text(SceneEditor* editor,const char* text) {
    SDL_Event e={0};e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof(e.text.text),"%s",text);
    SceneEditorSessionRuntimeHandleEvent(editor,&e);key(editor,SDLK_RETURN);SceneEditorSessionRuntimeRender(editor);
}
static void render_authoring_acceptance(SceneEditor* editor,const char* scene_path) {
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_RENDER);
    /* No fabricated camera path, timeline or direct entity-selection helpers. */
    unsigned long long before=SceneEditorDocumentRevision();
    authoring_control(editor,"light");assert(editor->currentMode==EDITOR_MODE_PATH);
    authoring_control(editor,"add");
    authoring_control(editor,"camera");assert(editor->currentMode==EDITOR_MODE_CAMERA);
    assert(SceneEditorToolStateGetActive()==SCENE_EDITOR_TOOL_SELECT);
    assert(SceneEditorDocumentRevision()==before);
    authoring_control(editor,"setup");
    static TimelineDocument doc;
    if(SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK) fprintf(stderr,"Setup: %s\n",SceneEditorTimelineStatus());
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    assert(SceneEditorDocumentRevision()==before+1);
    assert(editor->currentMode==EDITOR_MODE_CAMERA);
    authoring_control(editor,"frame_paths");
    authoring_control(editor,"next");assert(CameraEditorGetSelectedPointIndex()>=0);
    SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
    int cp=CameraEditorGetSelectedPointIndex();double z=sceneSettings.cameraPath3D.point_z[cp];
    char value[32];snprintf(value,sizeof(value),"%.9g",z+1);
    click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+64,80,28});authoring_text(editor,value);
    assert(fabs(sceneSettings.cameraPath3D.point_z[cp]-z-1)<1e-6);
    choose_menu(editor,1,0);assert(fabs(sceneSettings.cameraPath3D.point_z[cp]-z)<1e-6);
    choose_menu(editor,1,1);assert(fabs(sceneSettings.cameraPath3D.point_z[cp]-z-1)<1e-6);
    capture(editor,"render_camera_path.ppm");
    authoring_control(editor,"light");assert(editor->currentMode==EDITOR_MODE_PATH);
    assert(SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_RENDER);
    authoring_control(editor,"next");int lp=BezierEditorGetSelectedPointIndex();assert(lp>=0);
    double lz=sceneSettings.bezierPath3D.point_z[lp];snprintf(value,sizeof(value),"%.9g",lz+2);
    click(editor,(SDL_Rect){layout.right_content_rect.x+20,layout.right_content_rect.y+46+64,80,28});authoring_text(editor,value);
    assert(fabs(sceneSettings.bezierPath3D.point_z[lp]-lz-2)<1e-6);
    capture(editor,"render_light_path.ppm");
    authoring_control(editor,"add");authoring_control(editor,"timing");
    assert(SceneEditorRenderAuthoringTiming());
    assert(SceneEditorToolStateGetActive()==SCENE_EDITOR_TOOL_SELECT);
    unsigned long long timing_revision=SceneEditorDocumentRevision();
    click(editor,layout.viewport_rect);
    assert(SceneEditorDocumentRevision()==timing_revision);
    authoring_control(editor,"shape");assert(!SceneEditorRenderAuthoringTiming());authoring_control(editor,"select");authoring_control(editor,"animation");
    authoring_control(editor,"frame");authoring_text(editor,"40");
    authoring_control(editor,"value");authoring_text(editor,"0.35");
    authoring_control(editor,"frame");authoring_text(editor,"70");
    authoring_control(editor,"value");authoring_text(editor,"0.35");
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    TimelineTrack selected_track;TimelineRate selected_rate;TimelineRange selected_range;TimelineSample selected_sample;
    assert(SceneEditorTimelineSelectedTrack(&selected_track,&selected_rate,&selected_range,&selected_sample));
    fprintf(stderr,"Authoring timing proof: keys=%zu range=%llu frame=%lld property=%s\n",selected_track.key_count,(unsigned long long)selected_range.frame_count,(long long)selected_sample.absolute_frame,selected_track.property_id);
    assert(selected_track.key_count==4);
    authoring_control(editor,"intensity");
    authoring_control(editor,"value");authoring_text(editor,"2");
    RayEvaluatedSceneSnapshot snapshot;assert(SceneEditorTimelineCopyEvaluated(&snapshot));
    assert(snapshot.light.valid && fabs(snapshot.light.intensity-2)<1e-9);
    authoring_control(editor,"camera");assert(editor->currentMode==EDITOR_MODE_CAMERA);
    TimelineSample sample;assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==70);
    authoring_control(editor,"yaw");authoring_control(editor,"value");authoring_text(editor,"0.25");
    capture(editor,"render_animation.ppm");
    choose_menu(editor,0,0);
    char diagnostics[256];assert(SceneEditorDocumentOpen(scene_path,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.track_count>=5);
    assert(fabs(sceneSettings.cameraPath3D.point_z[cp]-z-1)<1e-6);
    assert(fabs(sceneSettings.bezierPath3D.point_z[lp]-lz-2)<1e-6);
    SDL_SetWindowSize(editor->window,1000,700);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
    capture(editor,"render_compact.ppm");
    fprintf(stderr,"Render authoring UI PASS: existing scene setup, camera/light selection, XYZ, undo/redo, pause keys, intensity, yaw, save/reopen\n");
}
