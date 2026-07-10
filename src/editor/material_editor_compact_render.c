#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/material_editor_compact_response_render.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"

static void material_editor_reset_compact_control_rects(void) {
    memset(s_slider_sections, 0, sizeof(s_slider_sections));
    memset(s_slider_tracks, 0, sizeof(s_slider_tracks));
    memset(s_param_sections, 0, sizeof(s_param_sections));
    memset(s_pattern_rects, 0, sizeof(s_pattern_rects));
    memset(s_layer_kind_rects, 0, sizeof(s_layer_kind_rects));
    memset(s_layer_kind_rect_kinds, 0, sizeof(s_layer_kind_rect_kinds));
    memset(s_graph_action_rects, 0, sizeof(s_graph_action_rects));
    memset(s_recipe_action_rects, 0, sizeof(s_recipe_action_rects));
    memset(s_recipe_menu_item_rects, 0, sizeof(s_recipe_menu_item_rects));
    memset(s_response_action_rects, 0, sizeof(s_response_action_rects));
    memset(s_response_action_fields, 0, sizeof(s_response_action_fields));
    memset(s_layer_influence_action_rects, 0, sizeof(s_layer_influence_action_rects));
    memset(s_layer_influence_action_fields, 0, sizeof(s_layer_influence_action_fields));
    memset(s_layer_opacity_action_rects, 0, sizeof(s_layer_opacity_action_rects));
    memset(s_glass_overlay_action_rects, 0, sizeof(s_glass_overlay_action_rects));
    memset(s_glass_overlay_action_kinds, 0, sizeof(s_glass_overlay_action_kinds));
    MaterialEditorResetLayerListLayout();
    s_texture_none_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_rust_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_fog_rect = (SDL_Rect){0, 0, 0, 0};
    s_solid_faces_rect = (SDL_Rect){0, 0, 0, 0};
    s_reset_face_rect = (SDL_Rect){0, 0, 0, 0};
    s_copy_face_rect = (SDL_Rect){0, 0, 0, 0};
    s_proof_readback_rect = (SDL_Rect){0, 0, 0, 0};
    MaterialEditorResetGroupListLayout();
}

static void material_editor_draw_panel_frame(SDL_Renderer* renderer,
                                             SDL_Rect rect,
                                             RayTracingThemePalette palette) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_fill.r,
                           palette.panel_fill.g,
                           palette.panel_fill.b,
                           235);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &rect);
}

