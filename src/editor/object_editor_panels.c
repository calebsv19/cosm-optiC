#include "editor/object_editor_panels.h"

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/object_editor.h"
#include "editor/scene_editor.h"
#include "geo/shape_library.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/render_helper.h"
#include "ui/shared_theme_font_adapter.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

static int ObjectEditorHeaderHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, PANEL_HEADER_HEIGHT, 20);
}

static int ObjectEditorAssetRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, ASSET_ROW_HEIGHT, 18);
}

static int ObjectEditorMaterialRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, MATERIAL_ROW_HEIGHT, 16);
}

static int ObjectEditorColorPreviewSize(void) {
    return animation_config_scale_text_point_size(&animSettings, 22, 18);
}

static int ObjectEditorColorSliderSectionHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, 30, 26);
}

static int ObjectEditorAuxSliderSectionHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, 44, 38);
}

static Uint8 object_editor_color_offset(Uint8 value, int offset) {
    int out = (int)value + offset;
    if (out < 0) return 0;
    if (out > 255) return 255;
    return (Uint8)out;
}

static RayTracingThemePalette ObjectEditorResolvePanelPalette(void) {
    RayTracingThemePalette palette = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        palette.background_fill = (SDL_Color){46, 46, 52, 255};
        palette.panel_fill = (SDL_Color){58, 58, 68, 230};
        palette.panel_border = (SDL_Color){95, 95, 112, 255};
        palette.button_fill = (SDL_Color){180, 180, 180, 255};
        palette.button_active_fill = (SDL_Color){70, 140, 215, 255};
        palette.button_text = (SDL_Color){0, 0, 0, 255};
        palette.text_primary = (SDL_Color){220, 220, 230, 255};
        palette.text_muted = (SDL_Color){210, 210, 215, 255};
        palette.accent_primary = (SDL_Color){120, 200, 255, 255};
    }
    return palette;
}

static SDL_Color ObjectEditorPanelBodyFill(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){58, 58, 68, 230};
    return palette->panel_fill;
}

static SDL_Color ObjectEditorPanelHeaderFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = ObjectEditorPanelBodyFill(palette);
    fill.r = object_editor_color_offset(fill.r, 8);
    fill.g = object_editor_color_offset(fill.g, 8);
    fill.b = object_editor_color_offset(fill.b, 8);
    fill.a = 240;
    return fill;
}

static SDL_Color ObjectEditorPanelInactiveRowFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = ObjectEditorPanelBodyFill(palette);
    fill.r = object_editor_color_offset(fill.r, -10);
    fill.g = object_editor_color_offset(fill.g, -10);
    fill.b = object_editor_color_offset(fill.b, -10);
    fill.a = 220;
    return fill;
}

static SDL_Color ObjectEditorPanelActiveRowFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = palette ? palette->accent_primary : (SDL_Color){120, 200, 255, 255};
    fill.r = object_editor_color_offset(fill.r, -24);
    fill.g = object_editor_color_offset(fill.g, -24);
    fill.b = object_editor_color_offset(fill.b, -24);
    fill.a = 228;
    return fill;
}

static SDL_Color ObjectEditorPanelBorderColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){95, 95, 112, 255};
    return palette->panel_border;
}

static SDL_Color ObjectEditorPanelTextColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){220, 220, 230, 255};
    return palette->text_primary;
}

static SDL_Color ObjectEditorPanelMutedTextColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){210, 210, 215, 255};
    return palette->text_muted;
}

static SDL_Color ObjectEditorPanelButtonFill(const RayTracingThemePalette* palette, bool active) {
    SDL_Color fill = active && palette ? palette->button_active_fill :
                     palette ? palette->button_fill : (SDL_Color){180, 180, 180, 255};
    fill.a = 230;
    return fill;
}

static int ObjectEditorAssetVisibleCount(void) {
    return showImports ? importCount : (int)assetLib.count;
}

static void ObjectEditorResolveAssetListMetrics(int* out_list_y,
                                                int* out_row_h,
                                                int* out_max_rows,
                                                int* out_max_scroll) {
    int row_h = ObjectEditorAssetRowHeight();
    int list_y = assetToggleRect.y + assetToggleRect.h + 4;
    int visible = ObjectEditorAssetVisibleCount();
    int row_area_h = assetPanelRect.h - (list_y - assetPanelRect.y) - PANEL_PADDING;
    int max_rows = 1;
    int max_scroll = 0;
    if (row_area_h < row_h) row_area_h = row_h;
    max_rows = row_area_h / row_h;
    if (max_rows < 1) max_rows = 1;
    max_scroll = visible - max_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (out_list_y) *out_list_y = list_y;
    if (out_row_h) *out_row_h = row_h;
    if (out_max_rows) *out_max_rows = max_rows;
    if (out_max_scroll) *out_max_scroll = max_scroll;
}

