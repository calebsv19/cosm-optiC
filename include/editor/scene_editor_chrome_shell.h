#ifndef SCENE_EDITOR_CHROME_SHELL_H
#define SCENE_EDITOR_CHROME_SHELL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_pane_host.h"
#include "ui/shared_theme_font_adapter.h"

extern SDL_Rect applyButton;
extern SDL_Rect previewButton;
extern SDL_Rect changeModeButton;
extern SDL_Rect saveButton;
extern SDL_Rect backToMenuButton;
extern SDL_Rect selectButton;
extern SDL_Rect addButton;
extern SDL_Rect deleteButton;

RayTracingThemePalette SceneEditorChromeShellResolvePalette(void);
void SceneEditorChromeShellSetActionFeedback(const char* text, Uint32 lifetime_ms);
int SceneEditorChromeShellResolveModeButtonAtPoint(int mx, int my);
bool SceneEditorChromeShellIsButtonHit(int mx, int my);
void SceneEditorChromeShellLayoutFallback(int width, int height);
void SceneEditorChromeShellLayoutFromPane(const SceneEditorPaneLayout* layout);
void SceneEditorChromeShellRender(SDL_Renderer* renderer,
                                  const SceneEditorPaneLayout* layout,
                                  bool layout_valid,
                                  const SceneEditorControlSurfaceContract* contract);

#endif
