#include "editor/scene_editor_object_list.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/object_editor_selection_tracker.h"
static void verify_u23_selection(SceneEditor* editor,const char* path,int selected) {
    char id[128],diagnostics[256],snapshot[4096];
    SceneEditorObjectReadback readback;
    assert(runtime_scene_bridge_get_last_object_id_for_scene_index(selected,id,sizeof(id)));
    SceneEditorSidebarShowLibrary(false);
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorSessionRuntimeRender(editor);
    char first_id[128];assert(runtime_scene_bridge_get_last_object_id_for_scene_index(0,first_id,sizeof(first_id)));
    assert(SceneEditorObjectExecute(SCENE_OBJECT_VISIBILITY,first_id,NULL,false,SceneEditorDocumentRevision(),&readback,diagnostics,sizeof(diagnostics)));
    assert(strcmp(readback.selection.id,id)==0 && readback.selection.runtime_index==selected-1);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(ObjectEditorGetSelectedObjectIndex()==selected);
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(SceneEditorObjectExecute(SCENE_OBJECT_SELECT,id,NULL,false,revision,&readback,diagnostics,sizeof(diagnostics)));
    assert(readback.has_selection && readback.selection_count==1 && strcmp(readback.selection.id,id)==0);
    assert(strcmp(readback.selection.name,id)!=0);
    SDL_Rect row,eye,lock;assert(SceneEditorObjectListRowRects(id,&row,&eye,&lock));
    ObjectEditorSetSelectedObjectIndex(0);click(editor,row);
    SceneEditorObjectInspect(&readback);assert(strcmp(readback.selection.id,id)==0);
    click(editor,lock);SceneEditorObjectInspect(&readback);assert(readback.selection.locked);
    click(editor,lock);SceneEditorObjectInspect(&readback);assert(!readback.selection.locked);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    revision=SceneEditorDocumentRevision();

    assert(!SceneEditorObjectExecute(SCENE_OBJECT_SELECT,"missing-id",NULL,false,revision,&readback,diagnostics,sizeof(diagnostics)));
    assert(strcmp(readback.selection.id,id)==0 && readback.revision==revision);
    assert(!SceneEditorObjectExecute(SCENE_OBJECT_RENAME,id,"stale",false,revision-1,&readback,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorObjectExecute(SCENE_OBJECT_RENAME,id,"U2.3 review mesh",false,revision,&readback,diagnostics,sizeof(diagnostics)));
    assert(readback.revision==revision+1 && strcmp(readback.selection.name,"U2.3 review mesh")==0);
    char label[128];assert(SceneEditorDocumentObjectLabel(selected,label,sizeof(label)));
    assert(strcmp(label,readback.selection.name)==0);
    assert(SceneEditorObjectExecute(SCENE_OBJECT_LOCK,id,NULL,true,readback.revision,&readback,diagnostics,sizeof(diagnostics)));
    SceneEditorDocumentTransform transform;
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected,&transform,diagnostics,sizeof(diagnostics)));
    transform.position[0]+=1;
    assert(!SceneEditorDocumentSetTransformForSceneIndex(selected,&transform,diagnostics,sizeof(diagnostics)));
    assert(strstr(diagnostics,"Unlock"));
    assert(!SceneEditorDocumentRemoveForSceneIndex(selected,diagnostics,sizeof(diagnostics)));
    assert(!SceneEditorDocumentSetMaterialIdForSceneIndex(selected,1,diagnostics,sizeof(diagnostics)));
    assert(!ObjectEditorDeleteObjectIndex(selected));
    ObjectEditorAssignColorToSelected(0x123456);
    assert(SceneEditorDocumentRevision()==readback.revision);

    assert(!SceneEditorObjectExecute(SCENE_OBJECT_RENAME,id,"blocked",false,readback.revision,NULL,diagnostics,sizeof(diagnostics)));
    assert(!SceneEditorObjectExecute(SCENE_OBJECT_VISIBILITY,id,NULL,false,readback.revision,NULL,diagnostics,sizeof(diagnostics)));
    capture(editor,"workspace_u23_locked.ppm");
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    int count=sceneSettings.objectCount;
    assert(SceneEditorObjectExecute(SCENE_OBJECT_VISIBILITY,id,NULL,false,SceneEditorDocumentRevision(),&readback,diagnostics,sizeof(diagnostics)));
    assert(sceneSettings.objectCount==count-1 && readback.selection.runtime_index==-1);
    assert(readback.has_selection && strcmp(readback.selection.id,id)==0 && !readback.selection.visible);
    unsigned long long hidden_revision=readback.revision;
    assert(SceneEditorObjectExecute(SCENE_OBJECT_VISIBILITY,id,NULL,false,hidden_revision,&readback,diagnostics,sizeof(diagnostics)));
    assert(readback.revision==hidden_revision);
    assert(SceneEditorObjectExecute(SCENE_OBJECT_LOCK,id,NULL,true,readback.revision,&readback,diagnostics,sizeof(diagnostics)));
    assert(readback.selection.locked);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    assert(MaterialEditorResolveFocusedObjectIndex()==-1);
    assert(strcmp(ObjectEditorSelectionTrackerId(),id)==0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);

    capture(editor,"workspace_u23_hidden_locked.ppm");
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    snprintf(snapshot,sizeof(snapshot),"%s.u23-flags.json",path);move_copy_scene(path,snapshot);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    assert(ObjectEditorGetSelectedObjectIndex()==selected && strcmp(ObjectEditorSelectionTrackerId(),id)==0);
    capture(editor,"workspace_u23_restored.ppm");
}