static void material_editor_draw_shell(SDL_Renderer* renderer,
                                       const SceneObject* obj,
                                       int focused_index,
                                       RayTracingThemePalette palette) {
    MaterialEditorMaterialReadback material = {0};
    MaterialEditorRecipeReadback recipe = {0};
    const char* source_label = "Preset";
    char line[160];
    char chip_label[3][80];
    SDL_Rect header_text = s_material_editor_compact_layout_rects.identity_header;
    SDL_Rect recipe_area = s_material_editor_compact_layout_rects.identity_header;
    if (!renderer) return;
    if (obj && MaterialEditorBuildMaterialReadback(&material) && material.graph_backed) {
        source_label = "Graph";
    } else if (material.custom_stack || material.authored_texture_bound) {
        source_label = "Custom";
    } else if (focused_index >= 0 && SceneEditorMaterialStackHasObjectStack(focused_index)) {
        source_label = "Custom";
    }
    material_editor_draw_panel_frame(renderer,
                                     s_material_editor_compact_layout_rects.identity_header,
                                     palette);
    header_text.x += 6;
    header_text.w -= s_material_editor_compact_layout_rects.identity_disclosure.w + 12;
    recipe_area.x += 6;
    recipe_area.y += 2;
    recipe_area.w -= s_material_editor_compact_layout_rects.identity_disclosure.w + 14;
    recipe_area.h = MATERIAL_EDITOR_BUTTON_HEIGHT;
    if (obj) {
        if (MaterialEditorBuildRecipeReadback(&recipe)) {
            snprintf(chip_label[0], sizeof(chip_label[0]), "%s v", recipe.family_label);
            snprintf(chip_label[1], sizeof(chip_label[1]), "%s v", recipe.surface_label);
            snprintf(chip_label[2], sizeof(chip_label[2]), "%s v", recipe.finish_label);
            for (int i = 0; i < MATERIAL_EDITOR_RECIPE_ACTION_COUNT; ++i) {
                int chip_w = (recipe_area.w - MATERIAL_EDITOR_BUTTON_GAP * 2) /
                             MATERIAL_EDITOR_RECIPE_ACTION_COUNT;
                int chip_x = recipe_area.x + i * (chip_w + MATERIAL_EDITOR_BUTTON_GAP);
                if (i == MATERIAL_EDITOR_RECIPE_ACTION_COUNT - 1) {
                    chip_w = recipe_area.x + recipe_area.w - chip_x;
                }
                s_recipe_action_rects[i] =
                    (SDL_Rect){chip_x, recipe_area.y, chip_w, recipe_area.h};
                MaterialEditorDrawButton(
                    renderer,
                    s_recipe_action_rects[i],
                    chip_label[i],
                    MaterialEditorGetRecipeMenuAxis() == (MaterialEditorRecipeAxis)i,
                    palette);
            }
            line[0] = '\0';
        } else {
            snprintf(line,
                     sizeof(line),
                     "Mat Obj #%d  id=%d  %s",
                     focused_index,
                     obj->material_id,
                     source_label);
        }
    } else {
        snprintf(line, sizeof(line), "Mat no object");
    }
    if (line[0]) {
        RenderLabelTextLeft(renderer, header_text, line, palette.text_primary);
    }
    MaterialEditorDrawButton(renderer,
                             s_material_editor_compact_layout_rects.identity_disclosure,
                             MaterialEditorIdentityPopoverOpen() ? "^" : "v",
                             MaterialEditorIdentityPopoverOpen(),
                             palette);
    for (int i = 0; i < MATERIAL_EDITOR_SUBPANE_COUNT; ++i) {
        SDL_Rect tab = s_material_editor_compact_layout_rects.tab_rects[i];
        if (tab.w <= 0 || tab.h <= 0) continue;
        MaterialEditorDrawButton(renderer,
                                 tab,
                                 MaterialEditorSubPaneCompactLabel((MaterialEditorSubPane)i),
                                 MaterialEditorGetActiveSubPane() == (MaterialEditorSubPane)i,
                                 palette);
    }
}