static int ObjectEditorMaterialVisibleCount(void) {
    return MaterialManagerCount();
}

static const SceneObject* ObjectEditorPanels_SelectedObject(void) {
    int selected_object_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_object_index < 0 || selected_object_index >= sceneSettings.objectCount) {
        return NULL;
    }
    return &sceneSettings.sceneObjects[selected_object_index];
}

static bool ObjectEditorPanels_ObjectUsesAlpha(const SceneObject* selected) {
    return selected && selected->material_id == MATERIAL_PRESET_TRANSPARENT;
}

static bool ObjectEditorPanels_ObjectUsesEmissiveStrength(const SceneObject* selected) {
    return selected && selected->material_id == MATERIAL_PRESET_EMISSIVE;
}

static int ObjectEditorPanels_ColorSliderCount(void) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected) return 0;
    return 3 + (ObjectEditorPanels_ObjectUsesAlpha(selected) ? 1 : 0);
}

static int ObjectEditorPanels_AuxSliderCount(void) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    return ObjectEditorPanels_ObjectUsesEmissiveStrength(selected) ? 1 : 0;
}

static int ObjectEditorColorSectionHeight(void) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int label_h = animation_config_scale_text_point_size(&animSettings, 14, 12);
    int preview = ObjectEditorColorPreviewSize();
    int row_h = ObjectEditorColorSliderSectionHeight();
    if (color_slider_count <= 0) return 0;
    return label_h + 6 + preview + 8 + color_slider_count * row_h +
           (color_slider_count - 1) * 4;
}

static int ObjectEditorAuxSectionHeight(void) {
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    if (aux_slider_count <= 0) return 0;
    return aux_slider_count * ObjectEditorAuxSliderSectionHeight() +
           (aux_slider_count - 1) * 6 + 8;
}

static ObjectEditorPanelSliderKind ObjectEditorPanels_ColorSliderKindForOrdinal(int ordinal) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected) return OBJECT_EDITOR_PANEL_SLIDER_NONE;
    switch (ordinal) {
        case 0: return OBJECT_EDITOR_PANEL_SLIDER_COLOR_R;
        case 1: return OBJECT_EDITOR_PANEL_SLIDER_COLOR_G;
        case 2: return OBJECT_EDITOR_PANEL_SLIDER_COLOR_B;
        case 3:
            return ObjectEditorPanels_ObjectUsesAlpha(selected)
                       ? OBJECT_EDITOR_PANEL_SLIDER_COLOR_A
                       : OBJECT_EDITOR_PANEL_SLIDER_NONE;
        default:
            return OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
}

static ObjectEditorPanelSliderKind ObjectEditorPanels_AuxSliderKindForOrdinal(int ordinal) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected || ordinal != 0) return OBJECT_EDITOR_PANEL_SLIDER_NONE;
    return ObjectEditorPanels_ObjectUsesEmissiveStrength(selected)
               ? OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH
               : OBJECT_EDITOR_PANEL_SLIDER_NONE;
}

static double ObjectEditorPanels_ValueForSliderKind(ObjectEditorPanelSliderKind kind) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected) return 0.0;
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_R) {
        return (double)SceneObjectColorR(selected) / 255.0;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_G) {
        return (double)SceneObjectColorG(selected) / 255.0;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_B) {
        return (double)SceneObjectColorB(selected) / 255.0;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_A) {
        return selected->alpha;
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH) {
        return selected->emissiveStrength;
    }
    return 0.0;
}

static const char* ObjectEditorPanels_LabelForSliderKind(ObjectEditorPanelSliderKind kind) {
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_R) {
        return "R";
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_G) {
        return "G";
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_B) {
        return "B";
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_COLOR_A) {
        return "A";
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH) {
        return "Emitter Strength";
    }
    return "";
}

static void ObjectEditorResolveColorSectionMetrics(SDL_Rect* out_section,
                                                   SDL_Rect* out_label,
                                                   SDL_Rect* out_preview) {
    int header_h = ObjectEditorHeaderHeight();
    int color_h = ObjectEditorColorSectionHeight();
    int preview_size = ObjectEditorColorPreviewSize();
    int label_h = animation_config_scale_text_point_size(&animSettings, 14, 12);
    SDL_Rect section = {
        materialPanelRect.x + PANEL_PADDING,
        materialPanelRect.y + PANEL_PADDING + header_h + 4,
        materialPanelRect.w - PANEL_PADDING * 2,
        color_h
    };
    SDL_Rect label = {
        section.x,
        section.y,
        section.w - preview_size - 8,
        label_h
    };
    SDL_Rect preview = {
        section.x + section.w - preview_size,
        section.y,
        preview_size,
        preview_size
    };

    if (out_section) *out_section = section;
    if (out_label) *out_label = label;
    if (out_preview) *out_preview = preview;
}

