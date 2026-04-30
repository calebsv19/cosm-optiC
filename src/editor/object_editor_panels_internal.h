#ifndef OBJECT_EDITOR_PANELS_INTERNAL_H
#define OBJECT_EDITOR_PANELS_INTERNAL_H

#include "editor/object_editor_panels.h"

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/object_editor.h"
#include "editor/scene_editor.h"
#include "geo/shape_library.h"
#include "ui/shared_theme_font_adapter.h"

#define ASSET_ROW_HEIGHT 22
#define PANEL_HEADER_HEIGHT 26
#define PANEL_PADDING 6
#define ASSET_PANEL_WIDTH 200
#define MATERIAL_ROW_HEIGHT 18
#define PANEL_GAP 10
#define OBJECT_EDITOR_SLIDER_TRACK_HEIGHT 8
#define OBJECT_EDITOR_SLIDER_KNOB_WIDTH 12

extern SDL_Rect assetPanelRect;
extern SDL_Rect assetToggleRect;
extern SDL_Rect assetCollapseRect;
extern bool showImports;
extern int selectedAssetIndex;
extern ShapeAssetLibrary assetLib;
extern char importNames[][256];
extern int importCount;
extern SDL_Rect materialPanelRect;
extern SDL_Rect materialCollapseRect;
extern int selectedMaterialIndex;
extern bool assetsCollapsed;
extern bool materialsCollapsed;
extern int assetScroll;
extern int materialScroll;

int ObjectEditorPanels_HeaderHeight(void);
int ObjectEditorPanels_AssetRowHeight(void);
int ObjectEditorPanels_MaterialRowHeight(void);
int ObjectEditorPanels_ColorPreviewSize(void);
int ObjectEditorPanels_ColorSliderSectionHeight(void);
int ObjectEditorPanels_AuxSliderSectionHeight(void);
RayTracingThemePalette ObjectEditorPanels_ResolvePanelPalette(void);
SDL_Color ObjectEditorPanels_PanelBodyFill(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelHeaderFill(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelInactiveRowFill(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelActiveRowFill(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelBorderColor(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelTextColor(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelMutedTextColor(const RayTracingThemePalette* palette);
SDL_Color ObjectEditorPanels_PanelButtonFill(const RayTracingThemePalette* palette,
                                             bool active);
int ObjectEditorPanels_AssetVisibleCount(void);
int ObjectEditorPanels_MaterialVisibleCount(void);
const SceneObject* ObjectEditorPanels_SelectedObject(void);
bool ObjectEditorPanels_ObjectUsesAlpha(const SceneObject* selected);
bool ObjectEditorPanels_ObjectUsesEmissiveStrength(const SceneObject* selected);
int ObjectEditorPanels_ColorSliderCount(void);
int ObjectEditorPanels_AuxSliderCount(void);
int ObjectEditorPanels_ColorSectionHeight(void);
int ObjectEditorPanels_AuxSectionHeight(void);
ObjectEditorPanelSliderKind ObjectEditorPanels_ColorSliderKindForOrdinal(int ordinal);
ObjectEditorPanelSliderKind ObjectEditorPanels_AuxSliderKindForOrdinal(int ordinal);
double ObjectEditorPanels_ValueForSliderKind(ObjectEditorPanelSliderKind kind);
const char* ObjectEditorPanels_LabelForSliderKind(ObjectEditorPanelSliderKind kind);
void ObjectEditorPanels_ResolveAssetListMetrics(int* out_list_y,
                                                int* out_row_h,
                                                int* out_max_rows,
                                                int* out_max_scroll);
void ObjectEditorPanels_ResolveColorSectionMetrics(SDL_Rect* out_section,
                                                   SDL_Rect* out_label,
                                                   SDL_Rect* out_preview);
void ObjectEditorPanels_ResolveColorSliderMetrics(int ordinal,
                                                  SDL_Rect* out_section,
                                                  SDL_Rect* out_label,
                                                  SDL_Rect* out_value,
                                                  SDL_Rect* out_track,
                                                  SDL_Rect* out_knob);
void ObjectEditorPanels_ResolveAuxSliderMetrics(int ordinal,
                                                SDL_Rect* out_section,
                                                SDL_Rect* out_label,
                                                SDL_Rect* out_value,
                                                SDL_Rect* out_track,
                                                SDL_Rect* out_knob);
void ObjectEditorPanels_ResolveMaterialListMetrics(int* out_list_y,
                                                   int* out_row_h,
                                                   int* out_max_rows,
                                                   int* out_max_scroll);

void ObjectEditorPanels_UpdateLayoutForRegionImpl(const SDL_Rect* region);
int ObjectEditorPanels_AssetMaxScrollImpl(void);
int ObjectEditorPanels_MaterialMaxScrollImpl(void);
int ObjectEditorPanels_AssetIndexAtPointImpl(int mx, int my);
int ObjectEditorPanels_MaterialIndexAtPointImpl(int mx, int my);
bool ObjectEditorPanels_SliderValueAtPointImpl(int mx,
                                               int my,
                                               ObjectEditorPanelSliderKind* out_kind,
                                               double* out_value);
bool ObjectEditorPanels_SliderValueForKindAtXImpl(ObjectEditorPanelSliderKind kind,
                                                  int mx,
                                                  double* out_value);

void ObjectEditorPanels_DrawAssetListImpl(SDL_Renderer* renderer);
void ObjectEditorPanels_DrawMaterialListImpl(SDL_Renderer* renderer);

#endif
