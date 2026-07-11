#include "editor/object_editor_panels_internal.h"

#include "material/material_manager.h"

#include <math.h>

static Uint8 object_editor_panels_color_offset(Uint8 value, int offset) {
    int out = (int)value + offset;
    if (out < 0) return 0;
    if (out > 255) return 255;
    return (Uint8)out;
}

static bool object_editor_panels_point_in_rect(int mx, int my, const SDL_Rect* rect) {
    return rect && mx >= rect->x && mx < rect->x + rect->w && my >= rect->y &&
           my < rect->y + rect->h;
}

static double object_editor_panels_slider_value_from_track_x(const SDL_Rect* track, int mx) {
    double denom = (double)(track->w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH);
    double raw = 0.0;
    if (denom <= 0.0) return 0.0;
    raw = ((double)mx - (double)track->x - (OBJECT_EDITOR_SLIDER_KNOB_WIDTH * 0.5)) / denom;
    if (raw < 0.0) return 0.0;
    if (raw > 1.0) return 1.0;
    return raw;
}

int ObjectEditorPanels_HeaderHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, PANEL_HEADER_HEIGHT, 20);
}

int ObjectEditorPanels_AssetRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, ASSET_ROW_HEIGHT, 18);
}

int ObjectEditorPanels_MaterialRowHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, MATERIAL_ROW_HEIGHT, 16);
}

int ObjectEditorPanels_ColorPreviewSize(void) {
    return animation_config_scale_text_point_size(&animSettings, 22, 18);
}

int ObjectEditorPanels_ColorSliderSectionHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, 30, 26);
}

int ObjectEditorPanels_AuxSliderSectionHeight(void) {
    return animation_config_scale_text_point_size(&animSettings, 44, 38);
}

int ObjectEditorPanels_MotionSectionHeight(void) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected) return 0;
    return animation_config_scale_text_point_size(&animSettings, 74, 64);
}

RayTracingThemePalette ObjectEditorPanels_ResolvePanelPalette(void) {
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

SDL_Color ObjectEditorPanels_PanelBodyFill(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){58, 58, 68, 230};
    return palette->panel_fill;
}

SDL_Color ObjectEditorPanels_PanelHeaderFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = ObjectEditorPanels_PanelBodyFill(palette);
    fill.r = object_editor_panels_color_offset(fill.r, 8);
    fill.g = object_editor_panels_color_offset(fill.g, 8);
    fill.b = object_editor_panels_color_offset(fill.b, 8);
    fill.a = 240;
    return fill;
}

SDL_Color ObjectEditorPanels_PanelInactiveRowFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = ObjectEditorPanels_PanelBodyFill(palette);
    fill.r = object_editor_panels_color_offset(fill.r, -10);
    fill.g = object_editor_panels_color_offset(fill.g, -10);
    fill.b = object_editor_panels_color_offset(fill.b, -10);
    fill.a = 220;
    return fill;
}

SDL_Color ObjectEditorPanels_PanelActiveRowFill(const RayTracingThemePalette* palette) {
    SDL_Color fill = palette ? palette->accent_primary : (SDL_Color){120, 200, 255, 255};
    fill.r = object_editor_panels_color_offset(fill.r, -24);
    fill.g = object_editor_panels_color_offset(fill.g, -24);
    fill.b = object_editor_panels_color_offset(fill.b, -24);
    fill.a = 228;
    return fill;
}

SDL_Color ObjectEditorPanels_PanelBorderColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){95, 95, 112, 255};
    return palette->panel_border;
}

SDL_Color ObjectEditorPanels_PanelTextColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){220, 220, 230, 255};
    return palette->text_primary;
}

SDL_Color ObjectEditorPanels_PanelMutedTextColor(const RayTracingThemePalette* palette) {
    if (!palette) return (SDL_Color){210, 210, 215, 255};
    return palette->text_muted;
}

SDL_Color ObjectEditorPanels_PanelButtonFill(const RayTracingThemePalette* palette, bool active) {
    SDL_Color fill = active && palette ? palette->button_active_fill :
                     palette ? palette->button_fill : (SDL_Color){180, 180, 180, 255};
    fill.a = 230;
    return fill;
}