static void ObjectEditorResolveColorSliderMetrics(int ordinal,
                                                  SDL_Rect* out_section,
                                                  SDL_Rect* out_label,
                                                  SDL_Rect* out_value,
                                                  SDL_Rect* out_track,
                                                  SDL_Rect* out_knob) {
    SDL_Rect color_section = {0};
    SDL_Rect preview = {0};
    int section_h = ObjectEditorColorSliderSectionHeight();
    SDL_Rect section = {0};
    SDL_Rect label = {0};
    SDL_Rect value = {0};
    SDL_Rect track = {0};

    ObjectEditorResolveColorSectionMetrics(&color_section, &label, &preview);
    section = (SDL_Rect){
        color_section.x,
        preview.y + preview.h + 8 + ordinal * (section_h + 4),
        color_section.w,
        section_h
    };
    label = (SDL_Rect){
        section.x,
        section.y,
        animation_config_scale_text_point_size(&animSettings, 16, 14),
        animation_config_scale_text_point_size(&animSettings, 14, 12)
    };
    value = (SDL_Rect){
        section.x + section.w - animation_config_scale_text_point_size(&animSettings, 48, 40),
        section.y,
        animation_config_scale_text_point_size(&animSettings, 48, 40),
        label.h
    };
    track = (SDL_Rect){
        label.x + label.w + 6,
        section.y + 2,
        value.x - (label.x + label.w + 12),
        animation_config_scale_text_point_size(&animSettings,
                                               OBJECT_EDITOR_SLIDER_TRACK_HEIGHT,
                                               6)
    };

    if (out_section) *out_section = section;
    if (out_label) *out_label = label;
    if (out_value) *out_value = value;
    if (out_track) *out_track = track;
    if (out_knob) {
        double value_norm = ObjectEditorPanels_ValueForSliderKind(
            ObjectEditorPanels_ColorSliderKindForOrdinal(ordinal));
        int knob_x = track.x +
                     (int)lround(value_norm *
                                 (double)(track.w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH));
        *out_knob = (SDL_Rect){knob_x,
                               track.y - 4 + (track.h - OBJECT_EDITOR_SLIDER_TRACK_HEIGHT) / 2,
                               OBJECT_EDITOR_SLIDER_KNOB_WIDTH,
                               track.h + 8};
    }
}

static void ObjectEditorResolveAuxSliderMetrics(int ordinal,
                                                SDL_Rect* out_section,
                                                SDL_Rect* out_label,
                                                SDL_Rect* out_value,
                                                SDL_Rect* out_track,
                                                SDL_Rect* out_knob) {
    int header_h = ObjectEditorHeaderHeight();
    int section_h = ObjectEditorAuxSliderSectionHeight();
    int color_h = ObjectEditorColorSectionHeight();
    SDL_Rect section = {
        materialPanelRect.x + PANEL_PADDING,
        materialPanelRect.y + PANEL_PADDING + header_h + 4 + color_h + 8 +
            ordinal * (section_h + 6),
        materialPanelRect.w - PANEL_PADDING * 2,
        section_h
    };
    SDL_Rect label = {
        section.x,
        section.y,
        section.w / 2,
        animation_config_scale_text_point_size(&animSettings, 14, 12)
    };
    SDL_Rect value = {
        section.x + section.w / 2,
        section.y,
        section.w / 2,
        label.h
    };
    SDL_Rect track = {
        section.x + 2,
        label.y + label.h + 8,
        section.w - 4,
        animation_config_scale_text_point_size(&animSettings,
                                               OBJECT_EDITOR_SLIDER_TRACK_HEIGHT,
                                               6)
    };

    if (out_section) *out_section = section;
    if (out_label) *out_label = label;
    if (out_value) *out_value = value;
    if (out_track) *out_track = track;
    if (out_knob) {
        double value_norm = ObjectEditorPanels_ValueForSliderKind(
            ObjectEditorPanels_AuxSliderKindForOrdinal(ordinal));
        int knob_x = track.x +
                     (int)lround(value_norm *
                                 (double)(track.w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH));
        *out_knob = (SDL_Rect){knob_x,
                               track.y - 4,
                               OBJECT_EDITOR_SLIDER_KNOB_WIDTH,
                               track.h + 8};
    }
}

static void ObjectEditorResolveMaterialListMetrics(int* out_list_y,
                                                   int* out_row_h,
                                                   int* out_max_rows,
                                                   int* out_max_scroll) {
    int row_h = ObjectEditorMaterialRowHeight();
    int header_h = ObjectEditorHeaderHeight();
    int color_h = ObjectEditorColorSectionHeight();
    int aux_h = ObjectEditorAuxSectionHeight();
    int list_y = 0;
    int visible = ObjectEditorMaterialVisibleCount();
    int row_area_h = materialPanelRect.h - (list_y - materialPanelRect.y) - PANEL_PADDING;
    int max_rows = 1;
    int max_scroll = 0;
    list_y = materialPanelRect.y + PANEL_PADDING + header_h + 4;
    if (color_h > 0) list_y += color_h + 8;
    if (aux_h > 0) list_y += aux_h;
    list_y += 4;
    row_area_h = materialPanelRect.h - (list_y - materialPanelRect.y) - PANEL_PADDING;
    if (row_area_h < row_h) row_area_h = row_h;
    max_rows = row_area_h / row_h;
    if (max_rows < 1) max_rows = 1;
    max_scroll = visible - max_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (out_list_y) *out_list_y = list_y;
    if (out_row_h) *out_row_h = row_h;
    if (out_max_rows) *out_max_rows = max_rows;
    if (out_max_scroll) *out_max_scroll = max_scroll;
}

