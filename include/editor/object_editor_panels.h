#ifndef OBJECT_EDITOR_PANELS_H
#define OBJECT_EDITOR_PANELS_H

#include <SDL2/SDL.h>

void ObjectEditorPanels_UpdateLayout(void);
void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer);
void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer);

#endif
