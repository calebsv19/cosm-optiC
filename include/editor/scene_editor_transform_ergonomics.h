#ifndef SCENE_EDITOR_TRANSFORM_ERGONOMICS_H
#define SCENE_EDITOR_TRANSFORM_ERGONOMICS_H

#include <stdbool.h>
#include "editor/scene_editor_object_move_gizmo.h"

typedef enum SceneEditorTransformSpace {
    SCENE_EDITOR_TRANSFORM_SPACE_WORLD = 0,
    SCENE_EDITOR_TRANSFORM_SPACE_LOCAL = 1
} SceneEditorTransformSpace;

void SceneEditorTransformErgonomicsReset(void);
SceneEditorTransformSpace SceneEditorTransformSpaceGet(void);
void SceneEditorTransformSpaceToggle(void);
const char* SceneEditorTransformSpaceLabel(void);
bool SceneEditorTransformSnapEnabled(void);
void SceneEditorTransformSnapToggle(void);
const char* SceneEditorTransformSnapLabel(void);
double SceneEditorTransformSnapValue(SceneEditorObjectTransformMode mode, double value);

#endif