int ObjectEditorPanels_AssetMaxScroll(void) {
    int max_scroll = 0;
    ObjectEditorResolveAssetListMetrics(NULL, NULL, NULL, &max_scroll);
    return max_scroll;
}

int ObjectEditorPanels_MaterialMaxScroll(void) {
    int max_scroll = 0;
    ObjectEditorResolveMaterialListMetrics(NULL, NULL, NULL, &max_scroll);
    return max_scroll;
}

int ObjectEditorPanels_AssetIndexAtPoint(int mx, int my) {
    int list_y = 0;
    int row_h = 0;
    int max_rows = 0;
    int visible = ObjectEditorAssetVisibleCount();
    int idx = -1;
    if (assetsCollapsed) return -1;
    if (mx < assetPanelRect.x || mx >= assetPanelRect.x + assetPanelRect.w ||
        my < assetPanelRect.y || my >= assetPanelRect.y + assetPanelRect.h) {
        return -1;
    }
    ObjectEditorResolveAssetListMetrics(&list_y, &row_h, &max_rows, NULL);
    if (my < list_y) return -1;
    idx = (my - list_y) / row_h + assetScroll;
    if (idx < 0 || idx >= visible) return -1;
    if (idx >= assetScroll + max_rows) return -1;
    return idx;
}

int ObjectEditorPanels_MaterialIndexAtPoint(int mx, int my) {
    int list_y = 0;
    int row_h = 0;
    int max_rows = 0;
    int visible = ObjectEditorMaterialVisibleCount();
    int idx = -1;
    if (materialsCollapsed) return -1;
    if (mx < materialPanelRect.x || mx >= materialPanelRect.x + materialPanelRect.w ||
        my < materialPanelRect.y || my >= materialPanelRect.y + materialPanelRect.h) {
        return -1;
    }
    ObjectEditorResolveMaterialListMetrics(&list_y, &row_h, &max_rows, NULL);
    if (my < list_y) return -1;
    idx = (my - list_y) / row_h + materialScroll;
    if (idx < 0 || idx >= visible) return -1;
    if (idx >= materialScroll + max_rows) return -1;
    return idx;
}

static bool ObjectEditorPanels_PointInRect(int mx, int my, const SDL_Rect* rect) {
    return rect && mx >= rect->x && mx < rect->x + rect->w && my >= rect->y &&
           my < rect->y + rect->h;
}

static double ObjectEditorPanels_SliderValueFromTrackX(const SDL_Rect* track, int mx) {
    double denom = (double)(track->w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH);
    double raw = 0.0;
    if (denom <= 0.0) return 0.0;
    raw = ((double)mx - (double)track->x - (OBJECT_EDITOR_SLIDER_KNOB_WIDTH * 0.5)) / denom;
    if (raw < 0.0) return 0.0;
    if (raw > 1.0) return 1.0;
    return raw;
}

bool ObjectEditorPanels_SliderValueAtPoint(int mx,
                                           int my,
                                           ObjectEditorPanelSliderKind* out_kind,
                                           double* out_value) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect section = {0};
        SDL_Rect track = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_ColorSliderKindForOrdinal(i);
        ObjectEditorResolveColorSliderMetrics(i, &section, NULL, NULL, &track, NULL);
        if (!ObjectEditorPanels_PointInRect(mx, my, &section)) continue;
        if (out_kind) *out_kind = kind;
        if (out_value) *out_value = ObjectEditorPanels_SliderValueFromTrackX(&track, mx);
        return kind != OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect section = {0};
        SDL_Rect track = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_AuxSliderKindForOrdinal(i);
        ObjectEditorResolveAuxSliderMetrics(i, &section, NULL, NULL, &track, NULL);
        if (!ObjectEditorPanels_PointInRect(mx, my, &section)) continue;
        if (out_kind) *out_kind = kind;
        if (out_value) *out_value = ObjectEditorPanels_SliderValueFromTrackX(&track, mx);
        return kind != OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
    return false;
}

