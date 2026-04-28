#ifndef OBJECT_EDITOR_PANELS_H
#define OBJECT_EDITOR_PANELS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum ObjectEditorPanelSliderKind {
    OBJECT_EDITOR_PANEL_SLIDER_NONE = 0,
    OBJECT_EDITOR_PANEL_SLIDER_TRANSPARENCY = 1,
    OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH = 2
} ObjectEditorPanelSliderKind;

void ObjectEditorPanels_UpdateLayout(void);
void ObjectEditorPanels_UpdateLayoutForRegion(const SDL_Rect* region);
void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer);
void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer);
int ObjectEditorPanels_AssetIndexAtPoint(int mx, int my);
int ObjectEditorPanels_MaterialIndexAtPoint(int mx, int my);
int ObjectEditorPanels_ColorIndexAtPoint(int mx, int my);
int ObjectEditorPanels_PackedColorForIndex(int index);
int ObjectEditorPanels_ColorIndexForPackedRGB(int packed);
int ObjectEditorPanels_AssetMaxScroll(void);
int ObjectEditorPanels_MaterialMaxScroll(void);
bool ObjectEditorPanels_SliderValueAtPoint(int mx,
                                           int my,
                                           ObjectEditorPanelSliderKind* out_kind,
                                           double* out_value);
bool ObjectEditorPanels_SliderValueForKindAtX(ObjectEditorPanelSliderKind kind,
                                              int mx,
                                              double* out_value);

#endif
