#include "editor/scene_editor_transform_feedback.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/object_editor.h"
#include <stdio.h>

bool SceneEditorTransformOperationLabel(int selected,char* out,size_t size) {
    static const char* modes[]={"Move","Rotate","Scale"};
    static const char* instructions[]={"drag an X, Y or Z handle to move", "drag an X, Y or Z ring to rotate", "drag an X, Y or Z handle to scale"};
    SceneEditorDocumentTransform original,preview;
    int mode=(int)SceneEditorObjectTransformModeGet();
    int axis=(int)SceneEditorObjectMoveGizmoActiveAxis()-1;
    if (!out || !size) return false;
    if (axis>=0 && axis<3 && SceneEditorObjectTransformPreview(selected,&original,&preview)) {
        if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE)
            snprintf(out,size,"Op: Move %c %+.4g %s | Release applies; Escape cancels",'X'+axis,
                preview.position[axis]-original.position[axis],SceneEditorDocumentUnitLabel());
        else if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE)
            snprintf(out,size,"Op: Rotate %c %+.2f degrees | Release applies; Escape cancels",'X'+axis,
                preview.rotation_degrees[axis]-original.rotation_degrees[axis]);
        else snprintf(out,size,"Op: Scale %c x%.4g | Release applies; Escape cancels",'X'+axis,
                preview.scale[axis]/original.scale[axis]);
        return true;
    }
    snprintf(out,size,"Op: %s | %s",modes[mode],selected<0 ? "Select an object to transform" :
        !ObjectEditorTransformHandlesVisible() ? "Show transform handles to edit" : instructions[mode]);
    return false;
}
void SceneEditorTransformGroupLabel(int group,char* out,size_t size) {
    if (group==0) snprintf(out,size,"Position (scene space, %s)",SceneEditorDocumentUnitLabel());
    else snprintf(out,size,"%s",group==1 ? "Rotation (degrees)" : "Scale (unitless factor)");
}
