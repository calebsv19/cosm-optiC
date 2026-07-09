#ifndef EDITOR_MATERIAL_EDITOR_KNOB_CONTROL_H
#define EDITOR_MATERIAL_EDITOR_KNOB_CONTROL_H

#include <SDL2/SDL.h>

#include "ui/shared_theme_font_adapter.h"

double MaterialEditorKnobValueFromDrag(double start_value, int start_y, int current_y);
void MaterialEditorKnobDraw(SDL_Renderer* renderer,
                            SDL_Rect bounds,
                            const char* label,
                            double value,
                            RayTracingThemePalette palette);

#endif
