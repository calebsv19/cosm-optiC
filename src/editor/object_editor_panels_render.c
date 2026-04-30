#include "editor/object_editor_panels_internal.h"

#include "material/material.h"
#include "material/material_manager.h"
#include "render/render_helper.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int object_editor_panels_resolve_material_swatch_color(int material_id) {
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
                r = 156;
                g = 196;
                b = 255;
                break;
            case MATERIAL_PRESET_ROUGH_METAL:
                r = 214;
                g = 170;
                b = 116;
                break;
            case MATERIAL_PRESET_GLOSSY:
                r = 236;
                g = 120;
                b = 120;
                break;
            case MATERIAL_PRESET_EMISSIVE:
                r = 255;
                g = 220;
                b = 92;
                break;
            case MATERIAL_PRESET_TRANSPARENT:
                r = 144;
                g = 232;
                b = 255;
                break;
            case MATERIAL_PRESET_DEFAULT:
            default:
                r = 220;
                g = 220;
                b = 220;
                break;
        }
    }
    return (r << 16) | (g << 8) | b;
}

static SDL_Color object_editor_panels_color_from_packed_rgb(int packed, Uint8 alpha) {
    SDL_Color color = {0};
    color.r = (Uint8)((packed >> 16) & 0xFF);
    color.g = (Uint8)((packed >> 8) & 0xFF);
    color.b = (Uint8)(packed & 0xFF);
    color.a = alpha;
    return color;
}

void ObjectEditorPanels_DrawAssetListImpl(SDL_Renderer* renderer) {
    SDL_Rect panel = assetPanelRect;
    RayTracingThemePalette palette = ObjectEditorPanels_ResolvePanelPalette();
    SDL_Color text_color = ObjectEditorPanels_PanelTextColor(&palette);
    SDL_Color body_fill = ObjectEditorPanels_PanelBodyFill(&palette);
    SDL_Color border_color = ObjectEditorPanels_PanelBorderColor(&palette);
    SDL_Color active_row_fill = ObjectEditorPanels_PanelActiveRowFill(&palette);
    SDL_Color inactive_row_fill = ObjectEditorPanels_PanelInactiveRowFill(&palette);
    SDL_Color toggle_fill = ObjectEditorPanels_PanelButtonFill(&palette, showImports);
    SDL_Color collapse_fill = ObjectEditorPanels_PanelButtonFill(&palette, !assetsCollapsed);
    SDL_Rect label_rect = {0};
    int list_y = 0;
    int asset_row_h = 0;
    int max_rows = 0;
    int visible = ObjectEditorPanels_AssetVisibleCount();
#if !USE_VULKAN
    SDL_BlendMode prev_mode;
    SDL_GetRenderDrawBlendMode(renderer, &prev_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, body_fill.r, body_fill.g, body_fill.b, body_fill.a);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, toggle_fill.r, toggle_fill.g, toggle_fill.b, toggle_fill.a);
    SDL_RenderFillRect(renderer, &assetToggleRect);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &assetToggleRect);
    RenderLabelText(renderer, assetToggleRect, showImports ? "Imports" : "Assets", text_color);
    SDL_SetRenderDrawColor(renderer,
                           collapse_fill.r,
                           collapse_fill.g,
                           collapse_fill.b,
                           collapse_fill.a);
    SDL_RenderFillRect(renderer, &assetCollapseRect);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &assetCollapseRect);
    RenderLabelText(renderer, assetCollapseRect, assetsCollapsed ? "+" : "-", text_color);

    if (assetsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prev_mode);
#endif
        return;
    }

    ObjectEditorPanels_ResolveAssetListMetrics(&list_y, &asset_row_h, &max_rows, NULL);
    int max_scroll = visible - max_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (assetScroll > max_scroll) assetScroll = max_scroll;
    int start = assetScroll;
    int end = start + max_rows;
    if (end > visible) end = visible;

    for (int i = start; i < end; ++i) {
        int row_idx = i - start;
        SDL_Rect row = {panel.x + PANEL_PADDING,
                        list_y + row_idx * asset_row_h,
                        panel.w - PANEL_PADDING * 2,
                        asset_row_h - 2};
        bool selected = (!showImports && i == selectedAssetIndex);
        SDL_SetRenderDrawColor(renderer,
                               selected ? active_row_fill.r : inactive_row_fill.r,
                               selected ? active_row_fill.g : inactive_row_fill.g,
                               selected ? active_row_fill.b : inactive_row_fill.b,
                               selected ? active_row_fill.a : inactive_row_fill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               border_color.r,
                               border_color.g,
                               border_color.b,
                               selected ? 255 : 200);
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
        label_rect = (SDL_Rect){row.x + 8, row.y, row.w - 12, row.h};
        RenderLabelTextLeft(renderer, label_rect, label, text_color);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prev_mode);
#endif
}

