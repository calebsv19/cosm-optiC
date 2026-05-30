#include "editor/material_editor_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "editor/material_editor_knob_control.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_stack.h"

void MaterialEditorResetGroupListLayout(void) {
    memset(s_group_row_rects, 0, sizeof(s_group_row_rects));
    memset(s_group_row_addresses, 0, sizeof(s_group_row_addresses));
    memset(s_group_row_labels, 0, sizeof(s_group_row_labels));
    memset(s_group_header_label, 0, sizeof(s_group_header_label));
    for (int i = 0; i < MATERIAL_EDITOR_MAX_GROUP_ROWS; ++i) {
        s_group_row_face_groups[i] = -1;
    }
    s_group_row_count = 0;
    s_clear_groups_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_panel_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_list_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_visible_capacity = 0;
    s_group_total_count = 0;
}

void MaterialEditorResetLayerListLayout(void) {
    memset(s_layer_row_rects, 0, sizeof(s_layer_row_rects));
    memset(s_layer_toggle_rects, 0, sizeof(s_layer_toggle_rects));
    memset(s_layer_row_labels, 0, sizeof(s_layer_row_labels));
    memset(s_layer_action_rects, 0, sizeof(s_layer_action_rects));
    for (int i = 0; i < MATERIAL_EDITOR_MAX_LAYER_ROWS; ++i) {
        s_layer_row_indices[i] = -1;
    }
    s_layer_panel_rect = (SDL_Rect){0, 0, 0, 0};
    s_layer_list_rect = (SDL_Rect){0, 0, 0, 0};
    s_layer_row_count = 0;
    s_layer_visible_capacity = 0;
    s_layer_total_count = 0;
}