bool ObjectEditorPanels_SliderValueForKindAtX(ObjectEditorPanelSliderKind kind,
                                              int mx,
                                              double* out_value) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect track = {0};
        if (ObjectEditorPanels_ColorSliderKindForOrdinal(i) != kind) continue;
        ObjectEditorResolveColorSliderMetrics(i, NULL, NULL, NULL, &track, NULL);
        if (out_value) *out_value = ObjectEditorPanels_SliderValueFromTrackX(&track, mx);
        return true;
    }
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect track = {0};
        if (ObjectEditorPanels_AuxSliderKindForOrdinal(i) != kind) continue;
        ObjectEditorResolveAuxSliderMetrics(i, NULL, NULL, NULL, &track, NULL);
        if (out_value) *out_value = ObjectEditorPanels_SliderValueFromTrackX(&track, mx);
        return true;
    }
    return false;
}

static int ObjectEditorResolveMaterialSwatchColor(int material_id) {
    const Material* mat = MaterialManagerGet(material_id);
    int r = 220;
    int g = 220;
    int b = 220;

    if (mat) {
        r = (int)(mat->base_color.x * 255.0f + 0.5f);
        g = (int)(mat->base_color.y * 255.0f + 0.5f);
        b = (int)(mat->base_color.z * 255.0f + 0.5f);
    }
    if (abs(r - g) < 8 && abs(g - b) < 8) {
        switch (material_id) {
            case MATERIAL_PRESET_MIRROR:
                r = 156; g = 196; b = 255;
                break;
            case MATERIAL_PRESET_ROUGH_METAL:
                r = 214; g = 170; b = 116;
                break;
            case MATERIAL_PRESET_GLOSSY:
                r = 236; g = 120; b = 120;
                break;
            case MATERIAL_PRESET_EMISSIVE:
                r = 255; g = 220; b = 92;
                break;
            case MATERIAL_PRESET_TRANSPARENT:
                r = 144; g = 232; b = 255;
                break;
            case MATERIAL_PRESET_DEFAULT:
            default:
                r = 220; g = 220; b = 220;
                break;
        }
    }
    return (r << 16) | (g << 8) | b;
}

static SDL_Color ObjectEditorPanelColorFromPackedRGB(int packed, Uint8 alpha) {
    SDL_Color color;
    color.r = (Uint8)((packed >> 16) & 0xFF);
    color.g = (Uint8)((packed >> 8) & 0xFF);
    color.b = (Uint8)(packed & 0xFF);
    color.a = alpha;
    return color;
}

void ObjectEditorPanels_UpdateLayoutForRegion(const SDL_Rect* region) {
    SceneEditorPaneLayout pane_layout = {0};
    int headerH = ObjectEditorHeaderHeight();
    int assetRowH = ObjectEditorAssetRowHeight();
    int materialRowH = ObjectEditorMaterialRowHeight();
    int panelMaxH = 0;
    SDL_Rect available = {20, 40, ASSET_PANEL_WIDTH, 360};
    int panelW = ASSET_PANEL_WIDTH;
    int x = 20;
    int y = 40;
    int available_h = 0;
    int assetMinContent = 0;
    int materialMinContent = 0;
    int assetContent = 0;
    int matContent = 0;
    int colorContent = 0;
    int auxSliderContent = 0;
    int overflow = 0;
    if (region && region->w > 0 && region->h > 0) {
        available = *region;
    } else if (SceneEditorGetPaneLayout(&pane_layout)) {
        available = pane_layout.left_content_rect;
        available.y += 150;
        available.h -= 150;
    }
    if (available.w < 120) available.w = 120;
    if (available.h < 80) available.h = 80;

    x = available.x;
    y = available.y;
    panelW = available.w;
    available_h = available.h;
    panelMaxH = available_h;
    if (panelW > ASSET_PANEL_WIDTH) panelW = ASSET_PANEL_WIDTH;
    if (panelW < 160 && available.w >= 160) panelW = available.w;
    if (panelW < 120) panelW = 120;
    int headerW = panelW - PANEL_PADDING * 2;
    int assetRows = showImports ? importCount : (int)assetLib.count;
    colorContent = ObjectEditorColorSectionHeight();
    auxSliderContent = ObjectEditorAuxSectionHeight();
    if (assetRows < 1) assetRows = 1;
    assetMinContent = headerH + PANEL_PADDING * 2 + assetRowH;
    materialMinContent = headerH + PANEL_PADDING * 2 +
                         (colorContent > 0 ? colorContent + 8 : 0) + auxSliderContent +
                         materialRowH;
    assetContent = headerH + PANEL_PADDING * 2 + 4 + assetRows * assetRowH;
    if (assetContent > panelMaxH) assetContent = panelMaxH;
    assetPanelRect = (SDL_Rect){x, y, panelW, assetContent};
    assetToggleRect = (SDL_Rect){x + PANEL_PADDING,
                                 y + PANEL_PADDING,
                                 headerW - 20,
                                 headerH - PANEL_PADDING};
    assetCollapseRect = (SDL_Rect){assetToggleRect.x + assetToggleRect.w + 4,
                                   assetToggleRect.y,
                                   16,
                                   16};

    int matRows = MaterialManagerCount();
    if (matRows < 1) matRows = 1;
    matContent = headerH + PANEL_PADDING * 2 +
                 (colorContent > 0 ? colorContent + 8 : 0) + auxSliderContent + 4 +
                 matRows * materialRowH;
    if (matContent > panelMaxH) matContent = panelMaxH;

    overflow = assetContent + PANEL_GAP + matContent - available_h;
    if (overflow > 0 && assetContent > assetMinContent) {
        int reducible = assetContent - assetMinContent;
        int trim = overflow < reducible ? overflow : reducible;
        assetContent -= trim;
        overflow -= trim;
    }
    if (overflow > 0 && matContent > materialMinContent) {
        int reducible = matContent - materialMinContent;
        int trim = overflow < reducible ? overflow : reducible;
        matContent -= trim;
        overflow -= trim;
    }
    if (overflow > 0) {
        int totalAvailable = available_h - PANEL_GAP;
        if (totalAvailable < 0) totalAvailable = available_h;
        if (matContent > totalAvailable) matContent = totalAvailable;
        if (assetContent > totalAvailable - matContent) {
            assetContent = totalAvailable - matContent;
        }
        if (assetContent < assetMinContent) assetContent = assetMinContent;
        if (matContent < materialMinContent) matContent = materialMinContent;
    }

    assetPanelRect.h = assetContent;
    materialPanelRect = (SDL_Rect){x, assetPanelRect.y + assetPanelRect.h + PANEL_GAP, panelW, matContent};
    materialCollapseRect = (SDL_Rect){materialPanelRect.x + materialPanelRect.w - PANEL_PADDING - 16,
                                      materialPanelRect.y + PANEL_PADDING,
                                      16,
                                      16};
}

