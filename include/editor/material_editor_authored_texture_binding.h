#ifndef MATERIAL_EDITOR_AUTHORED_TEXTURE_BINDING_H
#define MATERIAL_EDITOR_AUTHORED_TEXTURE_BINDING_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "ui/shared_theme_font_adapter.h"

void MaterialEditorAuthoredTextureBindingReset(void);
int MaterialEditorAuthoredTextureBindingRenderPaneControls(SDL_Renderer* renderer,
                                                           SDL_Rect content_bounds,
                                                           int cursor_y,
                                                           int bottom_y,
                                                           int focused_object_index,
                                                           RayTracingThemePalette palette);
bool MaterialEditorAuthoredTextureBindingHandleEvent(const SDL_Event* event,
                                                     int focused_object_index);
bool MaterialEditorAuthoredTextureBindingBindForFocused(int focused_object_index,
                                                        const char* manifest_path);
bool MaterialEditorAuthoredTextureBindingClearForFocused(int focused_object_index);
bool MaterialEditorAuthoredTextureBindingGetSummary(int focused_object_index,
                                                    char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    int* out_face_count);
bool MaterialEditorAuthoredTextureBindingGetInvalidSummary(int focused_object_index,
                                                           char* out_manifest_path,
                                                           size_t out_manifest_path_size,
                                                           char* out_binding_mode,
                                                           size_t out_binding_mode_size,
                                                           char* out_reason,
                                                           size_t out_reason_size);

#endif