static void material_editor_draw_recipe_menu(SDL_Renderer* renderer,
                                             RayTracingThemePalette palette) {
    MaterialEditorRecipeAxis axis = MaterialEditorGetRecipeMenuAxis();
    MaterialEditorRecipeOption options[MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS];
    SDL_Rect anchor;
    SDL_Rect menu;
    int option_count = 0;
    int menu_w = 0;
    if (!renderer ||
        axis < MATERIAL_EDITOR_RECIPE_AXIS_FAMILY ||
        axis > MATERIAL_EDITOR_RECIPE_AXIS_FINISH) {
        return;
    }
    anchor = s_recipe_action_rects[(int)axis];
    if (anchor.w <= 0 || anchor.h <= 0) return;
    option_count = MaterialEditorBuildRecipeOptions(axis,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    if (option_count <= 0) return;
    menu_w = anchor.w < 128 ? 128 : anchor.w;
    menu = (SDL_Rect){anchor.x,
                      anchor.y + anchor.h + 2,
                      menu_w,
                      option_count * (MATERIAL_EDITOR_BUTTON_HEIGHT + 1) + 8};
    if (menu.x + menu.w > s_material_editor_compact_layout_rects.identity_header.x +
                             s_material_editor_compact_layout_rects.identity_header.w) {
        menu.x = s_material_editor_compact_layout_rects.identity_header.x +
                 s_material_editor_compact_layout_rects.identity_header.w - menu.w - 4;
    }
    if (menu.x < s_material_editor_compact_layout_rects.identity_header.x + 4) {
        menu.x = s_material_editor_compact_layout_rects.identity_header.x + 4;
    }
    material_editor_draw_panel_frame(renderer, menu, palette);
    for (int i = 0; i < option_count; ++i) {
        s_recipe_menu_item_rects[i] =
            (SDL_Rect){menu.x + 4,
                       menu.y + 4 + i * (MATERIAL_EDITOR_BUTTON_HEIGHT + 1),
                       menu.w - 8,
                       MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer,
                                 s_recipe_menu_item_rects[i],
                                 options[i].label,
                                 options[i].selected,
                                 palette);
    }
}

static int material_editor_draw_texture_kind_buttons(SDL_Renderer* renderer,
                                                     SDL_Rect bounds,
                                                     int cursor_y,
                                                     int bottom_y,
                                                     const SceneObject* obj,
                                                     RayTracingThemePalette palette) {
    int third_w = 0;
    int active_texture = 0;
    if (!obj) return cursor_y;
    if (material_editor_use_object_layer_controls(obj)) return cursor_y;
    third_w = (bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 2) / 3;
    if (cursor_y + 15 + MATERIAL_EDITOR_BUTTON_HEIGHT > bottom_y) return cursor_y;
    MATERIAL_EDITOR_SECTION_LABEL(renderer,
                                  bounds,
                                  cursor_y,
                                  MaterialEditorPanelGroupLabel(
                                      MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING),
                                  palette);
    cursor_y += 15;
    s_texture_none_rect = (SDL_Rect){bounds.x, cursor_y, third_w, MATERIAL_EDITOR_BUTTON_HEIGHT};
    s_texture_rust_rect =
        (SDL_Rect){s_texture_none_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP,
                   cursor_y,
                   third_w,
                   MATERIAL_EDITOR_BUTTON_HEIGHT};
    s_texture_fog_rect =
        (SDL_Rect){s_texture_rust_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP,
                   cursor_y,
                   bounds.w - third_w * 2 - MATERIAL_EDITOR_BUTTON_GAP * 2,
                   MATERIAL_EDITOR_BUTTON_HEIGHT};
    active_texture = material_editor_texture_kind_for_controls(obj);
    MaterialEditorDrawButton(renderer, s_texture_none_rect, "None", active_texture == 0, palette);
    MaterialEditorDrawButton(renderer, s_texture_rust_rect, "Rust", active_texture == 1, palette);
    MaterialEditorDrawButton(renderer, s_texture_fog_rect, "Fog", active_texture == 2, palette);
    return cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
}

static int material_editor_draw_texture_channel_readback(SDL_Renderer* renderer,
                                                         SDL_Rect bounds,
                                                         int cursor_y,
                                                         int bottom_y,
                                                         int focused_index,
                                                         RayTracingThemePalette palette) {
    MaterialEditorTextureChannelReadback readback = {0};
    char line[192];
    if (!renderer) return cursor_y;
    if (!material_editor_has_room_for_optional_control(cursor_y, 86, bottom_y)) {
        return cursor_y;
    }
    if (!MaterialEditorBuildTextureChannelReadback(focused_index, &readback)) {
        return cursor_y;
    }
    MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Channel Ownership", palette);
    cursor_y += 15;
    snprintf(line, sizeof(line), "Visual %s", readback.visual_channels);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        readback.visual_count > 0 ? palette.text_primary : palette.text_muted);
    cursor_y += 18;
    snprintf(line, sizeof(line), "Physical %s", readback.physical_channels);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        readback.physical_count > 0 ? palette.text_primary : palette.text_muted);
    cursor_y += 18;
    snprintf(line, sizeof(line), "Future %s", readback.future_channels);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        readback.future_count > 0 ? palette.text_primary : palette.text_muted);
    cursor_y += 18;
    snprintf(line, sizeof(line), "Procedural %s", readback.procedural_source);
    RenderLabelTextWrappedLeft(renderer,
                               (SDL_Rect){bounds.x, cursor_y, bounds.w, 32},
                               line,
                               palette.text_muted);
    cursor_y += 34;
    if (cursor_y + 16 <= bottom_y) {
        snprintf(line, sizeof(line), "Deferred %s", readback.deferred_channels);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                            line,
                            palette.text_muted);
        cursor_y += 18;
    }
    if (readback.has_glass_mapping && cursor_y + 50 <= bottom_y) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                   readback.glass_authored_mapping,
                                   palette.text_primary);
        cursor_y += 36;
    }
    if (readback.has_glass_mapping && cursor_y + 34 <= bottom_y) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                   readback.glass_procedural_mapping,
                                   palette.text_muted);
        cursor_y += 36;
    }
    if (readback.has_glass_mapping && cursor_y + 34 <= bottom_y) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                   readback.glass_deferred_mapping,
                                   palette.text_muted);
        cursor_y += 36;
    }
    return cursor_y;
}

