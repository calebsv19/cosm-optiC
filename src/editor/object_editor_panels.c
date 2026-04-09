#include "editor/object_editor_panels.h"

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "geo/shape_library.h"
#include "material/material_manager.h"
#include "render/render_helper.h"
#include <stdbool.h>
#include <stdio.h>

#define ASSET_ROW_HEIGHT 22
#define PANEL_HEADER_HEIGHT 26
#define PANEL_PADDING 6
#define PANEL_MAX_HEIGHT 220
#define ASSET_PANEL_WIDTH 200
#define MATERIAL_ROW_HEIGHT 18

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

void ObjectEditorPanels_UpdateLayout(void) {
    int headerH = ObjectEditorHeaderHeight();
    int assetRowH = ObjectEditorAssetRowHeight();
    int materialRowH = ObjectEditorMaterialRowHeight();
    int panelMaxH = animation_config_scale_text_point_size(&animSettings, PANEL_MAX_HEIGHT, 160);
    int headerW = ASSET_PANEL_WIDTH - PANEL_PADDING * 2;
    int x = 20;
    int y = 40;
    if (panelMaxH > sceneSettings.windowHeight - 80) {
        panelMaxH = sceneSettings.windowHeight - 80;
    }
    if (panelMaxH < 140) panelMaxH = 140;
    int assetRows = showImports ? importCount : (int)assetLib.count;
    if (assetRows < 1) assetRows = 1;
    int assetContent = headerH + PANEL_PADDING * 2 + assetRows * assetRowH;
    if (assetContent > panelMaxH) assetContent = panelMaxH;
    assetPanelRect = (SDL_Rect){x, y, ASSET_PANEL_WIDTH, assetContent};
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
    int matY = sceneSettings.windowHeight - matContent - 20;
    materialPanelRect = (SDL_Rect){x, matY, ASSET_PANEL_WIDTH, matContent};
    materialCollapseRect = (SDL_Rect){materialPanelRect.x + materialPanelRect.w - PANEL_PADDING - 16,
                                      materialPanelRect.y + PANEL_PADDING,
                                      16,
                                      16};
}

void ObjectEditorPanels_DrawAssetList(SDL_Renderer* renderer) {
    SDL_Rect panel = assetPanelRect;
    int assetRowH = ObjectEditorAssetRowHeight();
#if !USE_VULKAN
    SDL_BlendMode prevMode;
    SDL_GetRenderDrawBlendMode(renderer, &prevMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 30);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &assetToggleRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &assetToggleRect);
    RenderButtonText(renderer, assetToggleRect, showImports ? "Imports" : "Assets");
    SDL_SetRenderDrawColor(renderer, assetsCollapsed ? 200 : 100, assetsCollapsed ? 60 : 200, 80, 220);
    SDL_RenderFillRect(renderer, &assetCollapseRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &assetCollapseRect);

    if (assetsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
        return;
    }

    int listY = assetToggleRect.y + assetToggleRect.h + 4;
    int visible = showImports ? importCount : (int)assetLib.count;
    int rowAreaH = panel.h - (listY - panel.y) - PANEL_PADDING;
    if (rowAreaH < assetRowH) rowAreaH = assetRowH;
    int maxRows = rowAreaH / assetRowH;
    if (maxRows < 1) maxRows = 1;
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
        SDL_SetRenderDrawColor(renderer, selected ? 80 : 25, selected ? 160 : 25, selected ? 240 : 25, 200);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
        RenderButtonText(renderer, row, label);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
}

void ObjectEditorPanels_DrawMaterialList(SDL_Renderer* renderer) {
    SDL_Rect panel = materialPanelRect;
    int materialRowH = ObjectEditorMaterialRowHeight();
#if !USE_VULKAN
    SDL_BlendMode prevMode;
    SDL_GetRenderDrawBlendMode(renderer, &prevMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 30);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, materialsCollapsed ? 200 : 100, materialsCollapsed ? 60 : 200, 80, 220);
    SDL_RenderFillRect(renderer, &materialCollapseRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &materialCollapseRect);

    if (materialsCollapsed) {
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
        return;
    }

    int count = MaterialManagerCount();
    int listY = panel.y + PANEL_PADDING;
    int rowAreaH = panel.h - PANEL_PADDING * 2;
    if (rowAreaH < materialRowH) rowAreaH = materialRowH;
    int maxRows = rowAreaH / materialRowH;
    if (maxRows < 1) maxRows = 1;
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
        bool selected = (i == selectedMaterialIndex);
        SDL_SetRenderDrawColor(renderer, selected ? 70 : 25, selected ? 140 : 25, selected ? 220 : 25, 200);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &row);

        const char* label = "Material";
        switch (i) {
            case MATERIAL_PRESET_DEFAULT: label = "Default"; break;
            case MATERIAL_PRESET_MIRROR: label = "Mirror"; break;
            case MATERIAL_PRESET_ROUGH_METAL: label = "Rough Metal"; break;
            case MATERIAL_PRESET_GLOSSY: label = "Glossy"; break;
            case MATERIAL_PRESET_EMISSIVE: label = "Emissive"; break;
            default: break;
        }
        RenderButtonText(renderer, row, label);
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, prevMode);
#endif
}