void ObjectEditorPanels_DrawMaterialListImpl(SDL_Renderer* renderer) {
    SDL_Rect panel = materialPanelRect;
    RayTracingThemePalette palette = ObjectEditorPanels_ResolvePanelPalette();
    int material_row_h = 0;
    int header_h = ObjectEditorPanels_HeaderHeight();
    SDL_Color text_color = ObjectEditorPanels_PanelTextColor(&palette);
    SDL_Color muted_color = ObjectEditorPanels_PanelMutedTextColor(&palette);
    SDL_Color header_fill = ObjectEditorPanels_PanelHeaderFill(&palette);
    SDL_Color body_fill = ObjectEditorPanels_PanelBodyFill(&palette);
    SDL_Color border_color = ObjectEditorPanels_PanelBorderColor(&palette);
    SDL_Color active_row_fill = ObjectEditorPanels_PanelActiveRowFill(&palette);
    SDL_Color inactive_row_fill = ObjectEditorPanels_PanelInactiveRowFill(&palette);
    SDL_Color collapse_fill = ObjectEditorPanels_PanelButtonFill(&palette, !materialsCollapsed);
    SDL_Rect header_bar = {0};
    SDL_Rect title_rect = {0};
    SDL_Rect color_label_rect = {0};
    SDL_Rect color_preview_rect = {0};
    int color_slider_count = 0;
    int aux_slider_count = 0;
    int list_y = 0;
    int max_rows = 0;
#if !USE_VULKAN
    SDL_BlendMode prev_mode;
    SDL_GetRenderDrawBlendMode(renderer, &prev_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, body_fill.r, body_fill.g, body_fill.b, body_fill.a);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &panel);

    header_bar = (SDL_Rect){panel.x + PANEL_PADDING,
                            panel.y + PANEL_PADDING,
                            panel.w - PANEL_PADDING * 2,
                            header_h - PANEL_PADDING + 2};
    SDL_SetRenderDrawColor(renderer, header_fill.r, header_fill.g, header_fill.b, header_fill.a);
    SDL_RenderFillRect(renderer, &header_bar);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &header_bar);

    title_rect = (SDL_Rect){panel.x + PANEL_PADDING,
                            panel.y + PANEL_PADDING,
                            panel.w - PANEL_PADDING * 2 - materialCollapseRect.w - 6,
                            header_h - PANEL_PADDING};
    RenderLabelTextLeft(renderer, title_rect, "Materials", muted_color);

    SDL_SetRenderDrawColor(renderer,
                           collapse_fill.r,
                           collapse_fill.g,
                           collapse_fill.b,
                           collapse_fill.a);
    SDL_RenderFillRect(renderer, &materialCollapseRect);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &materialCollapseRect);
    RenderLabelText(renderer, materialCollapseRect, materialsCollapsed ? "+" : "-", text_color);

    if (materialsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prev_mode);