static int material_editor_draw_placement_controls(SDL_Renderer* renderer,
                                                   SDL_Rect bounds,
                                                   int cursor_y,
                                                   int bottom_y,
                                                   const SceneObject* obj,
                                                   RayTracingThemePalette palette) {
    int grid_y = cursor_y;
    int drawn_rows = 0;
    int col_w = (bounds.w - MATERIAL_EDITOR_CONTROL_GAP) / 2;
    if (!renderer || !obj) return cursor_y;
    if (material_editor_has_room_for_optional_control(
            cursor_y,
            15 + MATERIAL_EDITOR_SLIDER_HEIGHT * 2 + MATERIAL_EDITOR_CONTROL_GAP,
            bottom_y)) {
        MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Texture Placement", palette);
        cursor_y += 15;
        grid_y = cursor_y;
    }
    for (int i = 0; i < 4; ++i) {
        MaterialEditorSliderKind kind = (MaterialEditorSliderKind)(i + 1);
        int col = i % 2;
        int row = i / 2;
        int x = bounds.x + col * (col_w + MATERIAL_EDITOR_CONTROL_GAP);
        int y = grid_y + row * (MATERIAL_EDITOR_SLIDER_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
        int w = (col == 1) ? bounds.x + bounds.w - x : col_w;
        if (y + MATERIAL_EDITOR_SLIDER_HEIGHT > bottom_y) break;
        MaterialEditorDrawSlider(renderer,
                                 (SDL_Rect){x, y, w, MATERIAL_EDITOR_SLIDER_HEIGHT},
                                 kind,
                                 obj,
                                 palette);
        if (drawn_rows < row + 1) drawn_rows = row + 1;
    }
    return cursor_y + drawn_rows * (MATERIAL_EDITOR_SLIDER_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
}

static int material_editor_draw_face_region_readback(SDL_Renderer* renderer,
                                                     SDL_Rect bounds,
                                                     int cursor_y,
                                                     int bottom_y,
                                                     const MaterialEditorFaceRegionReadback* readback,
                                                     RayTracingThemePalette palette) {
    if (!renderer || !readback) return cursor_y;
    if (!material_editor_has_room_for_optional_control(cursor_y, 86, bottom_y)) {
        return cursor_y;
    }
    MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Region Selection", palette);
    cursor_y += 15;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        readback->active_label,
                        palette.text_primary);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        readback->selection_label,
                        palette.text_muted);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        readback->layer_label,
                        palette.text_muted);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        readback->override_label,
                        readback->can_reset ? palette.text_primary : palette.text_muted);
    return cursor_y + 20;
}

