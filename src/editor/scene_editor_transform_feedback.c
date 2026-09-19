#include "editor/scene_editor_transform_feedback.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/object_editor.h"
#include "editor/scene_editor_transform_ergonomics.h"
#include <stdio.h>

bool SceneEditorTransformOperationLabel(int selected,char* out,size_t size) {
    static const char* modes[]={"Move","Rotate","Scale"};
    static const char* instructions[]={"drag an X, Y or Z handle to move", "drag an X, Y or Z ring to rotate", "drag an X, Y or Z handle to scale"};
    SceneEditorDocumentTransform original,preview;
    int mode=(int)SceneEditorObjectTransformModeGet();
    int active_axis=(int)SceneEditorObjectMoveGizmoActiveAxis();
    int axis=active_axis-1;
    if (!out || !size) return false;
    if (active_axis==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM &&
        SceneEditorObjectTransformPreview(selected,&original,&preview)) {
        snprintf(out,size,"Scale all: x%.4g | Release applies; Esc cancels",
            preview.scale[0]/original.scale[0]);
        return true;
    }
    if (axis>=0 && axis<3 && SceneEditorObjectTransformPreview(selected,&original,&preview)) {
        if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE)
            snprintf(out,size,"Move %c: %+.4g %s | Release applies; Esc cancels",'X'+axis,
                preview.position[axis]-original.position[axis],SceneEditorDocumentUnitLabel());
        else if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE)
            snprintf(out,size,"Rotate %c: %+.2f degrees | Release applies; Esc cancels",'X'+axis,
                preview.rotation_degrees[axis]-original.rotation_degrees[axis]);
        else snprintf(out,size,"Scale %c: x%.4g | Release applies; Esc cancels",'X'+axis,
                preview.scale[axis]/original.scale[axis]);
        return true;
    }
    snprintf(out,size,"%s | %s | %s | %s",modes[mode],selected<0 ? "Select an object to transform" :
        !ObjectEditorTransformHandlesVisible() ? "Show transform handles to edit" : instructions[mode],
        SceneEditorTransformSpaceLabel(),SceneEditorTransformSnapLabel());
    return false;
}
void SceneEditorTransformGroupLabel(int group,char* out,size_t size) {
    if (group==0) snprintf(out,size,"Position (scene space, %s)",SceneEditorDocumentUnitLabel());
    else snprintf(out,size,"%s",group==1 ? "Rotation (degrees)" : "Scale (unitless factor)");
}
