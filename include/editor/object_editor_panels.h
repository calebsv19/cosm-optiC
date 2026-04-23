#ifndef OBJECT_EDITOR_PANELS_H
#define OBJECT_EDITOR_PANELS_H

#include <SDL2/SDL.h>

void ObjectEditorPanels_UpdateLayout(void);
void ObjectEditorPanels_UpdateLayoutForRegion(const SDL_Rect* region);
void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer);
void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer);
int ObjectEditorPanels_AssetIndexAtPoint(int mx, int my);
int ObjectEditorPanels_MaterialIndexAtPoint(int mx, int my);
int ObjectEditorPanels_AssetMaxScroll(void);
int ObjectEditorPanels_MaterialMaxScroll(void);

#endif