static int material_editor_draw_face_controls(SDL_Renderer* renderer,
                                              SDL_Rect bounds,
                                              int cursor_y,
                                              int bottom_y,
                                              const SceneObject* obj,
                                              int focused_index,
                                              int selected_faces,
                                              RayTracingThemePalette palette) {
    MaterialEditorFaceRegionReadback readback = {0};
    bool has_readback = false;
    bool show_reset = false;
    bool show_copy = false;
    if (!renderer || !obj) return cursor_y;
    (void)focused_index;
    (void)selected_faces;
    has_readback = MaterialEditorBuildFaceRegionReadback(&readback);
    if (has_readback) {
        cursor_y = material_editor_draw_face_region_readback(renderer,
                                                             bounds,
                                                             cursor_y,
                                                             bottom_y,
                                                             &readback,
                                                             palette);
    }
    show_reset = readback.can_reset;
    show_copy = readback.can_copy_to_selected;
    if (show_reset && show_copy &&
        cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        int half_w = (bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2;
        s_reset_face_rect = (SDL_Rect){bounds.x, cursor_y, half_w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        s_copy_face_rect = (SDL_Rect){bounds.x + half_w + MATERIAL_EDITOR_BUTTON_GAP,
                                      cursor_y,
                                      bounds.w - half_w - MATERIAL_EDITOR_BUTTON_GAP,
                                      MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer, s_reset_face_rect, "Reset", false, palette);
        MaterialEditorDrawButton(renderer, s_copy_face_rect, "Copy", false, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    } else if (show_reset && cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        s_reset_face_rect = (SDL_Rect){bounds.x, cursor_y, bounds.w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer, s_reset_face_rect, "Reset Face", false, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    } else if (show_copy && cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        s_copy_face_rect = (SDL_Rect){bounds.x, cursor_y, bounds.w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer, s_copy_face_rect, "Copy to Selected", false, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    return MaterialEditorDrawGroupList(renderer, bounds, cursor_y, bottom_y, palette);
}

static int material_editor_draw_proof_controls(SDL_Renderer* renderer,
                                               SDL_Rect bounds,
                                               int cursor_y,
                                               int bottom_y,
                                               RayTracingThemePalette palette) {
    if (cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        s_solid_faces_rect = (SDL_Rect){bounds.x, cursor_y, bounds.w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer,
                                 s_solid_faces_rect,
                                 "Solid Preview",
                                 s_material_editor_solid_faces_enabled,
                                 palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    if (material_editor_has_room_for_optional_control(cursor_y,
                                                       15 + MATERIAL_EDITOR_BUTTON_HEIGHT,
                                                       bottom_y)) {
        MATERIAL_EDITOR_SECTION_LABEL(renderer,
                                      bounds,
                                      cursor_y,
                                      MaterialEditorPanelGroupLabel(
                                          MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK),
                                      palette);
        cursor_y += 15;
        s_proof_readback_rect = (SDL_Rect){bounds.x, cursor_y, bounds.w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer,
                                 s_proof_readback_rect,
                                 "M4 Proof Readback",
                                 s_material_editor_proof_readback_valid,
                                 palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
        if (s_material_editor_proof_readback_valid &&
            material_editor_has_room_for_optional_control(cursor_y, 18, bottom_y)) {
            RenderLabelTextLeft(renderer,
                                (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                                s_material_editor_proof_readback_status,
                                palette.text_muted);
            cursor_y += 20;
        }
        if (s_material_editor_proof_readback_valid &&
            (s_material_editor_proof_readback.glass_proof_readback ||
             s_material_editor_proof_readback.mirror_proof_readback ||
             s_material_editor_proof_readback.metal_proof_readback) &&
            material_editor_has_room_for_optional_control(cursor_y, 54, bottom_y)) {
            const char* coverage = s_material_editor_proof_readback.glass_proof_readback
                                       ? s_material_editor_proof_readback.glass_proof_coverage
                                       : s_material_editor_proof_readback.mirror_proof_readback
                                             ? s_material_editor_proof_readback.mirror_proof_coverage
                                             : s_material_editor_proof_readback.metal_proof_coverage;
            const char* proof_package = s_material_editor_proof_readback.glass_proof_readback
                                            ? s_material_editor_proof_readback.glass_proof_package
                                            : s_material_editor_proof_readback.mirror_proof_readback
                                                  ? s_material_editor_proof_readback.mirror_proof_package
                                                  : s_material_editor_proof_readback.metal_proof_package;
            RenderLabelTextWrappedLeft(renderer,
                                       (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                       coverage,
                                       palette.text_primary);
            cursor_y += 36;
            RenderLabelTextLeft(renderer,
                                (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                                proof_package,
                                palette.text_muted);
            cursor_y += 18;
        }
        if (s_material_editor_proof_readback_valid &&
            (s_material_editor_proof_readback.glass_proof_readback ||
             s_material_editor_proof_readback.mirror_proof_readback ||
             s_material_editor_proof_readback.metal_proof_readback) &&
            material_editor_has_room_for_optional_control(cursor_y, 34, bottom_y)) {
            const char* missing = s_material_editor_proof_readback.glass_proof_readback
                                      ? s_material_editor_proof_readback.glass_missing_proof
                                      : s_material_editor_proof_readback.mirror_proof_readback
                                            ? s_material_editor_proof_readback.mirror_missing_proof
                                            : s_material_editor_proof_readback.metal_missing_proof;
            RenderLabelTextWrappedLeft(renderer,
                                       (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                       missing,
                                       palette.text_muted);
            cursor_y += 36;
        }
    }
    return cursor_y;
}

static int material_editor_draw_graph_readback(SDL_Renderer* renderer,
                                               SDL_Rect bounds,
                                               int cursor_y,
                                               int bottom_y,
                                               RayTracingThemePalette palette) {
    MaterialEditorGraphReadback readback = {0};
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    const char* action_labels[MATERIAL_EDITOR_GRAPH_ACTION_COUNT] = {
        "Create", "Add Layer", "Add Channel", "Clear"
    };
    char line[160];
    int action_w = 0;
    if (!renderer) return cursor_y;
    if (!MaterialEditorBuildFocusedGraphReadback(&readback)) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 36},
                                   "Graph readback unavailable for the current focus.",
                                   palette.text_muted);
        return cursor_y + 38;
    }
    if (material_editor_has_room_for_optional_control(cursor_y,
                                                       2 * (MATERIAL_EDITOR_BUTTON_HEIGHT +
                                                            MATERIAL_EDITOR_BUTTON_GAP),
                                                       bottom_y)) {
        action_w = (bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2;
        for (int i = 0; i < MATERIAL_EDITOR_GRAPH_ACTION_COUNT; ++i) {
            int row = i / 2;
            int col = i % 2;
            int x = bounds.x + col * (action_w + MATERIAL_EDITOR_BUTTON_GAP);
            int y = cursor_y + row * (MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP);
            int w = col == 1 ? bounds.x + bounds.w - x : action_w;
            s_graph_action_rects[i] = (SDL_Rect){x, y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
            MaterialEditorDrawButton(renderer,
                                     s_graph_action_rects[i],
                                     action_labels[i],
                                     false,
                                     palette);
        }
        cursor_y += 2 * (MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP);
    }
    snprintf(line,
             sizeof(line),
             "Graph %s",
             readback.has_graph ? readback.graph_id : "none");
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        palette.text_primary);
    cursor_y += 18;
    snprintf(line,
             sizeof(line),
             "Route %s",
             readback.evaluator_route);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        palette.text_muted);
    cursor_y += 18;
    snprintf(line,
             sizeof(line),
             "Nodes %d  Stack %d  Channels %d",
             readback.graph_node_count,
             readback.compiled_stack_layer_count,
             readback.channel_ref_count);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        line,
                        palette.text_muted);
    cursor_y += 18;
    RenderLabelTextWrappedLeft(renderer,
                               (SDL_Rect){bounds.x, cursor_y, bounds.w, 42},
                               readback.integration_status,
                               palette.text_muted);
    cursor_y += 42;
    if (readback.has_graph &&
        SceneEditorMaterialGraphGetObjectGraph(readback.scene_object_index, &graph) &&
        material_editor_has_room_for_optional_control(cursor_y, 20, bottom_y)) {
        int max_rows = 4;
        int row_h = 18;
        for (int i = 0; i < graph.nodeCount && i < max_rows; ++i) {
            const RuntimeMaterialGraphNode* node = &graph.nodes[i];
            const char* kind = "Node";
            if (!node->active || cursor_y + row_h > bottom_y) break;
            if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_LAYER) {
                kind = RuntimeMaterialTextureLayerKindDisplayName(node->layer.kind);
            } else if (node->kind == RUNTIME_MATERIAL_GRAPH_NODE_KIND_CHANNEL_OUTPUT) {
                kind = node->channelRef.channel;
            }
            snprintf(line, sizeof(line), "%s | %s", node->nodeId, kind);
            RenderLabelTextLeft(renderer,
                                (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                                line,
                                palette.text_muted);
            cursor_y += row_h;
        }
    }
    return cursor_y;
}

static void material_editor_draw_identity_popover(SDL_Renderer* renderer,
                                                  const SceneObject* obj,
                                                  int focused_index,
                                                  int selected_faces,
                                                  int focused_faces,
                                                  const char* edit_text,
                                                  RayTracingThemePalette palette) {
    MaterialEditorMaterialReadback material = {0};
    MaterialEditorRecipeReadback recipe = {0};
    char line[160];
    SDL_Rect pop = s_material_editor_compact_layout_rects.identity_popover;
    int y = pop.y + 8;
    if (!s_material_editor_compact_layout_rects.identity_popover_visible ||
        pop.w <= 0 ||
        pop.h <= 0) {
        return;
    }
    material_editor_draw_panel_frame(renderer, pop, palette);
    if (!obj) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){pop.x + 8, y, pop.w - 16, pop.h - 16},
                                   "No focused material object.",
                                   palette.text_muted);
        return;
    }
    if (MaterialEditorBuildRecipeReadback(&recipe)) {
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                            recipe.header_label,
                            palette.text_primary);
        y += 18;
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                            recipe.detail_label,
                            palette.text_muted);
        y += 18;
    } else {
        snprintf(line, sizeof(line), "Object #%d  material id %d", focused_index, obj->material_id);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                            line,
                            palette.text_primary);
        y += 18;
    }
    if (MaterialEditorBuildMaterialReadback(&material)) {
        snprintf(line,
                 sizeof(line),
                 "%s | %s",
                 material.preset_label,
                 material.state_label);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                            line,
                            palette.text_muted);
        y += 18;
    }
    snprintf(line,
             sizeof(line),
             "Faces %d/%d selected | %s",
             selected_faces,
             focused_faces,
             edit_text);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                        line,
                        palette.text_muted);
    y += 18;
    if (material.source_label[0] && y + 16 <= pop.y + pop.h - 8) {
        snprintf(line,
                 sizeof(line),
                 "Source %s | %s",
                 material.source_label,
                 material.save_request_label);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){pop.x + 8, y, pop.w - 16, 16},
                            line,
                            palette.text_muted);
        y += 18;
    }
    if (y + 18 <= pop.y + pop.h - 8) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){pop.x + 8, y, pop.w - 16, pop.h - (y - pop.y) - 8},
                                   "Recipe choices seed editable stack, response, texture, face, and graph panes.",
                                   palette.text_muted);
    }
}

