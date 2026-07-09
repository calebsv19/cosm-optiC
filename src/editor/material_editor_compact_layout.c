#include "editor/material_editor_internal.h"

MaterialEditorCompactLayoutRects s_material_editor_compact_layout_rects;

const char* MaterialEditorSubPaneLabel(MaterialEditorSubPane pane) {
    pane = MaterialEditorSubPaneClamp(pane);
    if (pane == MATERIAL_EDITOR_SUBPANE_STACK) return "Layer Stack";
    if (pane == MATERIAL_EDITOR_SUBPANE_RESPONSE) return "Surface Response";
    if (pane == MATERIAL_EDITOR_SUBPANE_TEXTURES) return "Textures & Channels";
    if (pane == MATERIAL_EDITOR_SUBPANE_FACE) return "Face / Region";
    if (pane == MATERIAL_EDITOR_SUBPANE_GRAPH) return "Node Graph";
    if (pane == MATERIAL_EDITOR_SUBPANE_PROOF) return "Preview & Proof";
    return "Layer Stack";
}

const char* MaterialEditorSubPaneCompactLabel(MaterialEditorSubPane pane) {
    pane = MaterialEditorSubPaneClamp(pane);
    if (pane == MATERIAL_EDITOR_SUBPANE_STACK) return "Stack";
    if (pane == MATERIAL_EDITOR_SUBPANE_RESPONSE) return "Resp";
    if (pane == MATERIAL_EDITOR_SUBPANE_TEXTURES) return "Tex";
    if (pane == MATERIAL_EDITOR_SUBPANE_FACE) return "Face";
    if (pane == MATERIAL_EDITOR_SUBPANE_GRAPH) return "Graph";
    if (pane == MATERIAL_EDITOR_SUBPANE_PROOF) return "Proof";
    return "Stack";
}

MaterialEditorSubPane MaterialEditorSubPaneClamp(MaterialEditorSubPane pane) {
    if ((int)pane < 0 || (int)pane >= MATERIAL_EDITOR_SUBPANE_COUNT) {
        return MATERIAL_EDITOR_SUBPANE_STACK;
    }
    return pane;
}

MaterialEditorSubPane MaterialEditorGetActiveSubPane(void) {
    s_material_editor_active_subpane =
        MaterialEditorSubPaneClamp(s_material_editor_active_subpane);
    return s_material_editor_active_subpane;
}

void MaterialEditorSetActiveSubPane(MaterialEditorSubPane pane) {
    s_material_editor_active_subpane = MaterialEditorSubPaneClamp(pane);
}

bool MaterialEditorIdentityPopoverOpen(void) {
    return s_material_editor_identity_popover_open;
}

void MaterialEditorSetIdentityPopoverOpen(bool open) {
    s_material_editor_identity_popover_open = open;
}

bool MaterialEditorToggleIdentityPopover(void) {
    s_material_editor_identity_popover_open = !s_material_editor_identity_popover_open;
    return s_material_editor_identity_popover_open;
}

MaterialEditorCompactLayoutMetrics MaterialEditorCompactLayoutDefaultMetrics(void) {
    MaterialEditorCompactLayoutMetrics metrics;
    metrics.pad_x = 6;
    metrics.pad_y = 4;
    metrics.gap = 4;
    metrics.header_h = MATERIAL_EDITOR_COMPACT_HEADER_HEIGHT;
    metrics.tab_h = MATERIAL_EDITOR_COMPACT_TAB_HEIGHT;
    metrics.tab_gap = 3;
    metrics.min_tab_w = MATERIAL_EDITOR_COMPACT_MIN_TAB_WIDTH;
    metrics.popover_h = MATERIAL_EDITOR_IDENTITY_POPOVER_HEIGHT;
    return metrics;
}

MaterialEditorCompactLayoutRects MaterialEditorCompactLayoutBuild(
    SDL_Rect content_bounds,
    bool identity_popover_open) {
    MaterialEditorCompactLayoutMetrics metrics = MaterialEditorCompactLayoutDefaultMetrics();
    MaterialEditorCompactLayoutRects rects = {0};
    int inner_x = content_bounds.x + metrics.pad_x;
    int inner_w = content_bounds.w - metrics.pad_x * 2;
    int y = content_bounds.y + metrics.pad_y;
    int total_tab_gap = metrics.tab_gap * (MATERIAL_EDITOR_SUBPANE_COUNT - 1);
    int tab_w = 0;

    if (inner_w < 0) inner_w = 0;
    rects.identity_header = (SDL_Rect){inner_x, y, inner_w, metrics.header_h};
    rects.identity_disclosure =
        (SDL_Rect){inner_x + inner_w - metrics.header_h,
                   y,
                   metrics.header_h,
                   metrics.header_h};
    y += metrics.header_h + metrics.gap;

    rects.tab_row = (SDL_Rect){inner_x, y, inner_w, metrics.tab_h};
    tab_w = (inner_w - total_tab_gap) / MATERIAL_EDITOR_SUBPANE_COUNT;
    if (tab_w < metrics.min_tab_w) tab_w = metrics.min_tab_w;
    for (int i = 0; i < MATERIAL_EDITOR_SUBPANE_COUNT; ++i) {
        int x = inner_x + i * (tab_w + metrics.tab_gap);
        int w = tab_w;
        if (i == MATERIAL_EDITOR_SUBPANE_COUNT - 1 && x < inner_x + inner_w) {
            w = inner_x + inner_w - x;
        }
        if (x + w > inner_x + inner_w) {
            w = inner_x + inner_w - x;
        }
        if (w < 0) w = 0;
        rects.tab_rects[i] = (SDL_Rect){x, y, w, metrics.tab_h};
    }
    y += metrics.tab_h + metrics.gap;

    rects.content = (SDL_Rect){inner_x,
                               y,
                               inner_w,
                               content_bounds.y + content_bounds.h - y - metrics.pad_y};
    if (rects.content.h < 0) rects.content.h = 0;

    rects.identity_popover_visible = identity_popover_open;
    if (identity_popover_open) {
        int popover_h = metrics.popover_h;
        int max_h = content_bounds.y + content_bounds.h - rects.identity_header.y -
                    rects.identity_header.h - metrics.pad_y;
        if (popover_h > max_h) popover_h = max_h;
        if (popover_h < 0) popover_h = 0;
        rects.identity_popover =
            (SDL_Rect){rects.identity_header.x,
                       rects.identity_header.y + rects.identity_header.h + metrics.gap,
                       rects.identity_header.w,
                       popover_h};
    }

    return rects;
}
