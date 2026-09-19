#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/object_editor.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <string.h>
#include <stdio.h>
void SceneEditorObjectInspect(SceneEditorObjectReadback* out) {
    if(!out) return;
    memset(out,0,sizeof(*out));
    out->revision=SceneEditorDocumentRevision();out->dirty=SceneEditorDocumentIsDirty();
    out->can_undo=SceneEditorDocumentCanUndo();out->can_redo=SceneEditorDocumentCanRedo();
    out->has_selection=SceneEditorDocumentObjectById(ObjectEditorSelectionTrackerId(),&out->selection);
    out->selection_count=out->has_selection ? 1 : 0;
}
bool SceneEditorObjectExecute(SceneEditorObjectCommand command,const char* id,
    const char* name,bool value,unsigned long long expected_revision,
    SceneEditorObjectReadback* out,char* diagnostics,size_t size) {
    bool ok=false;SceneEditorDocumentObjectInfo info;
    if(expected_revision!=SceneEditorDocumentRevision()) {
        if(diagnostics && size) snprintf(diagnostics,size,"Scene changed; inspect and retry");goto done;
    }
    if(SceneEditorTransformPanelInteractionActive()) {
        if(diagnostics && size) snprintf(diagnostics,size,"Finish or cancel the active Inspector edit first");goto done;
    }
    if(command==SCENE_OBJECT_SELECT && (!id || !id[0])) {
        ObjectEditorSetSelectedObjectIndex(-1);ok=true;goto done;
    }
    if(!SceneEditorDocumentObjectById(id,&info)) {
        if(diagnostics && size) snprintf(diagnostics,size,"Object ID not found");goto done;
    }
    if(command==SCENE_OBJECT_SELECT) {
        SceneEditorObjectMoveGizmoReset();
        if(info.runtime_index>=0) ObjectEditorSetSelectedObjectIndex(info.runtime_index);
        else ObjectEditorSelectionTrackerSelectId(id);
        ok=true;
    } else if(RayTracingDeepRenderDesktopHost_HasActiveWork()) {
        if(diagnostics && size) snprintf(diagnostics,size,"Object edits wait for the active render");
    } else if(command==SCENE_OBJECT_RENAME) {
        ok=SceneEditorDocumentRenameById(id,name,expected_revision,diagnostics,size);
    } else if(command==SCENE_OBJECT_VISIBILITY || command==SCENE_OBJECT_LOCK) {
        ok=SceneEditorDocumentSetFlag(id,command==SCENE_OBJECT_VISIBILITY ? "visible" : "locked",value,expected_revision,diagnostics,size);
    }
done:
    if(ok && diagnostics && size) snprintf(diagnostics,size,"ok");
    SceneEditorObjectInspect(out);return ok;
}