void ObjectEditorPanels_UpdateLayout(void) {
    ObjectEditorPanels_UpdateLayoutForRegion(NULL);
}

void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer) {
    SDL_Rect panel = assetPanelRect;
    RayTracingThemePalette palette = ObjectEditorResolvePanelPalette();
    SDL_Color textColor = ObjectEditorPanelTextColor(&palette);
    SDL_Color bodyFill = ObjectEditorPanelBodyFill(&palette);
    SDL_Color borderColor = ObjectEditorPanelBorderColor(&palette);
    SDL_Color activeRowFill = ObjectEditorPanelActiveRowFill(&palette);
    SDL_Color inactiveRowFill = ObjectEditorPanelInactiveRowFill(&palette);
    SDL_Color toggleFill = ObjectEditorPanelButtonFill(&palette, showImports);
    SDL_Color collapseFill = ObjectEditorPanelButtonFill(&palette, !assetsCollapsed);
    SDL_Rect labelRect = {0};
    int listY = 0;
    int assetRowH = 0;
    int maxRows = 0;
    int visible = ObjectEditorAssetVisibleCount();
#if !USE_VULKAN
    SDL_BlendMode prevMode;
    SDL_GetRenderDrawBlendMode(renderer, &prevMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, bodyFill.r, bodyFill.g, bodyFill.b, bodyFill.a);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, toggleFill.r, toggleFill.g, toggleFill.b, toggleFill.a);
    SDL_RenderFillRect(renderer, &assetToggleRect);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &assetToggleRect);
    RenderLabelText(renderer, assetToggleRect, showImports ? "Imports" : "Assets", textColor);
    SDL_SetRenderDrawColor(renderer, collapseFill.r, collapseFill.g, collapseFill.b, collapseFill.a);
    SDL_RenderFillRect(renderer, &assetCollapseRect);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &assetCollapseRect);
    RenderLabelText(renderer,
                    assetCollapseRect,
                    assetsCollapsed ? "+" : "-",
                    textColor);

    if (assetsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
        return;
    }

    ObjectEditorResolveAssetListMetrics(&listY, &assetRowH, &maxRows, NULL);
    int maxScroll = visible - maxRows;
    if (maxScroll < 0) maxScroll = 0;
    if (assetScroll > maxScroll) assetScroll = maxScroll;
    int start = assetScroll;
    int end = start + maxRows;
    if (end > visible) end = visible;
    for (int i = start; i < end; ++i) {
        int rowIdx = i - start;
        SDL_Rect row = {panel.x + PANEL_PADDING,
                        listY + rowIdx * assetRowH,
                        panel.w - PANEL_PADDING * 2,
                        assetRowH - 2};
        bool selected = (!showImports && i == selectedAssetIndex);
        SDL_SetRenderDrawColor(renderer,
                               selected ? activeRowFill.r : inactiveRowFill.r,
                               selected ? activeRowFill.g : inactiveRowFill.g,
                               selected ? activeRowFill.b : inactiveRowFill.b,
                               selected ? activeRowFill.a : inactiveRowFill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, selected ? 255 : 200);
        SDL_RenderDrawRect(renderer, &row);

        const char* label = "";
        char buffer[256];
        if (showImports) {
            label = importNames[i];
        } else if (assetLib.assets && assetLib.assets[i].name) {
            label = assetLib.assets[i].name;
        } else {
            snprintf(buffer, sizeof(buffer), "asset_%d", i);
            label = buffer;
        }
        labelRect = (SDL_Rect){row.x + 8, row.y, row.w - 12, row.h};
        RenderLabelTextLeft(renderer, labelRect, label, textColor);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
}

