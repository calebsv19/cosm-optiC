#pragma once

#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL.h>

SDL_Color SceneEditorMeshPreviewOutlineColor(int scene_object_index,
                                             int selected_object_index,
                                             int hover_object_index);

size_t SceneEditorMeshPreviewApplyOutlines(uint8_t* rgba,
                                           const double* depth,
                                           const int* owner,
                                           int width,
                                           int height,
                                           int selected_object_index,
                                           int hover_object_index);
