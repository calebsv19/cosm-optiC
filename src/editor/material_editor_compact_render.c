#include "render/runtime_surface_graph.h"
#include "editor/scene_editor_surface_material_panel.h"
#include "editor/scene_editor_surface_mapping_panel.h"
#include "editor/scene_editor_surfaces.h"
#include "render/runtime_surface_mapping.h"
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
                           255);
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
    (void)focused_index;
    MaterialEditorRecipeReadback recipe = {0};
    SDL_Rect area = s_material_editor_compact_layout_rects.identity_header;
    s_material_editor_compact_layout_rects.identity_disclosure = (SDL_Rect){0};
    if (!obj || !MaterialEditorBuildRecipeReadback(&recipe)) return;
    const char* labels[] = {"Material", "Pattern", "Finish"};
    const char* values[] = {recipe.family_label, recipe.surface_label, recipe.finish_label};
    int row_h = animation_config_scale_text_point_size(&animSettings, 30, 30);
    int label_w = area.w * 32 / 100;
    for (int i=0; i<3; ++i) {
        SDL_Rect row = {area.x, area.y + i*row_h, label_w-6, row_h-4};
        MaterialEditorTextLeft(renderer,row,labels[i],palette.text_muted);
        s_recipe_action_rects[i]=(SDL_Rect){area.x+label_w,row.y,area.w-label_w,row.h};
        char value[100]; snprintf(value,sizeof(value),"%s  v",values[i]);
        MaterialEditorDrawButton(renderer,s_recipe_action_rects[i],value,
            MaterialEditorGetRecipeMenuAxis()==(MaterialEditorRecipeAxis)i,palette);
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
    snprintf(line,sizeof(line),"%d layers · %d texture outputs",readback.compiled_stack_layer_count,readback.channel_ref_count);
    RenderLabelTextLeft(renderer,(SDL_Rect){bounds.x,cursor_y,bounds.w,22},line,palette.text_muted);
    cursor_y+=26;
    RenderLabelTextWrappedLeft(renderer,(SDL_Rect){bounds.x,cursor_y,bounds.w,42},
        "Layer-based graph. Connected node editing is not available yet.",palette.text_muted);
    cursor_y+=48;
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

int MaterialEditorRenderCompactPaneControls(SDL_Renderer* renderer,
                                            SDL_Rect content_bounds,
                                            int top_y, int bottom_y) {
    SceneObject* obj = material_editor_focused_object();
    RayTracingThemePalette palette = material_editor_palette();
    int index = MaterialEditorResolveFocusedObjectIndex();
    SDL_Rect shell = {content_bounds.x,top_y,content_bounds.w,bottom_y-top_y};
    material_editor_reset_compact_control_rects();
    SceneEditorSurfaceMaterialPanelInvalidateControls();
    s_material_editor_compact_layout_rects = MaterialEditorCompactLayoutBuild(shell,false);
    memset(s_material_editor_compact_layout_rects.tab_rects,0,sizeof(s_material_editor_compact_layout_rects.tab_rects));
    if(!renderer || shell.w<=0 || shell.h<=0)return bottom_y;
    int y=SceneEditorSurfaceMaterialHeaderRender(renderer,shell,index);
    if(SceneEditorSurfaceMaterialHeaderModal() || !obj)return y;
    SDL_Rect body={shell.x,y,shell.w,bottom_y-y};
    if(body.h<=0)return y;
    if(RuntimeSurfaceMappingActive(index) || RuntimeSurfaceGraphActive(index))
        return SceneEditorSurfaceMaterialPanelRender(renderer,body,index);
    int section=SceneEditorSurfaceMaterialSection();
    if(section==0) {
        int row_h=animation_config_scale_text_point_size(&animSettings,30,30);
        s_material_editor_compact_layout_rects.identity_header=(SDL_Rect){body.x,y,body.w,row_h*3};
        material_editor_draw_shell(renderer,obj,index,palette);y+=row_h*3+6;
        y=MaterialEditorDrawCompactResponsePane(renderer,body,y,bottom_y,obj,palette);
        material_editor_draw_recipe_menu(renderer,palette);
    } else if(section==1) {
        const MaterialEditorSubPane panes[]={MATERIAL_EDITOR_SUBPANE_TEXTURES,MATERIAL_EDITOR_SUBPANE_STACK,MATERIAL_EDITOR_SUBPANE_GRAPH};
        const char* labels[]={"Pattern","Layers","Legacy graph"};
        MaterialEditorSubPane pane=MaterialEditorGetActiveSubPane();
        if(pane!=panes[0] && pane!=panes[1] && pane!=panes[2])pane=panes[1];
        for(int i=0;i<3;++i) {
            SDL_Rect tab={body.x+i*body.w/3,y,body.w/3-3,26};
            s_material_editor_compact_layout_rects.tab_rects[panes[i]]=tab;
            MaterialEditorDrawButton(renderer,tab,labels[i],pane==panes[i],palette);
        }
        y+=30;
        if(pane==MATERIAL_EDITOR_SUBPANE_STACK) {
            y=MaterialEditorDrawLayerList(renderer,body,y,bottom_y,obj,palette);
            if(material_editor_use_object_layer_controls(obj))
                y=MaterialEditorDrawLayerKindButtons(renderer,body,y,bottom_y,obj,palette);
            y=MaterialEditorDrawLayerComposition(renderer,body,y,bottom_y,palette);
        } else if(pane==MATERIAL_EDITOR_SUBPANE_TEXTURES) {
            y=MaterialEditorAuthoredTextureBindingRenderPaneControls(renderer,body,y,bottom_y,index,palette);
            y=material_editor_draw_texture_kind_buttons(renderer,body,y,bottom_y,obj,palette);
            y=material_editor_draw_placement_controls(renderer,body,y,bottom_y,obj,palette);
            y=MaterialEditorDrawPatternParameters(renderer,body,y,bottom_y,obj,palette);
        } else y=material_editor_draw_graph_readback(renderer,body,y,bottom_y,palette);
    } else if(section==2) {
        y=SceneEditorSurfaceMappingPanelRender(renderer,body,y,index,true);
        y=material_editor_draw_face_controls(renderer,body,y,bottom_y,obj,index,
            MaterialEditorSelectedFaceGroupCount(),palette);
    } else {
        y=MaterialEditorDrawLayerDiagnostics(renderer,body,y,bottom_y,palette);
        y=material_editor_draw_texture_channel_readback(renderer,body,y,bottom_y,index,palette);
        y=material_editor_draw_proof_controls(renderer,body,y,bottom_y,palette);
    }
    return y;
}