void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer) {
    SDL_Rect panel = materialPanelRect;
    RayTracingThemePalette palette = ObjectEditorResolvePanelPalette();
    int materialRowH = 0;
    int headerH = ObjectEditorHeaderHeight();
    SDL_Color textColor = ObjectEditorPanelTextColor(&palette);
    SDL_Color mutedColor = ObjectEditorPanelMutedTextColor(&palette);
    SDL_Color headerFill = ObjectEditorPanelHeaderFill(&palette);
    SDL_Color bodyFill = ObjectEditorPanelBodyFill(&palette);
    SDL_Color borderColor = ObjectEditorPanelBorderColor(&palette);
    SDL_Color activeRowFill = ObjectEditorPanelActiveRowFill(&palette);
    SDL_Color inactiveRowFill = ObjectEditorPanelInactiveRowFill(&palette);
    SDL_Color collapseFill = ObjectEditorPanelButtonFill(&palette, !materialsCollapsed);
    SDL_Rect headerBar = {0};
    SDL_Rect titleRect = {0};
    SDL_Rect colorLabelRect = {0};
    SDL_Rect colorPreviewRect = {0};
    int color_slider_count = 0;
    int aux_slider_count = 0;
    int listY = 0;
    int maxRows = 0;
#if !USE_VULKAN
    SDL_BlendMode prevMode;
    SDL_GetRenderDrawBlendMode(renderer, &prevMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, bodyFill.r, bodyFill.g, bodyFill.b, bodyFill.a);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &panel);

    headerBar = (SDL_Rect){panel.x + PANEL_PADDING,
                           panel.y + PANEL_PADDING,
                           panel.w - PANEL_PADDING * 2,
                           headerH - PANEL_PADDING + 2};
    SDL_SetRenderDrawColor(renderer, headerFill.r, headerFill.g, headerFill.b, headerFill.a);
    SDL_RenderFillRect(renderer, &headerBar);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &headerBar);

    titleRect = (SDL_Rect){panel.x + PANEL_PADDING,
                           panel.y + PANEL_PADDING,
                           panel.w - PANEL_PADDING * 2 - materialCollapseRect.w - 6,
                           headerH - PANEL_PADDING};
    RenderLabelTextLeft(renderer, titleRect, "Materials", mutedColor);

    SDL_SetRenderDrawColor(renderer, collapseFill.r, collapseFill.g, collapseFill.b, collapseFill.a);
    SDL_RenderFillRect(renderer, &materialCollapseRect);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &materialCollapseRect);
    RenderLabelText(renderer,
                    materialCollapseRect,
                    materialsCollapsed ? "+" : "-",
                    textColor);

    if (materialsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
        return;
    }

    ObjectEditorResolveColorSectionMetrics(NULL, &colorLabelRect, &colorPreviewRect);
    if (ObjectEditorPanels_ColorSliderCount() > 0) {
        const SceneObject* selected = ObjectEditorPanels_SelectedObject();
        SDL_Color previewColor = selected
                                     ? ObjectEditorPanelColorFromPackedRGB(
                                           selected->color,
                                           ObjectEditorPanels_ObjectUsesAlpha(selected)
                                               ? SceneObjectAlphaByte(selected)
                                               : 255)
                                     : (SDL_Color){255, 255, 255, 255};
        RenderLabelTextLeft(renderer, colorLabelRect, "Object Color", mutedColor);
        SDL_SetRenderDrawColor(renderer,
                               inactiveRowFill.r,
                               inactiveRowFill.g,
                               inactiveRowFill.b,
                               220);
        SDL_RenderFillRect(renderer, &colorPreviewRect);
        SDL_SetRenderDrawColor(renderer,
                               previewColor.r,
                               previewColor.g,
                               previewColor.b,
                               previewColor.a);
        SDL_RenderFillRect(renderer, &colorPreviewRect);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
        SDL_RenderDrawRect(renderer, &colorPreviewRect);
    }

    color_slider_count = ObjectEditorPanels_ColorSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect labelRect = {0};
        SDL_Rect valueRect = {0};
        SDL_Rect trackRect = {0};
        SDL_Rect knobRect = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_ColorSliderKindForOrdinal(i);
        double value = ObjectEditorPanels_ValueForSliderKind(kind);
        char valueText[32];

        ObjectEditorResolveColorSliderMetrics(i,
                                              NULL,
                                              &labelRect,
                                              &valueRect,
                                              &trackRect,
                                              &knobRect);
        RenderLabelTextLeft(renderer,
                            labelRect,
                            ObjectEditorPanels_LabelForSliderKind(kind),
                            mutedColor);
        snprintf(valueText, sizeof(valueText), "%d", (int)lround(value * 255.0));
        RenderLabelText(renderer, valueRect, valueText, textColor);
        SDL_SetRenderDrawColor(renderer,
                               inactiveRowFill.r,
                               inactiveRowFill.g,
                               inactiveRowFill.b,
                               220);
        SDL_RenderFillRect(renderer, &trackRect);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 220);
        SDL_RenderDrawRect(renderer, &trackRect);
        SDL_SetRenderDrawColor(renderer,
                               activeRowFill.r,
                               activeRowFill.g,
                               activeRowFill.b,
                               activeRowFill.a);
        SDL_RenderFillRect(renderer, &knobRect);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
        SDL_RenderDrawRect(renderer, &knobRect);
    }

    aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect labelRect = {0};
        SDL_Rect valueRect = {0};
        SDL_Rect trackRect = {0};
        SDL_Rect knobRect = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_AuxSliderKindForOrdinal(i);
        double value = ObjectEditorPanels_ValueForSliderKind(kind);
        char valueText[32];

        ObjectEditorResolveAuxSliderMetrics(i,
                                            NULL,
                                            &labelRect,
                                            &valueRect,
                                            &trackRect,
                                            &knobRect);
        RenderLabelTextLeft(renderer,
                            labelRect,
                            ObjectEditorPanels_LabelForSliderKind(kind),
                            mutedColor);
        snprintf(valueText, sizeof(valueText), "%.2f", value);
        RenderLabelText(renderer, valueRect, valueText, textColor);
        SDL_SetRenderDrawColor(renderer,
                               inactiveRowFill.r,
                               inactiveRowFill.g,
                               inactiveRowFill.b,
                               220);
        SDL_RenderFillRect(renderer, &trackRect);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 220);
        SDL_RenderDrawRect(renderer, &trackRect);
        SDL_SetRenderDrawColor(renderer,
                               activeRowFill.r,
                               activeRowFill.g,
                               activeRowFill.b,
                               activeRowFill.a);
        SDL_RenderFillRect(renderer, &knobRect);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
        SDL_RenderDrawRect(renderer, &knobRect);
    }

    int count = MaterialManagerCount();
    ObjectEditorResolveMaterialListMetrics(&listY, &materialRowH, &maxRows, NULL);
    int maxScroll = count - maxRows;
    if (maxScroll < 0) maxScroll = 0;
    if (materialScroll > maxScroll) materialScroll = maxScroll;
    int start = materialScroll;
    int end = start + maxRows;
    if (end > count) end = count;

    for (int i = start; i < end; ++i) {
        int rowIdx = i - start;
        SDL_Rect row = {panel.x + PANEL_PADDING,
                        listY + rowIdx * materialRowH,
                        panel.w - PANEL_PADDING * 2,
                        materialRowH - 2};
        SDL_Rect swatch = {row.x + 6, row.y + 4, materialRowH - 8, materialRowH - 8};
        SDL_Rect labelRect = {row.x + swatch.w + 14, row.y, row.w - swatch.w - 20, row.h};
        SDL_Color swatchColor = ObjectEditorPanelColorFromPackedRGB(ObjectEditorResolveMaterialSwatchColor(i), 255);
        bool selected = (i == selectedMaterialIndex);
        SDL_SetRenderDrawColor(renderer,
                               selected ? activeRowFill.r : inactiveRowFill.r,
                               selected ? activeRowFill.g : inactiveRowFill.g,
                               selected ? activeRowFill.b : inactiveRowFill.b,
                               selected ? activeRowFill.a : inactiveRowFill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, selected ? 255 : 200);
        SDL_RenderDrawRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, swatchColor.r, swatchColor.g, swatchColor.b, swatchColor.a);
        SDL_RenderFillRect(renderer, &swatch);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
        SDL_RenderDrawRect(renderer, &swatch);

        const char* label = "Material";
        switch (i) {
            case MATERIAL_PRESET_DEFAULT: label = "Default"; break;
            case MATERIAL_PRESET_MIRROR: label = "Mirror"; break;
            case MATERIAL_PRESET_ROUGH_METAL: label = "Rough Metal"; break;
            case MATERIAL_PRESET_GLOSSY: label = "Glossy"; break;
            case MATERIAL_PRESET_EMISSIVE: label = "Emissive"; break;
            case MATERIAL_PRESET_TRANSPARENT: label = "Transparent"; break;
            default: break;
        }
        RenderLabelTextLeft(renderer, labelRect, label, textColor);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
}