void MaterialEditorDrawButton(SDL_Renderer* renderer,
                              SDL_Rect rect,
                              const char* label,
                              bool active,
                              RayTracingThemePalette palette) {
    SDL_Color fill = active ? ray_tracing_theme_resolve_button_active_fill(palette)
                            : palette.button_fill;
    SDL_Color text = ray_tracing_theme_choose_button_text(fill, palette);
    if (!renderer || !label || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

int MaterialEditorDrawGroupList(SDL_Renderer* renderer,
                                SDL_Rect content_bounds,
                                int cursor_y,
                                int bottom_y,
                                RayTracingThemePalette palette) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    int group_count = 0;
    int selected_count = MaterialEditorSelectedFaceGroupCount();
    int header_h = 24;
    int row_stride = MATERIAL_EDITOR_GROUP_ROW_HEIGHT + MATERIAL_EDITOR_GROUP_ROW_GAP;
    int list_h = 0;
    int visible_capacity = 0;
    int scrollbar_w = 0;
    SDL_Rect header_rect = {0, 0, 0, 0};
    SDL_Rect list_rect = {0, 0, 0, 0};
    SDL_Rect clear_rect = {0, 0, 0, 0};
    if (!renderer || content_bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;
    MaterialEditorResetGroupListLayout();
    group_count = material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
    s_group_total_count = group_count;
    s_group_panel_rect = (SDL_Rect){content_bounds.x,
                                    cursor_y,
                                    content_bounds.w,
                                    bottom_y - cursor_y};
    if (cursor_y + header_h > bottom_y) return cursor_y;
    header_rect = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 18};
    snprintf(s_group_header_label,
             sizeof(s_group_header_label),
             "Face Groups %d/%d selected",
             selected_count,
             group_count);
    if (selected_count > 0 && content_bounds.w >= 180) {
        clear_rect = (SDL_Rect){content_bounds.x + content_bounds.w - 112,
                                cursor_y - 2,
                                112,
                                22};
        header_rect.w -= 120;
        s_clear_groups_rect = clear_rect;
    }
    RenderLabelTextLeft(renderer, header_rect, s_group_header_label, palette.text_primary);
    if (s_clear_groups_rect.w > 0) {
        MaterialEditorDrawButton(renderer, clear_rect, "Clear", false, palette);
    }
    cursor_y += header_h;
    list_h = bottom_y - cursor_y;
    if (group_count <= 0 || list_h < MATERIAL_EDITOR_GROUP_ROW_HEIGHT) {
        if (list_h > 18) {
            RenderLabelTextWrappedLeft(renderer,
                                       (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, list_h},
                                       "No focused-object faces available.",
                                       palette.text_muted);
        }
        return bottom_y;
    }
    visible_capacity = list_h / row_stride;
    if (visible_capacity < 1) visible_capacity = 1;
    if (visible_capacity > MATERIAL_EDITOR_MAX_GROUP_ROWS) visible_capacity = MATERIAL_EDITOR_MAX_GROUP_ROWS;
    if (visible_capacity > group_count) visible_capacity = group_count;
    if (s_group_scroll_offset > group_count - visible_capacity) {
        s_group_scroll_offset = group_count - visible_capacity;
    }
    if (s_group_scroll_offset < 0) s_group_scroll_offset = 0;
    s_group_visible_capacity = visible_capacity;
    scrollbar_w = group_count > visible_capacity ? 8 : 0;
    list_rect = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, list_h};
    s_group_list_rect = list_rect;
    for (int i = 0; i < visible_capacity; ++i) {
        int group_index = i + s_group_scroll_offset;
        int face_group = groups[group_index].face_group_index;
        SDL_Rect row = {content_bounds.x,
                        cursor_y + i * row_stride,
                        content_bounds.w - scrollbar_w - 4,
                        MATERIAL_EDITOR_GROUP_ROW_HEIGHT};
        bool selected = groups[group_index].selected_count > 0;
        bool active = selected && face_group == s_material_editor_active_face_group_index;
        SDL_Color fill = active ? ray_tracing_theme_resolve_button_active_fill(palette)
                                : palette.panel_fill;
        SDL_Color text = active ? ray_tracing_theme_choose_button_text(fill, palette)
                                : selected ? palette.text_primary : palette.text_muted;
        s_group_row_rects[i] = row;
        s_group_row_face_groups[i] = face_group;
        s_group_row_addresses[i] = groups[group_index].representative;
        snprintf(s_group_row_labels[i],
                 sizeof(s_group_row_labels[i]),
                 "Face #%d  %d/%d tris",
                 face_group,
                 groups[group_index].selected_count,
                 groups[group_index].triangle_count);
        s_group_row_count += 1;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, active ? 255 : 210);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               selected ? palette.accent_primary.r : palette.panel_border.r,
                               selected ? palette.accent_primary.g : palette.panel_border.g,
                               selected ? palette.accent_primary.b : palette.panel_border.b,
                               255);
        SDL_RenderDrawRect(renderer, &row);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){row.x + 8, row.y + 4, row.w - 16, 16},
                            s_group_row_labels[i],
                            text);
    }
    if (scrollbar_w > 0) {
        int track_h = visible_capacity * row_stride - MATERIAL_EDITOR_GROUP_ROW_GAP;
        int thumb_h = (track_h * visible_capacity) / group_count;
        int max_offset = group_count - visible_capacity;
        int thumb_travel = 0;
        SDL_Rect track = {content_bounds.x + content_bounds.w - scrollbar_w,
                          cursor_y,
                          4,
                          track_h};
        SDL_Rect thumb = track;
        if (thumb_h < 18) thumb_h = 18;
        if (thumb_h > track_h) thumb_h = track_h;
        thumb_travel = track_h - thumb_h;
        thumb.h = thumb_h;
        thumb.y = track.y + (max_offset > 0 ? (thumb_travel * s_group_scroll_offset) / max_offset : 0);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r,
                               palette.panel_border.g,
                               palette.panel_border.b,
                               180);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer,
                               palette.accent_primary.r,
                               palette.accent_primary.g,
                               palette.accent_primary.b,
                               255);
        SDL_RenderFillRect(renderer, &thumb);
    }
    return bottom_y;
}

