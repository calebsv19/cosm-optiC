#ifndef SCENE_EDITOR_OBJECT_TIMELINE_H
#define SCENE_EDITOR_OBJECT_TIMELINE_H
#include "editor/scene_editor.h"
#include "animation/timeline_entity_binding.h"
bool SceneEditorObjectTimelineFrameOffset(int scene_index,double delta[3]);
bool SceneEditorObjectTimelineEditable(const char* target,char* diagnostics,size_t size);
bool SceneEditorObjectTimelineAdd(const char* id,char* diagnostics,size_t size);
void SceneEditorObjectTimelineBindings(TimelineEntityBindings* bindings);
bool SceneEditorObjectTimelinePosition(const char* id,double position[3]);
bool SceneEditorObjectTimelineControl(const char* name,SDL_Rect* out);
void SceneEditorObjectTimelineDraw(SDL_Renderer* renderer,SDL_Rect pane,int* y);
bool SceneEditorObjectTimelineEvent(SceneEditor* editor,SDL_Event* event);
#endif
