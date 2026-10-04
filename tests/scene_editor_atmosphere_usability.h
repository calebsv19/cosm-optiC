#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_atmosphere_viewport.h"
#include "editor/scene_editor_atmosphere_inspector.h"
#include "editor/scene_editor_object_transform_handles.h"

static void atmosphere_inspector_control(SceneEditor* editor,const char* name) {
    SceneEditorSessionRuntimeRender(editor);SDL_Rect rect;
    assert(SceneEditorAtmosphereInspectorControl(name,&rect));click(editor,rect);
}
static void atmosphere_inspector_xyz(SceneEditor* editor,const char* field,const char* value) {
    atmosphere_inspector_control(editor,field);
    SDL_Event input={0};input.type=SDL_TEXTINPUT;snprintf(input.text.text,sizeof(input.text.text),"%s",value);
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);SceneEditorSessionRuntimeRender(editor);
    assert(!SceneEditorAtmosphereInspectorActive());
}
static void atmosphere_usability(SceneEditor* editor,const char* scene,bool reopen) {
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_ENVIRONMENT);
    assert(!SceneEditorAtmospherePreviewEnabled());
    SceneAtmosphereAttachment v;RuntimeVolumeAttachment3D volume;char diagnostic[512];
    assert(SceneEditorAtmosphereRead(&v));
    if(reopen) {
        assert(v.present && fabs(v.density_scale-1.25)<1e-9);
        assert(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic)));
        Vec3 c=SceneEditorAtmosphereCentre(&volume);
        double w=SceneEditorDocumentWorldScale();
        assert(fabs(c.x/w-12)<1e-6 && fabs(c.y/w-23)<1e-6 && fabs(c.z/w-34)<1e-6);
        assert(fabs(v.rotation_degrees[0]-15)<1e-9 && fabs(v.rotation_degrees[1]-25)<1e-9 && fabs(v.rotation_degrees[2]-35)<1e-9);
        atmosphere_control(editor,"select");atmosphere_control(editor,"frame");
        capture(editor,"atmosphere_complex_reopen.ppm");
        fprintf(stderr,"Atmosphere usability fresh-process reopen PASS\n");return;
    }
    if(v.present) assert(SceneEditorAtmosphereRemove(SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    /* Actual Cloud bank preset button, automatic fit/select/frame, no disk save. */
    atmosphere_control(editor,"preset");atmosphere_control(editor,"preset");
    unsigned long long revision=SceneEditorDocumentRevision();
    atmosphere_control(editor,"generate");
    assert(SceneEditorDocumentRevision()==revision+1 && SceneEditorAtmosphereSelected());
    assert(SceneEditorAtmosphereRead(&v) && strstr(v.source_path,"cloud_bank_v1"));
    assert(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic)));
    assert(v.scale[0]>1); /* Large complex scene should not get a tiny identity cloud. */
    assert(!SceneEditorAtmospherePreviewEnabled());
    capture(editor,"atmosphere_complex_created.ppm");
    /* Presentation-only selection, guide and view modes preserve the document. */
    revision=SceneEditorDocumentRevision();
    atmosphere_inspector_control(editor,"proxy");atmosphere_inspector_control(editor,"proxy");
    choose_menu(editor,3,0);assert(SceneEditorDocumentRevision()==revision);
    /* Centre conversion under rotated unequal scale, also tests authored units. */
    SceneAtmosphereAttachment trial=v;trial.scale[0]=2;trial.scale[1]=3;trial.scale[2]=4;
    trial.rotation_degrees[2]=90;assert(SceneEditorAtmosphereSetCentre(&trial,&volume,vec3(10,20,30),2));
    RuntimeVolumeAttachment3D moved=volume;SceneEditorAtmospherePlaceGeometry(&moved,&trial,2);
    Vec3 c=SceneEditorAtmosphereCentre(&moved);assert(vec3_length(vec3_sub(c,vec3(10,20,30)))<1e-9);
    atmosphere_inspector_xyz(editor,"position","12,23,34");
    atmosphere_inspector_xyz(editor,"rotation","15,25,35");
    atmosphere_inspector_xyz(editor,"dimensions","24,32,40");
    assert(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic)));
    c=SceneEditorAtmosphereCentre(&volume);double w=SceneEditorDocumentWorldScale();
    assert(vec3_length(vec3_sub(c,vec3(12*w,23*w,34*w)))<1e-6);
    SceneAtmosphereAttachment before;assert(SceneEditorAtmosphereRead(&before));
    atmosphere_inspector_xyz(editor,"sizing","-1,2,3");assert(SceneEditorAtmosphereRead(&v) && !memcmp(v.scale,before.scale,sizeof(v.scale)));
    assert(SceneEditorAtmosphereFrame());SceneEditorSessionRuntimeRender(editor);
    for(int mode=0;mode<3;++mode) for(int axis=1;axis<=(mode==2?4:3);++axis) {
        SceneEditorObjectTransformModeSet(mode);SceneEditorSessionRuntimeRender(editor);
        RuntimeSceneBridge3DDigestState digest;SceneEditorDigestOverlayProjector p;SceneEditorPaneLayout layout;
        assert(SceneEditorGetPaneLayout(&layout) && SceneEditorDigestOverlayResolve(&digest));
        assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&p));
        assert(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic)));c=SceneEditorAtmosphereCentre(&volume);
        double centre[3]={c.x,c.y,c.z};SceneEditorObjectTransformHandle h;
        assert(SceneEditorObjectTransformHandleProject(&p,&digest,centre,mode,axis,&h));
        SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=SDL_GetWindowID(editor->window);
        e.button.button=SDL_BUTTON_LEFT;e.button.x=h.x;e.button.y=h.y;
        revision=SceneEditorDocumentRevision();SceneEditorSessionRuntimeHandleEvent(editor,&e);
        assert(SceneEditorAtmosphereGestureActive());
        e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(editor->window);
        e.motion.x=h.x+(int)lround((mode==1?1:h.ux)*28);e.motion.y=h.y+(int)lround((mode==1?-1:h.uy)*28);
        SceneEditorSessionRuntimeHandleEvent(editor,&e);assert(SceneEditorDocumentRevision()==revision);
        SceneAtmosphereAttachment preview;
        assert(SceneEditorAtmospherePlacementPreview(&preview));
        RuntimeVolumeAttachment3D preview_volume=volume;SceneEditorAtmospherePlaceGeometry(&preview_volume,&preview,w);
        if(mode!=0) assert(vec3_length(vec3_sub(SceneEditorAtmosphereCentre(&preview_volume),c))<1e-6);
        SceneEditorSessionRuntimeRender(editor); /* Inspector publishes the same live preview. */
        json_object* request=json_tokener_parse("{\"method\":\"scene.undo\",\"params\":{\"expected_revision\":0}}");
        json_object* response=SceneEditorAgentControlDispatch(request),*ok=NULL,*message=NULL;
        assert(json_object_object_get_ex(response,"ok",&ok) && !json_object_get_boolean(ok));
        assert(json_object_object_get_ex(response,"message",&message) && strstr(json_object_get_string(message),"active editor operation"));
        json_object_put(response);json_object_put(request);
        key(editor,SDLK_ESCAPE);assert(!SceneEditorAtmosphereGestureActive() && SceneEditorDocumentRevision()==revision);
        e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=SDL_GetWindowID(editor->window);e.button.button=SDL_BUTTON_LEFT;e.button.x=h.x;e.button.y=h.y;
        SceneEditorSessionRuntimeHandleEvent(editor,&e);assert(SceneEditorAtmosphereGestureActive());
        e.type=SDL_MOUSEBUTTONUP;e.button.x=h.x+(int)lround((mode==1?1:h.ux)*28);e.button.y=h.y+(int)lround((mode==1?-1:h.uy)*28);
        SceneEditorSessionRuntimeHandleEvent(editor,&e);assert(!SceneEditorAtmosphereGestureActive() && SceneEditorDocumentRevision()==revision+1);
        assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
        assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
        assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));SceneEditorSessionRuntimeRender(editor);
    }
    /* Selecting projected volume edges, not its screen rectangle, avoids
     * swallowing object clicks across the entire volume. */
    SceneEditorAtmosphereSelect(false);assert(!SceneEditorAtmosphereSelected());
    assert(SceneEditorAtmosphereGeometry(&volume,diagnostic,sizeof(diagnostic)));
    RuntimeSceneBridge3DDigestState digest;SceneEditorDigestOverlayProjector p;SceneEditorPaneLayout layout;
    assert(SceneEditorGetPaneLayout(&layout) && SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&p));
    c=SceneEditorAtmosphereCentre(&volume);int x,y;assert(SceneEditorDigestOverlayProjectPoint(&p,c.x,c.y,c.z,&x,&y));
    click(editor,(SDL_Rect){x-1,y-1,2,2});assert(SceneEditorAtmosphereSelected());
    /* Replacement keeps manually authored placement and appearance. */
    assert(SceneEditorAtmosphereRead(&v));v.density_scale=1.25;
    assert(SceneEditorAtmosphereSet(&v,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    before=v;atmosphere_control(editor,"generate");assert(SceneEditorAtmosphereRead(&v));
    assert(!memcmp(v.translation,before.translation,sizeof(v.translation)) && !memcmp(v.scale,before.scale,sizeof(v.scale)) && v.density_scale==1.25);
    capture(editor,"atmosphere_complex_edited.ppm");
    choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(scene,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorAtmosphereRead(&v) && v.present && v.density_scale==1.25);
    assert(!SceneEditorAtmospherePreviewEnabled());
    fprintf(stderr,"Atmosphere complex usability PASS: real preset, fit/select, XYZ numeric dimensions, ten gizmos, live preview, cancel, agent exclusion, Undo/Redo, replacement, File Save/reopen\n");
}