int MaterialEditorDrawLayerList(SDL_Renderer* renderer,
                                SDL_Rect content_bounds,
                                int cursor_y,
                                int bottom_y,
                                const SceneObject* obj,
                                RayTracingThemePalette palette) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    int active_index = 0;
    int header_h = 18;
    int action_h = 20;
    int row_stride = MATERIAL_EDITOR_LAYER_ROW_HEIGHT + MATERIAL_EDITOR_LAYER_ROW_GAP;
    int row_capacity = 0;
    int panel_h = 0;
    int action_w = 0;
    int scrollbar_w = 0;
    const char* action_labels[MATERIAL_EDITOR_LAYER_ACTION_COUNT] = {
        "+", "Mute", "Up", "Down", "Del"
    };
    char header_text[48];
    if (!renderer || !obj || content_bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;
    MaterialEditorResetLayerListLayout();
    if (!MaterialEditorLayerModelGetEffectiveStack(obj, focused_object_index, &stack) ||
        stack.layerCount <= 0) {
        return cursor_y;
    }
    active_index = MaterialEditorLayerModelGetActiveIndex(obj, focused_object_index);
    row_capacity = stack.layerCount < MATERIAL_EDITOR_MAX_LAYER_ROWS
                       ? stack.layerCount
                       : MATERIAL_EDITOR_MAX_LAYER_ROWS;
    if (row_capacity < 1) row_capacity = 1;
    panel_h = header_h + action_h + MATERIAL_EDITOR_BUTTON_GAP +
              row_capacity * row_stride;
    if (!material_editor_has_room_for_optional_control(cursor_y, panel_h, bottom_y)) {
        return cursor_y;
    }
    s_layer_panel_rect = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, panel_h};
    s_layer_total_count = stack.layerCount;
    s_layer_visible_capacity = row_capacity;
    if (s_layer_scroll_offset > stack.layerCount - row_capacity) {
        s_layer_scroll_offset = stack.layerCount - row_capacity;
    }
    if (s_layer_scroll_offset < 0) s_layer_scroll_offset = 0;

    snprintf(header_text,
             sizeof(header_text),
             "Layers %d %s",
             stack.layerCount,
             SceneEditorMaterialStackHasObjectStack(focused_object_index) ? "v2" : "legacy");
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, header_h},
                        header_text,
                        palette.text_primary);
    cursor_y += header_h;

    action_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * (MATERIAL_EDITOR_LAYER_ACTION_COUNT - 1)) /
               MATERIAL_EDITOR_LAYER_ACTION_COUNT;
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_ACTION_COUNT; ++i) {
        int x = content_bounds.x + i * (action_w + MATERIAL_EDITOR_BUTTON_GAP);
        int w = (i == MATERIAL_EDITOR_LAYER_ACTION_COUNT - 1)
                    ? content_bounds.x + content_bounds.w - x
                    : action_w;
        bool active = false;
        if (i == 1 && active_index >= 0 && active_index < stack.layerCount) {
            active = !stack.layers[active_index].enabled;
        }
        s_layer_action_rects[i] = (SDL_Rect){x, cursor_y, w, action_h};
        MaterialEditorDrawButton(renderer, s_layer_action_rects[i], action_labels[i], active, palette);
    }
    cursor_y += action_h + MATERIAL_EDITOR_BUTTON_GAP;

    scrollbar_w = stack.layerCount > row_capacity ? 8 : 0;
    s_layer_list_rect = (SDL_Rect){content_bounds.x,
                                   cursor_y,
                                   content_bounds.w,
                                   row_capacity * row_stride};
    for (int i = 0; i < row_capacity; ++i) {
        int layer_index = i + s_layer_scroll_offset;
        RuntimeMaterialTextureLayer* layer = &stack.layers[layer_index];
        SDL_Rect row = {content_bounds.x,
                        cursor_y + i * row_stride,
                        content_bounds.w - scrollbar_w - 4,
                        MATERIAL_EDITOR_LAYER_ROW_HEIGHT};
        int toggle_w = 38;
        SDL_Rect toggle = {row.x + row.w - toggle_w - 3,
                           row.y + 3,
                           toggle_w,
                           row.h - 6};
        bool row_active = layer_index == active_index;
        SDL_Color fill = row_active ? ray_tracing_theme_resolve_button_active_fill(palette)
                                    : palette.panel_fill;
        SDL_Color text = row_active ? ray_tracing_theme_choose_button_text(fill, palette)
                                    : layer->enabled ? palette.text_primary : palette.text_muted;
        const char* role = RuntimeMaterialTextureLayerKindIsBase(layer->kind) ? "Base" : "Layer";
        s_layer_row_rects[i] = row;
        s_layer_toggle_rects[i] = toggle;
        s_layer_row_indices[i] = layer_index;
        snprintf(s_layer_row_labels[i],
                 sizeof(s_layer_row_labels[i]),
                 "#%d %s %s",
                 layer_index,
                 role,
                 layer->displayName[0] ? layer->displayName
                                       : RuntimeMaterialTextureLayerKindDisplayName(layer->kind));
        s_layer_row_count += 1;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, row_active ? 255 : 210);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               row_active ? palette.accent_primary.r : palette.panel_border.r,
                               row_active ? palette.accent_primary.g : palette.panel_border.g,
                               row_active ? palette.accent_primary.b : palette.panel_border.b,
                               255);
        SDL_RenderDrawRect(renderer, &row);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){row.x + 6, row.y + 3, row.w - toggle_w - 18, 16},
                            s_layer_row_labels[i],
                            text);
        MaterialEditorDrawButton(renderer, toggle, layer->enabled ? "On" : "Off", layer->enabled, palette);
    }
    if (scrollbar_w > 0) {
        int track_h = row_capacity * row_stride - MATERIAL_EDITOR_LAYER_ROW_GAP;
        int thumb_h = (track_h * row_capacity) / stack.layerCount;
        int max_offset = stack.layerCount - row_capacity;
        int thumb_travel = 0;
        SDL_Rect track = {content_bounds.x + content_bounds.w - scrollbar_w,
                          cursor_y,
                          4,
                          track_h};
        SDL_Rect thumb = track;
        if (thumb_h < 16) thumb_h = 16;
        if (thumb_h > track_h) thumb_h = track_h;
        thumb_travel = track_h - thumb_h;
        thumb.h = thumb_h;
        thumb.y = track.y + (max_offset > 0 ? (thumb_travel * s_layer_scroll_offset) / max_offset : 0);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r,
                               palette.panel_border.g,
                               palette.panel_border.b,
                               180);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer,
                               palette.accent_primary.r,
                               palette.accent_primary.g,
                               palette.accent_primary.b,
                               255);
        SDL_RenderFillRect(renderer, &thumb);
    }
    return s_layer_panel_rect.y + s_layer_panel_rect.h + MATERIAL_EDITOR_BUTTON_GAP;
}

