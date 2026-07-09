#ifndef SCENE_EDITOR_SURFACE_RENDER_H
#define SCENE_EDITOR_SURFACE_RENDER_H

#include <SDL2/SDL.h>

#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_pane_host.h"

void SceneEditorSurfaceRenderLeftPaneContent(SDL_Renderer* renderer,
                                             const SceneEditorPaneLayout* layout,
                                             const SceneEditorControlSurfaceContract* contract,
                                             SDL_Color title_color,
                                             SDL_Color body_color);

void SceneEditorSurfaceRenderRightPaneStatus(SDL_Renderer* renderer,
                                             const SceneEditorPaneLayout* layout,
                                             const SceneEditorControlSurfaceContract* contract,
                                             int status_bottom,
                                             SDL_Color title_color,
                                             SDL_Color body_color);

#endif
