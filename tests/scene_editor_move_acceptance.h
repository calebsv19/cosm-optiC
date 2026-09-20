/* Included after the workspace harness click/key/capture helpers. */
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/scene_editor_chrome_actions.h"

static void move_copy_scene(const char* scene_path,const char* destination) {
    FILE* source=fopen(scene_path,"rb"); FILE* output=fopen(destination,"wb");
    assert(source && output);
    char bytes[8192]; size_t count;
    while ((count=fread(bytes,1,sizeof(bytes),source))>0) assert(fwrite(bytes,1,count,output)==count);
    assert(!ferror(source)); assert(fclose(source)==0); assert(fclose(output)==0);
}

static void move_pointer(SceneEditor* editor,Uint32 type,int x,int y) {
    SDL_Event event={0}; event.type=type;
    if (type==SDL_MOUSEMOTION) {
        event.motion.windowID=SDL_GetWindowID(editor->window);
        event.motion.x=x; event.motion.y=y; event.motion.state=SDL_BUTTON_LMASK;
    } else {
        event.button.windowID=SDL_GetWindowID(editor->window);
        event.button.button=SDL_BUTTON_LEFT; event.button.x=x; event.button.y=y;
    }
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
}

static void verify_move_acceptance(SceneEditor* editor,const char* scene_path,int selected) {
    char diagnostics[256];
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    assert(SceneEditorFrameViewport(true));
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout layout;
    RuntimeSceneBridge3DDigestState digest={0};
    SceneEditorDigestOverlayProjector projector;
    assert(SceneEditorGetPaneLayout(&layout));
    assert(SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,
        SceneEditorGetViewportNavState(),&projector));
    SceneEditorDocumentTransform original,current;
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&original,diagnostics,sizeof(diagnostics)));
    unsigned long long revision=SceneEditorDocumentRevision();
    bool dirty=SceneEditorDocumentIsDirty();
    SceneEditorBezier3DInteractionMetrics metrics=SceneEditorDigestOverlayResolveBezierMetrics(&digest,&projector);
    for (int component=0;component<3;++component) {
        SceneEditorBezier3DGizmoAxis axis=(SceneEditorBezier3DGizmoAxis)(SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X+component);
        int ax,ay,bx,by; double ppu;
        assert(SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(&projector,
            original.position[0],original.position[1],original.position[2],axis,metrics.gizmo_world_length,
            &ax,&ay,&bx,&by,&ppu));
        assert(SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(&projector,
            original.position[0],original.position[1],original.position[2],axis,72.0/ppu,
            &ax,&ay,&bx,&by,&ppu));
        double length=hypot((double)bx-ax,(double)by-ay);
        int ex=bx+(int)lround(48.0*((double)bx-ax)/length);
        int ey=by+(int)lround(48.0*((double)by-ay)/length);
        move_pointer(editor,SDL_MOUSEMOTION,bx,by);
        assert(SceneEditorObjectMoveGizmoHoverAxis()==axis);
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        if (component==0) capture(editor,"workspace_move_hover.ppm");
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==axis);
        if (component==0) capture(editor,"workspace_move_before.ppm");
        move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SceneEditorDigestOverlayProjector display;
        assert(SceneEditorObjectMoveGizmoPreviewProjector(selected,&projector,&display));
        assert(display.center_x!=projector.center_x || display.center_y!=projector.center_y || display.center_z!=projector.center_z);
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        assert(memcmp(&original,&current,sizeof(original))==0);
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        if (component==0) {
            capture(editor,"workspace_move_preview.ppm");
            /* Even a direct save while previewing cannot serialize presentation state. */
            assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
            char path[4096]; snprintf(path,sizeof(path),"%s.preview-saved.json",scene_path);
            move_copy_scene(scene_path,path);
            dirty=SceneEditorDocumentIsDirty();
        }
        key(editor,SDLK_ESCAPE);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
        assert(!SceneEditorObjectMoveGizmoPreviewProjector(selected,&projector,&display));
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        assert(memcmp(&original,&current,sizeof(original))==0);
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        /* No-op release produces no command. */
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        move_pointer(editor,SDL_MOUSEBUTTONUP,bx,by);
        assert(SceneEditorDocumentRevision()==revision);
        /* Focus loss cancels the same transaction. */
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SDL_Event lost={0}; lost.type=SDL_WINDOWEVENT;
        lost.window.windowID=SDL_GetWindowID(editor->window); lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        SceneEditorSessionRuntimeHandleEvent(editor,&lost);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        /* A revision change during a gesture invalidates it rather than overwriting newer work. */
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SceneEditorDocumentTransform concurrent=original; concurrent.position[1]+=0.125;
        assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&concurrent,diagnostics,sizeof(diagnostics)));
        unsigned long long changed_revision=SceneEditorDocumentRevision();
        move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
        assert(SceneEditorDocumentRevision()==changed_revision);
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        assert(memcmp(&concurrent,&current,sizeof(current))==0);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorChromeActionsSaveAuthoring());
        SceneEditorSessionRuntimeRender(editor);
        revision=SceneEditorDocumentRevision(); dirty=SceneEditorDocumentIsDirty();
        /* Workspace switching is presentation-only and cancels the transient drag. */
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
        SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        if (component!=0) continue;
        /* Pending lifecycle confirmation must take priority over a handle behind it. */
        choose_menu(editor,0,1);
        assert(SceneEditorLifecycleClosePending());
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
        key(editor,SDLK_ESCAPE);
        /* A real drag is exactly one command, irrespective of motion event count. */
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
        for (int j=1;j<=4;++j) move_pointer(editor,SDL_MOUSEMOTION,bx+(ex-bx)*j/4,by+(ey-by)*j/4);
        assert(SceneEditorDocumentRevision()==revision);
        move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
        assert(SceneEditorDocumentRevision()==revision+1);
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        SceneEditorDocumentTransform committed=current;
        capture(editor,"workspace_move_committed.ppm");
        assert(fabs(committed.position[0]-original.position[0])>1e-5);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        assert(memcmp(&original,&current,sizeof(original))==0);
        assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        assert(memcmp(&committed,&current,sizeof(committed))==0);
        assert(SceneEditorChromeActionsSaveAuthoring());
        char path[4096]; snprintf(path,sizeof(path),"%s.committed-move.json",scene_path);
        move_copy_scene(scene_path,path);
        FILE* expected=fopen("move_expected.txt","w"); assert(expected);
        for (int k=0;k<3;++k) fprintf(expected,"%.17g ",committed.position[k]);
        assert(fclose(expected)==0);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorChromeActionsSaveAuthoring());
        SceneEditorSessionRuntimeRender(editor);
        revision=SceneEditorDocumentRevision(); dirty=SceneEditorDocumentIsDirty();
    }
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
    assert(memcmp(&original,&current,sizeof(original))==0);
}

