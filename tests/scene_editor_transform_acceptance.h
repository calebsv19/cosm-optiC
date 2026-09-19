/* Included by the native workspace harness, after the shared pointer helpers. */
#include "editor/scene_editor_object_transform_handles.h"
#include "editor/scene_editor_object_transform_preview.h"
#include "editor/scene_editor_transform_feedback.h"
#include "editor/scene_editor_mesh_preview_store.h"

static void transform_expected(const char* name,const SceneEditorDocumentTransform* t) {
    char path[100];snprintf(path,sizeof(path),"%s_expected.txt",name);
    FILE* f=fopen(path,"w");assert(f);
    for (int i=0;i<3;++i) fprintf(f,"%.17g %.17g %.17g\n",t->position[i],t->rotation_degrees[i],t->scale[i]);
    assert(fclose(f)==0);
}
static void transform_handle(SceneEditor* editor,int selected,int axis,
    SceneEditorObjectTransformHandle* handle,int* ex,int* ey) {
    SceneEditorSessionRuntimeRender(editor);
    SceneEditorPaneLayout layout; RuntimeSceneBridge3DDigestState digest={0};
    SceneEditorDigestOverlayProjector projector; SceneEditorDocumentTransform t; char diagnostics[256];
    assert(SceneEditorGetPaneLayout(&layout));assert(SceneEditorDigestOverlayResolve(&digest));
    assert(SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&projector));
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&t,diagnostics,sizeof(diagnostics)));
    double origin[3];SceneEditorObjectTransformHandleOrigin(selected,SceneEditorObjectTransformModeGet(),t.position,origin);
    assert(SceneEditorObjectTransformHandleProject(&projector,&digest,origin,SceneEditorObjectTransformModeGet(),
        (SceneEditorBezier3DGizmoAxis)axis,handle));
    if (SceneEditorObjectTransformModeGet()==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE) {
        double angle;
        if (SceneEditorObjectTransformHandleAngle(handle,handle->x,handle->y,&angle)) {
            angle+=0.5;
            *ex=(int)lround(handle->cx+cos(angle)*handle->ux+sin(angle)*handle->vx);
            *ey=(int)lround(handle->cy+cos(angle)*handle->uy+sin(angle)*handle->vy);
        } else { *ex=handle->x+30;*ey=handle->y; }
    } else { *ex=handle->x+(int)lround(48*handle->ux);*ey=handle->y+(int)lround(48*handle->uy); }
}
static void transform_mode_click(SceneEditor* editor,int mode) {
    SceneEditorPaneLayout layout;SceneEditorWorkspaceChrome chrome;
    assert(SceneEditorGetPaneLayout(&layout));SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
    click(editor,chrome.transforms[mode]);assert((int)SceneEditorObjectTransformModeGet()==mode);
}
static void transform_compare(const SceneEditorDocumentTransform* a,const SceneEditorDocumentTransform* b) {
    for (int i=0;i<3;++i) {
        assert(fabs(a->position[i]-b->position[i])<1e-12);
        assert(fabs(a->rotation_degrees[i]-b->rotation_degrees[i])<1e-12);
        assert(fabs(a->scale[i]-b->scale[i])<1e-12);
    }
}
static void verify_transform_acceptance(SceneEditor* editor,const char* scene_path,int selected) {
    static const char* names[]={"move","rotate","scale"};
    char diagnostics[256],label[256],path[4096],name[100];
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    ObjectEditorSetSelectedObjectIndex(selected);assert(SceneEditorFrameViewport(false));
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    SceneEditorDocumentTransform original,current,preview,baseline;
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&original,diagnostics,sizeof(diagnostics)));
    char id[64];assert(runtime_scene_bridge_get_last_object_id_for_scene_index(selected,id,sizeof(id)));
    transform_mode_click(editor,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE);
    capture(editor,"workspace_dense_move_idle.ppm");
    assert(!SceneEditorTransformOperationLabel(selected,label,sizeof(label)) && strstr(label,"Op: Move"));
    SceneEditorTransformGroupLabel(0,label,sizeof(label));assert(strstr(label,"Position") && strstr(label,"meters"));
    SceneEditorTransformGroupLabel(1,label,sizeof(label));assert(strstr(label,"Rotation (degrees)"));
    SceneEditorTransformGroupLabel(2,label,sizeof(label));assert(strstr(label,"Scale (unitless factor)"));
    /* Capture signed Move feedback in the same dense framing used below. */
    SceneEditorObjectTransformHandle h;int ex,ey;
    transform_handle(editor,selected,1,&h,&ex,&ey);
    move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
    assert(SceneEditorTransformOperationLabel(selected,label,sizeof(label)) && strstr(label,"Move X +") && strstr(label,"meters"));
    capture(editor,"workspace_dense_move_active.ppm");key(editor,SDLK_ESCAPE);
    for (int mode=1;mode<=2;++mode) {
        unsigned long long revision=SceneEditorDocumentRevision();
        transform_mode_click(editor,mode);assert(SceneEditorDocumentRevision()==revision);
        for (int axis=1;axis<=3;++axis) {
            transform_handle(editor,selected,axis,&h,&ex,&ey);
            move_pointer(editor,SDL_MOUSEMOTION,h.x,h.y);
            assert((int)SceneEditorObjectMoveGizmoHoverAxis()==axis);
            move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);
            assert((int)SceneEditorObjectMoveGizmoActiveAxis()==axis);
            if (axis==1) { snprintf(name,sizeof(name),"workspace_%s_before.ppm",names[mode]);capture(editor,name); }
            move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
            assert(SceneEditorObjectTransformPreview(selected,&baseline,&preview));
            transform_compare(&baseline,&original);
            assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
            transform_compare(&current,&original);
            assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
            assert(SceneEditorTransformOperationLabel(selected,label,sizeof(label)));
            assert(strstr(label,mode==1 ? "degrees" : " x"));
            if (axis==1) {
                snprintf(name,sizeof(name),"workspace_dense_%s_active.ppm",names[mode]);capture(editor,name);
                assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
                snprintf(path,sizeof(path),"%s.%s-preview.json",scene_path,names[mode]);move_copy_scene(scene_path,path);
                snprintf(name,sizeof(name),"%s-preview",names[mode]);transform_expected(name,&original);
            }
            key(editor,SDLK_ESCAPE);
            assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
            assert(!SceneEditorObjectTransformPreview(selected,&baseline,&preview));
            assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
            /* No-op, focus loss, and mode switch all preserve the starting state. */
            move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEBUTTONUP,h.x,h.y);
            assert(SceneEditorDocumentRevision()==revision);
            move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
            SDL_Event lost={0};lost.type=SDL_WINDOWEVENT;lost.window.windowID=SDL_GetWindowID(editor->window);
            lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;SceneEditorSessionRuntimeHandleEvent(editor,&lost);
            assert(!SceneEditorObjectTransformPreview(selected,&baseline,&preview));
            move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
            transform_mode_click(editor,0);assert(!SceneEditorObjectTransformPreview(selected,&baseline,&preview));
            transform_mode_click(editor,mode);assert(SceneEditorDocumentRevision()==revision);
            /* Navigation modifier is never interpreted as a transform gesture. */
            SDL_Keymod mods=SDL_GetModState();SDL_SetModState(KMOD_ALT);
            SDL_Event down={0};down.type=SDL_MOUSEBUTTONDOWN;down.button.button=SDL_BUTTON_LEFT;
            down.button.x=h.x;down.button.y=h.y;down.button.windowID=SDL_GetWindowID(editor->window);
            assert(!SceneEditorObjectMoveGizmoHandleEvent(&down,editor->window));SDL_SetModState(mods);
            move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
            assert(SceneEditorObjectTransformPreview(selected,&baseline,&preview));
            move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
            assert(SceneEditorDocumentRevision()==revision+1 && SceneEditorDocumentIsDirty());
            assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
            transform_compare(&current,&preview);
            assert(fabs(mode==1 ? current.rotation_degrees[axis-1]-original.rotation_degrees[axis-1] :
                current.scale[axis-1]-original.scale[axis-1])>1e-6);
            for (int i=0;i<3;++i) if (i!=axis-1) {
                assert(current.rotation_degrees[i]==original.rotation_degrees[i]);assert(current.scale[i]==original.scale[i]);
            }
            assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
            assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
            transform_compare(&current,&original);assert(SceneEditorDocumentIsDirty());
            assert(SceneEditorDocumentRedo(diagnostics,sizeof(diagnostics)));
            assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
            transform_compare(&current,&preview);
            if (axis==1) {
                snprintf(name,sizeof(name),"workspace_%s_committed.ppm",names[mode]);capture(editor,name);
                assert(SceneEditorChromeActionsSaveAuthoring());
                snprintf(path,sizeof(path),"%s.committed-%s.json",scene_path,names[mode]);move_copy_scene(scene_path,path);
                transform_expected(names[mode],&current);
            }
            assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
            assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
            revision=SceneEditorDocumentRevision();
            char current_id[64];assert(runtime_scene_bridge_get_last_object_id_for_scene_index(selected,current_id,sizeof(current_id)));
            assert(strcmp(id,current_id)==0 && ObjectEditorGetSelectedObjectIndex()==selected);
        }
    }
    /* Workspace, resize, pending modal and changed-document conflicts cancel both modes. */
    for (int mode=1;mode<=2;++mode) {
        transform_mode_click(editor,mode);transform_handle(editor,selected,1,&h,&ex,&ey);
        unsigned long long revision=SceneEditorDocumentRevision();
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
        assert(!SceneEditorObjectTransformPreview(selected,&baseline,&preview));
        SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
        assert(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
        transform_handle(editor,selected,1,&h,&ex,&ey);
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SDL_Event resize={0};resize.type=SDL_WINDOWEVENT;resize.window.windowID=SDL_GetWindowID(editor->window);
        resize.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;
        SceneEditorObjectMoveGizmoHandleEvent(&resize,editor->window);
        assert(!SceneEditorObjectTransformPreview(selected,&baseline,&preview));
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        SceneEditorDocumentTransform newer=original;newer.position[1]+=0.125;
        assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&newer,diagnostics,sizeof(diagnostics)));
        move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
        assert(SceneEditorDocumentRevision()==revision+1);
        assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
        transform_compare(&current,&newer);
        click(editor,backToMenuButton);assert(SceneEditorLifecycleClosePending());
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);
        assert(SceneEditorObjectMoveGizmoActiveAxis()==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE);
        key(editor,SDLK_ESCAPE);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    }
    transform_mode_click(editor,SCENE_EDITOR_OBJECT_TRANSFORM_SCALE);
    /* Extremely negative scale gestures remain positive and cancellable. */
    transform_handle(editor,selected,3,&h,&ex,&ey);
    move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);
    move_pointer(editor,SDL_MOUSEMOTION,h.x-(int)lround(2400*h.ux),h.y-(int)lround(2400*h.uy));
    assert(SceneEditorObjectTransformPreview(selected,&baseline,&preview));
    assert(isfinite(preview.scale[2]) && preview.scale[2]>=1e-6);
    key(editor,SDLK_ESCAPE);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&current,diagnostics,sizeof(diagnostics)));
    transform_compare(&current,&original);assert(!SceneEditorDocumentIsDirty());
    transform_mode_click(editor,0);
    puts("U1.3: readable modes, signed operation units, XYZ rotation/scale transactions and cancellation passed");
}

