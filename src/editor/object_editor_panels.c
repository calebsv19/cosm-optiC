#include "editor/object_editor_panels.h"

#include "editor/object_editor_panels_internal.h"

void ObjectEditorPanels_UpdateLayout(void) {
    ObjectEditorPanels_UpdateLayoutForRegion(NULL);
}

void ObjectEditorPanels_UpdateLayoutForRegion(const SDL_Rect* region) {
    ObjectEditorPanels_UpdateLayoutForRegionImpl(region);
}

void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer) {
    ObjectEditorPanels_DrawAssetListImpl(renderer);
}

void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer) {
    ObjectEditorPanels_DrawMaterialListImpl(renderer);
}

int ObjectEditorPanels_AssetIndexAtPoint(int mx, int my) {
    return ObjectEditorPanels_AssetIndexAtPointImpl(mx, my);
}

int ObjectEditorPanels_MaterialIndexAtPoint(int mx, int my) {
    return ObjectEditorPanels_MaterialIndexAtPointImpl(mx, my);
}

int ObjectEditorPanels_AssetMaxScroll(void) {
    return ObjectEditorPanels_AssetMaxScrollImpl();
}

int ObjectEditorPanels_MaterialMaxScroll(void) {
    return ObjectEditorPanels_MaterialMaxScrollImpl();
}

bool ObjectEditorPanels_SliderValueAtPoint(int mx,
                                           int my,
                                           ObjectEditorPanelSliderKind* out_kind,
                                           double* out_value) {
    return ObjectEditorPanels_SliderValueAtPointImpl(mx, my, out_kind, out_value);
}

bool ObjectEditorPanels_SliderValueForKindAtX(ObjectEditorPanelSliderKind kind,
                                              int mx,
                                              double* out_value) {
    return ObjectEditorPanels_SliderValueForKindAtXImpl(kind, mx, out_value);
}

bool ObjectEditorPanels_MotionActionAtPoint(int mx,
                                            int my,
                                            ObjectEditorPanelMotionAction* out_action) {
    return ObjectEditorPanels_MotionActionAtPointImpl(mx, my, out_action);
}
