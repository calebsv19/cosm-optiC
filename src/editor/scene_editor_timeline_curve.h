#ifndef SCENE_EDITOR_TIMELINE_CURVE_H
#define SCENE_EDITOR_TIMELINE_CURVE_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "scene_editor_timeline_view.h"
bool SceneEditorTimelineCurveEvent(SDL_Event* event,SDL_Rect graph,const TimelineView* view);
void SceneEditorTimelineCurveRender(SDL_Renderer* renderer,SDL_Rect graph,const TimelineView* view);
void SceneEditorTimelineCurveCancel(void);
#endif
