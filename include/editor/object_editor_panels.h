#ifndef OBJECT_EDITOR_PANELS_H
#define OBJECT_EDITOR_PANELS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum ObjectEditorPanelSliderKind {
    OBJECT_EDITOR_PANEL_SLIDER_NONE = 0,
    OBJECT_EDITOR_PANEL_SLIDER_COLOR_R = 1,
    OBJECT_EDITOR_PANEL_SLIDER_COLOR_G = 2,
    OBJECT_EDITOR_PANEL_SLIDER_COLOR_B = 3,
    OBJECT_EDITOR_PANEL_SLIDER_COLOR_A = 4,
    OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH = 5
} ObjectEditorPanelSliderKind;

typedef enum ObjectEditorPanelMotionAction {
    OBJECT_EDITOR_PANEL_MOTION_ACTION_NONE = 0,
    OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC = 1,
    OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED = 2,
    OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED = 3
} ObjectEditorPanelMotionAction;

void ObjectEditorPanels_UpdateLayout(void);
void ObjectEditorPanels_UpdateLayoutForRegion(const SDL_Rect* region);
void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer);
void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer);
int ObjectEditorPanels_AssetIndexAtPoint(int mx, int my);
int ObjectEditorPanels_MaterialIndexAtPoint(int mx, int my);
int ObjectEditorPanels_AssetMaxScroll(void);
int ObjectEditorPanels_MaterialMaxScroll(void);
bool ObjectEditorPanels_SliderValueAtPoint(int mx,
                                           int my,
                                           ObjectEditorPanelSliderKind* out_kind,
                                           double* out_value);
bool ObjectEditorPanels_SliderValueForKindAtX(ObjectEditorPanelSliderKind kind,
                                              int mx,
                                              double* out_value);
bool ObjectEditorPanels_MotionActionAtPoint(int mx,
                                            int my,
                                            ObjectEditorPanelMotionAction* out_action);

#endif