int ObjectEditorPanels_AssetVisibleCount(void) {
    return showImports ? importCount : (int)assetLib.count;
}

int ObjectEditorPanels_MaterialVisibleCount(void) {
    return MaterialManagerCount();
}

const SceneObject* ObjectEditorPanels_SelectedObject(void) {
    int selected_object_index = ObjectEditorGetSelectedObjectIndex();
    if (selected_object_index < 0 || selected_object_index >= sceneSettings.objectCount) {
        return NULL;
    }
    return &sceneSettings.sceneObjects[selected_object_index];
}

bool ObjectEditorPanels_ObjectColorLocked(const SceneObject* selected) {
    return selected && SceneObjectIsGuideOnly(selected);
}

bool ObjectEditorPanels_ObjectUsesAlpha(const SceneObject* selected) {
    return selected &&
           !ObjectEditorPanels_ObjectColorLocked(selected) &&
           selected->material_id == MATERIAL_PRESET_TRANSPARENT;
}

bool ObjectEditorPanels_ObjectUsesEmissiveStrength(const SceneObject* selected) {
    return selected && selected->material_id == MATERIAL_PRESET_EMISSIVE;
}

int ObjectEditorPanels_ColorSliderCount(void) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected) return 0;
    if (ObjectEditorPanels_ObjectColorLocked(selected)) return 0;
    return 3 + (ObjectEditorPanels_ObjectUsesAlpha(selected) ? 1 : 0);
}

int ObjectEditorPanels_AuxSliderCount(void) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    return ObjectEditorPanels_ObjectUsesEmissiveStrength(selected) ? 1 : 0;
}

int ObjectEditorPanels_ColorSectionHeight(void) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int label_h = animation_config_scale_text_point_size(&animSettings, 14, 12);
    int preview = ObjectEditorPanels_ColorPreviewSize();
    int row_h = ObjectEditorPanels_ColorSliderSectionHeight();
    if (color_slider_count <= 0) return 0;
    return label_h + 6 + preview + 8 + color_slider_count * row_h +
           (color_slider_count - 1) * 4;
}

int ObjectEditorPanels_AuxSectionHeight(void) {
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    if (aux_slider_count <= 0) return 0;
    return aux_slider_count * ObjectEditorPanels_AuxSliderSectionHeight() +
           (aux_slider_count - 1) * 6 + 8;
}