int MaterialEditorDrawLayerKindButtons(SDL_Renderer* renderer,
                                       SDL_Rect content_bounds,
                                       int cursor_y,
                                       int bottom_y,
                                       const SceneObject* obj,
                                       RayTracingThemePalette palette) {
    RuntimeMaterialTextureLayer layer;
    RuntimeMaterialTextureLayerKind options[MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT];
    int button_w = 0;
    bool layer_controls = material_editor_use_object_layer_controls(obj);
    memset(s_layer_kind_rects, 0, sizeof(s_layer_kind_rects));
    memset(s_layer_kind_rect_kinds, 0, sizeof(s_layer_kind_rect_kinds));
    if (!layer_controls || !material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        return cursor_y;
    }
    if (!material_editor_has_room_for_optional_control(cursor_y,
                                                       15 + MATERIAL_EDITOR_BUTTON_HEIGHT,
                                                       bottom_y)) {
        return cursor_y;
    }
    if (RuntimeMaterialTextureLayerKindIsBase(layer.kind)) {
        options[0] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
        options[1] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL;
        options[2] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD;
        options[3] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK;
    } else {
        options[0] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
        options[1] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
        options[2] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME;
        options[3] = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL;
    }
    MATERIAL_EDITOR_SECTION_LABEL(renderer,
                                  content_bounds,
                                  cursor_y,
                                  RuntimeMaterialTextureLayerKindIsBase(layer.kind) ? "Base Type"
                                                                                    : "Overlay Type",
                                  palette);
    cursor_y += 15;
    button_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 3) /
               MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT;
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT; ++i) {
        int x = content_bounds.x + i * (button_w + MATERIAL_EDITOR_BUTTON_GAP);
        int w = (i == MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT - 1)
                    ? content_bounds.x + content_bounds.w - x
                    : button_w;
        s_layer_kind_rects[i] = (SDL_Rect){x, cursor_y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        s_layer_kind_rect_kinds[i] = options[i];
        MaterialEditorDrawButton(renderer,
                                 s_layer_kind_rects[i],
                                 material_editor_short_layer_kind_label(options[i]),
                                 layer.kind == options[i],
                                 palette);
    }
    return cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
}

