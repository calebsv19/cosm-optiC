#ifndef SCENE_EDITOR_TRANSFORM_FEEDBACK_H
#define SCENE_EDITOR_TRANSFORM_FEEDBACK_H
#include <stdbool.h>
#include <stddef.h>
/* Presentation-only summaries: live relative operation vs committed absolute fields. */
bool SceneEditorTransformOperationLabel(int selected,char* out,size_t size);
void SceneEditorTransformGroupLabel(int group,char* out,size_t size);
#endif
