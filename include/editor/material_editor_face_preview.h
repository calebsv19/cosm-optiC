#ifndef MATERIAL_EDITOR_FACE_PREVIEW_H
#define MATERIAL_EDITOR_FACE_PREVIEW_H

#include <SDL2/SDL.h>

#include "scene/object_manager.h"
#include "ui/shared_theme_font_adapter.h"

int MaterialEditorFacePreviewPreferredHeight(const SceneObject* object,
                                             int scene_object_index,
                                             int active_face_group_index,
                                             int panel_width);

bool MaterialEditorFacePreviewResolveDisplaySize(const SceneObject* object,
                                                 int scene_object_index,
                                                 int active_face_group_index,
                                                 int panel_width,
                                                 int* out_width,
                                                 int* out_height);

void MaterialEditorFacePreviewReset(void);
void MaterialEditorFacePreviewInvalidate(void);

int MaterialEditorFacePreviewRenderPane(SDL_Renderer* renderer,
                                        SDL_Rect content_bounds,
                                        int cursor_y,
                                        const SceneObject* object,
                                        int scene_object_index,
                                        int active_face_group_index,
                                        RayTracingThemePalette palette);

#endif