void MaterialEditorDrawSlider(SDL_Renderer* renderer,
                              SDL_Rect bounds,
                              MaterialEditorSliderKind kind,
                              const SceneObject* obj,
                              RayTracingThemePalette palette) {
    double normalized = material_editor_value_for_slider(obj, kind);
    SDL_Rect label_rect = {bounds.x, bounds.y, bounds.w / 2, 14};
    SDL_Rect value_rect = {bounds.x + bounds.w / 2, bounds.y, bounds.w / 2, 14};
    SDL_Rect track_bg = {bounds.x + 1, bounds.y + 18, bounds.w - 2, MATERIAL_EDITOR_SLIDER_TRACK_HEIGHT};
    SDL_Rect track_fill = track_bg;
    SDL_Rect knob = {0, 0, MATERIAL_EDITOR_SLIDER_KNOB_WIDTH, track_bg.h + 9};
    char value_text[32];
    int slot = (int)kind - 1;
    if (!renderer || slot < 0 || slot >= 4) return;
    s_slider_sections[slot] = bounds;
    s_slider_tracks[slot] = track_bg;
    material_editor_format_slider_value(obj, kind, value_text, sizeof(value_text));
    RenderLabelTextLeft(renderer, label_rect, material_editor_label_for_slider(kind), palette.text_primary);
    RenderLabelTextLeft(renderer, value_rect, value_text, palette.text_muted);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_fill.r,
                           palette.panel_fill.g,
                           palette.panel_fill.b,
                           255);
    SDL_RenderFillRect(renderer, &track_bg);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           255);
    SDL_RenderDrawRect(renderer, &track_bg);
    track_fill.w = (int)lround(normalized * (double)track_bg.w);
    if (track_fill.w < 0) track_fill.w = 0;
    if (track_fill.w > track_bg.w) track_fill.w = track_bg.w;
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_primary.r,
                           palette.accent_primary.g,
                           palette.accent_primary.b,
                           190);
    SDL_RenderFillRect(renderer, &track_fill);
    knob.x = track_bg.x + (int)lround(normalized * (double)(track_bg.w - MATERIAL_EDITOR_SLIDER_KNOB_WIDTH));
    knob.y = track_bg.y - 4;
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_primary.r,
                           palette.accent_primary.g,
                           palette.accent_primary.b,
                           255);
    SDL_RenderFillRect(renderer, &knob);
    SDL_SetRenderDrawColor(renderer,
                           palette.text_primary.r,
                           palette.text_primary.g,
                           palette.text_primary.b,
                           210);
    SDL_RenderDrawRect(renderer, &knob);
}

void MaterialEditorDrawParamSlider(SDL_Renderer* renderer,
                                   SDL_Rect bounds,
                                   MaterialEditorTextureParamKind kind,
                                   const SceneObject* obj,
                                   RayTracingThemePalette palette) {
    double normalized = material_editor_value_for_param_slider(obj, kind);
    int slot = material_editor_texture_param_slot(kind);
    if (!renderer || slot < 0) return;
    s_param_sections[slot] = bounds;
    MaterialEditorKnobDraw(renderer,
                           bounds,
                           material_editor_label_for_param(obj, kind),
                           normalized,
                           palette);
}
