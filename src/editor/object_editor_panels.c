#include "editor/object_editor_panels.h"

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "geo/shape_library.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/render_helper.h"
#include "ui/shared_theme_font_adapter.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSET_ROW_HEIGHT 22
#define PANEL_HEADER_HEIGHT 26
#define PANEL_PADDING 6
#define PANEL_MAX_HEIGHT 220
#define ASSET_PANEL_WIDTH 200
#define MATERIAL_ROW_HEIGHT 18
#define PANEL_GAP 10

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

static void ObjectEditorResolveMaterialListMetrics(int* out_list_y,
                                                   int* out_row_h,
                                                   int* out_max_rows,
                                                   int* out_max_scroll) {
    int row_h = ObjectEditorMaterialRowHeight();
    int header_h = ObjectEditorHeaderHeight();
    int list_y = materialPanelRect.y + PANEL_PADDING + header_h + 4;
    int visible = ObjectEditorMaterialVisibleCount();
    int row_area_h = materialPanelRect.h - (list_y - materialPanelRect.y) - PANEL_PADDING;
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
    int panelMaxH = animation_config_scale_text_point_size(&animSettings, PANEL_MAX_HEIGHT, 160);
    SDL_Rect available = {20, 40, ASSET_PANEL_WIDTH, panelMaxH * 2 + PANEL_GAP};
    int panelW = ASSET_PANEL_WIDTH;
    int x = 20;
    int y = 40;
    int available_h = 0;
    int max_each_h = 0;
    if (panelMaxH > sceneSettings.windowHeight - 80) {
        panelMaxH = sceneSettings.windowHeight - 80;
    }
    if (panelMaxH < 140) panelMaxH = 140;
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
    if (panelW > ASSET_PANEL_WIDTH) panelW = ASSET_PANEL_WIDTH;
    if (panelW < 160 && available.w >= 160) panelW = available.w;
    if (panelW < 120) panelW = 120;
    max_each_h = (available_h - PANEL_GAP) / 2;
    if (max_each_h < 80) {
        max_each_h = available_h;
    }
    int headerW = panelW - PANEL_PADDING * 2;
    int assetRows = showImports ? importCount : (int)assetLib.count;
    if (assetRows < 1) assetRows = 1;
    int assetContent = headerH + PANEL_PADDING * 2 + assetRows * assetRowH;
    if (assetContent > panelMaxH) assetContent = panelMaxH;
    if (assetContent > max_each_h) assetContent = max_each_h;
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
    int matContent = headerH + PANEL_PADDING * 2 + matRows * materialRowH;
    if (matContent > panelMaxH) matContent = panelMaxH;
    if (matContent > max_each_h) matContent = max_each_h;
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
            default: break;
        }
        RenderLabelTextLeft(renderer, labelRect, label, textColor);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
}
