#include "editor/scene_editor_transform_ergonomics.h"

#include <math.h>

static SceneEditorTransformSpace s_space = SCENE_EDITOR_TRANSFORM_SPACE_WORLD;
static bool s_snap;

void SceneEditorTransformErgonomicsReset(void) {
    s_space = SCENE_EDITOR_TRANSFORM_SPACE_WORLD;
    s_snap = false;
}

SceneEditorTransformSpace SceneEditorTransformSpaceGet(void) { return s_space; }
void SceneEditorTransformSpaceToggle(void) {
    s_space = s_space == SCENE_EDITOR_TRANSFORM_SPACE_WORLD
        ? SCENE_EDITOR_TRANSFORM_SPACE_LOCAL : SCENE_EDITOR_TRANSFORM_SPACE_WORLD;
}
const char* SceneEditorTransformSpaceLabel(void) {
    return s_space == SCENE_EDITOR_TRANSFORM_SPACE_LOCAL ? "Local" : "World";
}
bool SceneEditorTransformSnapEnabled(void) { return s_snap; }
void SceneEditorTransformSnapToggle(void) { s_snap = !s_snap; }
const char* SceneEditorTransformSnapLabel(void) { return s_snap ? "Snap on" : "Snap off"; }

double SceneEditorTransformSnapValue(SceneEditorObjectTransformMode mode, double value) {
    if (!s_snap || !isfinite(value)) return value;
    double increment = mode == SCENE_EDITOR_OBJECT_TRANSFORM_MOVE ? 0.1 :
        mode == SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE ? 15.0 : 0.1;
    return round(value / increment) * increment;
}