static void verify_primitive_move_preview(SceneEditor* editor,int selected) {
    SceneEditorDigestOverlayNavState prior_view=*SceneEditorGetViewportNavState();
    unsigned long long revision=SceneEditorDocumentRevision();
    bool dirty=SceneEditorDocumentIsDirty();
    ObjectEditorSetSelectedObjectIndex(0);
    assert(SceneEditorFrameViewport(true)); SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout layout; RuntimeSceneBridge3DDigestState digest={0};
    SceneEditorDigestOverlayProjector projector;
    SceneEditorDocumentTransform original,current; char diagnostics[256];
    assert(SceneEditorGetPaneLayout(&layout) && SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&projector));
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&original,diagnostics,sizeof(diagnostics)));
    int ax,ay,bx,by; double ppu;
    SceneEditorBezier3DInteractionMetrics metrics=SceneEditorDigestOverlayResolveBezierMetrics(&digest,&projector);
    assert(SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(&projector,original.position[0],original.position[1],original.position[2],
        SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X,metrics.gizmo_world_length,&ax,&ay,&bx,&by,&ppu));
    assert(SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(&projector,original.position[0],original.position[1],original.position[2],
        SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X,72.0/ppu,&ax,&ay,&bx,&by,&ppu));
    move_pointer(editor,SDL_MOUSEBUTTONDOWN,bx,by);
    assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X);
    capture(editor,"workspace_primitive_before.ppm");
    move_pointer(editor,SDL_MOUSEMOTION,bx+48,by);
    capture(editor,"workspace_primitive_preview.ppm");
    key(editor,SDLK_ESCAPE);
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&current,diagnostics,sizeof(diagnostics)));
    assert(memcmp(&original,&current,sizeof(original))==0);
    assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
    ObjectEditorSetSelectedObjectIndex(selected); SceneEditorRestoreViewportNav(&prior_view);
}
