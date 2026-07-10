#ifndef EDITOR_MATERIAL_EDITOR_COMPACT_RESPONSE_RENDER_H
#define EDITOR_MATERIAL_EDITOR_COMPACT_RESPONSE_RENDER_H

#include <SDL2/SDL.h>

#include "render/render_helper.h"
#include "scene/object_manager.h"
#include "ui/shared_theme_font_adapter.h"

int MaterialEditorDrawCompactResponsePane(SDL_Renderer* renderer,
                                          SDL_Rect bounds,
                                          int cursor_y,
                                          int bottom_y,
                                          const SceneObject* obj,
                                          RayTracingThemePalette palette);

#endif