int MaterialEditorRenderCompactPaneControls(SDL_Renderer* renderer,
                                            SDL_Rect content_bounds,
                                            int top_y,
                                            int bottom_y) {
    SceneObject* obj = material_editor_focused_object();
    RayTracingThemePalette palette = material_editor_palette();
    SDL_Rect shell_bounds = {content_bounds.x,
                             top_y,
                             content_bounds.w,
                             bottom_y - top_y};
    SDL_Rect pane_bounds;
    int cursor_y = 0;
    int pane_bottom = 0;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    int selected_faces = MaterialEditorSelectedFaceGroupCount();
    int focused_faces = MaterialEditorFocusedFaceGroupCount();
    char edit_text[48];

    material_editor_reset_compact_control_rects();
    s_material_editor_compact_layout_rects =
        MaterialEditorCompactLayoutBuild(shell_bounds, MaterialEditorIdentityPopoverOpen());
    if (!renderer || content_bounds.w <= 0 || top_y >= bottom_y) return bottom_y;

    if (obj && s_material_editor_active_face_group_index >= 0) {
        RuntimeMaterialTextureLayer active_layer = {0};
        bool has_override = false;
        if (SceneEditorMaterialStackHasObjectStack(focused_index) &&
            material_editor_get_active_layer(obj, NULL, &active_layer, NULL)) {
            has_override = SceneEditorMaterialFacePlacementHasOverrideForLayer(
                focused_index,
                s_material_editor_active_face_group_index,
                active_layer.layerId);
        } else {
            has_override =
                SceneEditorMaterialFacePlacementHasOverride(focused_index,
                                                            s_material_editor_active_face_group_index);
        }
        snprintf(edit_text,
                 sizeof(edit_text),
                 "Face #%d %s",
                 s_material_editor_active_face_group_index,
                 has_override ? "override" : "default");
    } else {
        snprintf(edit_text, sizeof(edit_text), "Object defaults");
    }

    material_editor_draw_shell(renderer, obj, focused_index, palette);
    pane_bounds = s_material_editor_compact_layout_rects.content;
    cursor_y = pane_bounds.y;
    pane_bottom = pane_bounds.y + pane_bounds.h;

    if (!obj) {
        RenderLabelTextWrappedLeft(renderer,
                                   pane_bounds,
                                   "No object selected. Select an object in Objects mode first.",
                                   palette.text_muted);
        material_editor_draw_identity_popover(renderer,
                                              obj,
                                              focused_index,
                                              selected_faces,
                                              focused_faces,
                                              edit_text,
                                              palette);
        material_editor_draw_recipe_menu(renderer, palette);
        return bottom_y;
    }

    if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_STACK) {
        cursor_y = MaterialEditorDrawLayerList(renderer, pane_bounds, cursor_y, pane_bottom, obj, palette);
        if (material_editor_use_object_layer_controls(obj)) {
            cursor_y =
                MaterialEditorDrawLayerKindButtons(renderer, pane_bounds, cursor_y, pane_bottom, obj, palette);
        }
    } else if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_RESPONSE) {
        cursor_y = MaterialEditorDrawCompactResponsePane(renderer,
                                                         pane_bounds,
                                                         cursor_y,
                                                         pane_bottom,
                                                         obj,
                                                         palette);
    } else if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_TEXTURES) {
        cursor_y = material_editor_draw_texture_channel_readback(renderer,
                                                                 pane_bounds,
                                                                 cursor_y,
                                                                 pane_bottom,
                                                                 focused_index,
                                                                 palette);
        cursor_y = MaterialEditorAuthoredTextureBindingRenderPaneControls(renderer,
                                                                          pane_bounds,
                                                                          cursor_y,
                                                                          pane_bottom,
                                                                          focused_index,
                                                                          palette);
        cursor_y =
            material_editor_draw_texture_kind_buttons(renderer, pane_bounds, cursor_y, pane_bottom, obj, palette);
        cursor_y =
            material_editor_draw_placement_controls(renderer, pane_bounds, cursor_y, pane_bottom, obj, palette);
    } else if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_FACE) {
        cursor_y = material_editor_draw_face_controls(renderer,
                                                      pane_bounds,
                                                      cursor_y,
                                                      pane_bottom,
                                                      obj,
                                                      focused_index,
                                                      selected_faces,
                                                      palette);
    } else if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_GRAPH) {
        cursor_y = material_editor_draw_graph_readback(renderer,
                                                       pane_bounds,
                                                       cursor_y,
                                                       pane_bottom,
                                                       palette);
    } else if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_PROOF) {
        cursor_y = material_editor_draw_proof_controls(renderer, pane_bounds, cursor_y, pane_bottom, palette);
    }

    (void)cursor_y;
    material_editor_draw_identity_popover(renderer,
                                          obj,
                                          focused_index,
                                          selected_faces,
                                          focused_faces,
                                          edit_text,
                                          palette);
    material_editor_draw_recipe_menu(renderer, palette);
    return bottom_y;
}