#endif
        return;
    }

    ObjectEditorPanels_ResolveColorSectionMetrics(NULL, &color_label_rect, &color_preview_rect);
    if (ObjectEditorPanels_ColorSliderCount() > 0) {
        const SceneObject* selected = ObjectEditorPanels_SelectedObject();
        SDL_Color preview_color = selected
                                      ? object_editor_panels_color_from_packed_rgb(
                                            selected->color,
                                            ObjectEditorPanels_ObjectUsesAlpha(selected)
                                                ? SceneObjectAlphaByte(selected)
                                                : 255)
                                      : (SDL_Color){255, 255, 255, 255};
        RenderLabelTextLeft(renderer, color_label_rect, "Object Color", muted_color);
        SDL_SetRenderDrawColor(renderer,
                               inactive_row_fill.r,
                               inactive_row_fill.g,
                               inactive_row_fill.b,
                               220);
        SDL_RenderFillRect(renderer, &color_preview_rect);
        SDL_SetRenderDrawColor(renderer,
                               preview_color.r,
                               preview_color.g,
                               preview_color.b,
                               preview_color.a);
        SDL_RenderFillRect(renderer, &color_preview_rect);
        SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 255);
        SDL_RenderDrawRect(renderer, &color_preview_rect);
    }

    color_slider_count = ObjectEditorPanels_ColorSliderCount();
    for (int i = 0; i < color_slider_count; ++i) {
        SDL_Rect label_rect = {0};
        SDL_Rect value_rect = {0};
        SDL_Rect track_rect = {0};
        SDL_Rect knob_rect = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_ColorSliderKindForOrdinal(i);
        double value = ObjectEditorPanels_ValueForSliderKind(kind);
        char value_text[32];

        ObjectEditorPanels_ResolveColorSliderMetrics(i,
                                                     NULL,
                                                     &label_rect,
                                                     &value_rect,
                                                     &track_rect,
                                                     &knob_rect);
        RenderLabelTextLeft(renderer,
                            label_rect,
                            ObjectEditorPanels_LabelForSliderKind(kind),
                            muted_color);
        snprintf(value_text, sizeof(value_text), "%d", (int)lround(value * 255.0));
        RenderLabelText(renderer, value_rect, value_text, text_color);
        SDL_SetRenderDrawColor(renderer,
                               inactive_row_fill.r,
                               inactive_row_fill.g,
                               inactive_row_fill.b,
                               220);
        SDL_RenderFillRect(renderer, &track_rect);
        SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 220);
        SDL_RenderDrawRect(renderer, &track_rect);
        SDL_SetRenderDrawColor(renderer,
                               active_row_fill.r,
                               active_row_fill.g,
                               active_row_fill.b,
                               active_row_fill.a);
        SDL_RenderFillRect(renderer, &knob_rect);
        SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 255);
        SDL_RenderDrawRect(renderer, &knob_rect);
    }

    aux_slider_count = ObjectEditorPanels_AuxSliderCount();
    for (int i = 0; i < aux_slider_count; ++i) {
        SDL_Rect label_rect = {0};
        SDL_Rect value_rect = {0};
        SDL_Rect track_rect = {0};
        SDL_Rect knob_rect = {0};
        ObjectEditorPanelSliderKind kind = ObjectEditorPanels_AuxSliderKindForOrdinal(i);
        double value = ObjectEditorPanels_ValueForSliderKind(kind);
        char value_text[32];

        ObjectEditorPanels_ResolveAuxSliderMetrics(i,
                                                   NULL,
                                                   &label_rect,
                                                   &value_rect,
                                                   &track_rect,
                                                   &knob_rect);
        RenderLabelTextLeft(renderer,
                            label_rect,
                            ObjectEditorPanels_LabelForSliderKind(kind),
                            muted_color);
        snprintf(value_text, sizeof(value_text), "%.2f", value);
        RenderLabelText(renderer, value_rect, value_text, text_color);
        SDL_SetRenderDrawColor(renderer,
                               inactive_row_fill.r,
                               inactive_row_fill.g,
                               inactive_row_fill.b,
                               220);
        SDL_RenderFillRect(renderer, &track_rect);
        SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 220);
        SDL_RenderDrawRect(renderer, &track_rect);
        SDL_SetRenderDrawColor(renderer,
                               active_row_fill.r,
                               active_row_fill.g,
                               active_row_fill.b,
                               active_row_fill.a);
        SDL_RenderFillRect(renderer, &knob_rect);
        SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 255);
        SDL_RenderDrawRect(renderer, &knob_rect);
    }

    int count = MaterialManagerCount();
    ObjectEditorPanels_ResolveMaterialListMetrics(&list_y, &material_row_h, &max_rows, NULL);
    int max_scroll = count - max_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (materialScroll > max_scroll) materialScroll = max_scroll;
    int start = materialScroll;
    int end = start + max_rows;
    if (end > count) end = count;

    for (int i = start; i < end; ++i) {
        int row_idx = i - start;
        SDL_Rect row = {panel.x + PANEL_PADDING,
                        list_y + row_idx * material_row_h,
                        panel.w - PANEL_PADDING * 2,
                        material_row_h - 2};
        SDL_Rect swatch = {row.x + 6, row.y + 4, material_row_h - 8, material_row_h - 8};
        SDL_Rect label_rect = {row.x + swatch.w + 14, row.y, row.w - swatch.w - 20, row.h};
        SDL_Color swatch_color =
            object_editor_panels_color_from_packed_rgb(object_editor_panels_resolve_material_swatch_color(i),
                                                       255);
        bool selected = (i == selectedMaterialIndex);
        SDL_SetRenderDrawColor(renderer,
                               selected ? active_row_fill.r : inactive_row_fill.r,
                               selected ? active_row_fill.g : inactive_row_fill.g,
                               selected ? active_row_fill.b : inactive_row_fill.b,
                               selected ? active_row_fill.a : inactive_row_fill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               border_color.r,
                               border_color.g,
                               border_color.b,
                               selected ? 255 : 200);
        SDL_RenderDrawRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               swatch_color.r,
                               swatch_color.g,
                               swatch_color.b,
                               swatch_color.a);
        SDL_RenderFillRect(renderer, &swatch);
        SDL_SetRenderDrawColor(renderer,
                               border_color.r,
                               border_color.g,
                               border_color.b,
                               border_color.a);
        SDL_RenderDrawRect(renderer, &swatch);

        const char* label = "Material";
        switch (i) {
            case MATERIAL_PRESET_DEFAULT:
                label = "Default";
                break;
            case MATERIAL_PRESET_MIRROR:
                label = "Mirror";
                break;
            case MATERIAL_PRESET_ROUGH_METAL:
                label = "Rough Metal";
                break;
            case MATERIAL_PRESET_GLOSSY:
                label = "Glossy";
                break;
            case MATERIAL_PRESET_EMISSIVE:
                label = "Emissive";
                break;
            case MATERIAL_PRESET_TRANSPARENT:
                label = "Transparent";
                break;
            default:
                break;
        }
        RenderLabelTextLeft(renderer, label_rect, label, text_color);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prev_mode);
#endif
}