static RuntimeSceneBridgePrimitiveSeed transform_seed(int selected) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds={0};runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    for (int i=0;seeds.valid && i<seeds.primitive_count;++i)
        if (seeds.primitives[i].scene_object_index==selected) return seeds.primitives[i];
    assert(false);return (RuntimeSceneBridgePrimitiveSeed){0};
}
static void verify_primitive_transform_parity(SceneEditor* editor,int prior_selected) {
    char diagnostics[256];SceneEditorDocumentTransform original,current;
    ObjectEditorSetSelectedObjectIndex(0);assert(SceneEditorFrameViewport(true));
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&original,diagnostics,sizeof(diagnostics)));
    for (int mode=1;mode<=2;++mode) {
        transform_mode_click(editor,mode);
        SceneEditorObjectTransformHandle h;int ex,ey;transform_handle(editor,0,1,&h,&ex,&ey);
        RuntimeSceneBridgePrimitiveSeed baseline=transform_seed(0),preview;
        unsigned long long revision=SceneEditorDocumentRevision();bool dirty=SceneEditorDocumentIsDirty();
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        assert(SceneEditorObjectTransformPreviewPrimitive(&baseline,&preview));
        assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
        char name[80];snprintf(name,sizeof(name),"workspace_primitive_%s_preview.ppm",mode==1 ? "rotate" : "scale");capture(editor,name);
        move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);
        RuntimeSceneBridgePrimitiveSeed committed=transform_seed(0);
        assert(SceneEditorDocumentRevision()==revision+1);
        assert(fabs(preview.width-committed.width)<1e-9 && fabs(preview.height-committed.height)<1e-9 && fabs(preview.depth-committed.depth)<1e-9);
        assert(fabs(preview.axis_u_x-committed.axis_u_x)<1e-9 && fabs(preview.axis_u_y-committed.axis_u_y)<1e-9 && fabs(preview.axis_u_z-committed.axis_u_z)<1e-9);
        assert(fabs(preview.axis_v_x-committed.axis_v_x)<1e-9 && fabs(preview.axis_v_y-committed.axis_v_y)<1e-9 && fabs(preview.axis_v_z-committed.axis_v_z)<1e-9);
        assert(fabs(preview.normal_x-committed.normal_x)<1e-9 && fabs(preview.normal_y-committed.normal_y)<1e-9 && fabs(preview.normal_z-committed.normal_z)<1e-9);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
        assert(SceneEditorDocumentGetTransformForSceneIndex(0,&current,diagnostics,sizeof(diagnostics)));transform_compare(&current,&original);
    }
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    ObjectEditorSetSelectedObjectIndex(prior_selected);transform_mode_click(editor,0);assert(SceneEditorFrameViewport(false));
}

