#ifndef SCENE_EDITOR_SHARED_UI_H
#define SCENE_EDITOR_SHARED_UI_H
#include "editor/scene_editor.h"
#include "ui/shared_theme_font_adapter.h"
void SceneEditorSharedUiBegin(SceneEditor *);
void SceneEditorSharedUiEnd(void);
void SceneEditorSharedUiReset(void);
bool SceneEditorSharedUiFilter(SceneEditor *,const SDL_Event *);
bool SceneEditorSharedUiRoute(SceneEditor *,SDL_Event *);
bool SceneEditorSharedUiButton(SDL_Renderer *,SDL_Rect,const char *,bool,SDL_Color,RayTracingThemePalette);
void SceneEditorSharedUiModalButton(SDL_Renderer *,unsigned,unsigned,SDL_Rect,const char *,bool,SDL_Color,RayTracingThemePalette);
void SceneEditorSharedUiPanelButton(SDL_Renderer *,unsigned,unsigned,SDL_Rect,const char *,bool,bool);
void SceneEditorSharedUiExclude(SDL_Rect);
#endif