ObjectEditorPanelSliderKind ObjectEditorPanels_ColorSliderKindForOrdinal(int ordinal) {
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

ObjectEditorPanelSliderKind ObjectEditorPanels_AuxSliderKindForOrdinal(int ordinal) {
    const SceneObject* selected = ObjectEditorPanels_SelectedObject();
    if (!selected || ordinal != 0) return OBJECT_EDITOR_PANEL_SLIDER_NONE;
    return ObjectEditorPanels_ObjectUsesEmissiveStrength(selected)
               ? OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH
               : OBJECT_EDITOR_PANEL_SLIDER_NONE;
}

double ObjectEditorPanels_ValueForSliderKind(ObjectEditorPanelSliderKind kind) {
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

const char* ObjectEditorPanels_LabelForSliderKind(ObjectEditorPanelSliderKind kind) {
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
        return "Transparency";
    }
    if (kind == OBJECT_EDITOR_PANEL_SLIDER_EMISSIVE_STRENGTH) {
        return "Emitter Strength";
    }
    return "";
}

void ObjectEditorPanels_ResolveAssetListMetrics(int* out_list_y,
                                                int* out_row_h,
                                                int* out_max_rows,
                                                int* out_max_scroll) {
    int row_h = ObjectEditorPanels_AssetRowHeight();
    int list_y = assetToggleRect.y + assetToggleRect.h + 4;
    int visible = ObjectEditorPanels_AssetVisibleCount();
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

void ObjectEditorPanels_ResolveColorSectionMetrics(SDL_Rect* out_section,
                                                   SDL_Rect* out_label,
                                                   SDL_Rect* out_preview) {
    int header_h = ObjectEditorPanels_HeaderHeight();
    int color_h = ObjectEditorPanels_ColorSectionHeight();
    int preview_size = ObjectEditorPanels_ColorPreviewSize();
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

void ObjectEditorPanels_ResolveColorSliderMetrics(int ordinal,
                                                  SDL_Rect* out_section,
                                                  SDL_Rect* out_label,
                                                  SDL_Rect* out_value,
                                                  SDL_Rect* out_track,
                                                  SDL_Rect* out_knob) {
    SDL_Rect color_section = {0};
    SDL_Rect preview = {0};
    int section_h = ObjectEditorPanels_ColorSliderSectionHeight();
    SDL_Rect section = {0};
    SDL_Rect label = {0};
    SDL_Rect value = {0};
    SDL_Rect track = {0};

    ObjectEditorPanels_ResolveColorSectionMetrics(&color_section, &label, &preview);
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
        double value_norm =
            ObjectEditorPanels_ValueForSliderKind(ObjectEditorPanels_ColorSliderKindForOrdinal(ordinal));
        int knob_x = track.x +
                     (int)lround(value_norm *
                                 (double)(track.w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH));
        *out_knob = (SDL_Rect){knob_x,
                               track.y - 4 + (track.h - OBJECT_EDITOR_SLIDER_TRACK_HEIGHT) / 2,
                               OBJECT_EDITOR_SLIDER_KNOB_WIDTH,
                               track.h + 8};
    }
}

void ObjectEditorPanels_ResolveAuxSliderMetrics(int ordinal,
                                                SDL_Rect* out_section,
                                                SDL_Rect* out_label,
                                                SDL_Rect* out_value,
                                                SDL_Rect* out_track,
                                                SDL_Rect* out_knob) {
    int header_h = ObjectEditorPanels_HeaderHeight();
    int section_h = ObjectEditorPanels_AuxSliderSectionHeight();
    int color_h = ObjectEditorPanels_ColorSectionHeight();
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
        double value_norm =
            ObjectEditorPanels_ValueForSliderKind(ObjectEditorPanels_AuxSliderKindForOrdinal(ordinal));
        int knob_x = track.x +
                     (int)lround(value_norm *
                                 (double)(track.w - OBJECT_EDITOR_SLIDER_KNOB_WIDTH));
        *out_knob = (SDL_Rect){knob_x,
                               track.y - 4,
                               OBJECT_EDITOR_SLIDER_KNOB_WIDTH,
                               track.h + 8};
    }
}

void ObjectEditorPanels_ResolveMotionSectionMetrics(SDL_Rect* out_section,
                                                    SDL_Rect* out_label,
                                                    SDL_Rect* out_static_button,
                                                    SDL_Rect* out_authored_button,
                                                    SDL_Rect* out_physics_button,
                                                    SDL_Rect* out_status_label,
                                                    SDL_Rect* out_status_swatch) {
    int header_h = ObjectEditorPanels_HeaderHeight();
    int color_h = ObjectEditorPanels_ColorSectionHeight();
    int aux_h = ObjectEditorPanels_AuxSectionHeight();
    int motion_h = ObjectEditorPanels_MotionSectionHeight();
    int label_h = animation_config_scale_text_point_size(&animSettings, 14, 12);
    int button_h = animation_config_scale_text_point_size(&animSettings, 22, 20);
    int swatch_size = animation_config_scale_text_point_size(&animSettings, 10, 8);
    int y = materialPanelRect.y + PANEL_PADDING + header_h + 4;
    SDL_Rect section = {0};
    SDL_Rect label = {0};
    SDL_Rect static_button = {0};
    SDL_Rect authored_button = {0};
    SDL_Rect physics_button = {0};
    SDL_Rect status_label = {0};
    SDL_Rect status_swatch = {0};
    int gap = 4;
    int button_w = 0;

    if (color_h > 0) y += color_h + 8;
    if (aux_h > 0) y += aux_h;
    if (motion_h <= 0) {
        if (out_section) *out_section = section;
        if (out_label) *out_label = label;
        if (out_static_button) *out_static_button = static_button;
        if (out_authored_button) *out_authored_button = authored_button;
        if (out_physics_button) *out_physics_button = physics_button;
        if (out_status_label) *out_status_label = status_label;
        if (out_status_swatch) *out_status_swatch = status_swatch;
        return;
    }

    section = (SDL_Rect){
        materialPanelRect.x + PANEL_PADDING,
        y + 4,
        materialPanelRect.w - PANEL_PADDING * 2,
        motion_h
    };
    label = (SDL_Rect){section.x,
                       section.y,
                       section.w,
                       label_h};
    button_w = (section.w - gap * 2) / 3;
    if (button_w < 34) button_w = 34;
    static_button = (SDL_Rect){section.x,
                               label.y + label.h + 6,
                               button_w,
                               button_h};
    authored_button = (SDL_Rect){static_button.x + static_button.w + gap,
                                 static_button.y,
                                 button_w,
                                 button_h};
    physics_button = (SDL_Rect){authored_button.x + authored_button.w + gap,
                                static_button.y,
                                section.x + section.w -
                                    (authored_button.x + authored_button.w + gap),
                                button_h};
    if (physics_button.w < 34) physics_button.w = 34;
    status_swatch = (SDL_Rect){section.x + 2,
                               static_button.y + static_button.h + 9,
                               swatch_size,
                               swatch_size};
    status_label = (SDL_Rect){status_swatch.x + status_swatch.w + 6,
                              static_button.y + static_button.h + 4,
                              section.w - swatch_size - 10,
                              label_h + 4};

    if (out_section) *out_section = section;
    if (out_label) *out_label = label;
    if (out_static_button) *out_static_button = static_button;
    if (out_authored_button) *out_authored_button = authored_button;
    if (out_physics_button) *out_physics_button = physics_button;
    if (out_status_label) *out_status_label = status_label;
    if (out_status_swatch) *out_status_swatch = status_swatch;
}

void ObjectEditorPanels_ResolveMaterialListMetrics(int* out_list_y,
                                                   int* out_row_h,
                                                   int* out_max_rows,
                                                   int* out_max_scroll) {
    int row_h = ObjectEditorPanels_MaterialRowHeight();
    int header_h = ObjectEditorPanels_HeaderHeight();
    int color_h = ObjectEditorPanels_ColorSectionHeight();
    int aux_h = ObjectEditorPanels_AuxSectionHeight();
    int motion_h = ObjectEditorPanels_MotionSectionHeight();
    int list_y = 0;
    int visible = ObjectEditorPanels_MaterialVisibleCount();
    int row_area_h = materialPanelRect.h - (list_y - materialPanelRect.y) - PANEL_PADDING;
    int max_rows = 1;
    int max_scroll = 0;
    list_y = materialPanelRect.y + PANEL_PADDING + header_h + 4;
    if (color_h > 0) list_y += color_h + 8;
    if (aux_h > 0) list_y += aux_h;
    if (motion_h > 0) list_y += motion_h + 4;
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

void ObjectEditorPanels_UpdateLayoutForRegionImpl(const SDL_Rect* region) {
    SceneEditorPaneLayout pane_layout = {0};
    int header_h = ObjectEditorPanels_HeaderHeight();
    int asset_row_h = ObjectEditorPanels_AssetRowHeight();
    int material_row_h = ObjectEditorPanels_MaterialRowHeight();
    int panel_max_h = 0;
    SDL_Rect available = {20, 40, ASSET_PANEL_WIDTH, 360};
    int panel_w = ASSET_PANEL_WIDTH;
    int x = 20;
    int y = 40;
    int available_h = 0;
    int asset_min_content = 0;
    int material_min_content = 0;
    int asset_content = 0;
    int material_content = 0;
    int color_content = 0;
    int aux_slider_content = 0;
    int motion_content = 0;
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
    panel_w = available.w;
    available_h = available.h;
    panel_max_h = available_h;
    if (panel_w > ASSET_PANEL_WIDTH) panel_w = ASSET_PANEL_WIDTH;
    if (panel_w < 160 && available.w >= 160) panel_w = available.w;
    if (panel_w < 120) panel_w = 120;

    int header_w = panel_w - PANEL_PADDING * 2;
    int asset_rows = showImports ? importCount : (int)assetLib.count;
    color_content = ObjectEditorPanels_ColorSectionHeight();
    aux_slider_content = ObjectEditorPanels_AuxSectionHeight();
    motion_content = ObjectEditorPanels_MotionSectionHeight();
    if (asset_rows < 1) asset_rows = 1;
    asset_min_content = header_h + PANEL_PADDING * 2 + asset_row_h;
    material_min_content = header_h + PANEL_PADDING * 2 +
                           (color_content > 0 ? color_content + 8 : 0) +
                           aux_slider_content +
                           (motion_content > 0 ? motion_content + 4 : 0) +
                           material_row_h;
    asset_content = header_h + PANEL_PADDING * 2 + 4 + asset_rows * asset_row_h;
    if (asset_content > panel_max_h) asset_content = panel_max_h;
    assetPanelRect = (SDL_Rect){x, y, panel_w, asset_content};
    assetToggleRect = (SDL_Rect){x + PANEL_PADDING,
                                 y + PANEL_PADDING,
                                 header_w - 20,
                                 header_h - PANEL_PADDING};
    assetCollapseRect = (SDL_Rect){assetToggleRect.x + assetToggleRect.w + 4,
                                   assetToggleRect.y,
                                   16,
                                   16};

    int material_rows = MaterialManagerCount();
    if (material_rows < 1) material_rows = 1;
    material_content = header_h + PANEL_PADDING * 2 +
                       (color_content > 0 ? color_content + 8 : 0) +
                       aux_slider_content +
                       (motion_content > 0 ? motion_content + 4 : 0) +
                       4 + material_rows * material_row_h;
    if (material_content > panel_max_h) material_content = panel_max_h;

    overflow = asset_content + PANEL_GAP + material_content - available_h;
    if (overflow > 0 && asset_content > asset_min_content) {
        int reducible = asset_content - asset_min_content;
        int trim = overflow < reducible ? overflow : reducible;
        asset_content -= trim;
        overflow -= trim;
    }
    if (overflow > 0 && material_content > material_min_content) {
        int reducible = material_content - material_min_content;
        int trim = overflow < reducible ? overflow : reducible;
        material_content -= trim;
        overflow -= trim;
    }
    if (overflow > 0) {
        int total_available = available_h - PANEL_GAP;
        if (total_available < 0) total_available = available_h;
        if (material_content > total_available) material_content = total_available;
        if (asset_content > total_available - material_content) {
            asset_content = total_available - material_content;
        }
        if (asset_content < asset_min_content) asset_content = asset_min_content;
        if (material_content < material_min_content) material_content = material_min_content;
    }

    assetPanelRect.h = asset_content;
    materialPanelRect = (SDL_Rect){x,
                                   assetPanelRect.y + assetPanelRect.h + PANEL_GAP,
                                   panel_w,
                                   material_content};
    materialCollapseRect = (SDL_Rect){materialPanelRect.x + materialPanelRect.w - PANEL_PADDING - 16,
                                      materialPanelRect.y + PANEL_PADDING,
                                      16,
                                      16};
}

int ObjectEditorPanels_AssetMaxScrollImpl(void) {
    int max_scroll = 0;
    ObjectEditorPanels_ResolveAssetListMetrics(NULL, NULL, NULL, &max_scroll);
    return max_scroll;
}

int ObjectEditorPanels_MaterialMaxScrollImpl(void) {
    int max_scroll = 0;
    ObjectEditorPanels_ResolveMaterialListMetrics(NULL, NULL, NULL, &max_scroll);
    return max_scroll;
}

int ObjectEditorPanels_AssetIndexAtPointImpl(int mx, int my) {
    int list_y = 0;
    int row_h = 0;
    int max_rows = 0;
    int visible = ObjectEditorPanels_AssetVisibleCount();
    int idx = -1;
    if (assetsCollapsed) return -1;
    if (mx < assetPanelRect.x || mx >= assetPanelRect.x + assetPanelRect.w ||
        my < assetPanelRect.y || my >= assetPanelRect.y + assetPanelRect.h) {
        return -1;
    }
    ObjectEditorPanels_ResolveAssetListMetrics(&list_y, &row_h, &max_rows, NULL);
    if (my < list_y) return -1;
    idx = (my - list_y) / row_h + assetScroll;
    if (idx < 0 || idx >= visible) return -1;
    if (idx >= assetScroll + max_rows) return -1;
    return idx;
}

int ObjectEditorPanels_MaterialIndexAtPointImpl(int mx, int my) {
    int list_y = 0;
    int row_h = 0;
    int max_rows = 0;
    int visible = ObjectEditorPanels_MaterialVisibleCount();
    int idx = -1;
    if (materialsCollapsed) return -1;
    if (mx < materialPanelRect.x || mx >= materialPanelRect.x + materialPanelRect.w ||
        my < materialPanelRect.y || my >= materialPanelRect.y + materialPanelRect.h) {
        return -1;
    }
    ObjectEditorPanels_ResolveMaterialListMetrics(&list_y, &row_h, &max_rows, NULL);
    if (my < list_y) return -1;
    idx = (my - list_y) / row_h + materialScroll;
    if (idx < 0 || idx >= visible) return -1;
    if (idx >= materialScroll + max_rows) return -1;
    return idx;
}

bool ObjectEditorPanels_MotionActionAtPointImpl(int mx,
                                                int my,
                                                ObjectEditorPanelMotionAction* out_action) {
    SDL_Rect static_button = {0};
    SDL_Rect authored_button = {0};
    SDL_Rect physics_button = {0};
    if (out_action) {
        *out_action = OBJECT_EDITOR_PANEL_MOTION_ACTION_NONE;
    }
    if (materialsCollapsed || ObjectEditorPanels_MotionSectionHeight() <= 0) {
        return false;
    }
    if (mx < materialPanelRect.x || mx >= materialPanelRect.x + materialPanelRect.w ||
        my < materialPanelRect.y || my >= materialPanelRect.y + materialPanelRect.h) {
        return false;
    }
    ObjectEditorPanels_ResolveMotionSectionMetrics(NULL,
                                                   NULL,
                                                   &static_button,
                                                   &authored_button,
                                                   &physics_button,
                                                   NULL,
                                                   NULL);
    if (object_editor_panels_point_in_rect(mx, my, &static_button)) {
        if (out_action) *out_action = OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC;
        return true;
    }
    if (object_editor_panels_point_in_rect(mx, my, &authored_button)) {
        if (out_action) *out_action = OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED;
        return true;
    }
    if (object_editor_panels_point_in_rect(mx, my, &physics_button)) {
        if (out_action) *out_action = OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED;
        return true;
    }
    return false;
}

bool ObjectEditorPanels_SliderValueAtPointImpl(int mx,
                                               int my,
                                               ObjectEditorPanelSliderKind* out_kind,
                                               double* out_value) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect section = {0};
        SDL_Rect track = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_ColorSliderKindForOrdinal(i);
        ObjectEditorPanels_ResolveColorSliderMetrics(i, &section, NULL, NULL, &track, NULL);
        if (!object_editor_panels_point_in_rect(mx, my, &section)) continue;
        if (out_kind) *out_kind = kind;
        if (out_value) *out_value = object_editor_panels_slider_value_from_track_x(&track, mx);
        return kind != OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect section = {0};
        SDL_Rect track = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_AuxSliderKindForOrdinal(i);
        ObjectEditorPanels_ResolveAuxSliderMetrics(i, &section, NULL, NULL, &track, NULL);
        if (!object_editor_panels_point_in_rect(mx, my, &section)) continue;
        if (out_kind) *out_kind = kind;
        if (out_value) *out_value = object_editor_panels_slider_value_from_track_x(&track, mx);
        return kind != OBJECT_EDITOR_PANEL_SLIDER_NONE;
    }
    return false;
}

bool ObjectEditorPanels_SliderValueForKindAtXImpl(ObjectEditorPanelSliderKind kind,
                                                  int mx,
                                                  double* out_value) {
    int color_slider_count = ObjectEditorPanels_ColorSliderCount();
    int aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect track = {0};
        if (ObjectEditorPanels_ColorSliderKindForOrdinal(i) != kind) continue;
        ObjectEditorPanels_ResolveColorSliderMetrics(i, NULL, NULL, NULL, &track, NULL);
        if (out_value) *out_value = object_editor_panels_slider_value_from_track_x(&track, mx);
        return true;
    }
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect track = {0};
        if (ObjectEditorPanels_AuxSliderKindForOrdinal(i) != kind) continue;
        ObjectEditorPanels_ResolveAuxSliderMetrics(i, NULL, NULL, NULL, &track, NULL);
        if (out_value) *out_value = object_editor_panels_slider_value_from_track_x(&track, mx);
        return true;
    }
    return false;
}