static RayTracingRuntimeMeshAssetInstance transform_mesh(int selected) {
    for (int i=0;i<SceneEditorMeshPreviewStoreInstanceCount();++i) {
        const RayTracingRuntimeMeshAssetInstance* instance=SceneEditorMeshPreviewStoreGetInstance(i);
        if (instance && instance->scene_object_index==selected) return *instance;
    }
    assert(false);return (RayTracingRuntimeMeshAssetInstance){0};
}
static void verify_mesh_transform_parity(SceneEditor* editor,int selected) {
    char diagnostics[256];SceneEditorDocumentTransform original,posed;
    ObjectEditorSetSelectedObjectIndex(selected);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&original,diagnostics,sizeof(diagnostics)));
    posed=original;posed.rotation_degrees[0]=17;posed.rotation_degrees[1]=23;posed.rotation_degrees[2]=-11;
    posed.scale[0]=0.7;posed.scale[1]=1.25;posed.scale[2]=1.6;
    assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&posed,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorFrameViewport(true));
    for (int mode=1;mode<=2;++mode) {
        transform_mode_click(editor,mode);
        SceneEditorObjectTransformHandle h;int ex,ey;transform_handle(editor,selected,2,&h,&ex,&ey);
        RayTracingRuntimeMeshAssetInstance baseline=transform_mesh(selected),preview;
        move_pointer(editor,SDL_MOUSEBUTTONDOWN,h.x,h.y);move_pointer(editor,SDL_MOUSEMOTION,ex,ey);
        assert(SceneEditorObjectTransformPreviewMesh(&baseline,&preview));
        RayTracingRuntimeMeshAssetInstance untouched=transform_mesh(selected);
        assert(memcmp(&untouched,&baseline,sizeof(baseline))==0);
        move_pointer(editor,SDL_MOUSEBUTTONUP,ex,ey);SceneEditorSessionRuntimeRender(editor);
        RayTracingRuntimeMeshAssetInstance committed=transform_mesh(selected);
        assert(fabs(preview.rotation_x-committed.rotation_x)<1e-9);
        assert(fabs(preview.rotation_y-committed.rotation_y)<1e-9);
        assert(fabs(preview.rotation_z-committed.rotation_z)<1e-9);
        assert(fabs(preview.scale_x-committed.scale_x)<1e-9);
        assert(fabs(preview.scale_y-committed.scale_y)<1e-9);
        assert(fabs(preview.scale_z-committed.scale_z)<1e-9);
        assert(preview.rotation_pivot_policy==committed.rotation_pivot_policy);
        assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    }
    assert(SceneEditorDocumentSetTransformForSceneIndex(selected,&original,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    transform_mode_click(editor,0);assert(SceneEditorFrameViewport(false));
}
